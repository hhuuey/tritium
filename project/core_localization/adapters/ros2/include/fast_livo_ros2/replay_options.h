#pragma once

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace fast_livo_ros2
{
struct ReplayOptions
{
  std::string dataset;
  std::string stats_file;
  double rate = 1.0;
  double wait_timeout = 60.0;
  double drain_time = 1.0;
  double duration = 0.0;
  bool help = false;
};

inline double number(const std::string &text)
{
  std::size_t consumed = 0;
  const double value = std::stod(text, &consumed);
  if (consumed != text.size() || !std::isfinite(value))
    throw std::invalid_argument("回放数字参数必须是有限实数: " + text);
  return value;
}

inline ReplayOptions replay_options(const std::vector<std::string> &arguments)
{
  ReplayOptions options;
  for (std::size_t i = 1; i < arguments.size(); ++i)
  {
    const auto &key = arguments[i];
    if (key == "--help" || key == "-h")
    {
      options.help = true;
      continue;
    }
    if (key != "--dataset" && key != "--rate" && key != "--wait-timeout" &&
        key != "--drain-time" && key != "--duration" && key != "--stats-file")
      throw std::invalid_argument("未知回放参数: " + key);
    if (++i == arguments.size()) throw std::invalid_argument("回放参数缺少值: " + key);
    const auto &value = arguments[i];
    if (key == "--dataset") options.dataset = value;
    if (key == "--stats-file") options.stats_file = value;
    if (key == "--rate") options.rate = number(value);
    if (key == "--wait-timeout") options.wait_timeout = number(value);
    if (key == "--drain-time") options.drain_time = number(value);
    if (key == "--duration")
    {
      options.duration = number(value);
      if (options.duration <= 0) throw std::invalid_argument("--duration 必须为正秒数");
    }
  }
  if (options.help) return options;
  if (options.dataset.empty()) throw std::invalid_argument("必须指定 --dataset 中立数据集目录");
  if (options.rate <= 0 || options.wait_timeout <= 0 || options.drain_time < 0)
    throw std::invalid_argument("--rate/--wait-timeout 必须为正，--drain-time 必须非负");
  return options;
}
}
