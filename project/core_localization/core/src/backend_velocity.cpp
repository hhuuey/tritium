/*
This file is part of FAST-LIVO2: Fast, Direct LiDAR-Inertial-Visual Odometry.
Developer: Chunran Zheng <zhengcr@connect.hku.hk>
For commercial use, please contact the developer or Prof. Fu Zhang <fuzhang@hku.hk>.
This file is subject to the terms and conditions in the accompanying LICENSE file.
*/
#define FAST_LIVO_BACKEND_INTERNAL
#include "fast_livo/backend.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace fast_livo::backend_detail
{
bool interpolateLinear(const VelocitySeries& series, double t, double& value)
{
  if (series.times.size() < 2 || t < series.times.front() || t > series.times.back())
    return false;
  auto upper = std::upper_bound(series.times.begin(), series.times.end(), t);
  if (upper == series.times.begin())
  {
    value = series.values.front();
    return true;
  }
  if (upper == series.times.end())
  {
    value = series.values.back();
    return true;
  }
  const size_t i = static_cast<size_t>(upper - series.times.begin());
  const double t0 = series.times[i - 1];
  const double t1 = series.times[i];
  if (t1 <= t0)
    return false;
  const double a = (t - t0) / (t1 - t0);
  value = series.values[i - 1] * (1.0 - a) + series.values[i] * a;
  return true;
}

void smoothVelocitySeries(VelocitySeries& series)
{
  if (series.values.size() < 5)
    return;
  std::vector<double> smoothed(series.values.size(), 0.0);
  for (size_t i = 0; i < series.values.size(); ++i)
  {
    const size_t begin = (i < 2) ? 0 : i - 2;
    const size_t end = std::min(series.values.size() - 1, i + 2);
    double sum = 0.0;
    for (size_t j = begin; j <= end; ++j)
      sum += series.values[j];
    smoothed[i] = sum / static_cast<double>(end - begin + 1);
  }
  series.values.swap(smoothed);
}

bool normalizeVelocitySeries(VelocitySeries& series)
{
  if (series.values.size() < 20)
    return false;
  const double mean = std::accumulate(series.values.begin(), series.values.end(), 0.0)
                    / static_cast<double>(series.values.size());
  double sq_sum = 0.0;
  for (double v : series.values)
  {
    const double d = v - mean;
    sq_sum += d * d;
  }
  const double stddev = std::sqrt(sq_sum / static_cast<double>(series.values.size()));
  if (!std::isfinite(stddev) || stddev < 0.05)
    return false;
  for (double& v : series.values)
    v = (v - mean) / stddev;
  return true;
}

VelocitySeries buildVelocitySeries(const std::vector<std::vector<double>>& data, double time_shift)
{
  VelocitySeries series;
  if (data.size() < 3)
    return series;
  constexpr int IDX_TIME = 0, IDX_X = 1, IDX_Y = 2, IDX_Z = 3;
  constexpr double MIN_DT = 1e-3, MAX_DT = 2.0, MAX_SPEED = 100.0;
  series.times.reserve(data.size() - 1);
  series.values.reserve(data.size() - 1);
  for (size_t i = 1; i < data.size(); ++i)
  {
    if (data[i].size() < 4 || data[i - 1].size() < 4)
      continue;
    const double t0 = data[i - 1][IDX_TIME];
    const double t1 = data[i][IDX_TIME];
    const double dt = t1 - t0;
    if (!std::isfinite(dt) || dt < MIN_DT || dt > MAX_DT)
      continue;
    const double dx = data[i][IDX_X] - data[i - 1][IDX_X];
    const double dy = data[i][IDX_Y] - data[i - 1][IDX_Y];
    const double dz = data[i][IDX_Z] - data[i - 1][IDX_Z];
    const double speed = std::sqrt(dx * dx + dy * dy + dz * dz) / dt;
    if (!std::isfinite(speed) || speed > MAX_SPEED)
      continue;
    series.times.push_back(0.5 * (t0 + t1) + time_shift);
    series.values.push_back(speed);
  }
  smoothVelocitySeries(series);
  return series;
}

VelocitySeries buildVelocitySeriesFromSamples(const std::vector<std::vector<double>>& data, double time_shift)
{
  VelocitySeries series;
  if (data.size() < 3)
    return series;
  constexpr int IDX_TIME = 0, IDX_VX = 1, IDX_VY = 2, IDX_VZ = 3;
  constexpr double MIN_DT = 1e-3, MAX_DT = 2.0, MAX_SPEED = 100.0;
  series.times.reserve(data.size());
  series.values.reserve(data.size());
  double last_time = std::numeric_limits<double>::quiet_NaN();
  for (const auto& sample : data)
  {
    if (sample.size() < 4)
      continue;
    const double t = sample[IDX_TIME];
    if (!std::isfinite(t))
      continue;
    if (std::isfinite(last_time))
    {
      const double dt = t - last_time;
      if (dt < MIN_DT || dt > MAX_DT)
      {
        last_time = t;
        continue;
      }
    }
    const double vx = sample[IDX_VX];
    const double vy = sample[IDX_VY];
    const double vz = sample[IDX_VZ];
    const double speed = std::sqrt(vx * vx + vy * vy + vz * vz);
    if (!std::isfinite(speed) || speed > MAX_SPEED)
    {
      last_time = t;
      continue;
    }
    series.times.push_back(t + time_shift);
    series.values.push_back(speed);
    last_time = t;
  }
  smoothVelocitySeries(series);
  return series;
}

double scoreVelocityOffset(const VelocitySeries& slam, const VelocitySeries& gps,
                           double fine_offset, int& pair_count)
{
  double sum_sg = 0.0, sum_ss = 0.0, sum_gg = 0.0;
  pair_count = 0;
  for (size_t i = 0; i < slam.times.size(); ++i)
  {
    double gps_value = 0.0;
    if (!interpolateLinear(gps, slam.times[i] - fine_offset, gps_value))
      continue;
    const double slam_value = slam.values[i];
    sum_sg += slam_value * gps_value;
    sum_ss += slam_value * slam_value;
    sum_gg += gps_value * gps_value;
    ++pair_count;
  }
  if (pair_count < 30 || sum_ss <= 1e-12 || sum_gg <= 1e-12)
    return -std::numeric_limits<double>::infinity();
  return sum_sg / std::sqrt(sum_ss * sum_gg);
}
}
