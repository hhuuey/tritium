#include <asset_protocol/protocol.hpp>
#include <iostream>

int main(int argc, char** argv) {
  if ((argc != 5 && argc != 6) || std::string(argv[1]) != "--request" ||
      std::string(argv[3]) != "--result" || (argc == 6 && std::string(argv[5]) != "--fail")) {
    std::cerr << "Usage: asset-protocol-example --request request.json --result result.json [--fail]\n";
    return 2;
  }
  std::optional<asset_protocol::AssetIdentity> identity;
  try {
    identity = asset_protocol::read_request_identity(argv[2]);
    const auto request = asset_protocol::read_request(argv[2]);
    // 实际算法在这里解析自己的 config_path 并写入 output_uri 所需产物。
    if (argc == 6) throw std::runtime_error("Example algorithm failure");
    const auto result = asset_protocol::success_result(request);
    asset_protocol::validate_result_identity(request, result);
    asset_protocol::write_result(argv[4], result);
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    if (identity) {
      try {
        const asset_protocol::AssetError failure{"algorithm", "EXAMPLE_FAILURE", error.what()};
        asset_protocol::write_result(argv[4], asset_protocol::failure_result(*identity, failure));
      } catch (const std::exception& write_error) { std::cerr << write_error.what() << '\n'; }
    }
    return 1;
  }
}
