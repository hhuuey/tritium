#include "fast_livo_ros2/config.h"
#include <algorithm>
#include <filesystem>
#include <limits>

namespace fast_livo_ros2
{
namespace
{
std::string parameter_name(std::string key)
{
  if (key.find('/') == std::string::npos) key = "laserMapping/" + key;
  std::replace(key.begin(), key.end(), '/', '.');
  return key;
}

template <typename T>
void read(rclcpp::Node &node, const std::string &key, T &value)
{
  value = node.declare_parameter<T>(parameter_name(key), value);
}

int checked_int(std::int64_t value)
{
  if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
    throw std::invalid_argument("ROS 2 整数参数超出核心 int 范围");
  return static_cast<int>(value);
}

void read(rclcpp::Node &node, const std::string &key, int &value)
{
  value = checked_int(node.declare_parameter<std::int64_t>(parameter_name(key), value));
}

void read(rclcpp::Node &node, const std::string &key, std::vector<int> &value)
{
  std::vector<std::int64_t> defaults(value.begin(), value.end());
  const auto values = node.declare_parameter<std::vector<std::int64_t>>(parameter_name(key), defaults);
  value.clear();
  for (const auto item : values) value.push_back(checked_int(item));
}
}

fast_livo::Config load_config(rclcpp::Node &node)
{
  fast_livo::Config config;
#define FAST_LIVO_PARAMETER(type, field, key, initial) read(node, key, config.field);
#include "fast_livo/config_fields.inc"
#undef FAST_LIVO_PARAMETER
  config.source_dir = node.declare_parameter<std::string>("laserMapping.source_dir", "");
  if (config.laserMapping_outputfilepath.empty())
    throw std::invalid_argument("必须设置 laserMapping.outputfilepath，结果不能写回源码目录");
  std::filesystem::create_directories(config.laserMapping_outputfilepath);
  return config;
}
}
