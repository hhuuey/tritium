#pragma once

#include "fast_livo/engine.h"
#include "fast_livo_ros2/signals.h"
#include <cerrno>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <sys/select.h>
#include <unistd.h>

namespace fast_livo_ros2
{
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
