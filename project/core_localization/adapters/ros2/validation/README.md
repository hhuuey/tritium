# Eigen 3.3 候选组合的 Jazzy 实测

此 helper 仅使用已安装的候选 core/io；不编译算法，不改变默认环境或中立数据。
基础组合保持 GCC 13、PCL 1.14、OpenCV 4.6、FLANN 1.9.2、Boost 1.83，
仅使用 Eigen 3.3 及其一致性依赖。这里只做 ROS 2 接线与短窗口验收，
不能据此宣称全序列数值等价。

## 创建隔离容器

在仓库根执行。首次创建前确认同名容器不属于其他任务；不要删除或复用用户的 `fast-livo2-rtk`。

```bash
output="$PWD/.result/fast-livo2/full-migration-baseline"
mkdir -p "$output/ros2-eigen33-check"
docker run --detach --init --name full-livo-jazzy-eigen33-check --shm-size 512m \
  --volumes-from full-livo-cpp-check:ro \
  -v full-livo-eigen33-deps-install:/opt/tritium/eigen33-check/install:ro \
  -v "$output/hh-neutral-v2:/neutral:ro" \
  -v "$output/ros2-eigen33-check:/results" \
  -v full-livo-jazzy-eigen33-build:/jbuild \
  -v full-livo-jazzy-eigen33-install:/jinstall \
  -e ROS_DOMAIN_ID=73 -e ROS_HOME=/results/ros \
  -p 127.0.0.1:18775:8765 --entrypoint bash tritium-env:ros2-jazzy \
  -c 'exec sleep infinity'
```

继承的源码、核心、IO 均只读。候选核心路径：
`/catkin_ws/install/eigen33/core`、`/catkin_ws/install/eigen33/io`。
依赖卷必须挂到上述绝对路径，否则安装导出的 CMake 目标和运行库路径不成立。

## 串行构建与测试

```bash
docker exec full-livo-jazzy-eigen33-check bash \
  /catkin_ws/src/fast_livo/adapters/ros2/validation/eigen33.sh build \
  > "$output/ros2-eigen33-check/build.log" 2>&1
```

helper 精确设置：

```text
CMAKE_PREFIX_PATH=/catkin_ws/install/eigen33/core:/catkin_ws/install/eigen33/io:/opt/tritium/eigen33-check/install:/opt/ros/jazzy
Eigen3_DIR=/opt/tritium/eigen33-check/install/share/eigen3/cmake
CMAKE_BUILD_PARALLEL_LEVEL=1
MAKEFLAGS=-j1 -l1
ROS_DOMAIN_ID=73
```

只对 `adapters/ros2`、`adapters/ros2_interfaces` 执行 colcon build/test，
使用 `--executor sequential`，没有算法 cpp 或依赖重建。
`/results/candidate-linkage.txt` 记录编译器、Eigen/PCL/OpenCV 版本、CMake 缓存和两个二进制的实际动态库。
`/jbuild/fast_livo_ros2/compile_commands.json` 可核对真正生效的候选 Eigen include 路径。
同时复制到宿主结果目录的 `compile_commands.json`；helper 检查算法源文件编译单元为零。

## 12 秒真实回放、服务和退出

```bash
docker exec full-livo-jazzy-eigen33-check bash \
  /catkin_ws/src/fast_livo/adapters/ros2/validation/eigen33.sh run 12 /results/run-12s \
  > "$output/ros2-eigen33-check/run-12s.log" 2>&1
```

helper 拒绝覆盖已有结果目录。重复验收须传新目录，例如 `/results/run-12s-repeat`。
默认 `REPLAY_RATE=1`；可显式通过 `docker exec -e REPLAY_RATE=0.5 ...` 降速。
`--duration 12` 只选择首个 record_time 起的 12 秒闭区间，不改 Engine、配置或优化算法。
验收前将 native 参考轨迹只读复制到候选结果根目录：

```bash
cp "$output/resume-eigen33-kernel-12/Log/result/Grass01.txt" \
  "$output/ros2-eigen33-check/native-12s.txt"
```

也可通过 `EXPECTED_TRAJECTORY` 指定容器内其他只读参考路径。

真实进程包括：

- `/jinstall/fast_livo_ros2/lib/fast_livo_ros2/fastlivo_mapping`，
  使用安装后的 HH YAML、`use_sim_time=true` 和独立结果目录；
  仅验收时显式启用 `adapter.input_reliability=reliable` 与 `adapter.input_statistics=true`。
- 同安装前缀的 `fast_livo_replayer --dataset /neutral --rate 1 --duration 12`，
  等待实际订阅就绪后发布五类输入及 `/clock`，记录“输入窗口完成”，不伪称完整 EOF。
