#include "receipt_gate.h"
#include <nav_msgs/msg/odometry.hpp>
#include <rclcpp/rclcpp.hpp>
#include <std_srvs/srv/trigger.hpp>
#include <chrono>
#include <filesystem>
#include <iostream>

namespace fast_livo_ros2
{
class ReceiptGate
{
public:
  ReceiptGate()
  {
    node_ = std::make_shared<rclcpp::Node>("fast_livo_receipt_gate");
    directory_ = node_->declare_parameter<std::string>("output_directory", "");
    published_path_ = node_->declare_parameter<std::string>("published_statistics", "");
    ready_path_ = node_->declare_parameter<std::string>("ready_file", "");
    timeout_ = node_->declare_parameter<double>("timeout", 180);
    optimize_on_pass_ = node_->declare_parameter<bool>("request_optimization", false);
    const auto reference = node_->declare_parameter<std::string>("expected_trajectory", "");
    frontend_expected_ = native_timeline(reference);
    expected_ = native_timeline(node_->declare_parameter<std::string>("expected_odometry_trajectory", reference));
    frontend_path_ = node_->declare_parameter<std::string>(
        "frontend_trajectory", directory_ + "/Log/result/Grass01.txt");
    const auto capture = node_->declare_parameter<std::string>("recorded_odometry", "");
    const auto recorded_inputs = node_->declare_parameter<std::string>("recorded_input_statistics", "");
    if (directory_.empty() || published_path_.empty() || ready_path_.empty() || timeout_ <= 0)
      throw std::invalid_argument("验收文件路径必须显式指定，timeout 必须为正");
    if (!capture.empty())
    {
      std::ifstream input(capture);
      std::int64_t timestamp;
      while (input >> timestamp) actual_.push_back(timestamp);
      if (actual_.empty()) throw std::runtime_error("真实 odometry 记录为空");
      received_ = YAML::LoadFile(recorded_inputs);
      recorded_ = true;
      executor_.add_node(node_);
      return;
    }
    client_ = node_->create_client<std_srvs::srv::Trigger>("/laserMapping/input_status");
    odometry_ = node_->create_subscription<nav_msgs::msg::Odometry>(
        "/odometry/fast_livo2", rclcpp::QoS(10000).reliable(),
        [this](nav_msgs::msg::Odometry::ConstSharedPtr message) {
          actual_.push_back(static_cast<std::int64_t>(message->header.stamp.sec) * 1000000000LL +
                            message->header.stamp.nanosec);
          last_pose_ = Clock::now();
          std::ofstream output(directory_ + "/odometry-stamps.txt", std::ios::app);
          output << actual_.back() << '\n';
        });
    executor_.add_node(node_);
  }

