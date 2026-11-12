#pragma once

#include <array>

namespace fast_livo::detail
{
struct VioErrorPartial
{
  float error = 0;
  int measurements = 0;
  int rejected = 0;
};

template<std::size_t Threads>
VioErrorPartial combine_vio_errors(const std::array<VioErrorPartial, Threads> &partials)
{
  // 固定合法的线程编号顺序，并阻止 fast-math 重新结合浮点加法。
  volatile float error = 0;
  int measurements = 0;
  int rejected = 0;
  for (const auto &partial : partials)
  {
    error = error + partial.error;
    measurements += partial.measurements;
    rejected += partial.rejected;
  }
  return {error, measurements, rejected};
}
}
