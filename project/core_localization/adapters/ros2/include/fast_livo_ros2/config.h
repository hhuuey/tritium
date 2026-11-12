#pragma once

#include "fast_livo/config.h"
#include <rclcpp/rclcpp.hpp>

namespace fast_livo_ros2
{
fast_livo::Config load_config(rclcpp::Node &node);
}
