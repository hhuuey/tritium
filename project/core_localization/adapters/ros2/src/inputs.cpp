#include "fast_livo_ros2/inputs.h"
#include "fast_livo_ros2/conversions.h"
#include <rclcpp/create_timer.hpp>

namespace fast_livo_ros2
{
Inputs::Inputs(rclcpp::Node &node, fast_livo::Engine &engine, const fast_livo::Config &config)
    : logger_(node.get_logger()),
      statistics_(node.declare_parameter<bool>("adapter.input_statistics", false))
{
  const auto depth = node.declare_parameter<std::int64_t>("adapter.sensor_queue_depth", 200000);
  const auto reliability = node.declare_parameter<std::string>("adapter.input_reliability", "best_effort");
  if (depth < 1 || (reliability != "reliable" && reliability != "best_effort"))
    throw std::invalid_argument("adapter QoS 必须是正数队列与 reliable/best_effort 可靠性");
  auto qos = rclcpp::SensorDataQoS().keep_last(static_cast<std::size_t>(depth));
  if (reliability == "reliable") qos.reliable();
  if (config.preprocess_lidar_type == 1)
    subscriptions_.push_back(node.create_subscription<fast_livo_ros2_interfaces::msg::CustomMsg>(
        config.common_lid_topic, qos,
        [this, &engine](fast_livo_ros2_interfaces::msg::CustomMsg::ConstSharedPtr m) {
          auto sample = livox(*m);
          statistics_.record(*sample);
          engine.push_livox(sample);
        }));
  else
    subscriptions_.push_back(node.create_subscription<sensor_msgs::msg::PointCloud2>(
        config.common_lid_topic, qos,
        [this, &engine](sensor_msgs::msg::PointCloud2::ConstSharedPtr m) {
          auto sample = cloud(*m);
          statistics_.record(*sample);
          engine.push_cloud(sample);
        }));
  subscriptions_.push_back(node.create_subscription<sensor_msgs::msg::Imu>(
      config.common_imu_topic, qos,
      [this, &engine](sensor_msgs::msg::Imu::ConstSharedPtr m) {
        auto sample = fast_livo::from_ros2(*m);
        statistics_.record(*sample);
        engine.push_imu(sample);
      }));
  subscriptions_.push_back(node.create_subscription<sensor_msgs::msg::Image>(
      config.common_img_topic, qos,
      [this, &engine](sensor_msgs::msg::Image::ConstSharedPtr m) {
        auto sample = image(m);
        statistics_.record(*sample);
        engine.push_image(sample);
      }));
  auto gnss_qos = qos;
  gnss_qos.keep_last(2000);
  subscriptions_.push_back(node.create_subscription<fast_livo_ros2_interfaces::msg::GnssPVTSolnMsg>(
      config.gps_gps_topic, gnss_qos,
      [this, &engine](fast_livo_ros2_interfaces::msg::GnssPVTSolnMsg::ConstSharedPtr m) {
        auto sample = gnss(*m);
        statistics_.record(*sample);
        engine.push_gnss(sample);
      }));
  if (statistics_.enabled())
    input_status_ = node.create_service<std_srvs::srv::Trigger>(
        "~/input_status", [this](const std_srvs::srv::Trigger::Request::SharedPtr,
                                std_srvs::srv::Trigger::Response::SharedPtr response) {
          response->success = true;
          response->message = statistics_.json();
        });
  timer_ = rclcpp::create_timer(&node, node.get_clock(), rclcpp::Duration(0, 4000000),
                              [&engine] { engine.propagate_imu(); });
  optimization_ = node.create_service<std_srvs::srv::Trigger>(
      "~/optimize", [&engine](const std_srvs::srv::Trigger::Request::SharedPtr,
                             std_srvs::srv::Trigger::Response::SharedPtr response) {
        engine.request_optimization();
        response->success = true;
        response->message = "后端优化已请求；关闭节点时等待完成";
      });
}

Inputs::~Inputs()
{
  if (statistics_.enabled()) RCLCPP_INFO(logger_, "RECEIVED_INPUTS %s", statistics_.json().c_str());
}
}
