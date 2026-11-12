#include "fast_livo_ros1/config.h"
#include "fast_livo_ros1/inputs.h"
#include "fast_livo_ros1/outputs.h"
#include "fast_livo_ros1/runtime.h"

int main(int argc, char **argv)
{
  int result = 0;
  try
  {
    ros::init(argc, argv, "laserMapping", ros::init_options::NoSigintHandler);
    fast_livo_ros1::install_signals();
    ros::NodeHandle node;
    const auto config = fast_livo_ros1::load_config(node);
    fast_livo_ros1::OutputBridge output(node);
    fast_livo::Engine engine(config, output.callbacks());
    fast_livo_ros1::FinishGuard finish(engine);
    fast_livo_ros1::EnterTrigger keyboard;
    fast_livo_ros1::Inputs input(node, engine, config);
    ROS_INFO("FAST-LIVO2 ROS 1：5000Hz 单线程调度，250Hz IMU 传播；Enter 或 ~optimize 请求后端");
    ros::WallRate rate(5000);
    while (ros::ok() && !fast_livo_ros1::stop_requested)
    {
      ros::spinOnce();
      if (!ros::ok() || fast_livo_ros1::stop_requested) break;
      engine.process_one();
      keyboard.poll(engine);
      rate.sleep();
    }
    finish.close();
  }
  catch (const std::exception &error)
  {
    ROS_FATAL_STREAM("FAST-LIVO2 ROS 1 失败: " << error.what());
    result = 1;
  }
  catch (...)
  {
    ROS_FATAL("FAST-LIVO2 ROS 1 发生未知异常");
    result = 1;
  }
  ros::shutdown();
  return result;
}
