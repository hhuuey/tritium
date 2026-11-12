#pragma once

#include "fast_livo/measurements.h"
#include <ros/ros.h>
#include <tf2_ros/transform_broadcaster.h>
#include <map>

namespace fast_livo_ros1
{
class OutputBridge
{
public:
  explicit OutputBridge(ros::NodeHandle &node);
  fast_livo::Outputs callbacks();
private:
  void pose(const std::string &topic, const fast_livo::PoseSample &sample);
  void cloud(const std::string &topic, const fast_livo::CloudSample &sample);
  void image(const std::string &topic, const fast_livo::ImageSample &sample);
  void path(const std::vector<fast_livo::PoseSample> &samples);
  void planes(const std::vector<fast_livo::PlaneMarker> &samples);
  ros::NodeHandle &node_;
  tf2_ros::TransformBroadcaster tf_;
  std::map<std::string, ros::Publisher> poses_, clouds_, images_;
  ros::Publisher path_, planes_;
};
}
