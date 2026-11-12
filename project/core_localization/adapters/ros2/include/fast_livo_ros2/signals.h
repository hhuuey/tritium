#pragma once

#include <csignal>
#include <stdexcept>

namespace fast_livo_ros2
{
inline volatile std::sig_atomic_t stop_requested = 0;
inline void stop_signal(int) { stop_requested = 1; }
inline void install_signals()
{
  // 信号只置位，先完成收尾再关闭 ROS，保留收尾期间可能使用的发布器。
  if (std::signal(SIGINT, stop_signal) == SIG_ERR || std::signal(SIGTERM, stop_signal) == SIG_ERR)
    throw std::runtime_error("无法注册退出信号处理器");
}
}
