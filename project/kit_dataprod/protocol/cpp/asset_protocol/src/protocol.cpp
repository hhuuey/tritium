#include "asset_protocol/protocol.hpp"
#include "validation.hpp"
#include <valijson/adapters/nlohmann_json_adapter.hpp>
#include <valijson/constraint_builder.hpp>
#include <valijson/schema.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validator.hpp>
#include <mutex>

namespace asset_protocol {
namespace {
const Json& contract() {
  static const Json value = Json::parse(
#include "schema.inc"
  );
  return value;
}
const Json& definition(const std::string& name) { return contract().at("definitions").at(name); }

// JSON Schema format extension shared with Python's FormatChecker. Decode
// Unicode code points rather than treating UTF-8 bytes as regex characters.
bool blank(const std::string& value) {
  std::size_t position = 0;
  while (position < value.size()) {
    const auto lead = static_cast<unsigned char>(value[position++]);
    std::uint32_t point = lead;
    if (lead >= 0x80) {
      const int count = lead < 0xe0 ? 1 : lead < 0xf0 ? 2 : 3;
      point = lead & (count == 1 ? 0x1f : count == 2 ? 0x0f : 0x07);
      for (int i = 0; i < count; ++i) point = (point << 6) | (static_cast<unsigned char>(value.at(position++)) & 0x3f);
    }
    if (!((point >= 9 && point <= 13) || (point >= 28 && point <= 32) || point == 0x85 ||
          point == 0xa0 || point == 0x1680 || (point >= 0x2000 && point <= 0x200a) ||
          point == 0x2028 || point == 0x2029 || point == 0x202f || point == 0x205f || point == 0x3000)) return false;
  }
  return true;
}
class NonBlankConstraint final : public valijson::constraints::PolyConstraint {
public:
  bool validate(const valijson::adapters::Adapter& value, const std::vector<std::string>& context,
                valijson::ValidationResults* results) const override {
    if (!value.isString() || !blank(value.asString())) return true;
    if (results) results->pushError(context, "String must be non-blank");
    return false;
  }
private:
  Constraint* cloneInto(void* memory) const override { return new (memory) NonBlankConstraint(*this); }
  std::size_t sizeOf() const override { return sizeof(*this); }
};
class FormatBuilder final : public valijson::ConstraintBuilder {
public:
  valijson::constraints::Constraint* make(const valijson::adapters::Adapter& node) const override {
    if (node.asString() != "non-blank") detail::reject("Unsupported protocol schema format: " + node.asString());
    return new NonBlankConstraint;
  }
};
const valijson::Schema& compiled_schema(const std::string& name) {
  static std::map<std::string, std::unique_ptr<valijson::Schema>> schemas;
  static std::mutex mutex;
  const std::lock_guard<std::mutex> lock(mutex);
  auto& result = schemas[name];
  if (!result) {
    Json root = {{"$ref", "#/definitions/" + name}, {"definitions", contract().at("definitions")}};
    auto built = std::make_unique<valijson::Schema>();
    valijson::SchemaParser parser(valijson::SchemaParser::kDraft7);
    parser.addConstraintBuilder("format", new FormatBuilder);
    parser.populateSchema(valijson::adapters::NlohmannJsonAdapter(root), *built);
    result = std::move(built);
  }
  return *result;
}
void defaults(Json& value, const Json& schema) {
  if (!schema.is_object()) return;
  if (schema.contains("$ref")) {
    defaults(value, contract().at(Json::json_pointer(schema.at("$ref").get<std::string>().substr(1))));
    return;
  }
  if (value.is_object()) {
    const auto properties = schema.value("properties", Json::object());
    for (auto it = properties.begin(); it != properties.end(); ++it) {
      const auto required = schema.value("required", std::vector<std::string>{});
      if (value.contains(it.key()) && value.at(it.key()).is_null() && !it.value().contains("default") &&
          std::find(required.begin(), required.end(), it.key()) == required.end()) value.erase(it.key());
      if (!value.contains(it.key()) && it.value().contains("default")) value[it.key()] = it.value().at("default");
      if (value.contains(it.key())) defaults(value.at(it.key()), it.value());
    }
    if (schema.contains("additionalProperties") && schema.at("additionalProperties").is_object()) {
      for (auto it = value.begin(); it != value.end(); ++it)
        if (!properties.contains(it.key())) defaults(it.value(), schema.at("additionalProperties"));
    }
  } else if (value.is_array() && schema.contains("items") && schema.at("items").is_object()) {
    for (auto& item : value) defaults(item, schema.at("items"));
  }
  if (!value.is_null() && schema.contains("anyOf")) {
    for (const auto& branch : schema.at("anyOf"))
      if (branch.value("type", "") != "null") defaults(value, branch);
  }
}
}
namespace detail {
void validate_payload(const std::string& model, const Json& value) {
  // Reject non-finite values and invalid Unicode in constructed models as well
  // as text input before the schema adapter can coerce any value.
  static_cast<void>(encode_json(value));
  valijson::Validator validator(valijson::Validator::kStrongTypes);
  valijson::ValidationResults errors;
  if (!validator.validate(compiled_schema(model), valijson::adapters::NlohmannJsonAdapter(value), &errors)) {
    valijson::ValidationResults::Error error;
    std::string message = "Protocol validation failed for " + model;
    if (errors.popError(error)) message += ": " + error.description;
    reject(message);
  }
  if (model == "AssetRequest") {
    for (auto it = value.at("inputs").begin(); it != value.at("inputs").end(); ++it)
      for (const auto& reference : it.value())
        if (reference.at("asset_name") != it.key()) reject("ArtifactRef asset_name must match its input key");
  }
}
Json normalize_payload(const std::string& model, Json value) {
  defaults(value, definition(model));
  return value;
}
Json identity_json(const Json& value) {
  if (!value.is_object()) reject("Request identity must be a JSON object");
  Json identity = Json::object();
  for (auto it = definition("AssetIdentity").at("properties").begin();
       it != definition("AssetIdentity").at("properties").end(); ++it)
    if (value.contains(it.key())) identity[it.key()] = value.at(it.key());
  return identity;
}
}
AssetIdentity read_request_identity(const std::filesystem::path& path) {
  return from_dict<AssetIdentity>(detail::identity_json(read_json(path)));
}
AssetRequest parse_request(const std::string& text) { return from_dict<AssetRequest>(parse_json(text)); }
AssetResult parse_result(const std::string& text) { return from_dict<AssetResult>(parse_json(text)); }
AssetRequest read_request(const std::filesystem::path& path) { return from_dict<AssetRequest>(read_json(path)); }
AssetResult read_result(const std::filesystem::path& path) { return from_dict<AssetResult>(read_json(path)); }
void write_request(const std::filesystem::path& path, const AssetRequest& value) { write_json_atomic(path, to_dict(value)); }
void write_result(const std::filesystem::path& path, const AssetResult& value) { write_json_atomic(path, to_dict(value)); }
const std::vector<ArtifactRef>& input_refs(const AssetRequest& request, const std::string& name) {
  validate(request);
  const auto found = request.inputs.find(name);
  if (found == request.inputs.end()) detail::reject(request.asset_name + " requires input " + name);
  return found->second;
}
const ArtifactRef& input_ref(const AssetRequest& request, const std::string& name) {
  const auto& references = input_refs(request, name);
  if (references.size() != 1) detail::reject(request.asset_name + " requires exactly one reference for " + name);
  return references.front();
}
std::string input_key(const AssetRequest& request, std::initializer_list<std::string> names) {
  validate(request);
  std::string match;
  std::size_t count = 0;
  for (const auto& name : names) if (request.inputs.count(name)) { match = name; ++count; }
  if (count != 1) detail::reject(request.asset_name + " requires exactly one matching input asset");
  return match;
}
AssetResult success_result(const AssetIdentity& identity, Json metrics, Json metadata) {
  Json value = to_dict(identity);
  value.update({{"status", "success"}, {"error", nullptr}, {"metrics", std::move(metrics)}, {"metadata", std::move(metadata)}});
  return from_dict<AssetResult>(value);
}
AssetResult failure_result(const AssetIdentity& identity, const AssetError& error, Json metrics, Json metadata) {
  Json value = to_dict(identity);
  value.update({{"status", "failed"}, {"error", to_dict(error)}, {"metrics", std::move(metrics)}, {"metadata", std::move(metadata)}});
  return from_dict<AssetResult>(value);
}
void validate_result_identity(const AssetIdentity& request, const AssetResult& result) {
  validate(result);
  if (to_dict(identity_of(request)) != to_dict(identity_of(result))) detail::reject("Asset result identity does not match the request");
}
}