- Foxglove bridge，监听容器 8765 / 宿主 18775；
  实际完成 websocket 升级并检查 `serverInfo`，不只检查端口。
  当前 bridge 3.6 使用 `foxglove.sdk.v1` 子协议；旧版可显式设置
  `FOXGLOVE_PROTOCOL=foxglove.websocket.v1`，不把握手失败当作成功。
- C++ `fast_livo_receipt_gate` 持续记录全部 `/odometry/fast_livo2` 时间戳，
  先确认订阅和统计服务就绪，才允许回放开始；TF CLI 探针使用 `--no-daemon`。

回放明确写出 `published-inputs.json`，不再以 sleep 两秒视为排空。
gate 对齐五类实际接收计数、末 header stamp、GNSS week/tow，
并等待 native 最后 odometry 时间戳以及整个时间轴一致，再允许调用优化服务。
native 文本仅有六位小数，时间比较容差 1000ns 仅用于文本舍入，不是算法阈值。
它记录 `received-inputs.json`、`odometry-stamps.txt`、`drain-result.json`：
`input_receipts_incomplete` 区分输入回调未齐，
`frontend_not_drained` 区分输入已齐但前端未到末帧，
`timeline_mismatch` 表示输入已齐但前端时间轴不同；任何一种都禁止冒充验收成功。

通过 gate 后实际调用 `/laserMapping/optimize`，检查 Trigger 成功响应；
给定位节点发送 SIGINT，等待其真实优化与 finish 成功退出，再关闭 Foxglove。
检查前端轨迹、后端轨迹和优化全局点云非空，
再用 `cmp` 严格对齐落盘前后端的全部时间列与 native 文本时间列；
只有全部成功才生成 `summary.txt`。
默认 gate 最多等待 180 秒，可用整数秒 `DRAIN_TIMEOUT` 调整；收尾默认 300 秒，
可显式设置 `FINISH_TIMEOUT`。实时默认 QoS、主循环和算法参数均不改变。
所有 PID 写入当前运行的 `owned-pids.txt`，异常 trap 只处理本 helper 启动的明确 PID。

## 输出与后续全序列

宿主输出均位于 `.result/fast-livo2/full-migration-baseline/ros2-eigen33-check`：

- `build.log`、`colcon/`、`candidate-linkage.txt`；
- 每次运行自己的 `node.log`、`replay.log`、`optimize-service.txt`；
- `odometry.yaml`、`tf.yaml`、`foxglove-handshake.txt`、`summary.txt`；
- `Log/result/Grass01.txt`、`TUM/opt_trajectory_after.txt`、`global_pcd/after_optimization.pcd`。

本任务不要运行全 HH。父线程明确准备好后，可复用同一候选容器、二进制和 helper：

```bash
docker exec -e EXPECTED_TRAJECTORY=/results/native-full.txt -e DRAIN_TIMEOUT=1800 \
  -e EXPECTED_BACKEND_TRAJECTORY=/results/native-full-backend.txt \
  full-livo-jazzy-eigen33-check bash \
  /catkin_ws/src/fast_livo/adapters/ros2/validation/eigen33.sh run full /results/run-full \
  > "$output/ros2-eigen33-check/run-full.log" 2>&1
```

`full` 仅去掉输入窗口限制，不更换库或算法参数。容器保持空闲以便复用；
helper 退出时不留下定位、回放或 Foxglove 有界进程。

候选实测修正了 HH YAML 中 `local_map.sliding_thresh` 的 ROS 2 类型：
原数字 8 明确写成 double `8.0`，数值和算法含义不变。

## 本次实际结果（2026-11-14）

最终构建输出：

```text
Summary: 2 packages finished
Summary: 8 tests, 0 errors, 0 failures, 0 skipped
algorithm_source_units=0
```

实际执行的短窗口命令如下，已有结果不得覆盖：

```bash
docker exec full-livo-jazzy-eigen33-check bash \
  /catkin_ws/src/fast_livo/adapters/ros2/validation/eigen33.sh run 12 /results/run-12s-verified
docker exec full-livo-jazzy-eigen33-check bash \
  /catkin_ws/src/fast_livo/adapters/ros2/validation/eigen33.sh run 12 /results/run-12s-repeat
```

| 结果目录 | 发布事件 | 前端位姿 | 后端位姿（不含表头） | node/replayer/bridge 退出码 |
|---|---:|---:|---:|---|
| `run-12s-verified` | 2786 | 106 | 106 | 0 / 0 / 0 |
| `run-12s-repeat` | 2786 | 108 | 108 | 0 / 0 / 0 |

2786 与只读中立索引的 12 秒闭区间计数相符；首记录纳秒 `1763437339879452152`，
最后选中记录为 View 序号 `2904`、纳秒 `1763437351812791779`。
两次均得到真实优化服务成功响应、`camera_init → aft_mapped` TF、前后端轨迹和非空优化地图。
宿主 `127.0.0.1:18775` 也实测返回 `101 Switching Protocols`、
`foxglove.sdk.v1` 和 `serverInfo`，证据为结果根目录的 `host-18775-handshake.txt`。

