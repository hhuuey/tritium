# FAST-LIVO2 ROS 2 Jazzy 薄适配器

完整定位节点，不是 IMU 原型。算法只通过安装包 `fast_livo_core` 的 `fast_livo::core` 链接，
不编译原算法文件、不引用原算法目录。上游开发者为 Chunran Zheng，
算法授权以 FAST-LIVO2 上游 LICENSE 为准，本包不改变原包 BSD 声明或算法授权。

## CMake 依赖与运行

依赖 ament_cmake、fast_livo_core、rclcpp、builtin_interfaces、std_msgs、sensor_msgs、geometry_msgs、nav_msgs、
visualization_msgs、std_srvs、tf2_ros、cv_bridge、pcl_conversions、fast_livo_ros2_interfaces；
测试另外依赖 ament_cmake_gtest。核心及转换包提供 PCL/OpenCV 公开依赖。
launch 需要 launch_ros、launch_xml、Foxglove、image_transport、compressed_image_transport。
不需要 ROS 2 的 gnss_comm 或 Livox 驱动包。
回放器额外依赖已安装的 `fast_livo_io` 和 `rosgraph_msgs`；
只有 `fast_livo_replayer` 链接 `fast_livo::fast_livo_io`，定位节点仍只链接中立核心。

先安装中立核心；source Jazzy，并把核心前缀加入 `CMAKE_PREFIX_PATH`。
从独立适配器工作空间执行（替换 `<repo>`）：

```bash
colcon build --base-paths \
  <repo>/project/core_localization/adapters/ros2_interfaces \
  <repo>/project/core_localization/adapters/ros2
source install/setup.bash
ros2 launch fast_livo_ros2 HH.launch.xml outputfilepath:=/results/output
ros2 service call /laserMapping/optimize std_srvs/srv/Trigger "{}"
```

默认隔离安装二进制：
`install/fast_livo_ros2/lib/fast_livo_ros2/fastlivo_mapping`；
合并安装为 `<prefix>/lib/fast_livo_ros2/fastlivo_mapping`。节点名 `laserMapping`。
默认结果目录 `/results`，可用 `FAST_LIVO_OUTPUT_DIR` 或 launch 参数覆盖，
实际挂载到仓库外的数据只读目录之外、宿主机 `result/fast-livo2`。
直接终端可运行：

```bash
ros2 run fast_livo_ros2 fastlivo_mapping --ros-args \
  --params-file <share>/fast_livo_ros2/config/HH.yaml \
  -p laserMapping.outputfilepath:=/results/output
```

Enter 直接触发后端；launch 通常不转发 stdin，改用服务。
launch 给 Ctrl+C 后端收尾预留 600 秒；特别大的后端任务应进一步延长 `sigterm_timeout`。

统一入口应使用如下 launch 参数（不是点语法的节点参数名）：

```bash
ros2 launch fast_livo_ros2 HH.launch.xml outputfilepath:=/results/output \
  input_reliability:=reliable input_statistics:=true use_sim_time:=true \
  republish_image:=false foxglove:=true foxglove_port:=8765
```

分别映射到节点 `adapter.input_reliability`、`adapter.input_statistics`、`use_sim_time`；
实时默认仍为 best_effort/false，HH 的 use_sim_time 默认保持 true。

## 参数、时钟、QoS 与调度

- 宏遍历 `config_fields.inc` 调用 `declare_parameter`，默认值取 `Config`。
  原 `common/lid_topic` 变成 `common.lid_topic`；原无斜杠相机键 `cam_fx`
  变成 `laserMapping.cam_fx`；原 `laserMapping/outputfilepath`
  变成 `laserMapping.outputfilepath`。`source_dir` 为可选辅助脚本目录。
  整数和整数数组通过 ROS 2 int64 参数读取，检查范围后转换到核心 int。
- `HH.yaml` 保持当前 HH 算法/相机配置，double 数组显式用浮点数，避免 Jazzy 类型不匹配。
  未在核心字段表中使用的旧 YAML 键不参与算法参数读取。
- `Outputs.now` 为节点 ROS clock。默认 `use_sim_time=true`，回放必须发布 `/clock`；
  实时运行设 `use_sim_time:=false`。250Hz timer 同样使用该 ROS clock，暂停回放时不会伪造传播时间。
- 单线程 `SingleThreadedExecutor::spin_some()` 后恰好调用一次 `process_one()`，
  再按 5000Hz wall rate 等待；false 表示暂无同步帧，不退出也不批量补处理。
  所有传感器、timer、服务回调在同一 executor 执行，不并发调用 Engine。
