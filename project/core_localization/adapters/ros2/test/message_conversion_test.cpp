#include "fast_livo_ros2/replay_conversion.h"
#include "fast_livo_ros2/replay_options.h"
#include "fast_livo_ros2/sensor_statistics.h"
#include "../validation/receipt_gate.h"
#include <gtest/gtest.h>

TEST(MessageConversion, PreservesCloudBytesFieldsPaddingAndNanoseconds)
{
  sensor_msgs::msg::PointCloud2 message;
  message.header.stamp.sec = 1763437339;
  message.header.stamp.nanosec = 879452003;
  message.header.frame_id = "lidar";
  message.height = 1;
  message.width = 2;
  message.point_step = 16;
  message.row_step = 40;
  message.is_bigendian = true;
  message.is_dense = false;
  sensor_msgs::msg::PointField field;
  field.name = "offset_time";
  field.offset = 8;
  field.datatype = sensor_msgs::msg::PointField::UINT32;
  field.count = 1;
  message.fields.push_back(field);
  for (std::uint8_t i = 0; i < 40; ++i) message.data.push_back(i);
  const auto sample = fast_livo_ros2::cloud(message);
  EXPECT_EQ(sample->timestamp.nanoseconds, 1763437339879452003LL);
  EXPECT_EQ(sample->frame_id, "lidar");
  EXPECT_EQ(sample->cloud.data, message.data);
  EXPECT_EQ(sample->cloud.row_step, 40U);
  EXPECT_EQ(sample->cloud.point_step, 16U);
  EXPECT_TRUE(sample->cloud.is_bigendian);
  EXPECT_FALSE(sample->cloud.is_dense);
  ASSERT_EQ(sample->cloud.fields.size(), 1U);
  EXPECT_EQ(sample->cloud.fields[0].name, "offset_time");
  EXPECT_EQ(sample->cloud.fields[0].offset, 8U);
  EXPECT_EQ(sample->cloud.fields[0].datatype, field.datatype);
  auto outgoing = *sample;
  outgoing.cloud.header.stamp = std::numeric_limits<std::uint64_t>::max();
  outgoing.cloud.header.frame_id = "不可作为ROS输出header";
  const auto restored = fast_livo_ros2::to_ros2(outgoing);
  EXPECT_EQ(restored.data, message.data);
  EXPECT_EQ(restored.header.stamp, message.header.stamp);
  EXPECT_EQ(restored, message);
}

TEST(MessageConversion, PreservesPvtVelocityQualityAndLivoxMetadata)
{
  fast_livo_ros2_interfaces::msg::GnssPVTSolnMsg pvt;
  pvt.time.week = 2444;
  pvt.time.tow = 123456.123456789;
  pvt.fix_type = 3;
  pvt.valid_fix = true;
  pvt.diff_soln = true;
  pvt.carr_soln = 2;
  pvt.num_sv = 24;
  pvt.latitude = 31.23;
  pvt.longitude = 121.47;
  pvt.altitude = 35.6;
  pvt.height_msl = 28.1;
  pvt.h_acc = 0.02;
  pvt.v_acc = 0.04;
  pvt.p_dop = 1.2;
  pvt.vel_n = 3.2;
  pvt.vel_e = -1.1;
  pvt.vel_d = 0.4;
  pvt.vel_acc = 0.03;
  const auto gps = fast_livo_ros2::gnss(pvt);
  EXPECT_EQ(gps->time.week, pvt.time.week);
  EXPECT_DOUBLE_EQ(gps->time.tow, pvt.time.tow);
#define EXPECT_FIELD(field) EXPECT_EQ(gps->field, pvt.field)
  EXPECT_FIELD(fix_type); EXPECT_FIELD(valid_fix); EXPECT_FIELD(diff_soln); EXPECT_FIELD(carr_soln);
  EXPECT_FIELD(num_sv); EXPECT_FIELD(latitude); EXPECT_FIELD(longitude); EXPECT_FIELD(altitude);
  EXPECT_FIELD(height_msl); EXPECT_FIELD(h_acc); EXPECT_FIELD(v_acc); EXPECT_FIELD(p_dop);
  EXPECT_FIELD(vel_n); EXPECT_FIELD(vel_e); EXPECT_FIELD(vel_d); EXPECT_FIELD(vel_acc);
#undef EXPECT_FIELD
  EXPECT_EQ(fast_livo_ros2::to_ros2(*gps), pvt);
  fast_livo_ros2_interfaces::msg::CustomMsg message;
  message.header.stamp.nanosec = 7;
  message.timebase = 1234567890123456789ULL;
  message.lidar_id = 4;
  message.rsvd = {1, 2, 3};
  message.point_num = 1;
  message.points.resize(1);
  message.points[0].offset_time = 9876543;
  message.points[0].tag = 0x20;
  message.points[0].line = 5;
  message.points[0].reflectivity = 127;
  message.points[0].x = 1.5f;
  message.points[0].y = -2.0f;
  message.points[0].z = 3.75f;
  const auto lidar = fast_livo_ros2::livox(message);
  EXPECT_EQ(lidar->timebase, message.timebase);
  EXPECT_EQ(lidar->rsvd, message.rsvd);
  EXPECT_EQ(lidar->lidar_id, message.lidar_id);
  EXPECT_EQ(lidar->timestamp.nanoseconds, 7);
  ASSERT_EQ(lidar->points.size(), 1U);
  EXPECT_EQ(lidar->points[0].offset_time, 9876543U);
  EXPECT_EQ(lidar->points[0].tag, 0x20);
  EXPECT_EQ(lidar->points[0].line, 5);
  EXPECT_EQ(lidar->points[0].reflectivity, 127);
  EXPECT_EQ(fast_livo_ros2::to_ros2(*lidar), message);
  message.point_num = 2;
  EXPECT_THROW(fast_livo_ros2::livox(message), std::invalid_argument);
}

