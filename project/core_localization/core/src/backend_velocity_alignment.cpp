/*
This file is part of FAST-LIVO2: Fast, Direct LiDAR-Inertial-Visual Odometry.
Developer: Chunran Zheng <zhengcr@connect.hku.hk>
For commercial use, please contact the developer or Prof. Fu Zhang <fuzhang@hku.hk>.
This file is subject to the terms and conditions in the accompanying LICENSE file.
*/
#define FAST_LIVO_BACKEND_INTERNAL
#include "fast_livo/backend.h"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>

namespace fast_livo
{
namespace backend_detail
{
void writeVelocityAlignmentCsv(const std::string& csv_path,
    const std::vector<std::vector<double>>& gps_position_data,
    const std::vector<std::vector<double>>& slam_position_data,
    const std::vector<std::vector<double>>& gps_velocity_data,
    const std::vector<std::vector<double>>& slam_velocity_data,
    const std::string& velocity_source, double coarse_time_offset, double applied_fine_offset)
{
  VelocitySeries slam_velocity, gps_velocity;
  if (velocity_source == "direct_twist")
  {
    slam_velocity = buildVelocitySeriesFromSamples(slam_velocity_data, 0.0);
    gps_velocity = buildVelocitySeriesFromSamples(gps_velocity_data, coarse_time_offset);
  }
  else
  {
    slam_velocity = buildVelocitySeries(slam_position_data, 0.0);
    gps_velocity = buildVelocitySeries(gps_position_data, coarse_time_offset);
  }
  auto csv_file = openOutput(csv_path);
  csv_file << std::fixed << std::setprecision(9);
  csv_file << "time_sec,livo_speed_mps,rtk_speed_coarse_mps,rtk_speed_applied_mps\n";
  for (size_t i = 0; i < slam_velocity.times.size(); ++i)
  {
    const double t = slam_velocity.times[i];
    double gps_speed_coarse = 0.0, gps_speed_applied = 0.0;
    const bool has_coarse = interpolateLinear(gps_velocity, t, gps_speed_coarse);
    const bool has_applied = interpolateLinear(gps_velocity, t - applied_fine_offset, gps_speed_applied);
    csv_file << t << "," << slam_velocity.values[i] << ",";
    if (has_coarse)
      csv_file << gps_speed_coarse;
    csv_file << ",";
    if (has_applied)
      csv_file << gps_speed_applied;
    csv_file << "\n";
  }
  checkOutput(csv_file, csv_path);
}
}

double Backend::Impl::estimateVelocityTimeOffset(
    const std::vector<std::vector<double>>& gps_position_data,
    const std::vector<std::vector<double>>& slam_position_data,
    const std::vector<std::vector<double>>& gps_velocity_data,
    const std::vector<std::vector<double>>& slam_velocity_data,
    double coarse_time_offset, double& best_score, double& zero_score,
    int& best_pair_count, std::string& velocity_source)
{
  using namespace backend_detail;
  best_score = -std::numeric_limits<double>::infinity();
  zero_score = -std::numeric_limits<double>::infinity();
  best_pair_count = 0;
  velocity_source = "none";
  auto canNormalize = [](const VelocitySeries& slam, const VelocitySeries& gps) {
    VelocitySeries slam_copy = slam;
    VelocitySeries gps_copy = gps;
    return normalizeVelocitySeries(slam_copy) && normalizeVelocitySeries(gps_copy);
  };
  VelocitySeries direct_slam_velocity = buildVelocitySeriesFromSamples(slam_velocity_data, 0.0);
  VelocitySeries direct_gps_velocity = buildVelocitySeriesFromSamples(gps_velocity_data, coarse_time_offset);
  VelocitySeries slam_velocity, gps_velocity;
  if (canNormalize(direct_slam_velocity, direct_gps_velocity))
  {
    slam_velocity = direct_slam_velocity;
    gps_velocity = direct_gps_velocity;
    velocity_source = "direct_twist";
  }
  else
  {
    slam_velocity = buildVelocitySeries(slam_position_data, 0.0);
    gps_velocity = buildVelocitySeries(gps_position_data, coarse_time_offset);
    velocity_source = "position_difference";
  }
  auto slam_file = openOutput(opt_vel_output_path_);
  slam_file << std::fixed << std::setprecision(6);
  for (size_t i = 0; i < slam_velocity.values.size(); ++i)
    slam_file << slam_velocity.times[i] << " " << slam_velocity.values[i] << std::endl;
  checkOutput(slam_file, opt_vel_output_path_);
  auto gps_file = openOutput(gps_vel_output_path_);
  gps_file << std::fixed << std::setprecision(6);
  for (size_t i = 0; i < gps_velocity.values.size(); ++i)
    gps_file << gps_velocity.times[i] << " " << gps_velocity.values[i] << std::endl;
  checkOutput(gps_file, gps_vel_output_path_);
  if (!normalizeVelocitySeries(slam_velocity) || !normalizeVelocitySeries(gps_velocity))
  {
    velocity_source = "unusable";
    return std::numeric_limits<double>::quiet_NaN();
  }
  constexpr double SEARCH_RANGE_SEC = 10.0, SEARCH_STEP_SEC = 0.05;
  int zero_pair_count = 0;
  zero_score = scoreVelocityOffset(slam_velocity, gps_velocity, 0.0, zero_pair_count);
  const std::string score_path = debug_optdata_path_ + "time_alignment_score.csv";
  auto score_file = openOutput(score_path);
  score_file << std::fixed << std::setprecision(9);
  score_file << "fine_offset_sec,score,pairs\n";
  double best_offset = 0.0;
  for (int step = static_cast<int>(std::round(-SEARCH_RANGE_SEC / SEARCH_STEP_SEC));
       step <= static_cast<int>(std::round(SEARCH_RANGE_SEC / SEARCH_STEP_SEC)); ++step)
  {
    checkCancelled();
    const double candidate_offset = static_cast<double>(step) * SEARCH_STEP_SEC;
    int pair_count = 0;
    const double score = scoreVelocityOffset(slam_velocity, gps_velocity, candidate_offset, pair_count);
    score_file << candidate_offset << ",";
    if (std::isfinite(score))
      score_file << score;
    else
      score_file << "nan";
    score_file << "," << pair_count << "\n";
    if (score > best_score)
    {
      best_score = score;
      best_offset = candidate_offset;
      best_pair_count = pair_count;
    }
  }
  checkOutput(score_file, score_path);
  if (!std::isfinite(best_score))
    return std::numeric_limits<double>::quiet_NaN();
  return best_offset;
}
}
