#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <cmath>

namespace fast_livo
{

struct Timestamp
{
  std::int64_t nanoseconds = 0;

  static Timestamp from_parts(std::int64_t seconds, std::uint32_t nanoseconds)
  {
    constexpr std::int64_t scale = 1000000000;
    if (seconds < 0 || nanoseconds >= scale ||
        seconds > (std::numeric_limits<std::int64_t>::max() - nanoseconds) / scale)
    {
      throw std::invalid_argument("传感器时间戳超出 int64 纳秒范围或纳秒字段无效");
    }
    return {seconds * scale + nanoseconds};
  }

  double seconds() const
  {
    return static_cast<double>(nanoseconds / 1000000000) +
           1e-9 * static_cast<double>(nanoseconds % 1000000000);
  }

  static Timestamp from_seconds(double value);
};

struct Vector3
{
  double x = 0;
  double y = 0;
  double z = 0;
};

struct ImuSample
{
  using Ptr = std::shared_ptr<ImuSample>;
  using ConstPtr = std::shared_ptr<const ImuSample>;

  Timestamp timestamp;
  std::string frame_id;
  Vector3 linear_acceleration;
  Vector3 angular_velocity;
  std::array<double, 4> orientation{{0, 0, 0, 0}};
  std::array<double, 9> orientation_covariance{};
  std::array<double, 9> angular_velocity_covariance{};
  std::array<double, 9> linear_acceleration_covariance{};
};

}
