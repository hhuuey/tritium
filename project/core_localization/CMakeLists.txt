cmake_minimum_required(VERSION 3.16)
project(fast_livo LANGUAGES C CXX)
find_package(catkin REQUIRED COMPONENTS
  roscpp std_msgs sensor_msgs geometry_msgs nav_msgs visualization_msgs std_srvs
  tf2_ros cv_bridge pcl_conversions image_transport gnss_comm livox_ros_driver)
catkin_package()
add_subdirectory(adapters/ros1)
install(DIRECTORY config launch scripts foxglove DESTINATION share/fast_livo)
