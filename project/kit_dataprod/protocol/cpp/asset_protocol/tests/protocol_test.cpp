#include <asset_protocol/protocol.hpp>
#include <iostream>
#include <limits>

using namespace asset_protocol;
namespace {
void require(bool condition, const std::string& message) { if (!condition) throw std::runtime_error(message); }
template<class F> void rejects(F function, const std::string& label) {
  try { function(); } catch (const std::exception&) { return; }
  throw std::runtime_error("Accepted invalid fixture: " + label);
}
template<class T> void models(const Json& fixtures, const std::string& group) {
  for (const auto& item : fixtures.at("valid_" + group)) {
    const auto payload = item.contains("value") ? item.at("value") : parse_json(item.at("document"));
    const auto restored = from_dict<T>(payload);
    const auto output = to_dict(restored);
    require(to_dict(from_dict<T>(output)) == output, "Roundtrip failed: " + item.at("name").get<std::string>());
  }
  for (const auto& item : fixtures.at("invalid_" + group)) {
    rejects([&] {
      const auto payload = item.contains("value") ? item.at("value") : parse_json(item.at("document"));
      static_cast<void>(from_dict<T>(payload));
    }, item.at("name"));
  }
}
std::string unhex(const std::string& text) {
  std::string output;
  for (std::size_t i = 0; i < text.size(); i += 2)
    output.push_back(static_cast<char>(std::stoul(text.substr(i, 2), nullptr, 16)));
  return output;
}
void lexical_boundaries() {
  rejects([] { parse_json("\xef\xbb\xbf{}"); }, "UTF-8 BOM");
  rejects([] { parse_json(std::string("{}\0garbage", 10)); }, "raw NUL after document");
  rejects([] { parse_json(std::string("\"a\0b\"", 5)); }, "raw NUL string value");
  require(parse_json("1e-400") == Json(0.0), "Finite floating underflow changed");
  // A scalar counts as a nesting level, just like an object or an array.
  const auto scalar_limit = std::string(1023, '[') + "0" + std::string(1023, ']');
  require(parse_json(encode_json(parse_json(scalar_limit))) == parse_json(scalar_limit), "Scalar depth boundary changed");
  const auto empty_limit = std::string(1024, '[') + std::string(1024, ']');
  require(parse_json(encode_json(parse_json(empty_limit))) == parse_json(empty_limit), "Empty container depth boundary changed");
  rejects([&] { parse_json("[" + scalar_limit + "]"); }, "scalar exceeds depth limit");
  rejects([&] { parse_json("[" + empty_limit + "]"); }, "container exceeds depth limit");
  rejects([&] { encode_json(Json::array({parse_json(scalar_limit)})); }, "constructed scalar exceeds depth limit");
  rejects([&] { encode_json(Json::array({parse_json(empty_limit)})); }, "constructed container exceeds depth limit");
  rejects([] { encode_json(Json(std::string("\xc0\xaf", 2))); }, "constructed invalid UTF-8");
  const Json nul_value = std::string("a\0b", 3);
  require(parse_json(encode_json(nul_value)) == nul_value, "NUL string value changed");
  std::string diagnostic;
  try { parse_json(std::string("\"bad") + char(0xff) + "\""); }
  catch (const std::exception& error) { diagnostic = error.what(); }
  require(!diagnostic.empty(), "Invalid UTF-8 input accepted");
  require(parse_json(encode_json(Json(diagnostic))) == Json(diagnostic), "Parser error contains invalid UTF-8");
}
}
int main() {
  try {
    lexical_boundaries();
    const auto fixtures = read_json(ASSET_PROTOCOL_FIXTURE_FILE);
    models<AssetRequest>(fixtures, "requests");
    models<AssetResult>(fixtures, "results");
    models<ArtifactRef>(fixtures, "artifact_refs");
    models<AssetError>(fixtures, "errors");
    models<AssetIdentity>(fixtures, "identities");
    for (const auto& item : fixtures.at("valid_json_documents")) {
      const auto output = parse_json(encode_json(parse_json(item.at("text"))));
      for (const auto& expected : item.value("expected_tokens", Json::array())) {
        auto node = output;
        for (const auto& component : expected.at("path"))
          node = component.is_string() ? node.at(component.get<std::string>()) : node.at(component.get<std::size_t>());
        require(node == parse_json(expected.at("token")), item.at("name"));
      }
    }
    for (const auto& item : fixtures.at("invalid_json_documents"))
      rejects([&] { parse_json(item.contains("text") ? item.at("text").get<std::string>() : unhex(item.at("hex"))); }, item.at("name"));
    for (const auto& item : fixtures.at("result_identities")) {
      const auto request = from_dict<AssetRequest>(item.at("request"));
      const auto result = from_dict<AssetResult>(item.at("result"));
      if (item.at("valid")) validate_result_identity(request, result);
      else rejects([&] { validate_result_identity(request, result); }, item.at("name"));
    }
    auto request = from_dict<AssetRequest>(fixtures.at("valid_requests").at(0).at("value"));
    const auto success = success_result(request, {{"max", std::numeric_limits<std::uint64_t>::max()}});
    require(success.metrics.at("max").get<std::uint64_t>() == std::numeric_limits<std::uint64_t>::max(), "Lost uint64 max");
    auto invalid = success;
    invalid.metadata["nan"] = std::numeric_limits<double>::quiet_NaN();
    rejects([&] { validate(invalid); }, "non-finite constructed value");
    for (const auto& unsupported : {Json::binary({1, 2}), Json(Json::value_t::discarded)}) {
      rejects([&] { encode_json(unsupported); }, "non-JSON constructed root");
      invalid = success;
      invalid.metadata["nested"] = Json::array({unsupported});
      rejects([&] { validate(invalid); }, "non-JSON constructed nested value");
    }
    std::cout << "Protocol conformance passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
