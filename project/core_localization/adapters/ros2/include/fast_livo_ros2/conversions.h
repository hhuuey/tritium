#pragma once

#include "fast_livo/measurements.h"
#include "fast_livo_ros2/imu_conversion.h"
#include <fast_livo_ros2_interfaces/msg/custom_msg.hpp>
#include <fast_livo_ros2_interfaces/msg/gnss_pvt_soln_msg.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <cv_bridge/cv_bridge.hpp>
#include <pcl_conversions/pcl_conversions.h>
#include <limits>

namespace fast_livo_ros2
{
inline builtin_interfaces::msg::Time stamp(fast_livo::Timestamp time)
{
  if (time.nanoseconds < 0 || time.nanoseconds / 1000000000LL > std::numeric_limits<std::int32_t>::max())
    throw std::invalid_argument("输出时间戳超出 ROS 2 时间范围");
  builtin_interfaces::msg::Time result;
  result.sec = static_cast<std::int32_t>(time.nanoseconds / 1000000000LL);
  result.nanosec = static_cast<std::uint32_t>(time.nanoseconds % 1000000000LL);
  return result;
}

inline fast_livo::Timestamp stamp(const builtin_interfaces::msg::Time &time)
{
  return fast_livo::Timestamp::from_parts(time.sec, time.nanosec);
}

inline fast_livo::CloudSample::ConstPtr cloud(const sensor_msgs::msg::PointCloud2 &message)
{
  auto sample = std::make_shared<fast_livo::CloudSample>();
  sample->timestamp = stamp(message.header.stamp);
  sample->frame_id = message.header.frame_id;
  pcl_conversions::toPCL(message, sample->cloud);
  return sample;
}

inline fast_livo::ImageSample::ConstPtr image(const sensor_msgs::msg::Image::ConstSharedPtr &message)
{
  auto sample = std::make_shared<fast_livo::ImageSample>();
  sample->timestamp = stamp(message->header.stamp);
  sample->frame_id = message->header.frame_id;
  sample->image = cv_bridge::toCvCopy(message, "bgr8")->image;
  return sample;
}

inline fast_livo::LivoxSample::ConstPtr livox(const fast_livo_ros2_interfaces::msg::CustomMsg &message)
{
  if (message.point_num != message.points.size())
    throw std::invalid_argument("Livox point_num 与 points 数量不一致");
  auto sample = std::make_shared<fast_livo::LivoxSample>();
  sample->timestamp = stamp(message.header.stamp);
  sample->frame_id = message.header.frame_id;
  sample->timebase = message.timebase;
  sample->point_num = message.point_num;
  sample->lidar_id = message.lidar_id;
  sample->rsvd = message.rsvd;
  sample->points.reserve(message.points.size());
  for (const auto &point : message.points)
    sample->points.push_back({point.offset_time, point.x, point.y, point.z, point.reflectivity, point.tag, point.line});
  return sample;
}

inline fast_livo::GnssSample::ConstPtr gnss(const fast_livo_ros2_interfaces::msg::GnssPVTSolnMsg &message)
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
