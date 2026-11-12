#pragma once

#include "fast_livo_ros2/conversions.h"

namespace fast_livo_ros2
{
inline std_msgs::msg::Header header(fast_livo::Timestamp time, const std::string &frame)
{
  std_msgs::msg::Header result;
  result.stamp = stamp(time);
  result.frame_id = frame;
  return result;
}

inline sensor_msgs::msg::Imu to_ros2(const fast_livo::ImuSample &sample)
{
  sensor_msgs::msg::Imu message;
  message.header = header(sample.timestamp, sample.frame_id);
  message.linear_acceleration.x = sample.linear_acceleration.x;
  message.linear_acceleration.y = sample.linear_acceleration.y;
  message.linear_acceleration.z = sample.linear_acceleration.z;
  message.angular_velocity.x = sample.angular_velocity.x;
  message.angular_velocity.y = sample.angular_velocity.y;
  message.angular_velocity.z = sample.angular_velocity.z;
  message.orientation.x = sample.orientation[0];
  message.orientation.y = sample.orientation[1];
  message.orientation.z = sample.orientation[2];
  message.orientation.w = sample.orientation[3];
  message.orientation_covariance = sample.orientation_covariance;
  message.angular_velocity_covariance = sample.angular_velocity_covariance;
  message.linear_acceleration_covariance = sample.linear_acceleration_covariance;
  return message;
}

inline sensor_msgs::msg::PointCloud2 to_ros2(const fast_livo::CloudSample &sample)
{
  sensor_msgs::msg::PointCloud2 message;
  // PCL header 既不是纳秒真值，也可能超出 ROS 时间范围，不能先转换再覆盖。
  pcl_conversions::fromPCL(sample.cloud.fields, message.fields);
  message.height = sample.cloud.height;
  message.width = sample.cloud.width;
  message.is_bigendian = sample.cloud.is_bigendian;
  message.point_step = sample.cloud.point_step;
  message.row_step = sample.cloud.row_step;
  message.data = sample.cloud.data;
  message.is_dense = sample.cloud.is_dense;
  message.header = header(sample.timestamp, sample.frame_id);
  return message;
}

inline sensor_msgs::msg::Image to_ros2(const fast_livo::ImageSample &sample)
{
  if (sample.image.empty() || sample.image.type() != CV_8UC3)
    throw std::invalid_argument("中立回放图像必须为非空 bgr8");
  return *cv_bridge::CvImage(header(sample.timestamp, sample.frame_id), "bgr8", sample.image).toImageMsg();
}

inline fast_livo_ros2_interfaces::msg::CustomMsg to_ros2(const fast_livo::LivoxSample &sample)
{
  if (sample.point_num != sample.points.size())
    throw std::invalid_argument("Livox point_num 与 points 数量不一致");
  fast_livo_ros2_interfaces::msg::CustomMsg message;
  message.header = header(sample.timestamp, sample.frame_id);
  message.timebase = sample.timebase;
  message.point_num = sample.point_num;
  message.lidar_id = sample.lidar_id;
  message.rsvd = sample.rsvd;
  message.points.reserve(sample.points.size());
  for (const auto &point : sample.points)
  {
    fast_livo_ros2_interfaces::msg::CustomPoint converted;
    converted.offset_time = point.offset_time;
    converted.x = point.x;
    converted.y = point.y;
    converted.z = point.z;
    converted.reflectivity = point.reflectivity;
    converted.tag = point.tag;
    converted.line = point.line;
    message.points.push_back(converted);
  }
  return message;
}

inline fast_livo_ros2_interfaces::msg::GnssPVTSolnMsg to_ros2(const fast_livo::GnssSample &sample)
{
  fast_livo_ros2_interfaces::msg::GnssPVTSolnMsg message;
  message.time.week = sample.time.week;
  message.time.tow = sample.time.tow;
#define COPY(field) message.field = sample.field
  COPY(fix_type); COPY(valid_fix); COPY(diff_soln); COPY(carr_soln); COPY(num_sv);
  COPY(latitude); COPY(longitude); COPY(altitude); COPY(height_msl);
  COPY(h_acc); COPY(v_acc); COPY(p_dop); COPY(vel_n); COPY(vel_e); COPY(vel_d); COPY(vel_acc);
#undef COPY
  return message;
}
}
