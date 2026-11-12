/*
This file is part of FAST-LIVO2: Fast, Direct LiDAR-Inertial-Visual Odometry.
Developer: Chunran Zheng <zhengcr@connect.hku.hk>
This file is subject to the accompanying LICENSE file.
*/
// 仅在适配器验证目标编译；生产后端和中立测试不依赖这些 ROS 头文件。
#define FAST_LIVO_BACKEND_INTERNAL
#include "fast_livo/backend.h"
#include <message_filters/sync_policies/approximate_time.h>
#include <message_filters/synchronizer.h>
#include <nav_msgs/Odometry.h>
#include <sensor_msgs/PointCloud2.h>
#include <tf/transform_datatypes.h>
#include <boost/make_shared.hpp>
#include <iostream>
#include <iomanip>
#include <random>
#include <stdexcept>

int main()
{
  using Pair = std::pair<std::uint32_t, std::uint32_t>;
  using Policy = message_filters::sync_policies::ApproximateTime<nav_msgs::Odometry, sensor_msgs::PointCloud2>;
  ros::Time::init();
  ros::console::set_logger_level(ROSCONSOLE_DEFAULT_NAME, ros::console::levels::Error);
  try
  {
    std::mt19937 orientation_random(12345);
    for (unsigned i = 0; i < 1000; ++i)
    {
      fast_livo::PoseSample pose;
      auto component = [&] { return static_cast<double>(orientation_random() % 200001) / 100000.0 - 1.0; };
      pose.orientation = {component(), component(), component(), component()};
      if (i == 0) pose.orientation = {0, std::sqrt(0.5), 0, std::sqrt(0.5)};
      if (i == 1) pose.orientation = {0, -std::sqrt(0.5), 0, std::sqrt(0.5)};
      if (i == 2) pose.orientation = {0, 0, 0, 2};
      const auto& q = pose.orientation;
      geometry_msgs::Quaternion message;
      message.x = q.x; message.y = q.y; message.z = q.z; message.w = q.w;
      for (bool from_message : {false, true})
      {
        tf::Quaternion reference(q.x, q.y, q.z, q.w);
        if (from_message) tf::quaternionMsgToTF(message, reference);
        double roll, pitch, yaw;
        tf::Matrix3x3(reference).getRPY(roll, pitch, yaw);
        const auto expected = gtsam::Rot3::RzRyRx(roll, pitch, yaw);
        const auto actual = fast_livo::backend_detail::poseFromSample(pose, from_message).rotation();
        if ((actual.matrix() - expected.matrix()).norm() > 1e-14)
        {
          std::cerr << std::setprecision(17) << "case=" << i << ", from_message=" << from_message
                    << ", quaternion=" << q.x << "," << q.y << "," << q.z << "," << q.w
                    << "\nactual:\n" << actual.matrix() << "\nexpected:\n" << expected.matrix() << "\n";
          throw std::runtime_error("中立四元数转换与旧 tf 数值行为不一致");
        }
      }
    }
    for (double offset : {0.5e-9, -0.5e-9, 0.123456789, -0.123456789, 1500000000.123456})
      if (fast_livo::backend_detail::durationNanoseconds(offset) != ros::Duration(offset).toNSec())
        throw std::runtime_error("中立 Duration 转换与 ROS 纳秒量化不一致");
    for (unsigned seed = 0; seed < 100; ++seed)
    {
      std::vector<Pair> actual, expected;
      fast_livo::backend_detail::Pairing pairing([&](const auto& pose, const auto& cloud) {
        actual.emplace_back(static_cast<std::uint32_t>(pose.position.x), std::stoul(cloud.frame_id));
      });
      message_filters::Synchronizer<Policy> original(Policy(10));
      boost::function<void(const nav_msgs::Odometry::ConstPtr&, const sensor_msgs::PointCloud2::ConstPtr&)> cb =
          [&](const auto& pose, const auto& cloud) { expected.emplace_back(pose->header.seq, cloud->header.seq); };
      original.registerCallback(boost::bind(cb, boost::placeholders::_1, boost::placeholders::_2));
      std::mt19937 random(seed);
      std::array<std::int64_t, 2> time{{1700000000000000000LL, 1700000000000000000LL}};
      for (unsigned index = 0; index < 600; ++index)
      {
        const unsigned stream = random() % 2;
        // 覆盖同时间、乱序、抖动、单侧积压溢出和纳秒舍入边界。
        time[stream] += random() % 100000000;
        std::int64_t stamp = time[stream];
        if (index % 7 == 0) stamp = time[1 - stream];
        if (index % 11 == 0) stamp -= random() % 200000000;
        if (stream == 0)
        {
          fast_livo::PoseSample pose;
          pose.timestamp.nanoseconds = stamp;
          pose.position.x = index;
          auto message = boost::make_shared<nav_msgs::Odometry>();
          message->header.stamp.fromNSec(stamp);
          message->header.seq = index;
          pairing.add(pose);
          original.add<0>(message);
        }
        else
        {
          fast_livo::CloudSample cloud;
          cloud.timestamp.nanoseconds = stamp;
          cloud.frame_id = std::to_string(index);
          auto message = boost::make_shared<sensor_msgs::PointCloud2>();
          message->header.stamp.fromNSec(stamp);
          message->header.seq = index;
          pairing.add(cloud);
          original.add<1>(message);
        }
        if (actual != expected)
          throw std::runtime_error("中立配对与 ROS1 ApproximateTime(10) 不同: seed=" +
                                   std::to_string(seed) + ", event=" + std::to_string(index));
      }
    }
  }
  catch (const std::exception& exception)
  {
    std::cerr << exception.what() << "\n";
    return 1;
  }
  return 0;
}
