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
#include <filesystem>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <cerrno>
#include <csignal>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace fast_livo
{
namespace
{
void plot(const std::string& script, const std::string& directory, const std::atomic_bool& cancelled)
{
  std::array<char*, 5> args{{const_cast<char*>("python3"), const_cast<char*>(script.c_str()),
                            const_cast<char*>("--debug-dir"), const_cast<char*>(directory.c_str()), nullptr}};
  pid_t child = -1;
  if (posix_spawnp(&child, "python3", nullptr, nullptr, args.data(), environ) != 0)
    throw std::runtime_error("无法启动时间对齐绘图: " + script);
  int status = 0;
  while (true)
  {
    const pid_t result = waitpid(child, &status, WNOHANG);
    if (result == child)
      break;
    if (result < 0 && errno != EINTR)
      throw std::runtime_error("无法等待时间对齐绘图: " + script);
    if (cancelled.load())
    {
      kill(child, SIGKILL);
      while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
      throw std::runtime_error("后端优化已取消");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    throw std::runtime_error("时间对齐绘图失败: " + script);
}
}

void Backend::Impl::prepareTimeAlignment()
{
  using namespace backend_detail;
  std::vector<std::vector<double>> slam_position_data, slam_velocity_data;
  slam_position_data.reserve(keyFrames.size());
  slam_velocity_data.reserve(keyFrames.size());
  for (const auto& frame : keyFrames)
  {
    const auto& p = frame.pose.translation();
    slam_position_data.push_back({frame.time, p.x(), p.y(), p.z()});
    slam_velocity_data.push_back({frame.time, frame.velocity.x(), frame.velocity.y(), frame.velocity.z()});
  }
  std::vector<std::vector<double>> gps_position_data, gps_velocity_data;
  gps_position_data.reserve(gpsQueue.size());
  gps_velocity_data.reserve(gpsQueue.size());
  for (const auto& g : gpsQueue)
  {
    const auto& p = g.position;
    const auto& v = g.velocity;
    gps_position_data.push_back({g.timestamp.seconds(), p.x, p.y, p.z});
    gps_velocity_data.push_back({g.timestamp.seconds(), v.x, v.y, v.z});
  }
  const double slam_start = slam_position_data.front()[0];
  const double slam_end = slam_position_data.back()[0];
  const double gps_start = gps_position_data.front()[0];
  const double gps_end = gps_position_data.back()[0];
  const double raw_overlap = std::min(slam_end, gps_end) - std::max(slam_start, gps_start);
  const bool epoch_mismatch = raw_overlap < 5.0 || std::fabs(slam_start - gps_start) > 100.0 ||
                             std::fabs(slam_end - gps_end) > 100.0;
  const double coarse_time_offset = epoch_mismatch ? (slam_start - gps_start) : 0.0;
  double best_velocity_score = -std::numeric_limits<double>::infinity();
  double zero_velocity_score = -std::numeric_limits<double>::infinity();
  int best_velocity_pairs = 0;
  std::string velocity_source;
  const double fine_time_offset = estimateVelocityTimeOffset(
      gps_position_data, slam_position_data, gps_velocity_data, slam_velocity_data, coarse_time_offset,
      best_velocity_score, zero_velocity_score, best_velocity_pairs, velocity_source);
  double applied_time_offset = coarse_time_offset;
  bool fine_time_offset_used = false;
  if (std::isfinite(fine_time_offset) && std::fabs(fine_time_offset) <= 10.0 &&
      std::isfinite(best_velocity_score) && best_velocity_score > 0.25)
  {
    const bool score_improved = !std::isfinite(zero_velocity_score) ||
                               (best_velocity_score - zero_velocity_score) > 0.05;
    if (epoch_mismatch || score_improved)
    {
      applied_time_offset += fine_time_offset;
      fine_time_offset_used = true;
    }
  }
  if (std::fabs(applied_time_offset) > 1e-9)
  {
    const auto offset_ns = durationNanoseconds(applied_time_offset);
    for (auto& g : gpsQueue)
      g.timestamp = shiftedTimestamp(g.timestamp, offset_ns);
  }
  writeVelocityAlignmentCsv(debug_optdata_path_ + "time_alignment_velocity.csv",
                            gps_position_data, slam_position_data, gps_velocity_data, slam_velocity_data,
                            velocity_source, coarse_time_offset, fine_time_offset_used ? fine_time_offset : 0.0);
  const std::string path = debug_optdata_path_ + "time_offset.txt";
  auto file = openOutput(path);
  file << std::fixed << std::setprecision(9);
  file << "slam_start " << slam_start << "\n";
  file << "slam_end " << slam_end << "\n";
  file << "gps_start_before " << gps_start << "\n";
  file << "gps_end_before " << gps_end << "\n";
  file << "raw_overlap_sec " << raw_overlap << "\n";
  file << "epoch_mismatch " << (epoch_mismatch ? 1 : 0) << "\n";
  file << "coarse_time_offset_sec " << coarse_time_offset << "\n";
  file << "velocity_fine_time_offset_sec " << fine_time_offset << "\n";
  file << "velocity_fine_used " << (fine_time_offset_used ? 1 : 0) << "\n";
  file << "velocity_source " << velocity_source << "\n";
  file << "velocity_best_score " << best_velocity_score << "\n";
  file << "velocity_zero_score " << zero_velocity_score << "\n";
  file << "velocity_best_pairs " << best_velocity_pairs << "\n";
  file << "applied_time_offset_sec " << applied_time_offset << "\n";
  file << "gps_start_after " << gpsQueue.front().timestamp.seconds() << "\n";
  file << "gps_end_after " << gpsQueue.back().timestamp.seconds() << "\n";
  checkOutput(file, path);
  if (config.plot_script.empty())
    return;
  if (!std::filesystem::is_regular_file(config.plot_script))
    throw std::runtime_error("绘图脚本不存在: " + config.plot_script);
  checkCancelled();
  plot(config.plot_script, debug_optdata_path_, cancelled);
}
}
