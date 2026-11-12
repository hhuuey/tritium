// SPDX-License-Identifier: GPL-2.0-only
#include "sample_codec.h"
#include <yaml-cpp/yaml.h>
#include <fstream>

namespace fast_livo::io
{
namespace
{
using detail::require;
constexpr char magic[] = "FLVIDX02";
constexpr const char *names[] = {"imu", "cloud", "livox", "image", "gnss"};

std::uint64_t checksum(const std::vector<std::uint8_t> &bytes)
{
  std::uint64_t hash = 14695981039346656037ULL;
  for (auto byte : bytes) { hash ^= byte; hash *= 1099511628211ULL; }
  return hash;
}
void write_bytes(std::ostream &out, const std::vector<std::uint8_t> &bytes)
{
  out.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
  require(bool(out), "写入数据集失败");
}
std::vector<std::uint8_t> read_bytes(std::istream &in, std::size_t count)
{
  std::vector<std::uint8_t> bytes(count);
  in.read(reinterpret_cast<char *>(bytes.data()), count);
  require(bool(in), "数据集文件截断");
  return bytes;
}
std::uint32_t read_size(std::istream &in)
{
  const auto bytes = read_bytes(in, 4);
  detail::Decoder decoder(bytes);
  return decoder.scalar<std::uint32_t>();
}
void update(Summary &summary, Kind kind)
{
  ++summary.events;
  ++summary.counts.at(static_cast<unsigned>(kind) - 1);
}
void check_order(const Event &event, const Summary &summary, std::uint64_t previous)
{
  require(!summary.events || event.sequence > previous, "事件序号必须严格递增，禁止重排");
}
}

struct Writer::Impl
{
  std::filesystem::path directory;
  std::ofstream index, payload;
  Summary summary;
  std::uint64_t offset = 0, previous = 0;
  bool finished = false, failed = false;

  explicit Impl(const std::filesystem::path &path) : directory(path)
  {
    require(!path.empty(), "输出目录不能为空");
    require(!std::filesystem::exists(path) || std::filesystem::is_empty(path), "拒绝覆盖非空输出目录");
    std::filesystem::create_directories(path);
    index.open(path / "index.bin", std::ios::binary);
    payload.open(path / "payload.bin", std::ios::binary);
    require(bool(index) && bool(payload), "不能创建数据集文件");
    index.write(magic, 8);
    require(bool(index), "不能写入索引标识");
  }
};

Writer::Writer(const std::filesystem::path &directory) : impl_(std::make_unique<Impl>(directory)) {}
Writer::~Writer() = default;
const Summary &Writer::summary() const { return impl_->summary; }

void Writer::append(const Event &event)
{
  auto &s = *impl_;
  require(!s.finished && !s.failed, "Writer 已结束或上次写入失败");
  detail::validate_header(event);
  check_order(event, s.summary, s.previous);
  const auto payload = detail::encode(event.sample);
  detail::Encoder entry;
  entry.scalar(event.sequence); entry.scalar(event.physical_sequence); entry.scalar(event.record_time.nanoseconds);
  entry.scalar<std::uint8_t>(event.has_header); entry.scalar(event.header_time.nanoseconds);
  entry.scalar(event.header_sequence); entry.scalar(static_cast<std::uint8_t>(event.kind()));
  entry.text(event.topic); entry.text(event.source_type);
  entry.scalar(s.offset); entry.scalar<std::uint64_t>(payload.size()); entry.scalar(checksum(payload));
  detail::Encoder size;
  size.scalar<std::uint32_t>(entry.bytes.size());
  s.failed = true;
  write_bytes(s.payload, payload); write_bytes(s.index, size.bytes); write_bytes(s.index, entry.bytes);
  s.offset += payload.size();
  s.previous = event.sequence;
  update(s.summary, event.kind());
  s.failed = false;
}

void Writer::finish()
{
  auto &s = *impl_;
  if (s.finished) return;
  require(!s.failed, "不能发布写入失败的数据集");
  s.index.flush(); s.payload.flush();
  require(bool(s.index) && bool(s.payload), "刷新数据集失败");
  s.index.close(); s.payload.close();
  require(!s.index.fail() && !s.payload.fail(), "关闭数据集失败");
  YAML::Node manifest;
  manifest["format"] = "fast-livo-neutral";
  manifest["version"] = 2;
  manifest["byte_order"] = "little";
  manifest["ordering"] = "bag-record-sequence";
  manifest["image_encoding"] = "bgr8-raw";
  manifest["index"] = "index.bin"; manifest["payload"] = "payload.bin";
  manifest["index_bytes"] = std::filesystem::file_size(s.directory / "index.bin");
  manifest["payload_bytes"] = s.offset;
  manifest["events"] = s.summary.events;
  for (std::size_t i = 0; i < 5; ++i) manifest["counts"][names[i]] = s.summary.counts[i];
  YAML::Emitter emitter;
  emitter << manifest;
  require(emitter.good(), "生成 manifest 失败");
  std::ofstream output(s.directory / "manifest.yaml.pending");
  output << emitter.c_str() << '\n';
  output.close();
  require(bool(output), "写入 manifest 失败");
  std::filesystem::rename(s.directory / "manifest.yaml.pending", s.directory / "manifest.yaml");
  s.finished = true;
}

struct Reader::Impl
{
  std::ifstream index, payload;
  Summary expected, observed;
  std::uint64_t max_bytes, payload_bytes, offset = 0, previous = 0;

