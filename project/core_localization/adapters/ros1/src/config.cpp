#include "fast_livo_ros1/config.h"
#include <filesystem>
#include <stdexcept>

namespace fast_livo_ros1
{
fast_livo::Config load_config(ros::NodeHandle &node)
{
  fast_livo::Config config;
  ros::NodeHandle camera(node, "laserMapping");
#define FAST_LIVO_PARAMETER(type, field, key, initial) \
  { \
    auto &source = std::string(key).find('/') == std::string::npos ? camera : node; \
    source.param<type>(key, config.field, config.field); \
  }
#include "fast_livo/config_fields.inc"
#undef FAST_LIVO_PARAMETER
  node.param<std::string>("laserMapping/source_dir", config.source_dir, "");
  if (config.laserMapping_outputfilepath.empty())
    throw std::invalid_argument("必须设置 laserMapping/outputfilepath，结果不能写回源码目录");
  std::filesystem::create_directories(config.laserMapping_outputfilepath);
  return config;
}
}