**历史结果限制：两次位姿数不一致，不能作为数值基线。** 上表是修复可靠接收/排空 gate 之前的记录，
只证明候选组合能构建并完成功能流程，
不证明 ROS 2 调度/传输下的数值重复性；没有运行全 HH，也没有复算或确认全序列 RMSE。
不要把父线程提供的离线全序列指标当成本 helper 的验收结论。

保留了首次失败目录 `run-12s` 及初次构建日志。
上表和 `build.log` 只记录历史功能测试；当前完整性验收以末节的可靠输入两轮结果为准，
对应最终构建日志为 `build-reliable-drain-final.log`（10 tests，0 errors/failures）。
结束后容器只剩 init/空闲 sleep；定位、回放、Foxglove、CLI daemon 均未遗留运行。

## 可靠输入与真正排空后的结果

以 `resume-eigen33-kernel-12/Log/result/Grass01.txt` 的 116 行及末行
`1763437351.699426` 为参考。只修验收端 QoS/统计/排空控制，
没有补帧、调用额外 `process_one()`、调整阈值或改变算法。

```bash
docker exec full-livo-jazzy-eigen33-check bash \
  /catkin_ws/src/fast_livo/adapters/ros2/validation/eigen33.sh build
docker exec full-livo-jazzy-eigen33-check bash \
  /catkin_ws/src/fast_livo/adapters/ros2/validation/eigen33.sh run 12 /results/run-reliable-drain-12s-1
docker exec full-livo-jazzy-eigen33-check bash \
  /catkin_ws/src/fast_livo/adapters/ros2/validation/eigen33.sh run 12 /results/run-reliable-drain-12s-2
```

两轮均为：

| 类别 | 发布 | 实际输入回调接收 | 末 header 纳秒 |
|---|---:|---:|---:|
| IMU | 2427 | 2427 | 1763437351795818090 |
| PointCloud2 | 0 | 0 | 无 |
| Livox | 120 | 120 | 1763437351699668884 |
| bgr8 Image | 120 | 120 | 1763437351599426031 |
| GNSS PVT | 119 | 119 | 原消息无 header |

GNSS 末时间同为 week=2393、tow=186169.7，未丢失速度或解质量字段。
两轮 `drain-result.json` 均为 `passed`、`receipts_match=true`，
odometry/前端轨迹/后端位姿均为 **116**。
两个 `odometry-stamps.txt` 的全部原始纳秒值逐项完全相同；
末值 `1763437351699425936`，相对 native 六位小数显示值差 64ns，
整条时间轴最大显示舍入差 321ns。落盘前、后端全部时间列均与 native 文本精确相同。
优化服务、TF、Foxglove 和所有进程退出码 0 同时通过。

证据分别保留在两轮目录的 `published-inputs.json`、`received-inputs.json`、
`drain-result.json`、`odometry-stamps.txt`、`native-12s-comparison.json`、`summary.txt`。
附加只读位姿比较得到两轮前端相对该 native 12 秒轨迹的 RMSE 约 0.000319m / 0.000437m；
这不是全 HH 指标，也不宣称位姿数值逐位相同。

当前证据说明可靠模式没有输入缺失，且确实等到了前端末帧才优化/退出。
历史 106/108 两轮没有接收统计，不能事后把漏帧单独归因于 DDS 或关停时机；
不能再使用那些结果作为完整性或数值基线。
最终测试还覆盖“末时间相同但中间缺帧/重排必须失败”和“缺输入/GNSS 末时间不同必须失败”，
避免 gate 仅凭计数或最后一帧冒充完整时间轴通过。

## 完整 HH：重建原受控 core，而非复制正式 cpp 产物

公共 Backend 的 NaN/Inf 验证更新后，必须先在原受控 SDK 下重建。不要复制
`full-livo-cpp-compatible` 的 `core.a`；其导出的 Sophus 绝对安装前缀不同。
先核对 CPP/Jazzy 两端 `/opt/tritium/eigen33-check/install` 的 Eigen、Sophus、
vikit、GTSAM 文件 SHA256 相同，两端核对结果保存在 `full-verification/sdk-*.sha256`。

```bash
docker exec full-livo-cpp-check bash \
  /catkin_ws/src/fast_livo/adapters/ros2/validation/rebuild_eigen33_core.sh
docker exec full-livo-jazzy-eigen33-check bash \
  /catkin_ws/src/fast_livo/adapters/ros2/validation/eigen33.sh build
```

