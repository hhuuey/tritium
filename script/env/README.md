# 多环境开发入口

统一命令和挂载规则，不把所有工具链安装到同一镜像。
一个 [Dockerfile](Dockerfile) 按 target 构建不同系统环境；项目配置声明实际支持的能力，
环境镜像存在不代表完整算法已经迁移。

## 当前能力

完整 FAST-LIVO2 库化已在本机 ARM64 容器完成 HH 全序列三入口验收，保留默认四线程。
**各环境有独立依赖指纹和基线，构建或局部测试通过不能代替完整验收。**
历史约 13.51 米位置差分的配置仍是未通过样本；没有调算法参数或放宽门槛登记为合格基线。

| 项目 / 环境 | 可执行内容 |
| --- | --- |
| fast-livo2 / ros1-noetic | 完整库、薄节点、RTK、Foxglove；完整旧 bag 与同环境离线轨迹逐时间戳一致 |
| fast-livo2 / cpp-cpu | 完整非 ROS 库、安装消费者和离线 main；默认四线程完整重复轨迹逐字节一致 |
| fast-livo2 / ros2-jazzy | 完整薄节点、接口、中立回放器及 RTK；完整可靠回放与同环境离线轨迹一致 |
| fast-calib2 / ros1-noetic | 原有离线单场景和多场景标定 |
| shell / 任意环境 | 通用开发终端，不伪装成某个算法或训练任务 |

环境为 `ros1-noetic`、`ros2-jazzy`、`cpp-cpu`、`python-cpu`、`python-cuda`。
Noetic 使用 Ubuntu 20.04，Jazzy 使用 Ubuntu 24.04。
Python CPU 与 CUDA 不共用系统 ROS Python，项目安装依赖必须使用虚拟环境和自己的锁文件。
现有 Dagster 继续管理业务依赖和资产发布，不由环境脚本重做调度。

## 统一操作

从仓库根目录执行，也可在其他目录使用绝对脚本路径：

```bash
./script/env/build_image.sh ros1-noetic
./script/env/prepare_deps.sh fast-livo2 ros1-noetic
./script/env/start_docker.sh fast-livo2 ros1-noetic
./script/env/build_deps.sh fast-livo2 ros1-noetic
./script/env/build.sh fast-livo2 ros1-noetic

# 不带包名只启动算法；带包名等待节点就绪后自动播包
./script/env/run.sh fast-livo2 ros1-noetic
./script/env/run.sh fast-livo2 ros1-noetic HH-LVGO-01_compressed.bag -u 12
./script/env/play_bag.sh fast-livo2 ros1-noetic -u 12
./script/env/stop_docker.sh fast-livo2 ros1-noetic
```

容器启动不编译、不运行算法。播包结束后在 `run.sh` 终端按 Enter 触发离线优化；
Ctrl+C 只停止当前算法及本次自动播包，不停止容器。
容器中使用 `/opt/tritium/scripts/build.sh fast-livo2 ros1-noetic` 等同一入口。
`BUILD_JOBS=1` 可在宿主机或容器中限制编译并行度。
支持的项目使用项目级配置，不接受未知项目或以其他环境静默替代。

```bash
# 完整无 ROS 库和离线入口
./script/env/build_image.sh cpp-cpu
./script/env/prepare_deps.sh fast-livo2 cpp-cpu
./script/env/start_docker.sh fast-livo2 cpp-cpu
./script/env/build_deps.sh fast-livo2 cpp-cpu
./script/env/build.sh fast-livo2 cpp-cpu

# ROS 2：构建完整节点和接口，build 会执行 colcon test
./script/env/build_image.sh ros2-jazzy
./script/env/prepare_deps.sh fast-livo2 ros2-jazzy
./script/env/start_docker.sh fast-livo2 ros2-jazzy
./script/env/build_deps.sh fast-livo2 ros2-jazzy
./script/env/build.sh fast-livo2 ros2-jazzy

# Python CPU 开发终端，不自动安装或运行任何业务算法
./script/env/build_image.sh python-cpu
./script/env/start_docker.sh shell python-cpu
./script/env/run.sh shell python-cpu
```

