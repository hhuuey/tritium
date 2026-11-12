#pragma once

#include "fast_livo/sensor_types.h"
#include <sensor_msgs/Imu.h>
#include <algorithm>

namespace fast_livo
{

inline ImuSample::ConstPtr from_ros1(const sensor_msgs::Imu &message)
{
  auto sample = std::make_shared<ImuSample>();
  sample->timestamp = Timestamp::from_parts(message.header.stamp.sec, message.header.stamp.nsec);
  sample->frame_id = message.header.frame_id;
  sample->linear_acceleration = {
      message.linear_acceleration.x, message.linear_acceleration.y, message.linear_acceleration.z};
  sample->angular_velocity = {message.angular_velocity.x, message.angular_velocity.y, message.angular_velocity.z};
  sample->orientation = {{message.orientation.x, message.orientation.y, message.orientation.z, message.orientation.w}};
  std::copy(message.orientation_covariance.begin(), message.orientation_covariance.end(),
            sample->orientation_covariance.begin());
  std::copy(message.angular_velocity_covariance.begin(), message.angular_velocity_covariance.end(),
            sample->angular_velocity_covariance.begin());
  std::copy(message.linear_acceleration_covariance.begin(), message.linear_acceleration_covariance.end(),
            sample->linear_acceleration_covariance.begin());
  return sample;
}

}
