# 对应 Livox ROS Driver 的 CustomMsg，保留原始时间基与保留字节。
# Copyright (c) 2019 Livox. All rights reserved. MIT，见 LICENSE.livox_ros_driver。
std_msgs/Header header
uint64 timebase
uint32 point_num
uint8 lidar_id
uint8[3] rsvd
CustomPoint[] points
