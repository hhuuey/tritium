#pragma once

#include "fast_livo/engine.h"
#include <ros/ros.h>
#include <std_srvs/Trigger.h>
#include <array>
#include <vector>

namespace fast_livo_ros1
{
class Inputs
{
public:
  Inputs(ros::NodeHandle &node, fast_livo::Engine &engine, const fast_livo::Config &config);
  ~Inputs();
private:
  bool statistics_ = false;
  std::array<std::uint64_t, 5> counts_{};
  std::vector<ros::Subscriber> subscriptions_;
  ros::Timer timer_;
  ros::ServiceServer optimization_;
};
}
