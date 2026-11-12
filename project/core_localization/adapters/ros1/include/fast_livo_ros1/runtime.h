#pragma once

#include "fast_livo/engine.h"
#include <cerrno>
#include <csignal>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <sys/select.h>
#include <unistd.h>

namespace fast_livo_ros1
{
inline volatile std::sig_atomic_t stop_requested = 0;
inline void stop_signal(int) { stop_requested = 1; }
inline void install_signals()
{
  // 信号只置位，先完成核心收尾再关闭 ROS，保留落盘期间可能使用的发布器。
  if (std::signal(SIGINT, stop_signal) == SIG_ERR || std::signal(SIGTERM, stop_signal) == SIG_ERR)
    throw std::runtime_error("无法注册退出信号处理器");
}

class EnterTrigger
{
public:
  void poll(fast_livo::Engine &engine)
  {
    if (!active_) return;
    fd_set input;
    FD_ZERO(&input);
    FD_SET(STDIN_FILENO, &input);
    timeval timeout{};
    const int ready = select(STDIN_FILENO + 1, &input, nullptr, nullptr, &timeout);
    if (ready < 0 && errno == EINTR) return;
    if (ready < 0) throw std::runtime_error("无法监听 Enter 输入");
    if (ready == 0) return;
    char buffer[256];
    const auto count = read(STDIN_FILENO, buffer, sizeof(buffer));
    if (count < 0 && errno == EINTR) return;
    if (count < 0) throw std::runtime_error("无法读取 Enter 输入");
    if (count == 0) active_ = false;
    for (ssize_t i = 0; i < count; ++i)
      if (buffer[i] == '\n') engine.request_optimization();
  }
private:
  bool active_ = true;
};

class FinishGuard
{
public:
  explicit FinishGuard(fast_livo::Engine &engine) : engine_(engine) {}
  void close()
  {
    if (closed_) return;
    closed_ = true;
    // 即使等待后端失败，也必须尝试落盘/释放；错误继续向主函数传播。
    std::exception_ptr failure;
    try { engine_.wait_for_optimization(); }
    catch (...) { failure = std::current_exception(); }
    engine_.finish();
    if (failure) std::rethrow_exception(failure);
  }
  ~FinishGuard()
  {
    try { close(); }
    catch (const std::exception &error) { std::cerr << "FAST-LIVO2 finish 失败: " << error.what() << '\n'; }
    catch (...) { std::cerr << "FAST-LIVO2 finish 发生未知异常\n"; }
  }
private:
  fast_livo::Engine &engine_;
  bool closed_ = false;
};
}