- 输入默认 SensorDataQoS：best effort、volatile；
  `adapter.sensor_queue_depth=200000`，GNSS 队列 2000。
  `adapter.input_reliability` 仅接受 `best_effort` 或 `reliable`，非法值明确失败。
  best effort 订阅兼容可靠或 best effort bag 发布端；可靠订阅需匹配可靠发布端。
  输出可靠、volatile，队列深度沿用原节点对应话题。
- 验收可显式设置 `adapter.input_reliability=reliable`；
  `adapter.input_statistics=true` 时提供只读 `/laserMapping/input_status` Trigger 服务。
  JSON 按 IMU/PointCloud2/Livox/Image/GNSS 记录实际输入回调计数及末 header 纳秒时间；
  GNSS 无 header，保留 `null` 并另记末 week/tow，不伪造 ROS stamp。统计默认关闭。
- Enter 或 `~/optimize` 只请求异步优化，服务成功不表示优化完成。
  Ctrl+C、回调错误或算法错误均经 RAII 调用 `wait_for_optimization()`、`finish()`。
  SIGINT/SIGTERM 只置标记，核心收尾完成后才关闭 ROS context，避免收尾发布器提前失效。
  失败输出错误并返回 1；不会隐藏后端/GPU失败、不会用 CPU fallback 冒充成功。

## 消息与发布

输入涵盖 PointCloud2、完整 Livox CustomMsg、自带 covariance 的 Imu、bgr8 图像和完整 GNSS PVT。
无 ROS 2 原驱动时使用 `fast_livo_ros2_interfaces/msg/CustomMsg`、
`fast_livo_ros2_interfaces/msg/GnssPVTSolnMsg`；发布者/rosbag 转换必须使用这些**类型名**，
仅改话题名不会转换 ROS 1/其他包的消息类型。接口保留全部原始字段，不用 NavSatFix 替代速度/质量。

点云直接使用 `pcl_conversions::toPCL/fromPCL` 传递 `pcl::PCLPointCloud2`，
保留任意字段、字节、行填充、大小端和布局；避免 typed XYZ 转换丢字段。
核心内部可用 `pcl::toPCLPointCloud2/fromPCLPointCloud2`；适配器单独保存/恢复纳秒 header，
不依赖 PCL header 的微秒精度。图像通过 cv_bridge 转为拥有数据的 bgr8。

输出原话题：`/odometry/fast_livo2`、`/gps/odometry`、`/aft_mapped_to_init`、
`/LIVO2/imu_propagate`、`/mavros/vision_pose/pose`、`/synced_cloud`、`/cloud_registered`、
`/cloud_visual_sub_map_before`、`/cloud_effected`、`/Laser_map`、`/dyn_obj`、`/dyn_obj_removed`、
`/dyn_obj_dbg_hist`、`/rgb_img`、`/path`、`/planes`。时间/frame 直接来自核心，
平面使用 CYLINDER；`Outputs.pose` 的 `child_frame_id` 非空时按样本广播 TF，
核心负责坐标树定义。Foxglove 默认端口 8765。

## 父线程集成验证

1. 在 Jazzy 环境链接真实已安装核心；运行 `colcon test --packages-select fast_livo_ros2`
   与 `colcon test-result --verbose`，覆盖 IMU、全字段点云、PVT、Livox、bgr8 转换。
2. `ros2 launch ... --show-args` 检查 launch；用匹配新接口类型的 HH bag 加 `/clock` 回放。
3. 核对 topic type/QoS、纳秒 stamp、frame、TF、路径、平面、图像和 Foxglove。
4. Enter/服务触发后端后 Ctrl+C，检查等待与真实结果文件；无效参数、输入、后端失败必须非零退出。

## 原生中立数据回放器

`fast_livo_replayer` 直接使用安装包的 `io::Reader/Event`，只读转换好的中立目录，
不启动 ROS 1、不调用 rosbag 或离线 Engine。二进制与定位节点同目录。

```bash
# 终端一：真实定位节点，接收回放器直接发布的原始 bgr8 图像。
ros2 launch fast_livo_ros2 HH.launch.xml \
  outputfilepath:=/results/hh-ros2 republish_image:=false use_sim_time:=true
# 终端二：中立目录必须由真实 bag 转换而来。
ros2 run fast_livo_ros2 fast_livo_replayer --dataset /results/hh-neutral --rate 1.0
# 仅发第一个记录时间起的 12 秒闭区间，不改算法参数，不把截取说成 EOF：
ros2 run fast_livo_ros2 fast_livo_replayer --dataset /results/hh-neutral --rate 1.0 --duration 12
```

