# FAST-LIVO2 ROS 1 薄适配器

仅包含 ROS 消息/参数/调度边界，不编译原算法 `.cpp`，也不引用原 `src/`、`include/`。
算法来自已安装的 `find_package(fast_livo_core CONFIG REQUIRED)` 与 `fast_livo::core`。
`package.xml` 只声明适配器 ROS 依赖；纯 CMake 核心通过安装前缀发现，不作为 catkin 包声明。
上游开发者为 Chunran Zheng，算法授权以 FAST-LIVO2 上游 LICENSE 为准；
本包保留原包的 BSD 声明，不改变算法授权。

## 构建与入口

CMake 依赖：catkin、fast_livo_core、roscpp、std_msgs、sensor_msgs、geometry_msgs、nav_msgs、
visualization_msgs、std_srvs、tf2_ros、cv_bridge、pcl_conversions、image_transport、
gnss_comm、livox_ros_driver。PCL/OpenCV 头文件与库由核心/转换包公开依赖提供。
`gnss_comm`、`livox_ros_driver` 必须来自真实消息 underlay，不能使用原算法目录中的消息头副本。
Foxglove 与 compressed_image_transport 是 launch 运行依赖，不参与算法编译。

先 source ROS/消息 underlay，再把核心安装前缀加入 `CMAKE_PREFIX_PATH`。
支持两种布局：

- **完整项目挂载** `/catkin_ws/src/fast_livo`：根 `project(fast_livo)` 后直接
  `add_subdirectory(adapters/ros1)`；适配器继承包名 `fast_livo`，
  根目录执行一次 `catkin_package()`，子目录不重复调用。
  根目录负责安装自己的 `launch/`、`config/`，继续使用原 `roslaunch fast_livo HH.launch`。
  二进制为 devel/install 的 `lib/fast_livo/fastlivo_mapping`。
- **独立适配器包**：只将此目录作为 catkin 包加入工作空间；
  不要对包含旧根 package.xml 的整个源码树递归建包。

默认二进制路径：catkin devel 的 `lib/fast_livo_ros1/fastlivo_mapping`；
安装后为 `<prefix>/lib/fast_livo_ros1/fastlivo_mapping`。节点名始终为 `laserMapping`。

```bash
roslaunch --sigint-timeout 600 --sigterm-timeout 30 fast_livo_ros1 HH_ros1.launch \
  outputfilepath:=/results/output
rosservice call /laserMapping/optimize "{}"
```

输出目录必须可写且位于源码/只读数据目录之外。统一入口使用 `/results`，
对应宿主机 `.result/fast-livo2`；可用 `FAST_LIVO_OUTPUT_DIR` 或 launch 参数替换。
需要更长后端优化时增大 roslaunch 的信号等待时间。
直接 `rosrun fast_livo_ros1 fastlivo_mapping` 时必须事先加载 HH 全局参数、
`/laserMapping` 下的相机参数和输出目录；直接终端运行可用 Enter 触发后端。
roslaunch 可能不转发 stdin，此时调用服务，不要误认为 Enter 已被节点接收。

## 参数与实际调度

`config_fields.inc` 每个字段均调用 `nh.param`，默认值直接取 `Config`，不另建默认表。
相机无斜杠参数加载到 `laserMapping`；其他键名及层级不变，
包括 `laserMapping/outputfilepath`。可选 `laserMapping/source_dir` 仅用于核心辅助脚本。

循环严格执行一次 `ros::spinOnce()` → 一次 `Engine::process_one()` → `ros::WallRate(5000)`；
`process_one()` 返回 false 仅表示暂无可处理同步帧，不退出、不补跑多帧。
250Hz ROS 时间 timer 调用 `propagate_imu()`，所有 ROS 回调均在该单线程执行。
`Outputs.now` 使用 `ros::Time::now()`；回放使用外部 `/use_sim_time` 与 `/clock`。
Enter 和 `~optimize` 服务仅调用 `request_optimization()`；服务成功表示请求已入队，
不是优化已完成。Ctrl+C 或输入/输出异常时等待 `wait_for_optimization()` 并调用 `finish()`，
后端异常/落盘失败打印错误且退出码为 1，不降级为占位成功。
SIGINT/SIGTERM 只置退出标记，等核心收尾后才关闭 ROS；wall rate 确保 `/clock` 暂停时也能退出。

## 输入与输出

- `common/lid_topic`：`lidar_type=1` 为 Livox `CustomMsg`，其他类型为 `PointCloud2`。
- `common/imu_topic`：`sensor_msgs/Imu`；`common/img_topic`：`sensor_msgs/Image`，转换成拥有数据的 bgr8。
- `gps/gps_topic`：`gnss_comm/GnssPVTSolnMsg`，完整保留周秒、速度、精度与解状态，不转 NavSatFix。
- 点云以 `pcl_conversions::toPCL/fromPCL` 在 ROS `PointCloud2` 与 `pcl::PCLPointCloud2`
  之间复制字段、布局和字节，不经过丢字段的 typed `pcl::PointCloud<PointXYZ>` 转换。
  核心内部可继续使用 `pcl::toPCLPointCloud2/fromPCLPointCloud2`；
  适配器单独保存/恢复纳秒 header，不能依赖 PCL header 的微秒时间。
- 原话题包括 `/odometry/fast_livo2`、`/gps/odometry`、`/aft_mapped_to_init`、
  `/LIVO2/imu_propagate`、`/mavros/vision_pose/pose`、`/synced_cloud`、
  `/cloud_registered`、`/cloud_visual_sub_map_before`、`/cloud_effected`、`/Laser_map`、
  `/dyn_obj`、`/dyn_obj_removed`、`/dyn_obj_dbg_hist`、`/rgb_img`、`/path`、`/planes`。
  输出 frame 和时间直接使用核心样本；平面为原有 CYLINDER marker。
  `Outputs.pose` 的 `child_frame_id` 非空时按样本原样广播 TF，核心负责坐标树定义。
- launch 保留原 HH 参数/相机数值、压缩图像 republish、`ws://localhost:8765` Foxglove。

## 集成验证

核心安装完成后，父线程需验证真实 catkin 配置/链接与上述二进制路径；
用只读 HH bag 回放核对订阅类型、所有实际发布话题、frame、纳秒时间、TF 与 Foxglove。
分别以 Enter、服务和 Ctrl+C 覆盖后端请求、等待、输出文件落盘；
无效配置/图像编码/失败后端须非零退出。不要用 fake core 或 CPU fallback 作为成功证据。

启用 catkin 测试时构建 `backend_pairing_ros1_test`，将 60,000 条固定种子事件的中立配对
逐事件与真实 `ApproximateTime(10)` 对照，同时核对旧 tf 四元数转换和 ROS Duration 纳秒量化。
测试只在适配器侧依赖 `message_filters` 和 `tf`，不会传入核心安装包。
Noetic 的 CMake 3.16 不支持 `ctest --test-dir`，应进入构建目录运行：

```bash
cd /catkin_ws/build
ctest --output-on-failure -R '^backend_pairing_ros1$'
```
