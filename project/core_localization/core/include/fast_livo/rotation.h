#pragma once
#include "fast_livo/measurements.h"
#include <cmath>

namespace fast_livo
{
inline Quaternion quaternion_from_rpy(double roll, double pitch, double yaw)
{
  const double halfYaw = yaw * 0.5, halfPitch = pitch * 0.5, halfRoll = roll * 0.5;
  const double cosYaw = std::cos(halfYaw), sinYaw = std::sin(halfYaw);
  const double cosPitch = std::cos(halfPitch), sinPitch = std::sin(halfPitch);
  const double cosRoll = std::cos(halfRoll), sinRoll = std::sin(halfRoll);
  return {sinRoll*cosPitch*cosYaw-cosRoll*sinPitch*sinYaw,
          cosRoll*sinPitch*cosYaw+sinRoll*cosPitch*sinYaw,
          cosRoll*cosPitch*sinYaw-sinRoll*sinPitch*cosYaw,
          cosRoll*cosPitch*cosYaw+sinRoll*sinPitch*sinYaw};
}
}
