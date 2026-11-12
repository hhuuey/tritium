#include "fast_livo_ros2/replay_options.h"
#include "fast_livo_ros2/replay_publishers.h"
#include "fast_livo_ros2/signals.h"
#include "fast_livo_ros2/sensor_statistics.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <sstream>
#include <thread>

namespace fast_livo_ros2
{
namespace
{
using Steady = std::chrono::steady_clock;

std::chrono::nanoseconds duration(long double seconds)
{
  const long double nanoseconds = seconds * 1000000000.0L;
  if (nanoseconds < 0 || nanoseconds > std::numeric_limits<std::int64_t>::max())
    throw std::invalid_argument("回放等待时间超出 int64 纳秒范围");
  return std::chrono::nanoseconds(static_cast<std::int64_t>(nanoseconds));
}

bool running() { return rclcpp::ok() && !stop_requested; }

Steady::time_point after(std::chrono::nanoseconds delay)
{
  const auto now = Steady::now();
  if (delay > Steady::time_point::max() - now)
    throw std::invalid_argument("回放等待时间超出 steady_clock 范围");
  return now + delay;
}

void sleep_until(rclcpp::executors::SingleThreadedExecutor &executor, Steady::time_point deadline)
{
  while (running() && Steady::now() < deadline)
  {
    executor.spin_some();
    const auto remaining = deadline - Steady::now();
    if (remaining > Steady::duration::zero())
      std::this_thread::sleep_for(std::min(remaining, Steady::duration(std::chrono::milliseconds(10))));
  }
}

void wait_ready(rclcpp::Node &node, rclcpp::executors::SingleThreadedExecutor &executor,
                const ReplayPublishers &publishers, double timeout)
{
  const auto deadline = after(duration(timeout));
  auto last_log = Steady::now() - std::chrono::seconds(2);
  while (running())
  {
    const auto missing = publishers.missing_subscribers();
    if (missing.empty()) return;
    std::ostringstream names;
    for (const auto &topic : missing) names << ' ' << topic;
    if (Steady::now() >= deadline)
      throw std::runtime_error("等待订阅就绪超时，尚未匹配:" + names.str());
    if (Steady::now() - last_log >= std::chrono::seconds(2))
    {
      RCLCPP_INFO(node.get_logger(), "等待订阅匹配:%s", names.str().c_str());
      last_log = Steady::now();
    }
    sleep_until(executor, std::min(deadline, Steady::now() + std::chrono::milliseconds(20)));
  }
}

void replay(const ReplayOptions &options)
{
  fast_livo::io::Reader reader(options.dataset);
  if (reader.summary().events == 0) throw std::invalid_argument("中立数据集没有事件，拒绝空回放成功");
  auto node = std::make_shared<rclcpp::Node>("fast_livo_replayer");
  ReplayPublishers publishers(*node, reader.summary());
  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node);
  RCLCPP_INFO(node->get_logger(), "回放目录=%s，事件数=%llu，速率=%.6g；等待传感器及 /clock 订阅",
              options.dataset.c_str(), static_cast<unsigned long long>(reader.summary().events), options.rate);
  wait_ready(*node, executor, publishers, options.wait_timeout);
  fast_livo::io::Event event;
  const auto window = options.duration > 0 ? duration(options.duration).count() : 0;
  if (options.duration > 0 && window == 0) throw std::invalid_argument("--duration 不能小于一纳秒");
  std::int64_t first_record = -1;
  std::int64_t previous = -1;
  auto deadline = Steady::now();
  std::uint64_t count = 0;
  SensorStatistics statistics;
  bool eof = false;
  bool window_complete = false;
  while (running())
  {
    if (!reader.next(event))
    {
      eof = true;
      break;
    }
    if (first_record < 0) first_record = event.record_time.nanoseconds;
    if (window > 0 && event.record_time.nanoseconds >= first_record &&
        event.record_time.nanoseconds - first_record > window)
    {
      window_complete = true;
      break;
    }
    // 不排序 header/physical_sequence；重复记录时间保持 Reader 给出的 View 原序。
    if (previous >= 0 && event.record_time.nanoseconds >= previous)
    {
      const auto delay = duration((event.record_time.nanoseconds - previous) / (options.rate * 1e9L));
      if (delay > Steady::time_point::max() - deadline)
        throw std::invalid_argument("回放时间线超出 steady_clock 范围");
      deadline += delay;
    }
    else
    {
      if (previous >= 0) RCLCPP_WARN(node->get_logger(), "记录时间倒退，保持原序并重置 wall 时基");
      deadline = Steady::now();
    }
    previous = event.record_time.nanoseconds;
    sleep_until(executor, deadline);
    if (!running()) break;
    publishers.publish(event);
    std::visit([&statistics](const auto &sample) { statistics.record(*sample); }, event.sample);
    ++count;
    executor.spin_some();
    if (count == 1 || count % 10000 == 0)
      RCLCPP_INFO(node->get_logger(), "已发布 %llu/%llu，View序号=%llu，记录时间=%.9f",
                  static_cast<unsigned long long>(count),
                  static_cast<unsigned long long>(reader.summary().events),
                  static_cast<unsigned long long>(event.sequence), event.record_time.seconds());
  }
  if (eof || window_complete)
  {
    statistics.save(options.stats_file);
    RCLCPP_INFO(node->get_logger(), "PUBLISHED_INPUTS %s", statistics.json().c_str());
    publishers.drain(std::chrono::seconds(5));
    sleep_until(executor, after(duration(options.drain_time)));
    if (eof)
      RCLCPP_INFO(node->get_logger(), "EOF：已发布全部 %llu 个事件，退出回放器；定位节点仍需单独优化/关闭",
                  static_cast<unsigned long long>(count));
    else
      RCLCPP_INFO(node->get_logger(), "输入窗口完成：%.9f 秒，已发布 %llu 个事件；定位节点仍需单独优化/关闭",
                  options.duration, static_cast<unsigned long long>(count));
  }
  else RCLCPP_INFO(node->get_logger(), "回放已中断，已发布 %llu 个事件", static_cast<unsigned long long>(count));
  executor.remove_node(node);
}
}
}

int main(int argc, char **argv)
{
  int result = 0;
  try
  {
    rclcpp::init(argc, argv, rclcpp::InitOptions(), rclcpp::SignalHandlerOptions::None);
    fast_livo_ros2::install_signals();
    const auto options = fast_livo_ros2::replay_options(rclcpp::remove_ros_arguments(argc, argv));
    if (options.help)
      std::cout << "fast_livo_replayer --dataset DIRECTORY [--rate 1.0] [--duration SECONDS] "
                   "[--wait-timeout 60] [--drain-time 1] [--stats-file NEW_JSON]\n";
    else fast_livo_ros2::replay(options);
  }
  catch (const std::exception &error)
  {
    std::cerr << "FAST-LIVO2 ROS 2 回放失败: " << error.what() << '\n';
    result = 1;
  }
  catch (...)
  {
    std::cerr << "FAST-LIVO2 ROS 2 回放发生未知异常\n";
    result = 1;
  }
  if (rclcpp::ok()) rclcpp::shutdown();
  return result;
}