完整 CLI：`--dataset DIRECTORY` 必需；`--rate` 默认 1；
`--duration` 可选且必须为正秒数，限制记录时间窗口，省略即完整 EOF；
`--wait-timeout` 默认 60 秒且必须为正；`--drain-time` 默认 1 秒且允许 0；
`--stats-file NEW_JSON` 可选且禁止覆盖；`--help/-h`。所有数字必须有限。
发布端使用 reliable/volatile，无需也不接受额外“发布可靠性”参数。
ROS 参数 `imu_topic`、`cloud_topic`、`livox_topic`、`image_topic`、`gnss_topic`
通过标准 `--ros-args -p name:=value` 覆盖。中立回放需定位节点使用 sim time，
否则缺少 `/clock` 订阅会明确等待超时；不能把它当作订阅已经就绪。

Foxglove 当前 3.6.0 的日志就绪信号为 `Server listening on port ...`。
强验证应使用子协议 `foxglove.sdk.v1` 完成 websocket `101 Switching Protocols`，
并收到 `{"op":"serverInfo",...}`，不能只检查 TCP 端口；
旧 `foxglove.websocket.v1` 在此版本会返回 400。

- 发布顺序严格沿用 Reader 的 `rosbag::View` 事件顺序，不排序 header 时间或物理序号。
  相邻记录时间差按 `--rate` 缩放到 steady clock；相同记录时间不插入人工延迟，
  倒退记录仍原序发布并重置 wall 时基。进度包含 View 序号和记录时间。
- 每个事件发布前先发布 `/clock=event.record_time`，传感器 header 保留独立的原纳秒时间，
  PVT 仍无 header。ROS 2 标准 Header 本身没有 ROS 1 的 `seq`；
  View 序号不冒充 header，也不被重新排序。跨话题 DDS 消费次序由接收端调度决定，
  发布端原序不等于已证明逐帧数值等价，仍需真实回放回归。
- `--duration SECONDS` 只限制传感器输入记录时间窗口；未指定时仍回放至 EOF。
  到达窗口边界后打印“输入窗口完成”并退出 0，不改变 Engine 或后端算法默认值。
- IMU 保留姿态及三组协方差；PointCloud2 保留全部字段/字节/组织布局/大小端；
  Livox 保留时间基、offset、tag、line、反射率、保留字节；PVT 保留全位置/速度/质量字段；
  图像发布为 `sensor_msgs/Image` 的 bgr8，不再重复解码压缩流。
- 按样本类型映射到 HH 默认话题，而不是直接照搬可能为 compressed 的 `Event.topic`。
  参数 `imu_topic`、`cloud_topic`、`livox_topic`、`image_topic`、`gnss_topic`
  可用 `--ros-args -p ...` 覆盖，也支持标准 ROS remap。
  例如 `--ros-args -p image_topic:=/camera/image`。
  PointCloud2 与 Livox 默认均为 `/livox/lidar`；混合两种类型的数据集必须显式指定不同目标话题，
  单个定位节点只消费其配置选择的一种雷达类型。
- 只创建数据集中实际存在的五种输入发布器，使用 reliable/volatile；
  与定位节点默认 best effort 订阅兼容。启动时等所有存在类型及 `/clock`
  至少匹配一个订阅者，期间不发布第一条事件；`--wait-timeout` 默认 60 秒，
  超时列明缺少的话题并非零退出，而不是假装回放完成。
- EOF 先等待 reliable DDS 确认（最多 5 秒），再留 `--drain-time` 默认 1 秒给接收端处理，
  明确打印 EOF、已发布事件数并退出 0。空数据集、Reader 校验失败、无效速率、
  未就绪或 DDS 确认超时均退出 1。SIGINT/SIGTERM 明确报告中断而不是 EOF。
  DDS 确认不代表 Engine 已消费/优化全部数据；接收端为 best effort 时没有逐条确认保证。
  EOF 不会关闭定位节点或偷偷触发后端：验收脚本需另行调用 `/laserMapping/optimize`，
  随后向定位节点发送 SIGINT，等待真实结果落盘。
- `--stats-file NEW_JSON` 原子发布五类发布计数、末 header 时间及 GNSS week/tow，
  拒绝覆盖已有文件。它只证明发布调用数，不等于 Engine 已消费。
  严格验收须对齐接收统计，并等真实 odometry 达到 native 最后时间戳和完整时间轴；
  不能用固定 `--drain-time` 作为已排空的依据。参见 `validation/receipt_gate.cpp`。