  void run()
  {
    const auto deadline = Clock::now() + std::chrono::duration<double>(timeout_);
    auto next_query = Clock::now();
    bool ready = false;
    rclcpp::WallRate rate(100);
    while (rclcpp::ok() && Clock::now() < deadline)
    {
      executor_.spin_some();
      if (!error_.empty()) throw std::runtime_error(error_);
      if (!ready && (recorded_ || (odometry_->get_publisher_count() && client_->service_is_ready())))
      {
        write_report(ready_path_, "ready");
        ready = true;
        RCLCPP_INFO(node_->get_logger(), "验收就绪，LIO=%zu，VIO里程计=%zu，期望VIO末时间=%lld",
                    frontend_expected_.size(), expected_.size(), static_cast<long long>(expected_.back()));
      }
      if (ready && !recorded_ && !pending_ && Clock::now() >= next_query)
      {
        pending_ = true;
        client_->async_send_request(std::make_shared<std_srvs::srv::Trigger::Request>(),
                                   [this](rclcpp::Client<std_srvs::srv::Trigger>::SharedFuture future) {
          pending_ = false;
          try
          {
            const auto response = future.get();
            if (!response->success) throw std::runtime_error("输入统计服务失败");
            received_ = YAML::Load(response->message);
            write_report(directory_ + "/received-inputs.json", response->message);
          }
          catch (const std::exception &error) { error_ = error.what(); }
        });
        next_query = Clock::now() + std::chrono::milliseconds(500);
      }
      if (!published_ && std::filesystem::exists(published_path_)) published_ = YAML::LoadFile(published_path_);
      const bool complete = published_ && received_ && same_receipts(received_, published_);
      if (std::filesystem::exists(frontend_path_)) frontend_actual_ = native_timeline(frontend_path_);
      const bool lio_complete = same_timeline(frontend_actual_, frontend_expected_);
      // 计数相等只证明回调已接收；必须再等实际 odometry 尾部和完整时间轴，不能靠固定 sleep。
      if (complete && !actual_.empty() && actual_.back() >= expected_.back() - 1000 &&
          Clock::now() - last_pose_ >= std::chrono::milliseconds(500))
      {
        const bool matched = same_timeline(actual_, expected_) && lio_complete;
        report(matched ? "passed" : "timeline_mismatch", complete);
        if (!matched) throw std::runtime_error("接收已齐全，但 odometry 完整时间轴与 native 不一致");
        if (optimize_on_pass_) request_optimization();
        RCLCPP_INFO(node_->get_logger(), "接收齐全，LIO=%zu、VIO里程计=%zu，独立native时间轴均一致，允许优化/退出",
                    frontend_actual_.size(), actual_.size());
        return;
      }
      rate.sleep();
    }
    const bool complete = published_ && received_ && same_receipts(received_, published_);
    report(complete ? "frontend_not_drained" : "input_receipts_incomplete", complete);
    throw std::runtime_error(complete ? "输入已全部接收，但未达到 native 最后 odometry 时间戳" :
                                      "DDS/输入回调未接收齐全，禁止优化/关闭冒充验收成功");
  }
private:
  using Clock = std::chrono::steady_clock;
  void request_optimization()
  {
    auto client = node_->create_client<std_srvs::srv::Trigger>("/laserMapping/optimize");
    if (!client->wait_for_service(std::chrono::seconds(30)))
      throw std::runtime_error("排空后优化服务未就绪");
    auto response = client->async_send_request(std::make_shared<std_srvs::srv::Trigger::Request>());
    if (executor_.spin_until_future_complete(response, std::chrono::seconds(30)) !=
        rclcpp::FutureReturnCode::SUCCESS)
      throw std::runtime_error("优化服务响应超时");
    const auto result = response.get();
    write_report(directory_ + "/optimize-service.txt",
                 std::string("success=") + (result->success ? "true" : "false") + "\n" + result->message);
    if (!result->success) throw std::runtime_error("优化请求被拒绝: " + result->message);
  }
  void report(const std::string &status, bool complete)
  {
    std::ostringstream result;
    result << "{\"status\":\"" << status << "\",\"receipts_match\":" << (complete ? "true" : "false")
           << ",\"odometry_count\":" << actual_.size() << ",\"expected_count\":" << expected_.size()
           << ",\"last_odometry_ns\":" << (actual_.empty() ? 0 : actual_.back())
           << ",\"expected_last_ns\":" << expected_.back() << ",\"lio_count\":" << frontend_actual_.size()
           << ",\"expected_lio_count\":" << frontend_expected_.size()
           << ",\"last_lio_ns\":" << (frontend_actual_.empty() ? 0 : frontend_actual_.back())
           << ",\"expected_last_lio_ns\":" << frontend_expected_.back()
           << ",\"recorded_capture\":" << (recorded_ ? "true" : "false") << ",\"timestamp_tolerance_ns\":1000}";
    write_report(directory_ + "/drain-result.json", result.str());
    RCLCPP_INFO(node_->get_logger(), "%s", result.str().c_str());
  }
  rclcpp::Node::SharedPtr node_;
  rclcpp::executors::SingleThreadedExecutor executor_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odometry_;
  rclcpp::Client<std_srvs::srv::Trigger>::SharedPtr client_;
  YAML::Node received_{YAML::NodeType::Undefined}, published_{YAML::NodeType::Undefined};
  std::string directory_, published_path_, ready_path_, error_, frontend_path_;
  std::vector<std::int64_t> expected_, actual_, frontend_expected_, frontend_actual_;
  double timeout_ = 180;
  bool pending_ = false;
  bool recorded_ = false;
  bool optimize_on_pass_ = false;
  Clock::time_point last_pose_ = Clock::now();
};
}

int main(int argc, char **argv)
{
  int result = 0;
  try
  {
    rclcpp::init(argc, argv);
    fast_livo_ros2::ReceiptGate gate;
    gate.run();
  }
  catch (const std::exception &error)
  {
    std::cerr << "回放排空验收失败: " << error.what() << '\n';
    result = 1;
  }
  if (rclcpp::ok()) rclcpp::shutdown();
  return result;
}
