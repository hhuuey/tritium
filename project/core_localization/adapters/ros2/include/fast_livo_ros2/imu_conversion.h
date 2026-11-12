#pragma once

#include "fast_livo/sensor_types.h"
#include <sensor_msgs/msg/imu.hpp>

namespace fast_livo
{

inline ImuSample::ConstPtr from_ros2(const sensor_msgs::msg::Imu &message)
{
  auto sample = std::make_shared<ImuSample>();
  sample->timestamp = Timestamp::from_parts(message.header.stamp.sec, message.header.stamp.nanosec);
  sample->frame_id = message.header.frame_id;
  sample->linear_acceleration = {
      message.linear_acceleration.x, message.linear_acceleration.y, message.linear_acceleration.z};
  sample->angular_velocity = {message.angular_velocity.x, message.angular_velocity.y, message.angular_velocity.z};
  sample->orientation = {{message.orientation.x, message.orientation.y, message.orientation.z, message.orientation.w}};
  sample->orientation_covariance = message.orientation_covariance;
  sample->angular_velocity_covariance = message.angular_velocity_covariance;
  sample->linear_acceleration_covariance = message.linear_acceleration_covariance;
  return sample;
}

}
