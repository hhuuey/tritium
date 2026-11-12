#include "fast_livo/sensor_types.h"

namespace fast_livo
{

Timestamp Timestamp::from_seconds(double value)
{
  if (!std::isfinite(value) || value < 0 || value > 9223372036.0)
    throw std::invalid_argument("秒时间戳超出有效范围");
  const auto seconds = static_cast<std::int64_t>(std::floor(value));
  const auto fraction = static_cast<std::int64_t>(std::round((value - seconds) * 1e9));
  return from_parts(seconds + fraction / 1000000000, fraction % 1000000000);
}

}
