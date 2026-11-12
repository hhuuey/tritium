// SPDX-License-Identifier: GPL-2.0-only
#include <cv_bridge/cv_bridge.h>
#include <opencv2/imgproc.hpp>
#include <ros/ros.h>
#include <rosbag/bag.h>
#include <rosbag/view.h>
#include <sensor_msgs/CompressedImage.h>
#include <sensor_msgs/Image.h>
#include <iostream>

namespace
{
void check(bool condition, const char *message)
{
  if (!condition) throw std::runtime_error(message);
}

int run(const std::string &filename)
{
  rosbag::Bag bag(filename, rosbag::bagmode::Read);
  rosbag::View view(bag, rosbag::TopicQuery("/left_camera/image/compressed"));
  check(view.begin() != view.end(), "真实 bag 中没有 HH 压缩图像话题");
  const auto compressed = view.begin()->instantiate<sensor_msgs::CompressedImage>();
  check(bool(compressed), "真实首帧消息不是 CompressedImage");
  check(compressed->format == "rgb8; jpeg compressed bgr8", "首帧不是待核验的真实 HH JPEG format");
  const cv::Mat direct = cv_bridge::toCvCopy(compressed, "bgr8")->image;
  ros::NodeHandle node;
  sensor_msgs::Image::ConstPtr received;
  const auto subscriber = node.subscribe<sensor_msgs::Image>(
      "/equivalence/raw", 1, [&](const sensor_msgs::Image::ConstPtr &message) { received = message; });
  auto publisher = node.advertise<sensor_msgs::CompressedImage>("/equivalence/image/compressed", 1, true);
  const auto deadline = ros::WallTime::now() + ros::WallDuration(15);
  while (ros::ok() && publisher.getNumSubscribers() == 0 && ros::WallTime::now() < deadline)
  {
    ros::spinOnce(); ros::WallDuration(0.02).sleep();
  }
  check(publisher.getNumSubscribers() > 0, "republish 未在期限内订阅压缩输入");
  publisher.publish(compressed);
  while (ros::ok() && !received && ros::WallTime::now() < deadline)
  {
    ros::spinOnce(); ros::WallDuration(0.02).sleep();
  }
  check(bool(received), "republish 未在期限内发布原始图像");
  const cv::Mat republished = cv_bridge::toCvCopy(received, "bgr8")->image;
  check(direct.type() == CV_8UC3 && direct.size() == republished.size(), "两条链路的图像尺寸或类型不同");
  cv::Mat difference, swapped;
  cv::absdiff(direct, republished, difference);
  cv::cvtColor(direct, swapped, cv::COLOR_BGR2RGB);
  const auto different_channels = cv::countNonZero(difference.reshape(1));
  const double max_error = cv::norm(direct, republished, cv::NORM_INF);
  const double swapped_error = cv::norm(swapped, republished, cv::NORM_INF);
  std::cout << "compressed_format: " << compressed->format << '\n'
            << "republished_encoding: " << received->encoding << '\n'
            << "width: " << direct.cols << "\nheight: " << direct.rows << '\n'
            << "different_channel_values: " << different_channels << '\n'
            << "max_absolute_error: " << max_error << '\n'
            << "red_blue_swapped_max_error: " << swapped_error << '\n'
            << "source_header_sequence: " << compressed->header.seq << '\n'
            << "republished_header_sequence: " << received->header.seq << '\n'
            << "header_stamp_equal: " << (compressed->header.stamp == received->header.stamp) << '\n'
            << "header_frame_equal: " << (compressed->header.frame_id == received->header.frame_id) << '\n';
  check(different_channels == 0 && max_error == 0, "直接压缩转换与实际 republish 链路不等价");
  check(compressed->header.stamp == received->header.stamp &&
        compressed->header.frame_id == received->header.frame_id, "republish 改变时间戳或 frame");
  return 0;
}
}

int main(int argc, char **argv)
{
  ros::init(argc, argv, "image_transport_equivalence_test");
  if (argc != 2) { std::cerr << "用法: image_transport_equivalence_test HH.bag\n"; return 2; }
  try { return run(argv[1]); }
  catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
