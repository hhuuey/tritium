#pragma once

#include "fast_livo/io/dataset.h"
#include "fast_livo_ros2/replay_conversion.h"
#include <rclcpp/rclcpp.hpp>
#include <rosgraph_msgs/msg/clock.hpp>

namespace fast_livo_ros2
{
class ReplayPublishers
{
public:
  ReplayPublishers(rclcpp::Node &node, const fast_livo::io::Summary &summary);
  std::vector<std::string> missing_subscribers() const;
  void publish(const fast_livo::io::Event &event);
  void drain(std::chrono::nanoseconds timeout);
private:
  rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr imu_;
  rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr cloud_;
  rclcpp::Publisher<fast_livo_ros2_interfaces::msg::CustomMsg>::SharedPtr livox_;
  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr image_;
  rclcpp::Publisher<fast_livo_ros2_interfaces::msg::GnssPVTSolnMsg>::SharedPtr gnss_;
  rclcpp::Publisher<rosgraph_msgs::msg::Clock>::SharedPtr clock_;
};
}
