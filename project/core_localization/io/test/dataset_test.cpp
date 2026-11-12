// SPDX-License-Identifier: GPL-2.0-only
#include "fast_livo/io/dataset.h"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <iostream>

namespace
{
using namespace fast_livo;
namespace fs = std::filesystem;

void check(bool condition, const char *message)
{
  if (!condition) throw std::runtime_error(message);
}
template<class F> void rejects(F action)
{
  bool rejected = false;
  try { action(); } catch (const std::exception &) { rejected = true; }
  check(rejected, "损坏数据或无效输入未被拒绝");
}
template<class T> io::Event event(std::uint64_t sequence, std::shared_ptr<T> sample)
{
  io::Event e;
  e.sequence = sequence; e.record_time = {1000 + static_cast<std::int64_t>(sequence)};
  e.physical_sequence = 100 - sequence;
  e.topic = "/sensor/" + std::to_string(sequence); e.source_type = "format-test";
  if constexpr (!std::is_same_v<T, GnssSample>)
  {
    e.has_header = true; e.header_time = sample->timestamp;
    e.header_sequence = 4000000000U + sequence;
  }
  e.sample = std::shared_ptr<const T>(sample);
  return e;
}

std::vector<io::Event> fixtures()
{
  auto imu = std::make_shared<ImuSample>();
  imu->timestamp = {9000000000000000123LL}; imu->frame_id = "imu/测试";
  imu->linear_acceleration = {1, -2, 3}; imu->angular_velocity = {4, 5, -6};
  imu->orientation = {{0.1, 0.2, 0.3, 0.4}};
  for (int i = 0; i < 9; ++i)
  {
    imu->orientation_covariance[i] = i - 1.5;
    imu->angular_velocity_covariance[i] = i + 0.25;
    imu->linear_acceleration_covariance[i] = i + 1.75;
  }
  auto cloud = std::make_shared<CloudSample>();
  cloud->timestamp = {17}; cloud->frame_id = "cloud-frame";
  auto &c = cloud->cloud;
  c.header.seq = 123; c.header.stamp = 9000000000000001ULL; c.header.frame_id = "pcl-frame";
  c.height = 2; c.width = 3; c.point_step = 32; c.row_step = 103;
  c.is_bigendian = true; c.is_dense = false;
  c.fields.resize(8);
  std::uint32_t offset = 0;
  constexpr std::uint8_t sizes[] = {1, 1, 2, 2, 4, 4, 4, 8};
  for (std::size_t i = 0; i < 8; ++i)
  {
    auto &f = c.fields[i];
    f.name = i == 7 ? "custom_time_ns" : "field_" + std::to_string(i);
    f.offset = offset; f.datatype = i + 1; f.count = i == 0 ? 2 : 1;
    offset += sizes[i] * f.count;
  }
  c.data.resize(206);
  for (std::size_t i = 0; i < c.data.size(); ++i) c.data[i] = (i * 43) & 255;
  auto livox = std::make_shared<LivoxSample>();
  livox->timestamp = {8}; livox->frame_id = "livox-frame";
  livox->timebase = UINT64_MAX - 100; livox->point_num = 2; livox->lidar_id = 7; livox->rsvd = {{3, 8, 255}};
  livox->points = {{12345, 1.25f, -2.5f, 3.75f, 255, 33, 6}, {UINT32_MAX, -4, 5, 6, 8, 9, 10}};
  auto image = std::make_shared<ImageSample>();
  image->timestamp = {6}; image->frame_id = "camera";
  cv::Mat backing(3, 7, CV_8UC3, cv::Scalar(11, 222, 33));
  image->image = backing(cv::Rect(1, 1, 3, 2));
  image->image.at<cv::Vec3b>(1, 2) = {254, 0, 127};
  auto gnss = std::make_shared<GnssSample>();
  gnss->time.week = 2400; gnss->time.tow = 123456.123456789;
  gnss->fix_type = 3; gnss->valid_fix = true; gnss->diff_soln = true; gnss->carr_soln = 2; gnss->num_sv = 42;
  double value = -8.125;
  for (double *v : {&gnss->latitude, &gnss->longitude, &gnss->altitude, &gnss->height_msl,
                   &gnss->h_acc, &gnss->v_acc, &gnss->p_dop, &gnss->vel_n, &gnss->vel_e,
                   &gnss->vel_d, &gnss->vel_acc}) { *v = value; value += 1.25; }
  return {event(4, imu), event(9, cloud), event(10, livox), event(20, image), event(21, gnss)};
}

void same(const io::Event &a, const io::Event &b)
{
  check(a.sequence == b.sequence && a.physical_sequence == b.physical_sequence &&
        a.record_time.nanoseconds == b.record_time.nanoseconds &&
        a.header_time.nanoseconds == b.header_time.nanoseconds && a.has_header == b.has_header &&
        a.header_sequence == b.header_sequence && a.topic == b.topic && a.source_type == b.source_type,
        "事件元数据或顺序改变");
  check(a.kind() == b.kind(), "传感器类型改变");
  std::visit([&](const auto &expected) {
    using T = std::decay_t<decltype(*expected)>;
    const auto &s = *std::get<std::shared_ptr<const T>>(b.sample);
    const auto &e = *expected;
    if constexpr (!std::is_same_v<T, GnssSample>)
      check(e.timestamp.nanoseconds == s.timestamp.nanoseconds && e.frame_id == s.frame_id, "header 丢失");
    if constexpr (std::is_same_v<T, ImuSample>)
      check(e.linear_acceleration.x == s.linear_acceleration.x && e.linear_acceleration.y == s.linear_acceleration.y &&
            e.linear_acceleration.z == s.linear_acceleration.z && e.angular_velocity.x == s.angular_velocity.x &&
            e.angular_velocity.y == s.angular_velocity.y && e.angular_velocity.z == s.angular_velocity.z &&
            e.orientation == s.orientation && e.orientation_covariance == s.orientation_covariance &&
            e.angular_velocity_covariance == s.angular_velocity_covariance &&
            e.linear_acceleration_covariance == s.linear_acceleration_covariance, "IMU 字段丢失");
    if constexpr (std::is_same_v<T, CloudSample>)
    {
      const auto &x = e.cloud; const auto &y = s.cloud;
      check(x.header.seq == y.header.seq && x.header.stamp == y.header.stamp &&
            x.header.frame_id == y.header.frame_id && x.width == y.width && x.height == y.height &&
            x.point_step == y.point_step && x.row_step == y.row_step && x.is_dense == y.is_dense &&
            x.is_bigendian == y.is_bigendian && x.data == y.data && x.fields.size() == y.fields.size(),
            "有组织大端点云或原字节丢失");
      for (std::size_t i = 0; i < x.fields.size(); ++i)
        check(x.fields[i].name == y.fields[i].name && x.fields[i].offset == y.fields[i].offset &&
              x.fields[i].datatype == y.fields[i].datatype && x.fields[i].count == y.fields[i].count,
              "PointCloud2 字段丢失");
    }
    if constexpr (std::is_same_v<T, LivoxSample>)
    {
      check(e.timebase == s.timebase && e.point_num == s.point_num && e.lidar_id == s.lidar_id &&
            e.rsvd == s.rsvd && e.points.size() == s.points.size(), "Livox 元数据丢失");
      for (std::size_t i = 0; i < e.points.size(); ++i)
      {
        const auto &x = e.points[i]; const auto &y = s.points[i];
        check(x.offset_time == y.offset_time && x.x == y.x && x.y == y.y && x.z == y.z &&
              x.reflectivity == y.reflectivity && x.tag == y.tag && x.line == y.line, "Livox 点字段丢失");
      }
    }
    if constexpr (std::is_same_v<T, ImageSample>)
      check(e.image.size() == s.image.size() && s.image.type() == CV_8UC3 &&
            cv::norm(e.image, s.image, cv::NORM_INF) == 0, "非连续 BGR 图像像素改变");
    if constexpr (std::is_same_v<T, GnssSample>)
    {
      check(e.time.week == s.time.week && e.time.tow == s.time.tow && e.fix_type == s.fix_type &&
            e.valid_fix == s.valid_fix && e.diff_soln == s.diff_soln && e.carr_soln == s.carr_soln &&
            e.num_sv == s.num_sv && e.latitude == s.latitude && e.longitude == s.longitude &&
            e.altitude == s.altitude && e.height_msl == s.height_msl && e.h_acc == s.h_acc &&
            e.v_acc == s.v_acc && e.p_dop == s.p_dop && e.vel_n == s.vel_n &&
            e.vel_e == s.vel_e && e.vel_d == s.vel_d && e.vel_acc == s.vel_acc, "GNSS 完整 PVT 丢失");
    }
  }, a.sample);
}

void run(const fs::path &root)
{
  check(!fs::exists(root), "测试目录已存在，拒绝覆盖");
  fs::create_directories(root);
  const auto source = fixtures();
  {
    io::Writer writer(root / "valid");
    for (const auto &e : source) writer.append(e);
    rejects([&] { writer.append(source.back()); });
    writer.finish(); writer.finish();
    rejects([&] { writer.append(source.back()); });
  }
  io::Reader reader(root / "valid");
  check(reader.summary().events == 5, "事件计数错误");
  io::Event actual;
  for (const auto &expected : source) { check(reader.next(actual), "事件丢失"); same(expected, actual); }
  check(!reader.next(actual), "出现额外事件");
  rejects([&] { io::Writer writer(root / "valid"); });
  rejects([&] { io::Reader limited(root / "valid", 10); limited.next(actual); });
  {
    io::Writer incomplete(root / "incomplete");
    incomplete.append(source.front());
  }
  rejects([&] { io::Reader incomplete(root / "incomplete"); });
  fs::copy(root / "valid", root / "truncated", fs::copy_options::recursive);
  fs::resize_file(root / "truncated/payload.bin", 1);
  rejects([&] { io::Reader truncated(root / "truncated"); });
  fs::copy(root / "valid", root / "corrupt", fs::copy_options::recursive);
  {
    std::fstream payload(root / "corrupt/payload.bin", std::ios::binary | std::ios::in | std::ios::out);
    char value; payload.get(value); payload.seekp(0); payload.put(value ^ 127);
  }
  rejects([&] { io::Reader corrupt(root / "corrupt"); corrupt.next(actual); });
  fs::copy(root / "valid", root / "version", fs::copy_options::recursive);
  auto manifest = YAML::LoadFile((root / "version/manifest.yaml").string());
  manifest["version"] = 1;
  { std::ofstream out(root / "version/manifest.yaml"); out << manifest; }
  rejects([&] { io::Reader version(root / "version"); });
  io::Writer empty(root / "empty"); empty.finish();
  io::Reader empty_reader(root / "empty"); check(!empty_reader.next(actual), "空数据集读取错误");
  io::Writer invalid(root / "invalid");
  auto invalid_event = source.front(); invalid_event.header_time = {5};
  rejects([&] { invalid.append(invalid_event); });
  invalid_event = source.front(); invalid_event.sample = ImuSample::ConstPtr{};
  rejects([&] { invalid.append(invalid_event); });
  fs::remove_all(root);
}
}

int main(int argc, char **argv)
{
  if (argc != 2) { std::cerr << "用法: dataset_test NEW_OUTPUT_DIRECTORY\n"; return 2; }
  try { run(argv[1]); std::cout << "中立数据集全传感器往返及损坏检测通过\n"; return 0; }
  catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
