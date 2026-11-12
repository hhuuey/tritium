#include "asset_protocol/protocol.hpp"
#include "validation.hpp"
#include <cerrno>
#include <cmath>
#include <fcntl.h>
#include <fstream>
#include <iterator>
#include <unistd.h>

namespace asset_protocol {
namespace {
void validate_value(const Json& value, std::size_t depth = 1) {
  if (depth > 1024) detail::reject("JSON nesting is too deep");
  if (value.is_binary() || value.is_discarded()) detail::reject("Unsupported non-JSON value");
  if (value.is_number_float() && !std::isfinite(value.get<double>()))
    detail::reject("Non-finite JSON number");
  if (value.is_object()) {
    for (auto it = value.begin(); it != value.end(); ++it) {
      if (it.key().find('\0') != std::string::npos) detail::reject("NUL is not supported in JSON object keys");
      validate_value(it.value(), depth + 1);
    }
  } else if (value.is_array()) {
    for (const auto& item : value) validate_value(item, depth + 1);
  }
}
}
Json parse_json(const std::string& text) {
  detail::validate_json_text(text);
  return Json::parse(text);
}
std::string encode_json(const Json& value) {
  validate_value(value);
  // The DOM has unique keys and bounded native integer types. Strict dumping
  // checks UTF-8; scanning its generated JSON again adds no validation.
  return value.dump(2, ' ', false, Json::error_handler_t::strict);
}
Json read_json(const std::filesystem::path& path) {
  if (path.string().find('\0') != std::string::npos) detail::reject("JSON file path must not contain NUL");
  std::ifstream stream(path, std::ios::binary);
  if (!stream) detail::reject("Cannot open JSON file: " + path.string());
  const std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
  if (stream.bad()) detail::reject("Cannot read JSON file: " + path.string());
  return parse_json(text);
}
void write_json_atomic(const std::filesystem::path& path, const Json& value) {
  if (path.string().find('\0') != std::string::npos) detail::reject("JSON file path must not contain NUL");
  const auto document = encode_json(value) + "\n";
  const auto parent = path.has_parent_path() ? path.parent_path() : std::filesystem::path(".");
  std::filesystem::create_directories(parent);
  std::string pattern = path.string() + ".tmp.XXXXXX";
  std::vector<char> buffer(pattern.begin(), pattern.end()); buffer.push_back('\0');
  const int descriptor = ::mkstemp(buffer.data());
  if (descriptor < 0) detail::reject("Cannot create temporary JSON file: " + path.string());
  const std::filesystem::path temporary(buffer.data());
  bool opened = true;
  try {
    std::size_t offset = 0;
    while (offset < document.size()) {
      const auto written = ::write(descriptor, document.data() + offset, document.size() - offset);
      if (written < 0 && errno == EINTR) continue;
      if (written <= 0) detail::reject("Cannot write JSON file: " + temporary.string());
      offset += static_cast<std::size_t>(written);
    }
    if (::fsync(descriptor) != 0) detail::reject("Cannot sync JSON file: " + temporary.string());
    const int close_status = ::close(descriptor); opened = false;
    if (close_status != 0) detail::reject("Cannot close JSON file: " + temporary.string());
    std::filesystem::rename(temporary, path);
    const int directory = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY);
    if (directory < 0) detail::reject("Cannot open JSON parent directory: " + parent.string());
    const int sync_status = ::fsync(directory);
    const int directory_close_status = ::close(directory);
    if (sync_status || directory_close_status) detail::reject("Cannot sync JSON parent directory: " + parent.string());
  } catch (...) {
    if (opened) ::close(descriptor);
    std::error_code ignored; std::filesystem::remove(temporary, ignored);
    throw;
  }
}
}  // namespace asset_protocol
