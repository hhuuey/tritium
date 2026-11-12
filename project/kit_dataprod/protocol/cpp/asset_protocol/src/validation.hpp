#pragma once
#include "asset_protocol/protocol.hpp"
namespace asset_protocol::detail {
[[noreturn]] inline void reject(const std::string& message) { throw std::runtime_error(message); }
}
