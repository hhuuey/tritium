#pragma once

#include "fast_livo/engine.h"
#include "fast_livo_ros2/sensor_statistics.h"
#include <rclcpp/rclcpp.hpp>
#include <std_srvs/srv/trigger.hpp>
#include <vector>

namespace fast_livo_ros2
{
class Inputs
{
public:
  Inputs(rclcpp::Node &node, fast_livo::Engine &engine, const fast_livo::Config &config);
  ~Inputs();
private:
  rclcpp::Logger logger_;
  SensorStatistics statistics_;
  std::vector<rclcpp::SubscriptionBase::SharedPtr> subscriptions_;
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr optimization_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr input_status_;
};
}
