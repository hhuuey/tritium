#include "fast_livo_ros1/inputs.h"
#include "fast_livo_ros1/conversions.h"

namespace fast_livo_ros1
{
Inputs::Inputs(ros::NodeHandle &node, fast_livo::Engine &engine, const fast_livo::Config &config)
{
  node.param("adapter/input_statistics", statistics_, false);
  const auto transport = ros::TransportHints().tcpNoDelay();
  if (config.preprocess_lidar_type == 1)
    subscriptions_.push_back(node.subscribe<livox_ros_driver::CustomMsg>(
        config.common_lid_topic, 200000,
        [this, &engine](const livox_ros_driver::CustomMsg::ConstPtr &m) {
          if (statistics_) ++counts_[2];
          engine.push_livox(livox(*m));
        },
        ros::VoidConstPtr(), transport));
  else
    subscriptions_.push_back(node.subscribe<sensor_msgs::PointCloud2>(
        config.common_lid_topic, 200000,
        [this, &engine](const sensor_msgs::PointCloud2::ConstPtr &m) {
          if (statistics_) ++counts_[1];
          engine.push_cloud(cloud(*m));
        },
        ros::VoidConstPtr(), transport));
  subscriptions_.push_back(node.subscribe<sensor_msgs::Imu>(
      config.common_imu_topic, 200000,
      [this, &engine](const sensor_msgs::Imu::ConstPtr &m) {
        if (statistics_) ++counts_[0];
        engine.push_imu(fast_livo::from_ros1(*m));
      },
      ros::VoidConstPtr(), transport));
  subscriptions_.push_back(node.subscribe<sensor_msgs::Image>(
      config.common_img_topic, 200000,
      [this, &engine](const sensor_msgs::Image::ConstPtr &m) {
        if (statistics_) ++counts_[3];
        engine.push_image(image(m));
      },
      ros::VoidConstPtr(), transport));
  subscriptions_.push_back(node.subscribe<gnss_comm::GnssPVTSolnMsg>(
      config.gps_gps_topic, 2000,
      [this, &engine](const gnss_comm::GnssPVTSolnMsg::ConstPtr &m) {
        if (statistics_) ++counts_[4];
        engine.push_gnss(gnss(*m));
      },
      ros::VoidConstPtr(), transport));
  timer_ = node.createTimer(ros::Duration(1.0 / 250.0),
                           [&engine](const ros::TimerEvent &) { engine.propagate_imu(); });
  ros::NodeHandle private_node("~");
  optimization_ = private_node.advertiseService<std_srvs::Trigger::Request, std_srvs::Trigger::Response>(
      "optimize", [&engine](std_srvs::Trigger::Request &, std_srvs::Trigger::Response &response) {
        engine.request_optimization();
        response.success = true;
        response.message = "后端优化已请求；关闭节点时等待完成";
        return true;
      });
}

Inputs::~Inputs()
{
  if (statistics_)
    ROS_INFO_STREAM("RECEIVED_COUNTS imu=" << counts_[0] << " cloud=" << counts_[1]
                    << " livox=" << counts_[2] << " image=" << counts_[3] << " gnss=" << counts_[4]);
}

}
