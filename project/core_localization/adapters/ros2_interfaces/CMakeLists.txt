cmake_minimum_required(VERSION 3.16)
project(fast_livo_ros2_interfaces)

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/GnssTimeMsg.msg"
  "msg/GnssPVTSolnMsg.msg"
  "msg/CustomPoint.msg"
  "msg/CustomMsg.msg"
  DEPENDENCIES std_msgs)
ament_export_dependencies(rosidl_default_runtime)
install(FILES LICENSE.gnss_comm LICENSE.livox_ros_driver DESTINATION share/${PROJECT_NAME})
ament_package()