重建保持原 `/catkin_ws/build/eigen33-core` 与 `/catkin_ws/install/eigen33/core`，
显式指定 Eigen3_DIR/EIGEN3_INCLUDE_DIR、Sophus_DIR、GTSAM_DIR、vikit_common_DIR，
`FAST_LIVO_FP_MODEL=legacy`、strict C++17、单线程构建/测试。
只有验证边界文件按 core 自身 CMake 指定 `-fno-fast-math`，
不更改求解公式或其他算法编译选项。helper 保存并检查 `validation.cpp` 的真实编译命令。

完整序列前端/后端参考不能混用：本次前端为 2738 位姿，后端为 2737 关键帧。
在宿主复制同环境 native 文本参考，不改原文件：

```bash
reference="$output/eigen33-native-full"
cp "$reference/Log/result/Grass01.txt" "$output/ros2-eigen33-check/native-full-frontend.txt"
cp "$reference/TUM/opt_trajectory_after.txt" "$output/ros2-eigen33-check/native-full-backend.txt"
docker exec -e EXPECTED_TRAJECTORY=/results/native-full-frontend.txt \
  -e EXPECTED_BACKEND_TRAJECTORY=/results/native-full-backend.txt \
  -e DRAIN_TIMEOUT=1800 -e FINISH_TIMEOUT=900 full-livo-jazzy-eigen33-check bash \
  /catkin_ws/src/fast_livo/adapters/ros2/validation/eigen33.sh run full /results/run-full-reliable
```

完整 HH 只运行一份，不与其他全场景并行。前端 gate 先验证全部必需事件、
2738 个时间戳及最后时间，再显式优化、关闭并检查 2737 个后端时间戳。
`fast_livo_compare` 输出 `frontend-comparison.json`、`backend-comparison.json`：
逐时间戳直接比较位置 RMSE/最大误差及四元数符号等价的角误差，
不插值、补帧或拟合刚体变换，也没有放宽算法阈值。
`map-completeness.json` 检查真实二进制 PCD 头、点数和全部载荷长度，拒绝截断地图。

## 修复图像采样窗口之前的完整 HH 实测（历史）

`run-full-complete` 已完成真实全序列及显式后端优化：

| 项目 | 实际结果 |
|---|---:|
| 发布 / 实际输入回调接收 | 63,720 / 63,720，五类计数与末时间逐项相同 |
| LIO 轨迹 | 2,738，完整时间轴一致 |
| VIO 里程计 / 后端关键帧 | 2,737 / 2,737，完整时间轴一致 |
| 相对同环境 native 前端位置 RMSE / 最大差异 | 0.086843 / 0.316365 米 |
| 相对同环境 native 后端位置 RMSE / 最大差异 | 0.003395 / 0.054432 米 |
| 优化地图点数 | 34,022,700 |
| 优化地图大小 | 544,363,388 字节，二进制载荷完整 |
| node / replayer / Foxglove 退出码 | 0 / 0 / 0 |

没有补帧、插值或额外拟合坐标变换；以上为差分指标，不是传感器绝对精度。
证据为该目录的 `summary.txt`、`drain-result.json`、`frontend-comparison.json`、
`backend-comparison.json`、`map-completeness.json` 和真实日志。

此前 `run-full-reliable`、`run-full-final` 保留为未完成后端的诊断记录，不得登记为成功。
前一个安装 gate 仍按 LIO 的 2,738 行等待 VIO 里程计，后者确认了旧安装二进制没有随源码更新。
修正 gate 后必须重新构建、安装，不能仅修改脚本。最终 gate 同时验证两条独立时间轴，
并在校验通过后通过已匹配的 C++ 客户端调用真实 `/laserMapping/optimize`；
`request_optimization` 参数默认关闭，只有验收 helper 显式开启，不改变算法或实时节点默认行为。

## 图像窗口修复后的最终结果

最终使用正式安装前缀 `/catkin_ws/install/core`、`/catkin_ws/install/io`、
`/opt/tritium/deps-install`，Jazzy 直接消费同一现代 core 二进制，不复制算法。
`FAST_LIVO_VALIDATION_CORE`、`FAST_LIVO_VALIDATION_IO`、`FAST_LIVO_VALIDATION_DEPS`
可显式指定这些前缀；原受控 Eigen33 验证前缀仍是 helper 默认值。

最终证据在 `.result/fast-livo2/full-migration-baseline/final-window-checked`：
ROS2 发布和实际输入回调均为63,720条，LIO2738/VIO2737两条时间轴完整；
前后端轨迹与同环境离线入口的位置、旋转差分均为零，实际优化服务、
TF、Foxglove及进程退出通过。Noetic/现代默认四线程完整重复也均为零，
未采用单线程或额外拟合/补帧替代。
`final-comparison.json` 汇总实际依赖指纹、旧冻结界限和差分；
各阶段的 `.exit` 为容器内最终脚本真实退出状态。