纯 C++ 可执行入口为 `fast_livo_offline`，读取独立 ROS 1 转换器产生的中立数据集，
不启动 ROS 或读取旧 bag。完整用法及格式见项目的 `io/README.md`。
默认不自动开启后端优化，显式传 `--optimize-on-eof` 才在 EOF 请求优化。
`--duration` 和 `--trace` 用于选定数据范围及记录可选阶段诊断，默认不改变数学计算。
新增工具链、依赖版本或硬件平台必须重新验收，不能仅因程序可运行就沿用已验证配置结论。
ROS 2 工作空间只选择 `adapters/ros2`；catkin 仍使用现有 ROS 1 包，
避免同时发现不兼容的包。

旧 ROS1 bag 先通过 `convert_bag.sh fast-livo2 ros1-noetic` 转换，不能直接交给 ROS2。
将中立目录准备到 `.dataset/fast-livo2/hh-neutral` 后，用两个容器终端运行：

```bash
./script/env/run.sh fast-livo2 ros2-jazzy input_reliability:=reliable republish_image:=false
./script/env/play_bag.sh fast-livo2 ros2-jazzy --neutral hh-neutral --rate 1
```

可靠模式是离线回放的显式选择，不改变实时默认 QoS。回放器 EOF 仅表示发布完毕，
不表示定位节点已排空；完整验收使用适配器的 receipt gate 确认输入统计及 LIO/VIO
各自时间轴后，再调用 `/laserMapping/optimize` 并等待完成。原生 ROS2 bag 仍使用
`play_bag.sh fast-livo2 ros2-jazzy <ROS2包路径>`。

## 数据、源码、缓存

| 宿主机 | 容器 | 权限 |
| --- | --- | --- |
| `.deps/` | `/deps` | 可读写 |
| `.dataset/<项目名>` | `/data` | 只读 |
| `.result/<项目名>` | `/results` | 可读写 |
| 项目源码 | `/catkin_ws/src/<包名>` | 可读写 |
| `script/env/` | `/opt/tritium/scripts` | 只读 |

项目 `Log` 和 `output` 继续单独映射到结果目录；算法辅助脚本不迁出项目。
Git 源码依赖按 [锁定清单](deps.lock.json) 和补丁准备，不覆盖已有版本冲突或用户修改。
完整库在各环境均使用 Sophus、GTSAM、纯 CMake 的 vikit_common；
ROS 消息驱动只属于 ROS 1 适配器及旧 bag 转换器，不传入算法库。
更改第三方后先重新构建依赖，再编译项目，最后重启算法；不需因此重建镜像。

FAST-LIVO2 的 cpp/Jazzy 已验证配置仅将 Eigen 固定为源码锁定的 3.3.7，
并一致重编 Sophus、GTSAM 和 vikit；PCL、OpenCV、FLANN、Boost 仍采用各发行版版本。
这是受控重复实验后的兼容约束，不是将整个环境回退到 Noetic。
Eigen 3.4 组合的完整序列重复差异仍未通过验收，不能视为已支持数值配置。
依赖清单变更会启用新的隔离缓存；不要把旧 Eigen 3.4 的库混入新前缀。
构建入口会核查实际核心指纹；完整三入口验收记录见项目 README，
Eigen 3.4 的历史失败组合不在已验证配置内。

Jazzy 容器预留 512 MiB 共享内存，避免大图像和点云受 Docker 默认共享内存容量限制。
这不替代回放完整性、QoS 和处理队列排空检查。

产物卷按项目、环境、架构、镜像和依赖配置隔离。catkin 的 devel、colcon 的 install 和
纯 CMake 缓存绝不互用，VS Code Server 单独持久化。
升级统一入口时会启用新的缓存，旧卷不删除。
原 `script/ros1` 操作入口已迁入本目录，旧容器不会被自动替换，需用户明确停止后再创建。

