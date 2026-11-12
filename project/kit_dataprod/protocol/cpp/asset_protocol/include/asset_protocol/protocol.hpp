#pragma once
#include "asset_protocol/generated.hpp"
#include <filesystem>
#include <initializer_list>
#include <stdexcept>

namespace asset_protocol {
using Json = nlohmann::json;

// All file readers share the same lexical boundary, including opaque metadata.
Json parse_json(const std::string& text);
std::string encode_json(const Json& value);
Json read_json(const std::filesystem::path& path);
void write_json_atomic(const std::filesystem::path& path, const Json& value);

namespace detail {
void validate_json_text(const std::string& text);
void validate_payload(const std::string& model, const Json& value);
Json normalize_payload(const std::string& model, Json value);
Json identity_json(const Json& value);
}

template<class T> T from_dict(const Json& value) {
  detail::validate_payload(detail::ModelName<T>::value, value);
  return detail::normalize_payload(detail::ModelName<T>::value, value).template get<T>();
}
template<class T> Json to_dict(const T& value) {
  Json document = detail::normalize_payload(detail::ModelName<T>::value, Json(value));
  detail::validate_payload(detail::ModelName<T>::value, document);
  return document;
}
template<class T> void validate(const T& value) { static_cast<void>(to_dict(value)); }
template<class T> AssetIdentity identity_of(const T& value) {
  return from_dict<AssetIdentity>(detail::identity_json(Json(value)));
}
AssetIdentity read_request_identity(const std::filesystem::path& path);
AssetRequest parse_request(const std::string& text);
AssetResult parse_result(const std::string& text);
AssetRequest read_request(const std::filesystem::path& path);
AssetResult read_result(const std::filesystem::path& path);
void write_request(const std::filesystem::path& path, const AssetRequest& value);
void write_result(const std::filesystem::path& path, const AssetResult& value);
const std::vector<ArtifactRef>& input_refs(const AssetRequest& request, const std::string& asset_name);
const ArtifactRef& input_ref(const AssetRequest& request, const std::string& asset_name);
std::string input_key(const AssetRequest& request, std::initializer_list<std::string> asset_names);
AssetResult success_result(const AssetIdentity& identity,
                          Json metrics = Json::object(), Json metadata = Json::object());
AssetResult failure_result(const AssetIdentity& identity, const AssetError& error,
                          Json metrics = Json::object(), Json metadata = Json::object());
template<class T> AssetResult success_result(const T& identity, Json metrics = Json::object(), Json metadata = Json::object()) {
  return success_result(identity_of(identity), std::move(metrics), std::move(metadata));
}
template<class T> AssetResult failure_result(const T& identity, const AssetError& error, Json metrics = Json::object(), Json metadata = Json::object()) {
  return failure_result(identity_of(identity), error, std::move(metrics), std::move(metadata));
}
void validate_result_identity(const AssetIdentity& request, const AssetResult& result);
template<class T> void validate_result_identity(const T& request, const AssetResult& result) {
  validate_result_identity(identity_of(request), result);
}
}
