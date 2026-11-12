// FAST-LIVO2 ROS 边界；上游开发者 Chunran Zheng，授权沿用上游 LICENSE。
#include "fast_livo_ros2/outputs.h"
#include "fast_livo_ros2/conversions.h"
#include "fast_livo_ros2/replay_conversion.h"
#include <geometry_msgs/msg/transform_stamped.hpp>

namespace fast_livo_ros2
{
namespace
{
geometry_msgs::msg::Pose pose_message(const fast_livo::PoseSample &sample)
{
  geometry_msgs::msg::Pose result;
  result.position.x = sample.position.x;
  result.position.y = sample.position.y;
  result.position.z = sample.position.z;
  result.orientation.x = sample.orientation.x;
  result.orientation.y = sample.orientation.y;
  result.orientation.z = sample.orientation.z;
  result.orientation.w = sample.orientation.w;
  return result;
}
}

OutputBridge::OutputBridge(rclcpp::Node &node) : node_(node), tf_(node)
{
  for (const auto *topic : {"/odometry/fast_livo2", "/gps/odometry", "/LIVO2/imu_propagate"})
    poses_[topic] = node_.create_publisher<nav_msgs::msg::Odometry>(topic, 10000);
  poses_["/aft_mapped_to_init"] = node_.create_publisher<nav_msgs::msg::Odometry>("/aft_mapped_to_init", 10);
  vision_pose_ = node_.create_publisher<geometry_msgs::msg::PoseStamped>("/mavros/vision_pose/pose", 10);
  clouds_["/synced_cloud"] = node_.create_publisher<sensor_msgs::msg::PointCloud2>("/synced_cloud", 10000);
  for (const auto *topic : {"/cloud_registered", "/cloud_visual_sub_map_before", "/cloud_effected", "/Laser_map",
                           "/dyn_obj", "/dyn_obj_removed", "/dyn_obj_dbg_hist"})
    clouds_[topic] = node_.create_publisher<sensor_msgs::msg::PointCloud2>(topic, 100);
  images_["/rgb_img"] = node_.create_publisher<sensor_msgs::msg::Image>("/rgb_img", 1);
  path_ = node_.create_publisher<nav_msgs::msg::Path>("/path", 10);
  planes_ = node_.create_publisher<visualization_msgs::msg::MarkerArray>("/planes", 10000);
}

fast_livo::Outputs OutputBridge::callbacks()
{
  fast_livo::Outputs result;
  result.now = [this] { return fast_livo::Timestamp{node_.now().nanoseconds()}; };
  result.pose = [this](const auto &topic, const auto &sample) { pose(topic, sample); };
  result.cloud = [this](const auto &topic, const auto &sample) { cloud(topic, sample); };
  result.image = [this](const auto &topic, const auto &sample) { image(topic, sample); };
  result.path = [this](const auto &samples) { path(samples); };
  result.planes = [this](const auto &samples) { planes(samples); };
  return result;
}

void OutputBridge::pose(const std::string &topic, const fast_livo::PoseSample &sample)
{
  std_msgs::msg::Header header;
  header.stamp = stamp(sample.timestamp);
  header.frame_id = sample.frame_id;
  const auto body = pose_message(sample);
  if (!sample.child_frame_id.empty())
  {
    geometry_msgs::msg::TransformStamped transform;
    transform.header = header;
    transform.child_frame_id = sample.child_frame_id;
    transform.transform.translation.x = sample.position.x;
    transform.transform.translation.y = sample.position.y;
    transform.transform.translation.z = sample.position.z;
    transform.transform.rotation = body.orientation;
    tf_.sendTransform(transform);
  }
  if (topic == "/mavros/vision_pose/pose")
  {
    geometry_msgs::msg::PoseStamped message;
    message.header = header;
    message.pose = body;
    vision_pose_->publish(message);
    return;
  }
  auto found = poses_.find(topic);
  if (found == poses_.end())
    found = poses_.emplace(topic, node_.create_publisher<nav_msgs::msg::Odometry>(topic, 10)).first;
  nav_msgs::msg::Odometry message;
  message.header = header;
  message.child_frame_id = sample.child_frame_id;
  message.pose.pose = body;
  message.pose.covariance = sample.covariance;
  message.twist.twist.linear.x = sample.velocity.x;
  message.twist.twist.linear.y = sample.velocity.y;
  message.twist.twist.linear.z = sample.velocity.z;
  found->second->publish(message);
}

void OutputBridge::cloud(const std::string &topic, const fast_livo::CloudSample &sample)
{
  auto message = to_ros2(sample);
  auto found = clouds_.find(topic);
  if (found == clouds_.end())
    found = clouds_.emplace(topic, node_.create_publisher<sensor_msgs::msg::PointCloud2>(topic, 100)).first;
  found->second->publish(message);
}

void OutputBridge::image(const std::string &topic, const fast_livo::ImageSample &sample)
{
  std_msgs::msg::Header header;
  header.stamp = stamp(sample.timestamp);
  header.frame_id = sample.frame_id;
  auto found = images_.find(topic);
  if (found == images_.end())
    found = images_.emplace(topic, node_.create_publisher<sensor_msgs::msg::Image>(topic, 1)).first;
  found->second->publish(*cv_bridge::CvImage(header, "bgr8", sample.image).toImageMsg());
}

void OutputBridge::path(const std::vector<fast_livo::PoseSample> &samples)
{
  nav_msgs::msg::Path message;
  message.header.frame_id = samples.empty() ? "camera_init" : samples.back().frame_id;
  message.header.stamp = samples.empty() ? static_cast<builtin_interfaces::msg::Time>(node_.now())
                                        : stamp(samples.back().timestamp);
  message.poses.reserve(samples.size());
  for (const auto &sample : samples)
  {
    geometry_msgs::msg::PoseStamped pose;
    pose.header.frame_id = sample.frame_id;
    pose.header.stamp = stamp(sample.timestamp);
    pose.pose = pose_message(sample);
    message.poses.push_back(pose);
  }
  path_->publish(message);
}

void OutputBridge::planes(const std::vector<fast_livo::PlaneMarker> &samples)
{
  visualization_msgs::msg::MarkerArray message;
  message.markers.reserve(samples.size());
  for (const auto &sample : samples)
  {
    visualization_msgs::msg::Marker marker;
    marker.header.frame_id = sample.frame_id;
    marker.header.stamp = stamp(sample.timestamp);
    marker.ns = sample.name;
    marker.id = sample.id;
    marker.type = visualization_msgs::msg::Marker::CYLINDER;
    marker.action = visualization_msgs::msg::Marker::ADD;
    fast_livo::PoseSample pose;
    pose.position = sample.position;
    pose.orientation = sample.orientation;
    marker.pose = pose_message(pose);
    marker.scale.x = sample.scale.x;
    marker.scale.y = sample.scale.y;
    marker.scale.z = sample.scale.z;
    marker.color.r = sample.color[0];
    marker.color.g = sample.color[1];
    marker.color.b = sample.color[2];
    marker.color.a = sample.color[3];
    message.markers.push_back(marker);
  }
  planes_->publish(message);
}
}
