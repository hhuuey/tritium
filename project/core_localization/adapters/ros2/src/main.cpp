#include "fast_livo_ros2/config.h"
#include "fast_livo_ros2/inputs.h"
#include "fast_livo_ros2/outputs.h"
#include "fast_livo_ros2/runtime.h"

int main(int argc, char **argv)
{
  int result = 0;
  try
  {
    rclcpp::init(argc, argv, rclcpp::InitOptions(), rclcpp::SignalHandlerOptions::None);
    fast_livo_ros2::install_signals();
    auto node = std::make_shared<rclcpp::Node>("laserMapping");
    const auto config = fast_livo_ros2::load_config(*node);
    fast_livo_ros2::OutputBridge output(*node);
    fast_livo::Engine engine(config, output.callbacks());
    fast_livo_ros2::FinishGuard finish(engine);
    fast_livo_ros2::EnterTrigger keyboard;
    fast_livo_ros2::Inputs input(*node, engine, config);
    rclcpp::executors::SingleThreadedExecutor executor;
    executor.add_node(node);
    RCLCPP_INFO(node->get_logger(), "FAST-LIVO2 ROS 2：5000Hz 单线程调度，250Hz IMU 传播；Enter 或 ~/optimize 请求后端");
    rclcpp::WallRate rate(5000);
    while (rclcpp::ok() && !fast_livo_ros2::stop_requested)
    {
      executor.spin_some();
      if (!rclcpp::ok() || fast_livo_ros2::stop_requested) break;
      engine.process_one();
      keyboard.poll(engine);
      rate.sleep();
    }
    executor.remove_node(node);
    finish.close();
  }
  catch (const std::exception &error)
  {
    std::cerr << "FAST-LIVO2 ROS 2 失败: " << error.what() << '\n';
    result = 1;
  }
  catch (...)
  {
    std::cerr << "FAST-LIVO2 ROS 2 发生未知异常\n";
    result = 1;
  }
  if (rclcpp::ok()) rclcpp::shutdown();
  return result;
}