  Impl(const std::filesystem::path &directory, std::uint64_t limit) : max_bytes(limit)
  {
    require(limit > 0 && limit <= static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max()),
            "读取大小限制无效");
    const auto manifest = YAML::LoadFile((directory / "manifest.yaml").string());
    require(manifest["format"].as<std::string>() == "fast-livo-neutral" &&
            manifest["version"].as<int>() == 2, "不支持的数据集格式版本");
    require(manifest["byte_order"].as<std::string>() == "little" &&
            manifest["ordering"].as<std::string>() == "bag-record-sequence" &&
            manifest["image_encoding"].as<std::string>() == "bgr8-raw", "不支持的数据集编码");
    require(manifest["index"].as<std::string>() == "index.bin" &&
            manifest["payload"].as<std::string>() == "payload.bin", "数据文件名称无效");
    payload_bytes = manifest["payload_bytes"].as<std::uint64_t>();
    require(std::filesystem::file_size(directory / "payload.bin") == payload_bytes &&
            std::filesystem::file_size(directory / "index.bin") == manifest["index_bytes"].as<std::uint64_t>(),
            "数据文件大小与 manifest 不一致");
    expected.events = manifest["events"].as<std::uint64_t>();
    std::uint64_t remaining = expected.events;
    for (std::size_t i = 0; i < 5; ++i)
    {
      expected.counts[i] = manifest["counts"][names[i]].as<std::uint64_t>();
      require(expected.counts[i] <= remaining, "manifest 计数无效");
      remaining -= expected.counts[i];
    }
    require(!remaining, "manifest 计数不匹配");
    index.open(directory / "index.bin", std::ios::binary);
    payload.open(directory / "payload.bin", std::ios::binary);
    require(bool(index) && bool(payload), "不能打开数据集文件");
    const auto prefix = read_bytes(index, 8);
    require(std::string(prefix.begin(), prefix.end()) == magic, "索引魔数无效");
  }
  void check_end()
  {
    require(index.peek() == std::char_traits<char>::eof() && offset == payload_bytes &&
            observed.counts == expected.counts, "数据集尾部或传感器计数不匹配");
  }
};

Reader::Reader(const std::filesystem::path &directory, std::uint64_t max_event_bytes)
  : impl_(std::make_unique<Impl>(directory, max_event_bytes)) {}
Reader::~Reader() = default;
const Summary &Reader::summary() const { return impl_->expected; }

bool Reader::next(Event &event)
{
  auto &s = *impl_;
  if (s.observed.events == s.expected.events) { s.check_end(); return false; }
  const auto size = read_size(s.index);
  require(size <= 2097228 && size <= s.max_bytes, "索引项过大");
  const auto bytes = read_bytes(s.index, size);
  detail::Decoder in(bytes);
  Event next;
  next.sequence = in.scalar<std::uint64_t>(); next.physical_sequence = in.scalar<std::uint64_t>();
  next.record_time.nanoseconds = in.scalar<std::int64_t>();
  next.has_header = in.boolean(); next.header_time.nanoseconds = in.scalar<std::int64_t>();
  next.header_sequence = in.scalar<std::uint32_t>();
  const auto kind = static_cast<Kind>(in.scalar<std::uint8_t>());
  next.topic = in.text(); next.source_type = in.text();
  const auto offset = in.scalar<std::uint64_t>(), length = in.scalar<std::uint64_t>();
  const auto hash = in.scalar<std::uint64_t>();
  require(!in.remaining(), "索引项存在未知尾部");
  require(offset == s.offset && offset <= s.payload_bytes && length <= s.payload_bytes - offset &&
          length <= s.max_bytes, "载荷偏移或长度无效");
  const auto payload = read_bytes(s.payload, length);
  require(checksum(payload) == hash, "载荷校验失败");
  next.sample = detail::decode(kind, payload);
  detail::validate_header(next);
  check_order(next, s.observed, s.previous);
  s.previous = next.sequence; s.offset += length;
  update(s.observed, next.kind());
  if (s.observed.events == s.expected.events) s.check_end();
  event = std::move(next);
  return true;
}
}
