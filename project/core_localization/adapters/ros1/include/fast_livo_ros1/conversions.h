#pragma once

#include "fast_livo/measurements.h"
#include "imu_conversion.h"
#include <gnss_comm/GnssPVTSolnMsg.h>
#include <livox_ros_driver/CustomMsg.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/PointCloud2.h>
#include <cv_bridge/cv_bridge.h>
#include <pcl_conversions/pcl_conversions.h>

namespace fast_livo_ros1
{
inline ros::Time stamp(fast_livo::Timestamp time)
{
  if (time.nanoseconds < 0 || time.nanoseconds / 1000000000LL > UINT32_MAX)
    throw std::invalid_argument("输出时间戳超出 ROS 1 时间范围");
  ros::Time result;
  result.fromNSec(static_cast<std::uint64_t>(time.nanoseconds));
  return result;
}

inline fast_livo::Timestamp stamp(const ros::Time &time)
{
  return fast_livo::Timestamp::from_parts(time.sec, time.nsec);
}

inline fast_livo::CloudSample::ConstPtr cloud(const sensor_msgs::PointCloud2 &message)
{
  auto sample = std::make_shared<fast_livo::CloudSample>();
  sample->timestamp = stamp(message.header.stamp);
  sample->frame_id = message.header.frame_id;
  pcl_conversions::toPCL(message, sample->cloud);
  return sample;
}

inline fast_livo::ImageSample::ConstPtr image(const sensor_msgs::ImageConstPtr &message)
{
  auto sample = std::make_shared<fast_livo::ImageSample>();
  sample->timestamp = stamp(message->header.stamp);
  sample->frame_id = message->header.frame_id;
  sample->image = cv_bridge::toCvCopy(message, "bgr8")->image;
  return sample;
}

inline fast_livo::LivoxSample::ConstPtr livox(const livox_ros_driver::CustomMsg &message)
{
  if (message.point_num != message.points.size())
    throw std::invalid_argument("Livox point_num 与 points 数量不一致");
  auto sample = std::make_shared<fast_livo::LivoxSample>();
  sample->timestamp = stamp(message.header.stamp);
  sample->frame_id = message.header.frame_id;
  sample->timebase = message.timebase;
  sample->point_num = message.point_num;
  sample->lidar_id = message.lidar_id;
  std::copy(message.rsvd.begin(), message.rsvd.end(), sample->rsvd.begin());
  sample->points.reserve(message.points.size());
  for (const auto &point : message.points)
    sample->points.push_back({point.offset_time, point.x, point.y, point.z, point.reflectivity, point.tag, point.line});
  return sample;
}

inline fast_livo::GnssSample::ConstPtr gnss(const gnss_comm::GnssPVTSolnMsg &message)
{
  auto sample = std::make_shared<fast_livo::GnssSample>();
  sample->time = {message.time.week, message.time.tow};
#define COPY(field) sample->field = message.field
  COPY(fix_type); COPY(valid_fix); COPY(diff_soln); COPY(carr_soln); COPY(num_sv);
  COPY(latitude); COPY(longitude); COPY(altitude); COPY(height_msl);
  COPY(h_acc); COPY(v_acc); COPY(p_dop); COPY(vel_n); COPY(vel_e); COPY(vel_d); COPY(vel_acc);
#undef COPY
  return sample;
}
}
