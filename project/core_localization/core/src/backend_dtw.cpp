/*
This file is part of FAST-LIVO2: Fast, Direct LiDAR-Inertial-Visual Odometry.
Developer: Chunran Zheng <zhengcr@connect.hku.hk>
For commercial use, please contact the developer or Prof. Fu Zhang <fuzhang@hku.hk>.
This file is subject to the terms and conditions in the accompanying LICENSE file.
*/
#define FAST_LIVO_BACKEND_INTERNAL
#include "fast_livo/backend.h"
#include "FastDTW/DTW.h"
#include "FastDTW/EuclideanDistance.h"
#include <cmath>
#include <iomanip>
#include <limits>

namespace fast_livo
{
double Backend::Impl::calculateDtwTimeOffset(const std::vector<std::vector<double>>& gpsdata,
                                           const std::vector<std::vector<double>>& slamdata)
{
  if (gpsdata.size() < 3 || slamdata.size() < 3)
    return std::numeric_limits<double>::quiet_NaN();
  const double MIN_EFFECTIVE_VELOCITY = 0.5;
  const int IDX_TIME = 0, IDX_X = 1, IDX_Y = 2, IDX_Z = 3;
  auto process_data = [=](const std::vector<std::vector<double>>& data)
      -> std::pair<std::vector<double>, std::vector<double>> {
    double duration = 50.0;
    const double t_start = data[0][IDX_TIME], t_end = t_start + duration;
    std::vector<double> times, norms;
    bool is_moving = false;
    for (size_t i = 1; i < data.size(); ++i)
    {
      const auto& row = data[i];
      const auto& previous = data[i - 1];
      const double current_time = row[IDX_TIME];
      if (current_time > t_end) break;
      if (!is_moving)
      {
        double dt = row[IDX_TIME] - previous[IDX_TIME];
        if (dt < 1e-6) continue;
        double dx = row[IDX_X] - previous[IDX_X];
        double dy = row[IDX_Y] - previous[IDX_Y];
        double dz = row[IDX_Z] - previous[IDX_Z];
        double vel_norm = std::sqrt(dx * dx + dy * dy + dz * dz) / dt;
        double pos_norm = std::sqrt(row[IDX_X] * row[IDX_X] + row[IDX_Y] * row[IDX_Y] + row[IDX_Z] * row[IDX_Z]);
        if (vel_norm > MIN_EFFECTIVE_VELOCITY && pos_norm > MIN_EFFECTIVE_VELOCITY)
          is_moving = true;
        else
          continue;
      }
      else
      {
        Eigen::Vector3d current_pos(row[IDX_X], row[IDX_Y], row[IDX_Z]);
        double pos_norm = current_pos.norm();
        times.push_back(row[IDX_TIME]);
        norms.push_back(pos_norm);
      }
    }
    return {times, norms};
  };
  auto [gps_pos_times, gps_pos_norms] = process_data(gpsdata);
  auto [slam_pos_times, slam_pos_norms] = process_data(slamdata);
  if (gps_pos_norms.empty() || slam_pos_norms.empty())
    return std::numeric_limits<double>::quiet_NaN();
  checkCancelled();
  // 保留 STRI::getWarpInfoBetween 精确 DTW，去除 example.hpp 的全局 sample1/2。
  fastdtw::TimeSeries<double, 1> tsI, tsJ;
  for (size_t i = 0; i < slam_pos_norms.size(); ++i)
    tsI.addLast(i, fastdtw::TimeSeriesPoint<double, 1>(slam_pos_norms.data() + i));
  for (size_t i = 0; i < gps_pos_norms.size(); ++i)
    tsJ.addLast(i, fastdtw::TimeSeriesPoint<double, 1>(gps_pos_norms.data() + i));
  auto info = fastdtw::STRI::getWarpInfoBetween(tsI, tsJ, fastdtw::EuclideanDistance());
  std::vector<std::pair<int, int>> path;
  info.getPath()->print(path);
  double total_time_diff = 0.0;
  int valid_pairs = 0;
  for (const auto& pair : path)
  {
    if (pair.first < slam_pos_times.size() && pair.second < gps_pos_times.size())
    {
      total_time_diff += slam_pos_times[pair.first] - gps_pos_times[pair.second];
      ++valid_pairs;
    }
  }
  if (valid_pairs == 0)
    return std::numeric_limits<double>::quiet_NaN();
  double avg_time_diff = total_time_diff / valid_pairs;
  auto slam_file = backend_detail::openOutput(opt_vel_output_path_);
  slam_file << std::fixed << std::setprecision(6);
  for (size_t i = 0; i < slam_pos_norms.size(); ++i)
    slam_file << slam_pos_times[i] << " " << slam_pos_norms[i] << std::endl;
  backend_detail::checkOutput(slam_file, opt_vel_output_path_);
  auto gps_file = backend_detail::openOutput(gps_vel_output_path_);
  gps_file << std::fixed << std::setprecision(6);
  for (size_t i = 0; i < gps_pos_norms.size(); ++i)
    gps_file << gps_pos_times[i] << " " << gps_pos_norms[i] << std::endl;
  backend_detail::checkOutput(gps_file, gps_vel_output_path_);
  return avg_time_diff;
}
}
