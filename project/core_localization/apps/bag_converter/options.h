// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "conversion.h"
#include <iostream>
#include <map>
#include <set>
#include <sstream>

namespace fast_livo::bag_converter
{
struct Options
{
  std::string bag, output;
  std::map<std::string, Role> topics{
      {"/livox/imu", Role::imu}, {"/livox/lidar", Role::lidar},
      {"/left_camera/image", Role::image}, {"/left_camera/image/compressed", Role::image},
      {"/ublox_driver/receiver_pvt", Role::gnss}};
  std::set<Role> required{Role::imu, Role::lidar, Role::image, Role::gnss};
};

inline Role role_from(const std::string &value)
{
  if (value == "imu") return Role::imu;
  if (value == "lidar") return Role::lidar;
  if (value == "image") return Role::image;
  if (value == "gnss") return Role::gnss;
  throw std::runtime_error("未知传感器角色: " + value);
}

inline std::vector<std::string> split(const std::string &value)
{
  std::vector<std::string> result;
  std::istringstream input(value);
  std::string item;
  while (std::getline(input, item, ','))
  {
    if (item.empty()) throw std::runtime_error("列表不能包含空项");
    result.push_back(item);
  }
  if (result.empty() || value.back() == ',') throw std::runtime_error("列表不能为空");
  return result;
}

inline Options parse(int argc, char **argv)
{
  Options options;
  std::set<std::string> seen;
  for (int i = 1; i < argc; ++i)
  {
    const std::string key = argv[i];
    if (i + 1 == argc || !seen.insert(key).second) throw std::runtime_error("参数缺值或重复: " + key);
    const std::string value = argv[++i];
    if (key == "--bag") { options.bag = value; continue; }
    if (key == "--output") { options.output = value; continue; }
    if (key == "--require")
    {
      options.required.clear();
      for (const auto &name : split(value)) options.required.insert(role_from(name));
      continue;
    }
    if (key != "--imu-topic" && key != "--lidar-topic" && key != "--image-topic" && key != "--gnss-topic")
      throw std::runtime_error("未知参数: " + key);
    const auto role = role_from(key.substr(2, key.size() - 8));
    for (auto it = options.topics.begin(); it != options.topics.end();)
    {
      if (it->second == role) it = options.topics.erase(it);
      else ++it;
    }
    for (const auto &topic : split(value))
      if (!options.topics.emplace(topic, role).second) throw std::runtime_error("同一话题不能指定给多个角色: " + topic);
  }
  if (options.bag.empty() || options.output.empty()) throw std::runtime_error("必须提供 --bag 和 --output");
  return options;
}

inline void usage()
{
  std::cout << "fast_livo_bag_converter --bag INPUT.bag --output DIRECTORY\n"
               "  [--imu-topic TOPIC,...] [--lidar-topic TOPIC,...]\n"
               "  [--image-topic TOPIC,...] [--gnss-topic TOPIC,...]\n"
               "  [--require imu,lidar,image,gnss]\n";
}
}
