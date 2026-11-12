#pragma once

#include <cstdio>

namespace fast_livo
{

template <typename... Args>
void log_info(const char *format, Args... args)
{
  std::fprintf(stderr, "[IMU] ");
  if constexpr (sizeof...(args) == 0)
    std::fputs(format, stderr);
  else
    std::fprintf(stderr, format, args...);
  std::fputc('\n', stderr);
}

}
