/*
This file is part of FAST-LIVO2: Fast, Direct LiDAR-Inertial-Visual Odometry.
Developer: Chunran Zheng <zhengcr@connect.hku.hk>
This file is subject to the accompanying LICENSE file.
*/
#define FAST_LIVO_BACKEND_INTERNAL
#include "fast_livo/backend.h"
#include <pcl/common/transforms.h>
#include <pcl/io/pcd_io.h>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace fast_livo::backend_detail
{
Eigen::Affine3f poseToAffine3f(const gtsam::Pose3&);
}

namespace
{
using namespace fast_livo::backend_detail;
namespace fs = std::filesystem;

void require(bool condition, const char* message)
{
  if (!condition)
    throw std::runtime_error(message);
}

std::vector<char> bytes(const fs::path& path)
{
  std::ifstream file(path, std::ios::binary);
  require(file.is_open(), "测试点云输出缺失");
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

KeyFrames fixture()
{
  KeyFrames frames;
  for (int i = 0; i < 4; ++i)
  {
    KeyFrame frame;
    frame.cloud.reset(new Cloud);
    frame.pose = gtsam::Pose3(gtsam::Rot3::RzRyRx(0.13 * i, -0.07 * i, 0.21 * i),
                             gtsam::Point3(1.3 * i, -2.7 * i, 0.37 * i));
    frame.cloud->sensor_origin_ = Eigen::Vector4f(5, 6, 7, 0);
    frame.cloud->sensor_orientation_ = Eigen::Quaternionf(0.5f, 0.5f, 0.5f, 0.5f);
    frame.cloud->header.stamp = 100 + i;
    // 中间空帧以及非默认逐帧 VIEWPOINT 不能改变全局拼接头。
    if (i == 1)
    {
      frames.push_back(frame);
      continue;
    }
    // 超过 64KiB，覆盖多次缓冲区 flush，仍然只是固定小云。
    for (int j = 0; j < 4200; ++j)
    {
      pcl::PointXYZRGB point;
      point.x = static_cast<float>(j) * 0.13f - 10.1f;
      point.y = static_cast<float>(j % 17) * -0.21f;
      point.z = static_cast<float>(j % 11) * 0.37f;
      point.rgba = 0x7f13579bU + i * 31 + j;
      if (j == 9) point.x = std::numeric_limits<float>::quiet_NaN();
      frame.cloud->push_back(point);
    }
    frame.cloud->width = 2100;
    frame.cloud->height = 2;
    frame.cloud->is_dense = false;
    frames.push_back(frame);
  }
  return frames;
}

void compare(const KeyFrames& frames, const fs::path& root, const std::string& name)
{
  Cloud joined;
  for (const auto& frame : frames)
  {
    // 原 saveOptimizedGlobalMap 调用的就是此函数，不在测试中另写一遍矩阵计算。
    // GCC13 的 fast-math 会使重复内联算式随调用上下文产生不同末位，掩盖真正的 IO 差异。
    Eigen::Affine3f transform = poseToAffine3f(frame.pose);
    Cloud transformed;
    pcl::transformPointCloud(*frame.cloud, transformed, transform);
    joined += transformed;
  }
  const auto old_path = root / ("old-" + name + ".pcd");
  const auto new_path = root / ("stream-" + name + ".pcd");
  require(pcl::io::savePCDFileBinary(old_path.string(), joined) == 0, "旧拼接地图写入失败");
  writeGlobalMapBinary(frames, new_path.string(), [] {});
  const auto old_bytes = bytes(old_path), new_bytes = bytes(new_path);
  if (old_bytes != new_bytes)
  {
    std::size_t offset = 0;
    while (offset < old_bytes.size() && offset < new_bytes.size() && old_bytes[offset] == new_bytes[offset])
      ++offset;
    auto byte = [offset](const std::vector<char>& data) -> int {
      return offset < data.size() ? static_cast<unsigned char>(data[offset]) : -1;
    };
    throw std::runtime_error("流式输出必须与旧 PCL binary 文件逐字节相同: offset=" +
                             std::to_string(offset) + ", old_size=" + std::to_string(old_bytes.size()) +
                             ", new_size=" + std::to_string(new_bytes.size()) + ", old_byte=" +
                             std::to_string(byte(old_bytes)) + ", new_byte=" + std::to_string(byte(new_bytes)));
  }

  pcl::PCLPointCloud2 old_cloud, new_cloud;
  Eigen::Vector4f old_origin, new_origin;
  Eigen::Quaternionf old_rotation, new_rotation;
  int old_version = 0, new_version = 0;
  pcl::PCDReader reader;
  require(reader.read(old_path.string(), old_cloud, old_origin, old_rotation, old_version) == 0 &&
          reader.read(new_path.string(), new_cloud, new_origin, new_rotation, new_version) == 0,
          "PCD 实际解码失败");
  require(old_cloud.data == new_cloud.data && old_cloud.width == new_cloud.width &&
          old_cloud.height == new_cloud.height && old_cloud.point_step == new_cloud.point_step &&
          old_cloud.row_step == new_cloud.row_step && old_cloud.fields.size() == new_cloud.fields.size() &&
          old_origin == new_origin && old_rotation.coeffs() == new_rotation.coeffs(),
          "实际解码的点数据顺序、布局或 VIEWPOINT 不同");
  for (std::size_t i = 0; i < old_cloud.fields.size(); ++i)
  {
    const auto& a = old_cloud.fields[i];
    const auto& b = new_cloud.fields[i];
    require(a.name == b.name && a.datatype == b.datatype && a.count == b.count && a.offset == b.offset,
            "实际解码的字段、位宽或偏移不同");
  }
}
}

int main()
{
  const fs::path root = "backend-output-test-" +
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  try
  {
    fs::create_directories(root);
    auto frames = fixture();
    compare(frames, root, "before_optimization");
    for (auto& frame : frames)
      frame.pose = frame.pose.compose(gtsam::Pose3(gtsam::Rot3::RzRyRx(-0.2, 0.11, 0.37),
                                                  gtsam::Point3(-1.7, 0.2, 0.83)));
    compare(frames, root, "after_optimization");
    bool failed = false;
    try { writeGlobalMapBinary(frames, (root / "missing/map.pcd").string(), [] {}); }
    catch (const std::exception&) { failed = true; }
    require(failed, "写入失败必须显式抛错");
    failed = false;
    try { writeGlobalMapBinary({}, (root / "empty.pcd").string(), [] {}); }
    catch (const std::exception&) { failed = true; }
    require(failed, "空地图必须显式抛错");
    failed = false;
    int calls = 0;
    try
    {
      writeGlobalMapBinary(frames, (root / "cancelled.pcd").string(), [&] {
        if (++calls == 7) throw std::runtime_error("测试取消");
      });
    }
    catch (const std::exception&) { failed = true; }
    require(failed, "流式导出必须保留取消检查");
    fs::remove_all(root);
  }
  catch (const std::exception& exception)
  {
    std::cerr << exception.what() << "\n";
    fs::remove_all(root);
    return 1;
  }
  return 0;
}
