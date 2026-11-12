#pragma once

#include "fast_livo/config.h"
#include <ros/node_handle.h>

namespace fast_livo_ros1
{
fast_livo::Config load_config(ros::NodeHandle &node);
}
