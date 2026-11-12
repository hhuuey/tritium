#include "validation.hpp"
#include <cmath>
#include <unordered_set>

namespace asset_protocol::detail {
namespace {
// nlohmann validates JSON syntax and Unicode. SAX keeps decoded object keys and
// original number tokens available before a DOM could discard that information.
class StrictJsonSax final : public nlohmann::json_sax<Json> {
public:
  bool null() override { return scalar(); }
  bool boolean(bool) override { return scalar(); }
  bool number_integer(number_integer_t) override { return scalar(); }
  bool number_unsigned(number_unsigned_t) override { return scalar(); }
  bool number_float(number_float_t value, const string_t& token) override {
    // Integer literals outside int64/uint64 are routed here with their original
    // spelling. Reject them by token kind, never by comparing rounded doubles.
    if (token.find_first_of(".eE") == std::string::npos)
      reject("JSON integer exceeds supported range");
    if (!std::isfinite(value)) reject("Non-finite JSON number");
    return scalar();
  }
  bool string(string_t&) override { return scalar(); }
  bool binary(binary_t&) override { reject("Unsupported non-JSON value"); }
  bool start_object(std::size_t) override {
    enter();
    keys_.emplace_back();
    return true;
  }
  bool key(string_t& key) override {
    if (key.find('\0') != std::string::npos) reject("NUL is not supported in JSON object keys");
    if (!keys_.back().insert(key).second) reject("Duplicate JSON object key: " + key);
    return true;
  }
  bool end_object() override { keys_.pop_back(); --depth_; return true; }
  bool start_array(std::size_t) override { enter(); return true; }
  bool end_array() override { --depth_; return true; }
  bool parse_error(std::size_t position, const std::string&, const nlohmann::detail::exception&) override {
    // Parser diagnostics can echo invalid input bytes. Keep errors safe for
    // UTF-8 stderr and structured failure results without copying that input.
    reject("Invalid JSON at byte " + std::to_string(position));
  }
private:
  bool scalar() const {
    if (depth_ >= 1024) reject("JSON nesting is too deep");
    return true;
  }
  void enter() {
    if (++depth_ > 1024) reject("JSON nesting is too deep");
  }
  std::size_t depth_ = 0;
  std::vector<std::unordered_set<std::string>> keys_;
};
}  // namespace
void validate_json_text(const std::string& text) {
  // nlohmann accepts an initial UTF-8 BOM, but protocol JSON remains strict.
  if (text.compare(0, 3, "\xef\xbb\xbf") == 0) reject("JSON must not begin with a UTF-8 BOM");
  // Its lexer also treats a raw NUL as EOF; only escaped NUL values are legal.
  if (text.find('\0') != std::string::npos) reject("Unescaped NUL in JSON text");
  StrictJsonSax validator;
  Json::sax_parse(text, &validator);
}
}  // namespace asset_protocol::detail
