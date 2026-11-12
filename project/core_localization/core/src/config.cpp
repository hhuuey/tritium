#include "fast_livo/engine.h"
#include <yaml-cpp/yaml.h>
#include <stdexcept>

namespace fast_livo
{
namespace
{
YAML::Node lookup(const YAML::Node &root, const std::string &key)
{
  YAML::Node current = YAML::Clone(root);
  std::size_t begin = 0;
  while (begin < key.size())
  {
    const auto end = key.find('/', begin);
    const auto part = key.substr(begin, end - begin);
    if (!current.IsMap() || !current[part]) return {};
    current.reset(current[part]);
    if (end == std::string::npos) break;
    begin = end + 1;
  }
  return current;
}
}

Config load_config(const std::string &algorithm_file, const std::string &camera_file, const std::string &output_dir)
{
  const auto algorithm = YAML::LoadFile(algorithm_file);
  const auto camera = YAML::LoadFile(camera_file);
  Config config;
#define FAST_LIVO_PARAMETER(type, field, key, initial) \
  { auto value = lookup(algorithm, key); \
    if (!value || value.IsNull()) value.reset(lookup(camera, key)); \
    if (value && !value.IsNull()) config.field = value.as<type>(); }
#include "fast_livo/config_fields.inc"
#undef FAST_LIVO_PARAMETER
  config.laserMapping_outputfilepath = output_dir;
  if (output_dir.empty()) throw std::invalid_argument("结果目录不能为空");
  if (config.extrin_calib_extrinsic_T.size() != 3 || config.extrin_calib_extrinsic_R.size() != 9 ||
      config.extrin_calib_Pcl.size() != 3 || config.extrin_calib_Rcl.size() != 9 ||
      config.gps_extrinsic_T.size() != 3)
    throw std::invalid_argument("外参维度不匹配");
  return config;
}

}
