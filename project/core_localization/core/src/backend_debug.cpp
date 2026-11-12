/*
This file is part of FAST-LIVO2: Fast, Direct LiDAR-Inertial-Visual Odometry.
Developer: Chunran Zheng <zhengcr@connect.hku.hk>
For commercial use, please contact the developer or Prof. Fu Zhang <fuzhang@hku.hk>.
This file is subject to the terms and conditions in the accompanying LICENSE file.
*/
#define FAST_LIVO_BACKEND_INTERNAL
#include "fast_livo/backend.h"
#include <pcl/io/pcd_io.h>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace fast_livo
{
void Backend::Impl::loadData(const std::string& directory)
{
  const std::string rtk_path = directory + "/rtk.txt";
  std::ifstream rtk_file(rtk_path);
  if (!rtk_file)
    throw std::runtime_error("无法打开调试 RTK 文件: " + rtk_path);
  std::string line;
  gpsQueue.clear();
  while (std::getline(rtk_file, line))
  {
    if (line.empty() || line[0] == '#') continue;
    std::stringstream ss(line);
    double t, x, y, z;
    double vx = 0.0, vy = 0.0, vz = 0.0, h_acc = 0.0, v_acc = 0.0;
    if (!(ss >> t >> x >> y >> z))
      throw std::runtime_error("调试 RTK 行格式无效: " + rtk_path);
    ss >> vx >> vy >> vz >> h_acc >> v_acc;
    PoseSample gps;
    gps.timestamp = backend_detail::shiftedTimestamp(Timestamp::from_seconds(t),
        -backend_detail::durationNanoseconds(config.gps_offset));
    gps.frame_id = "map";
    gps.position = {x, y, z};
    gps.orientation.w = 1.0;
    gps.velocity = {vx, vy, vz};
    gps.covariance[0] = h_acc;
    gps.covariance[7] = h_acc;
    gps.covariance[14] = v_acc;
    gpsQueue.push_back(gps);
  }
  if (rtk_file.bad())
    throw std::runtime_error("读取调试 RTK 文件失败: " + rtk_path);
  const std::string odom_path = directory + "/odom.txt", cov_path = directory + "/cov.txt";
  std::ifstream odom_file(odom_path), cov_file(cov_path);
  if (!odom_file || !cov_file)
    throw std::runtime_error("无法打开调试 odom.txt 或 cov.txt: " + directory);
  std::string line_odom, line_cov;
  int processed_count = 0;
  while (std::getline(odom_file, line_odom))
  {
    if (!std::getline(cov_file, line_cov))
      throw std::runtime_error("调试 odom 与 cov 行数不一致");
    if (line_odom.empty() || line_odom[0] == '#') continue;
    if (line_cov.empty() || line_cov[0] == '#') continue;
    std::stringstream ss_odom(line_odom), ss_cov(line_cov);
    double t_odom, t_cov;
    if (!(ss_odom >> t_odom) || !(ss_cov >> t_cov) || std::abs(t_odom - t_cov) > 1e-5)
      throw std::runtime_error("调试 odom 与 cov 时间戳不一致");
    double tx, ty, tz, qx, qy, qz, qw, vx = 0.0, vy = 0.0, vz = 0.0;
    if (!(ss_odom >> tx >> ty >> tz >> qx >> qy >> qz >> qw))
      throw std::runtime_error("调试 odom 行格式无效");
    ss_odom >> vx >> vy >> vz;
    PoseSample sample;
    sample.timestamp = Timestamp::from_seconds(t_odom);
    sample.position = {tx, ty, tz};
    sample.orientation = {qx, qy, qz, qw};
    sample.velocity = {vx, vy, vz};
    std::stringstream filename;
    filename << std::setw(6) << std::setfill('0') << processed_count;
    const std::string pcd_path = directory + "/pcd/" + filename.str() + ".pcd";
    backend_detail::Cloud::Ptr cloud(new backend_detail::Cloud);
    if (pcl::io::loadPCDFile(pcd_path, *cloud) == -1)
      throw std::runtime_error("缺少调试点云: " + pcd_path);
    saveKeyFrameAndFactor(backend_detail::poseFromSample(sample, false), t_odom,
                         Eigen::Vector3d(vx, vy, vz), cloud);
    ++processed_count;
  }
  if (std::getline(cov_file, line_cov) || odom_file.bad() || cov_file.bad())
    throw std::runtime_error("调试轨迹读取失败或 odom 与 cov 行数不一致");
  if (keyFrames.empty() || gpsQueue.empty())
    throw std::runtime_error("调试数据未提供关键帧或 GNSS");
}
}
