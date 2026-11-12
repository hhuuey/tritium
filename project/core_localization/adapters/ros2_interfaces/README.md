# ROS 2 等价传感器消息

本包使用 rosidl 生成 Jazzy C++/Python 接口，不依赖 ROS 1 或专有 ROS 2 驱动包。

- `GnssTimeMsg`、`GnssPVTSolnMsg`：字段逐项对应 `.deps/gnss_comm/msg`，
  包括完整周/周内秒、位置、NED 速度、精度、DOP、卫星数、fix/differential/carrier 状态。
  原作者 CAO Shaozu / HKUST Aerial Robotics Group；保留 GPLv3 授权全文 `LICENSE.gnss_comm`。
- `CustomMsg`、`CustomPoint`：字段逐项对应 Livox ROS Driver，
  包括 header、timebase、point_num、lidar_id、rsvd、offset_time、反射率、tag、line。
  Copyright (c) 2019 Livox，保留上游 MIT 授权全文 `LICENSE.livox_ros_driver`。
- 这些字段定义由 `.msg` 生成等价 IDL；不是把 GNSS 降级成 NavSatFix，也不是空类型占位。

CMake 依赖 ament_cmake、rosidl_default_generators、std_msgs；
运行依赖 rosidl_default_runtime。`colcon build --base-paths <本目录>` 即可生成，
接口安装于 `<prefix>/share/fast_livo_ros2_interfaces`。
上游消息包的消息名虽然相同，ROS 2 类型的包名不同；驱动、转换工具与 rosbag
必须实际发布 `fast_livo_ros2_interfaces` 类型，不能只 remap 话题。
