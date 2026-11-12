#include "fast_livo_ros2/replay_publishers.h"

namespace fast_livo_ros2
{
ReplayPublishers::ReplayPublishers(rclcpp::Node &node, const fast_livo::io::Summary &summary)
{
  const auto imu = node.declare_parameter<std::string>("imu_topic", "/livox/imu");
  const auto cloud = node.declare_parameter<std::string>("cloud_topic", "/livox/lidar");
  const auto livox = node.declare_parameter<std::string>("livox_topic", "/livox/lidar");
  const auto image = node.declare_parameter<std::string>("image_topic", "/left_camera/image");
  const auto gnss = node.declare_parameter<std::string>("gnss_topic", "/ublox_driver/receiver_pvt");
  if (summary.counts[1] && summary.counts[2] &&
      node.get_node_topics_interface()->resolve_topic_name(cloud) ==
          node.get_node_topics_interface()->resolve_topic_name(livox))
    throw std::invalid_argument("混合 PointCloud2/Livox 数据集必须设置不同 cloud_topic/livox_topic");
  if (summary.counts[0]) imu_ = node.create_publisher<sensor_msgs::msg::Imu>(imu, rclcpp::QoS(2000).reliable());
  if (summary.counts[1])
    cloud_ = node.create_publisher<sensor_msgs::msg::PointCloud2>(cloud, rclcpp::QoS(20).reliable());
  if (summary.counts[2])
    livox_ = node.create_publisher<fast_livo_ros2_interfaces::msg::CustomMsg>(livox, rclcpp::QoS(20).reliable());
  if (summary.counts[3]) image_ = node.create_publisher<sensor_msgs::msg::Image>(image, rclcpp::QoS(10).reliable());
  if (summary.counts[4])
    gnss_ = node.create_publisher<fast_livo_ros2_interfaces::msg::GnssPVTSolnMsg>(gnss, rclcpp::QoS(2000).reliable());
  clock_ = node.create_publisher<rosgraph_msgs::msg::Clock>("/clock", rclcpp::ClockQoS().reliable());
}

std::vector<std::string> ReplayPublishers::missing_subscribers() const
{
  std::vector<std::string> topics;
  const auto check = [&topics](const auto &publisher) {
    if (publisher && publisher->get_subscription_count() == 0) topics.emplace_back(publisher->get_topic_name());
  };
  check(imu_); check(cloud_); check(livox_); check(image_); check(gnss_); check(clock_);
  return topics;
}

void ReplayPublishers::publish(const fast_livo::io::Event &event)
{
  rosgraph_msgs::msg::Clock time;
  time.clock = stamp(event.record_time);
  clock_->publish(time);
  const auto send = [](const auto &publisher, const auto &sample) {
    if (!publisher || !sample) throw std::runtime_error("回放事件类型计数或样本无效");
    publisher->publish(to_ros2(*sample));
  };
  switch (event.kind())
  {
    case fast_livo::io::Kind::imu:
      send(imu_, std::get<fast_livo::ImuSample::ConstPtr>(event.sample)); break;
    case fast_livo::io::Kind::cloud:
      send(cloud_, std::get<fast_livo::CloudSample::ConstPtr>(event.sample)); break;
    case fast_livo::io::Kind::livox:
      send(livox_, std::get<fast_livo::LivoxSample::ConstPtr>(event.sample)); break;
    case fast_livo::io::Kind::image:
      send(image_, std::get<fast_livo::ImageSample::ConstPtr>(event.sample)); break;
    case fast_livo::io::Kind::gnss:
      send(gnss_, std::get<fast_livo::GnssSample::ConstPtr>(event.sample)); break;
    default: throw std::runtime_error("不支持的中立回放事件");
  }
}

void ReplayPublishers::drain(std::chrono::nanoseconds timeout)
{
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  const auto flush = [deadline](const auto &publisher) {
    if (!publisher) return;
    const auto remaining = deadline - std::chrono::steady_clock::now();
    if (remaining <= std::chrono::nanoseconds::zero() || !publisher->wait_for_all_acked(remaining))
      throw std::runtime_error(std::string("等待 DDS 确认超时: ") + publisher->get_topic_name());
  };
  flush(imu_); flush(cloud_); flush(livox_); flush(image_); flush(gnss_); flush(clock_);
}
}
