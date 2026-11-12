#include "imu_conversion.h"
#include <gtest/gtest.h>

TEST(ImuConversion, PreservesTimestampVectorsAndCovariances)
{
  sensor_msgs::Imu message;
  message.header.stamp = ros::Time(1763437339, 879452003);
  message.header.frame_id = "imu";
  message.linear_acceleration.x = 0.3;
  message.angular_velocity.z = -0.5;
  message.orientation.w = 1;
  message.orientation_covariance[0] = -1;
  message.angular_velocity_covariance[4] = 0.01;
  message.linear_acceleration_covariance[8] = 0.02;
  const auto sample = fast_livo::from_ros1(message);
  EXPECT_EQ(sample->timestamp.nanoseconds, 1763437339879452003LL);
  EXPECT_DOUBLE_EQ(sample->timestamp.seconds(), message.header.stamp.toSec());
  EXPECT_EQ(sample->frame_id, message.header.frame_id);
  EXPECT_DOUBLE_EQ(sample->linear_acceleration.x, message.linear_acceleration.x);
  EXPECT_DOUBLE_EQ(sample->angular_velocity.z, message.angular_velocity.z);
  EXPECT_DOUBLE_EQ(sample->orientation[3], 1);
  EXPECT_DOUBLE_EQ(sample->orientation_covariance[0], -1);
  EXPECT_DOUBLE_EQ(sample->angular_velocity_covariance[4], 0.01);
  EXPECT_DOUBLE_EQ(sample->linear_acceleration_covariance[8], 0.02);
}

int main(int argc, char **argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
