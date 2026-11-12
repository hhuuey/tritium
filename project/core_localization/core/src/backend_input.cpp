/*
This file is part of FAST-LIVO2: Fast, Direct LiDAR-Inertial-Visual Odometry.
Developer: Chunran Zheng <zhengcr@connect.hku.hk>
For commercial use, please contact the developer or Prof. Fu Zhang <fuzhang@hku.hk>.
This file is subject to the terms and conditions in the accompanying LICENSE file.
*/
/*
Copyright (c) 2003-2006 Gino van den Bergen / Erwin Coumans
http://continuousphysics.com/Bullet/
This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use
of this software. Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:
1. The origin of this software must not be misrepresented; you must not claim
   that you wrote the original software. If you use this software in a product,
   an acknowledgment in the product documentation would be appreciated but is
   not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
以下四元数到 RPY 转换改编自 tf::Matrix3x3，保留其奇异分支与运算顺序。
*/
#define FAST_LIVO_BACKEND_INTERNAL
#include "fast_livo/backend.h"
#include <pcl/conversions.h>
#include <pcl/common/io.h>
#include <cmath>
#include <stdexcept>

namespace fast_livo
{
namespace backend_detail
{
std::int64_t durationNanoseconds(double value)
{
  if (!allFinite({value}) || value < -2147483648.0 || value >= 2147483648.0)
    throw std::invalid_argument("时间偏移超出原有 Duration 的有效范围");
  const auto seconds = static_cast<std::int64_t>(std::floor(value));
  return seconds * 1000000000 +
         static_cast<std::int64_t>(std::round((value - static_cast<double>(seconds)) * 1e9));
}

Timestamp shiftedTimestamp(Timestamp stamp, std::int64_t delta)
{
  if (stamp.nanoseconds < 0 ||
      (delta > 0 && stamp.nanoseconds > std::numeric_limits<std::int64_t>::max() - delta) ||
      (delta < 0 && delta < -stamp.nanoseconds))
    throw std::invalid_argument("偏移后的时间戳超出有效范围");
  stamp.nanoseconds += delta;
  return stamp;
}

gtsam::Pose3 poseFromSample(const PoseSample& sample, bool from_message)
{
  auto q = sample.orientation;
  const auto& p = sample.position;
  const auto& v = sample.velocity;
  const Eigen::Quaterniond quaternion(q.w, q.x, q.y, q.z);
  if (!allFinite({q.x, q.y, q.z, q.w, p.x, p.y, p.z, v.x, v.y, v.z}) || quaternion.squaredNorm() <= 0)
    throw std::invalid_argument("位姿、四元数与速度必须为有效有限值");
  double norm2 = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
  if (!allFinite({norm2}) || norm2 <= 0)
    throw std::invalid_argument("四元数模长无效");
  // 实时回调原先经过 quaternionMsgToTF；调试载入则直接构造 tf 四元数。
  if (from_message && std::fabs(norm2 - 1.0) > static_cast<double>(0.1f))
  {
    const double scale = 1.0 / std::sqrt(norm2);
    q.x *= scale; q.y *= scale; q.z *= scale; q.w *= scale;
    norm2 = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
  }
  const double s = 2.0 / norm2;
  const double xs = q.x * s, ys = q.y * s, zs = q.z * s;
  const double wx = q.w * xs, wy = q.w * ys, wz = q.w * zs;
  const double xx = q.x * xs, xy = q.x * ys, xz = q.x * zs;
  const double yy = q.y * ys, yz = q.y * zs, zz = q.z * zs;
  const double m00 = 1.0 - (yy + zz), m01 = xy - wz, m02 = xz + wy;
  const double m10 = xy + wz, m20 = xz - wy, m21 = yz + wx, m22 = 1.0 - (xx + yy);
  double roll, pitch, yaw;
  if (std::abs(m20) >= 1.0)
  {
    yaw = 0;
    pitch = m20 < 0 ? std::acos(-1.0) / 2.0 : -std::acos(-1.0) / 2.0;
    roll = m20 < 0 ? std::atan2(m01, m02) : std::atan2(-m01, -m02);
  }
  else
  {
    pitch = -std::asin(m20);
    roll = std::atan2(m21 / std::cos(pitch), m22 / std::cos(pitch));
    yaw = std::atan2(m10 / std::cos(pitch), m00 / std::cos(pitch));
  }
  return gtsam::Pose3(gtsam::Rot3::RzRyRx(roll, pitch, yaw), gtsam::Point3(p.x, p.y, p.z));
}
}

void Backend::Impl::matched(const PoseSample& pose, const CloudSample& sample)
{
  backend_detail::Cloud::Ptr cloud(new backend_detail::Cloud);
  pcl::fromPCLPointCloud2(sample.cloud, *cloud);
  saveKeyFrameAndFactor(backend_detail::poseFromSample(pose), sample.timestamp.seconds(),
                       Eigen::Vector3d(pose.velocity.x, pose.velocity.y, pose.velocity.z), cloud);
}

void Backend::Impl::saveKeyFrameAndFactor(const gtsam::Pose3& pose, double time,
                                         const Eigen::Vector3d& velocity,
                                         const backend_detail::Cloud::Ptr& cloud)
{
  const std::size_t index = keyFrames.size();
  initialEstimate.insert(index, pose);
  backend_detail::KeyFrame frame;
  frame.time = time;
  frame.pose = pose;
  frame.velocity = velocity;
  frame.cloud.reset(new backend_detail::Cloud);
  if (cloud)
    pcl::copyPointCloud(*cloud, *frame.cloud);
  keyFrames.push_back(frame);
  frame_count.store(keyFrames.size());
  last_keyframe_time = std::chrono::steady_clock::now();
  changed.notify_all();
}

void Backend::push_gnss(const GnssSample& sample)
{
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->ensureCollecting();
  if (!backend_detail::allFinite({sample.latitude, sample.longitude, sample.altitude, sample.time.tow,
                                 sample.vel_e, sample.vel_n, sample.vel_d, sample.h_acc, sample.v_acc}) ||
      sample.latitude < -90 || sample.latitude > 90)
    throw std::invalid_argument("GNSS 坐标、时间、速度或精度无效");
  // 保留旧时间转换、18 秒闰秒和 offset 符号；不新增 fix/carr_soln 等过滤。
  const double timestamp = static_cast<double>(sample.time.week) * 604800.0 +
                           sample.time.tow + 315964800.0 - 18.0;
  const auto stamp = Timestamp::from_seconds(timestamp);
  PoseSample gps;
  // 先量化各时间再相减，与 ROS Time - Duration 的纳秒运算一致。
  const auto offset_ns = backend_detail::durationNanoseconds(impl_->config.gps_offset);
  gps.timestamp = backend_detail::shiftedTimestamp(stamp, -offset_ns);
  if (impl_->first_gps)
  {
    impl_->gps_trans_.Reset(sample.latitude, sample.longitude, sample.altitude);
    impl_->first_gps = false;
  }
  impl_->gps_trans_.Forward(sample.latitude, sample.longitude, sample.altitude,
                           gps.position.x, gps.position.y, gps.position.z);
  gps.frame_id = "map";
  gps.orientation.w = 1.0;
  gps.velocity = {sample.vel_e, sample.vel_n, -sample.vel_d};
  gps.covariance[0] = sample.h_acc;
  gps.covariance[7] = sample.h_acc;
  gps.covariance[14] = sample.v_acc;
  impl_->gpsQueue.push_back(gps);
}
}