TEST(MessageConversion, ConvertsRgbToOwnedBgrImage)
{
  auto message = std::make_shared<sensor_msgs::msg::Image>();
  message->header.stamp.nanosec = 123;
  message->header.frame_id = "camera";
  message->height = 1;
  message->width = 1;
  message->encoding = "rgb8";
  message->step = 3;
  message->data = {10, 20, 30};
  const auto sample = fast_livo_ros2::image(message);
  message->data.clear();
  EXPECT_EQ(sample->timestamp.nanoseconds, 123);
  EXPECT_EQ(sample->frame_id, "camera");
  const auto pixel = sample->image.at<cv::Vec3b>(0, 0);
  EXPECT_EQ(pixel[0], 30);
  EXPECT_EQ(pixel[1], 20);
  EXPECT_EQ(pixel[2], 10);
  const auto restored = fast_livo_ros2::to_ros2(*sample);
  EXPECT_EQ(restored.encoding, "bgr8");
  EXPECT_EQ(restored.header, message->header);
  EXPECT_EQ(restored.data, (std::vector<std::uint8_t>{30, 20, 10}));
}

TEST(ReplayConversion, PreservesImuHeaderOrientationAndCovariance)
{
  sensor_msgs::msg::Imu message;
  message.header.stamp.sec = 1763437339;
  message.header.stamp.nanosec = 879452003;
  message.header.frame_id = "imu";
  message.orientation.w = 1;
  message.linear_acceleration.z = -9.81;
  message.angular_velocity.x = 0.02;
  message.orientation_covariance[0] = -1;
  message.angular_velocity_covariance[4] = 0.01;
  message.linear_acceleration_covariance[8] = 0.02;
  EXPECT_EQ(fast_livo_ros2::to_ros2(*fast_livo::from_ros2(message)), message);
}

TEST(ReplayOptions, RejectsInvalidRatesAndMissingDataset)
{
  using fast_livo_ros2::replay_options;
  EXPECT_THROW(replay_options({"replay"}), std::invalid_argument);
  for (const auto *rate : {"0", "-1", "nan", "inf", "1.5x"})
    EXPECT_THROW(replay_options({"replay", "--dataset", "dataset", "--rate", rate}), std::invalid_argument);
  for (const auto *duration : {"0", "-1", "nan", "inf"})
    EXPECT_THROW(replay_options({"replay", "--dataset", "dataset", "--duration", duration}), std::invalid_argument);
  EXPECT_THROW(replay_options({"replay", "--dataset", "dataset", "--unknown"}), std::invalid_argument);
  const auto options = replay_options({"replay", "--dataset", "dataset", "--rate", "2.5", "--duration", "12"});
  EXPECT_EQ(options.dataset, "dataset");
  EXPECT_DOUBLE_EQ(options.rate, 2.5);
  EXPECT_DOUBLE_EQ(options.duration, 12);
}

TEST(ReceiptGate, RejectsMissingOrReorderedFramesEvenWhenTailMatches)
{
  const std::vector<std::int64_t> expected{1000, 101000, 201000};
  EXPECT_FALSE(fast_livo_ros2::same_timeline({1000, 201000}, expected));
  EXPECT_FALSE(fast_livo_ros2::same_timeline({101000, 1000, 201000}, expected));
  EXPECT_TRUE(fast_livo_ros2::same_timeline({1123, 101123, 201123}, expected));
}

TEST(ReceiptGate, DetectsMissingInputAndHeaderlessGnssTail)
{
  fast_livo_ros2::SensorStatistics transmitted, received;
  fast_livo::ImuSample imu;
  imu.timestamp = {1234567890123456789LL};
  fast_livo::GnssSample gnss;
  gnss.time = {2393, 186169.7};
  transmitted.record(imu);
  transmitted.record(gnss);
  received.record(imu);
  const auto published = YAML::Load(transmitted.json());
  EXPECT_FALSE(fast_livo_ros2::same_receipts(YAML::Load(received.json()), published));
  received.record(gnss);
  EXPECT_TRUE(fast_livo_ros2::same_receipts(YAML::Load(received.json()), published));
  auto wrong_tail = YAML::Load(received.json());
  wrong_tail["gnss_tow"] = 186169.8;
  EXPECT_FALSE(fast_livo_ros2::same_receipts(wrong_tail, published));
}
