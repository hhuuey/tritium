#pragma once

#include <yaml-cpp/yaml.h>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace fast_livo_ros2
{
inline void write_report(const std::string &path, const std::string &text)
{
  std::ofstream output(path);
  output << text << '\n';
  output.close();
  if (!output) throw std::runtime_error("无法写入验收报告: " + path);
}

inline std::vector<std::int64_t> native_timeline(const std::string &path)
{
  std::ifstream input(path);
  if (!input) throw std::runtime_error("无法读取 native 轨迹: " + path);
  std::vector<std::int64_t> result;
  std::string line, timestamp;
  while (std::getline(input, line))
  {
    std::istringstream fields(line);
    if (!(fields >> timestamp) || timestamp.front() == '#') continue;
    const auto dot = timestamp.find('.');
    auto fraction = dot == std::string::npos ? std::string() : timestamp.substr(dot + 1);
    if (fraction.size() > 9) throw std::runtime_error("native 时间戳小数精度超过纳秒");
    fraction.append(9 - fraction.size(), '0');
    const auto seconds = std::stoll(timestamp.substr(0, dot));
    result.push_back(seconds * 1000000000LL + std::stoll(fraction));
  }
  if (result.empty()) throw std::runtime_error("native 轨迹为空");
  return result;
}

inline bool same_receipts(const YAML::Node &received, const YAML::Node &published)
{
  for (std::size_t i = 0; i < 5; ++i)
  {
    if (received["counts"][i].as<std::uint64_t>() != published["counts"][i].as<std::uint64_t>()) return false;
    const auto r = received["last_stamp_ns"][i], p = published["last_stamp_ns"][i];
    if (r.IsNull() != p.IsNull()) return false;
    if (!r.IsNull() && r.as<std::int64_t>() != p.as<std::int64_t>()) return false;
  }
  return received["gnss_week"].as<std::uint32_t>() == published["gnss_week"].as<std::uint32_t>() &&
         received["gnss_tow"].as<double>() == published["gnss_tow"].as<double>();
}

inline bool same_timeline(const std::vector<std::int64_t> &actual, const std::vector<std::int64_t> &expected)
{
  if (actual.size() != expected.size()) return false;
  for (std::size_t i = 0; i < actual.size(); ++i)
    if (std::llabs(actual[i] - expected[i]) > 1000) return false;
  return true;
}
}
