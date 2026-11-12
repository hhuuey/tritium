#include "fast_livo_ros2/imu_conversion.h"
#include <gtest/gtest.h>

TEST(ImuConversion, PreservesDataAndRejectsInvalidTime)
{
  sensor_msgs::msg::Imu message;
  message.header.stamp.sec = 1763437339;
  message.header.stamp.nanosec = 879452003;
  message.header.frame_id = "imu";
  message.linear_acceleration.z = -9.81;
  message.angular_velocity.x = 0.02;
  message.orientation.w = 1;
  message.orientation_covariance[0] = -1;
  message.angular_velocity_covariance[4] = 0.01;
  message.linear_acceleration_covariance[8] = 0.02;
  const auto sample = fast_livo::from_ros2(message);
  EXPECT_EQ(sample->timestamp.nanoseconds, 1763437339879452003LL);
  EXPECT_EQ(sample->frame_id, message.header.frame_id);
  EXPECT_DOUBLE_EQ(sample->linear_acceleration.z, -9.81);
  EXPECT_DOUBLE_EQ(sample->angular_velocity.x, 0.02);
  EXPECT_DOUBLE_EQ(sample->orientation[3], 1);
  EXPECT_DOUBLE_EQ(sample->orientation_covariance[0], -1);
  EXPECT_DOUBLE_EQ(sample->angular_velocity_covariance[4], 0.01);
  EXPECT_DOUBLE_EQ(sample->linear_acceleration_covariance[8], 0.02);
  message.header.stamp.sec = -1;
  EXPECT_THROW(fast_livo::from_ros2(message), std::invalid_argument);
}
