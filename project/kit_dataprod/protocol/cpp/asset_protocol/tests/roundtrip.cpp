#include <asset_protocol/protocol.hpp>
#include <iostream>
int main(int argc, char** argv) {
  if (argc != 4) return 2;
  try {
    const std::string mode(argv[1]);
    if (mode == "request") asset_protocol::write_request(argv[3], asset_protocol::read_request(argv[2]));
    else if (mode == "result") asset_protocol::write_result(argv[3], asset_protocol::read_result(argv[2]));
    else if (mode == "json") asset_protocol::write_json_atomic(argv[3], asset_protocol::read_json(argv[2]));
    else return 2;
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
