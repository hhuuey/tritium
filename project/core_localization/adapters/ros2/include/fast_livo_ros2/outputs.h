#pragma once

#include "fast_livo/measurements.h"
#include <rclcpp/rclcpp.hpp>
#include <tf2_ros/transform_broadcaster.h>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <nav_msgs/msg/path.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <visualization_msgs/msg/marker_array.hpp>
#include <map>

namespace fast_livo_ros2
{
class OutputBridge
{
public:
  explicit OutputBridge(rclcpp::Node &node);
  fast_livo::Outputs callbacks();
private:
  void pose(const std::string &topic, const fast_livo::PoseSample &sample);
  void cloud(const std::string &topic, const fast_livo::CloudSample &sample);
  void image(const std::string &topic, const fast_livo::ImageSample &sample);
  void path(const std::vector<fast_livo::PoseSample> &samples);
  void planes(const std::vector<fast_livo::PlaneMarker> &samples);
  rclcpp::Node &node_;
  tf2_ros::TransformBroadcaster tf_;
  std::map<std::string, rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr> poses_;
  std::map<std::string, rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr> clouds_;
  std::map<std::string, rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr> images_;
  rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr vision_pose_;
  rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr path_;
  rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr planes_;
};
}
