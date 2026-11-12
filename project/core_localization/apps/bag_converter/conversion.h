// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "fast_livo/io/dataset.h"
#include <rosbag/message_instance.h>

namespace fast_livo::bag_converter
{
enum class Role { imu, lidar, image, gnss };
io::Event convert(const rosbag::MessageInstance &message, std::uint64_t sequence, Role role);
bool supported(const std::string &type, Role role);
}
