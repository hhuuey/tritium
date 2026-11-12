/*
This file is part of FAST-LIVO2: Fast, Direct LiDAR-Inertial-Visual Odometry.
Developer: Chunran Zheng <zhengcr@connect.hku.hk>
For commercial use, please contact the developer or Prof. Fu Zhang <fuzhang@hku.hk>.
This file is subject to the terms and conditions in the accompanying LICENSE file.
*/
#define FAST_LIVO_BACKEND_INTERNAL
#include "fast_livo/backend.h"
#include <pcl/common/transforms.h>
#include <pcl/io/pcd_io.h>
#include <cstring>
#include <iomanip>
#include <limits>
#include <stdexcept>

namespace fast_livo
{
namespace backend_detail
{
std::ofstream openOutput(const std::string& path)
{
  std::ofstream file(path);
  if (!file.is_open())
    throw std::runtime_error("无法创建后端输出: " + path);
  return file;
}

void checkOutput(std::ofstream& file, const std::string& path)
{
  file.flush();
  if (!file)
    throw std::runtime_error("写入后端输出失败: " + path);
  file.close();
  if (!file)
    throw std::runtime_error("关闭后端输出失败: " + path);
}

Eigen::Affine3f poseToAffine3f(const gtsam::Pose3& pose)
{
  const auto& t = pose.translation();
  const auto rpy = pose.rotation().rpy();
  return pcl::getTransformation(t.x(), t.y(), t.z(), rpy.x(), rpy.y(), rpy.z());
}

void writeGlobalMapBinary(const KeyFrames& frames, const std::string& path,
                         const std::function<void()>& check_cancelled)
{
  std::size_t count = 0;
  for (const auto& frame : frames)
  {
    check_cancelled();
    if (!frame.cloud || frame.cloud->size() > std::numeric_limits<std::size_t>::max() - count)
      throw std::runtime_error("全局地图关键帧无效或点数溢出");
    count += frame.cloud->size();
  }
  if (count == 0)
    throw std::runtime_error("无法保存空的优化地图");

  // 旧拼接从默认空云开始，只改变 width/height，不继承各帧的 VIEWPOINT。
  Cloud metadata;
  metadata.width = static_cast<std::uint32_t>(count);
  metadata.height = 1;
  std::string header = pcl::PCDWriter::generateHeader<pcl::PointXYZRGB>(metadata);
  const auto point_line = header.rfind("POINTS ");
  if (point_line == std::string::npos)
    throw std::runtime_error("PCL 生成的二进制点云头缺少 POINTS");
  // 不使用 generateHeader 的 int nr_points，避免 INT_MAX 哨兵和窄化限制。
  header.replace(point_line, std::string::npos, "POINTS " + std::to_string(count) + "\n");
  header += "DATA binary\n";
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file)
    throw std::runtime_error("无法创建全局地图: " + path);
  file.write(header.data(), static_cast<std::streamsize>(header.size()));

  std::vector<std::pair<std::size_t, std::size_t>> fields;
  for (const auto& field : pcl::getFields<pcl::PointXYZRGB>())
    if (field.name != "_")
      fields.emplace_back(field.offset, static_cast<std::size_t>(field.count) * pcl::getFieldSize(field.datatype));
  std::array<char, 65536> buffer;
  for (const auto& field : fields)
    if (field.second > buffer.size())
      throw std::runtime_error("二进制点云字段超出写入缓冲区");
  std::size_t used = 0;
  auto flush = [&] {
    check_cancelled();
    file.write(buffer.data(), static_cast<std::streamsize>(used));
    if (!file)
      throw std::runtime_error("写入全局地图失败: " + path);
    used = 0;
  };
  for (const auto& frame : frames)
  {
    check_cancelled();
    // 不复用或合并变换，保留原先 Affine3f 和 PCL 的逐帧浮点计算路径。
    Eigen::Affine3f transform = poseToAffine3f(frame.pose);
    Cloud transformed;
    pcl::transformPointCloud(*frame.cloud, transformed, transform);
    for (const auto& point : transformed.points)
      for (const auto& field : fields)
      {
        const auto size = field.second;
        if (used + size > buffer.size())
          flush();
        // 与 PCL writeBinary 相同：只按字段复制有效字节，不写 PointXYZRGB 的对齐填充。
        std::memcpy(buffer.data() + used, reinterpret_cast<const char*>(&point) + field.first, size);
        used += size;
      }
  }
  if (used != 0)
    flush();
  checkOutput(file, path);
}
}

void Backend::Impl::savekeyframescan()
{
  for (size_t i = 0; i < keyFrames.size(); ++i)
  {
    checkCancelled();
    backend_detail::Cloud::Ptr cloud(new backend_detail::Cloud);
    *cloud = *keyFrames[i].cloud;
    Eigen::Affine3f transform = backend_detail::poseToAffine3f(keyFrames[i].pose);
    pcl::transformPointCloud(*cloud, *cloud, transform);
    const std::string path = keyframe_scan_pcd_path_ + std::to_string(i) + ".pcd";
    if (pcl::io::savePCDFileBinary(path, *cloud) != 0)
      throw std::runtime_error("保存关键帧点云失败: " + path);
  }
}

void Backend::Impl::saveOptimizedGlobalMap()
{
  const std::string path = global_map_pcd_path_ +
      (is_optimized ? "after_optimization.pcd" : "before_optimization.pcd");
  backend_detail::writeGlobalMapBinary(keyFrames, path, [this] { checkCancelled(); });
}

void Backend::Impl::writeTumTrajectory(const std::string& path)
{
  if (keyFrames.empty())
    throw std::runtime_error("无法保存空的 TUM 轨迹");
  auto file = backend_detail::openOutput(path);
  file << "# timestamp tx ty tz qx qy qz qw" << std::endl;
  file << std::fixed << std::setprecision(6);
  for (const auto& frame : keyFrames)
  {
    checkCancelled();
    const auto& t = frame.pose.translation();
    const auto q = frame.pose.rotation().toQuaternion();
    file << frame.time << " " << t.x() << " " << t.y() << " " << t.z() << " "
         << q.x() << " " << q.y() << " " << q.z() << " " << q.w() << std::endl;
  }
  backend_detail::checkOutput(file, path);
}

void Backend::Impl::writeRtkTumTrajectory()
{
  if (gpsQueue.empty())
    throw std::runtime_error("无法保存空的 RTK 轨迹");
  auto file = backend_detail::openOutput(rtk_tum_output_path_);
  file << "# timestamp tx ty tz qx qy qz qw" << std::endl;
  file << std::fixed << std::setprecision(6);
  for (const auto& g : gpsQueue)
  {
    checkCancelled();
    file << g.timestamp.seconds() << " " << g.position.x << " " << g.position.y << " " << g.position.z << " "
         << g.orientation.x << " " << g.orientation.y << " " << g.orientation.z << " " << g.orientation.w
         << std::endl;
  }
  backend_detail::checkOutput(file, rtk_tum_output_path_);
}
}