仍支持 `FAST_LIVO2_DATA_DIR`、`FAST_LIVO2_RESULT_DIR`、`FAST_LIVO2_CONTAINER`、
`FAST_LIVO2_FOXGLOVE_PORT`，标定项目使用相应的 `FAST_CALIB2_*` 设置。
跨环境并行时使用不同端口；`TRITIUM_CONTAINER` 可明确覆盖当前操作的容器名称。
`shell` 的端口可通过 `TRITIUM_FOXGLOVE_PORT` 设置，但环境本身不会自动启动 Foxglove。
启动依赖 VS Code 和 Dev Containers，容器内可把 `/deps` 添加到工作区查看依赖。
`DOCKER_PLATFORM` 可指定镜像构建架构，CUDA 不支持通过模拟架构获得真实 GPU。

## FAST-Calib2

```bash
./script/env/prepare_deps.sh fast-calib2 ros1-noetic
./script/env/start_docker.sh fast-calib2 ros1-noetic
./script/env/build_deps.sh fast-calib2 ros1-noetic
./script/env/build.sh fast-calib2 ros1-noetic
./script/env/run.sh fast-calib2 ros1-noetic single
./script/env/run.sh fast-calib2 ros1-noetic multi
```

单场景默认读取 `/data/avia/mid.bag` 和 `/data/avia/mid.bmp`；
可以附加 `bag_path:=/data/...`、`image_path:=/data/...` 覆盖。
直接读取文件，不使用 `play_bag.sh`，运行入口关闭 RViz。

## 远程 GPU

`python-cuda` 提供 CUDA 12.8.1 开发环境，不提前捆绑互相冲突的 PyTorch/TensorRT。
框架、Python 包和扩展应由实际项目锁定；没有真实模型和输入不提供“成功”占位算法。
在 Linux NVIDIA 主机的本地或 Remote SSH 终端运行相同命令：

```bash
./script/env/build_image.sh python-cuda
./script/env/start_docker.sh shell python-cuda
./script/env/run.sh shell python-cuda
```

远端必须有自己的 checkout、`.deps`、`.dataset`、`.result` 和可用 NVIDIA Container Toolkit。
不自动同步私有数据，不自动安装驱动，不部署常驻服务。Mac 路径不会自动变成远端挂载，
因此启动入口拒绝远程 Docker endpoint；Mac 直接请求 CUDA 也明确失败。
容器启动后检查 `nvidia-smi`，失败停止该容器，不静默切换 CPU。
本机没有 NVIDIA GPU，CUDA 实际运行仍待远端验证。

## 后续算法迁移

完整算法库通过 `find_package(fast_livo_core CONFIG REQUIRED)` 和 `fast_livo::core` 调用，
ROS 1、ROS 2 和独立主程序不再各编译一套算法。完整源代码已抽出并移除旧算法副本；
最终三入口完整 HH、默认四线程重复以及原有模式回归均已完成；
物理传感器、其他数据和新的目标架构仍需实际验证，不沿用本机结论。

数值排查区分输入/同步、图像预处理、点云处理、IMU、LIO、VIO 和后端。
`fast_livo_numerics` 输出实际编译核心时采用的版本和浮点执行特征；
诊断用的 strict-FP 或线程限制实验不会自动成为默认配置或合格基线。
完整序列参考数据和当前报告位于 `.result/fast-livo2/full-migration-baseline/`。

## 前期基础环境验证（不代表完整迁移验收）

- arm64 的 Jazzy、无 ROS C++ 和 Python CPU 镜像实际构建通过。
- ROS 1 完整定位构建及 IMU 转换测试通过；相同 3 秒 bag 的两次旧基线与迁移后结果均为
  26 条位姿，TUM 文件逐字节一致，保存在 `.result/fast-livo2/migration-baseline/`。
- 无 ROS 环境实际编译同一份 IMU 算法，初始化测试通过，未安装或链接 ROS；
  重复构建不重新编译未变化的 C++ 源文件。
- Jazzy IMU 消息适配包的 colcon 构建和字段测试通过。
- 新入口的自动播包、Ctrl+C 清理、环境身份与不支持组合检查通过。
- CUDA 只完成环境声明与 Mac 拒绝路径检查，未验证远端 GPU 或训练。
- 本批未宣称完整无 ROS 定位、完整 ROS 2 定位或长序列 RTK 数值回归已经完成。
