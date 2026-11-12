# FAST-LIVO2 中立数据集 I/O（版本 2）

本模块延续仓库 GPL-2.0 授权。Reader/Writer 不依赖 ROS；ROS1 仅存在于
`apps/bag_converter`。ROS2 适配器可以直接读取相同的 `io::Event`，再转换成 ROS2 消息或直接调用 Engine。
这不是 rosbag 的通用归档替代品：只转换选定的算法输入，图像保存为解码后的 BGR8 像素。

## 数据流与运行

```text
只读 ROS1 bag → bag_converter → manifest.yaml + index.bin + payload.bin
只读中立目录 → Reader → 原序 Event → Engine.push_* → process_one → 算法结果目录
```

转换器默认选择 HH 的：

| 角色 | 默认话题 | 支持消息 |
|---|---|---|
| IMU | `/livox/imu` | `sensor_msgs/Imu` |
| 雷达 | `/livox/lidar` | `livox_ros_driver/CustomMsg`、`sensor_msgs/PointCloud2` |
| 图像 | `/left_camera/image`、`/left_camera/image/compressed` | `sensor_msgs/Image`、`sensor_msgs/CompressedImage` |
| GNSS | `/ublox_driver/receiver_pvt` | `gnss_comm/GnssPVTSolnMsg` |

HH 压缩 bag 的图像话题无需预先解压或重新录包。若同时录制原始和压缩图像，默认两者都会保留；
要避免算法重复输入，显式选择一个图像话题。`receiver_lla` 的 `NavSatFix` 是旁路信息，不是 Engine 的
GNSS PVT 输入，默认不转换；不得用它冒充完整 PVT。未选定的话题不输出事件，原 bag 序号仍计数。
选定话题的任何不支持类型、MD5 不匹配、空解码图像、损坏消息均报错，不产生“成功占位”。

```sh
# 在已构建 ROS1 underlay 的容器中运行；无需 roscore。
fast_livo_bag_converter \
  --bag /data/HH-LVGO-01_compressed.bag \
  --output /results/hh-neutral

# 话题参数接收逗号分隔列表，并替换该角色的默认列表。
fast_livo_bag_converter --bag INPUT.bag --output NEW_DIRECTORY \
  --imu-topic /imu --lidar-topic /points,/livox/custom \
  --image-topic /camera/image/compressed --gnss-topic /gnss/pvt

# 默认要求 IMU、雷达、图像、GNSS 都有实际消息；纯 LIO 数据可以显式改变要求。
fast_livo_bag_converter --bag LIO.bag --output NEW_DIRECTORY --require imu,lidar

# 此入口仅链接同一个 Engine 库，无 ROS 命令、链接库、节点或运行时。
fast_livo_offline --config config/HH-LVGO.yaml \
  --camera config/camera_HH-LVGO-01.yaml \
  --dataset /results/hh-neutral --output /results/hh-offline \
  --optimize-on-eof
```

所有输出位置必须由 `--output` 指定。Writer 拒绝覆盖非空目录；离线入口拒绝把结果写入输入数据集或其子目录。
Reader 只读。转换完成才原子重命名发布 `manifest.yaml`；中断或转换失败保留无 manifest 的不完整目录，
不会误认为完整数据集，下次运行应使用新的输出目录。它不提供断电级事务保证。

### 必须保持的调度契约

1. 转换器默认完全沿用 `rosbag::View` 原生迭代顺序，即旧 `rosbag play` 的记录时间消费顺序；
   同时间戳保留该原生迭代器给出的稳定次序，不另按物理序或 header 时间打破平局。
   `sequence` 是全话题 **View 回放序号**，过滤未选话题后允许有空洞，不是 `header.seq`。
   通过 View 的 protected 索引接口计算的 `physical_sequence` 仅保留原物理写入序号作为元数据，
   **不得用它决定消费顺序**；即使记录时间倒退，也必须与旧 rosbag play 一致，而不是恢复物理消费序。
   仅缓存轻量索引，索引处理时间复杂度为 O(N log N)，内存为 O(N)，不缓存全部消息载荷。
2. Writer/Reader 不再排序；Reader 严格检查 `sequence` 递增。header 时间允许倒退、重复、为零。
3. 离线入口对**每一个事件**先调用对应的 `push_*`，随后反复调用 `process_one()` 直到返回 false，
   然后调用 `propagate_imu()`。不能先按 header 时间整理再批量 push。
4. `Outputs.now` 返回当前事件的 bag 记录时间，精度为整数纳秒。其余 callbacks 可以不设置，
   轨迹、点云等离线结果文件仍由库按配置保存。
5. 默认不自动优化：交互终端 EOF 后 Enter 显式请求优化，其他输入后 Enter 跳过；
   非交互 EOF 立即 `finish()`，不无限等待、不偷偷请求优化。
   `--optimize-on-eof` 显式调用 `request_optimization()` 和 `wait_for_optimization()`。
6. SIGINT/SIGTERM 设置停止标志，在当前库调用返回后保存收尾；不会在信号处理函数中调用引擎。
   正在执行的同步优化等待由 Engine 的等待语义决定，不能强行中断库内部图优化。

该契约已用于完整 HH 的 63,720 条事件回放及前后端优化。
Noetic 与现代 Eigen 3.3.7 配置分别完成独立基线和默认四线程完整重复；
最终 ROS1/ROS2 对各自环境离线入口的完整轨迹一致。具体修复、版本指纹和结果见项目 README。
新的同步策略、传播时序或依赖版本仍需重新验证，不能直接放宽容差。

## 公开 API

头文件：`fast_livo/io/dataset.h`，命名空间 `fast_livo::io`。

- `Writer(directory)`：创建数据集；`append(const Event&)`；`finish()` 发布 manifest。
  析构不隐式 finish；忘记 finish 的输出不可读。
- `Reader(directory, max_event_bytes = 1 GiB)`：验证版本、文件大小、计数；`next(Event&)` 依次返回事件，
  EOF 返回 false。单事件载荷/索引受大小上限约束；不会一次加载整份索引或全部点云。
- `summary()`：总事件数与五类样本计数。Reader 的计数来自 manifest，遍历结束会校验实际计数。
- `Event.sample`：`variant<ImuSample::ConstPtr, CloudSample::ConstPtr, LivoxSample::ConstPtr,
  ImageSample::ConstPtr, GnssSample::ConstPtr>`；`kind()` 对应 1–5。
- Event 同时保留 `sequence`、`physical_sequence`、`record_time`、`has_header`、`header_time`、`header_sequence`、
  `topic`、`source_type`。GNSS PVT 本身无 ROS header，必须 `has_header=false`，
  header 时间和序号置零；不把 GPS week/tow 伪造为 ROS header 时间。
- 非 bag 来源可将 `physical_sequence` 留为 `UINT64_MAX`（未知），该字段无需单调递增。
- 客户端仅通过 `load_config(algorithm_file, camera_file, output_dir)` 获取 Config，不依赖 Config 字段。

## 二进制格式

固定文件名，未知主版本直接拒绝。版本 2 新增独立物理序号并修正默认回放契约；
不接受此前开发期按物理消费顺序输出的版本 1，旧输出必须重新转换。
除 PointCloud2 原始 data 外，多字节数值统一小端：
有符号数使用二进制补码、浮点使用 IEEE754；没有 C++ 结构体填充、没有每点 JSON。
`string = uint32 字节数 + UTF-8/原字符串字节`，单字符串最多 1 MiB，无结尾零。
`bool = uint8(0 或 1)`，`Timestamp = int64 纳秒`。

### manifest.yaml

字段为 `format: fast-livo-neutral`、`version: 2`、`byte_order: little`、
`ordering: bag-record-sequence`、`image_encoding: bgr8-raw`、`index: index.bin`、
`payload: payload.bin`、`index_bytes`、`payload_bytes`、`events`，
以及 `counts: {imu, cloud, livox, image, gnss}`。

### index.bin

8 字节 ASCII 魔数 `FLVIDX02`，随后每项是 `uint32 项长度` 和下列内容：

```text
uint64 sequence
uint64 physical_sequence
int64  record_time_ns
uint8  has_header
int64  header_time_ns
uint32 header_sequence
uint8  kind
string topic
string source_type
uint64 payload_offset
uint64 payload_length
uint64 payload_fnv1a64
```

payload 偏移从文件头起算，必须连续、无缝隙、无尾部多余数据。
FNV-1a 64 初值 `14695981039346656037`、乘数 `1099511628211`，
逐字节先异或再乘，模 2 的 64 次方；仅用于意外损坏检测，不是认证或安全哈希。

### payload.bin

事件载荷直接拼接。除 GNSS 外，每个载荷以 `int64 timestamp_ns + string frame_id` 开头：

| kind | 后续字段（顺序固定） |
|---|---|
| 1 IMU | 加速度 xyz、角速度 xyz、姿态 xyzw、姿态/角速度/加速度三组 9 元协方差，全部 float64 |
| 2 PointCloud2 | PCL header 的 uint32 seq、uint64 stamp、string frame_id；uint32 height/width/字段数；每字段 string name、uint32 offset、uint8 datatype、uint32 count；uint8 is_bigendian、uint32 point_step/row_step、uint8 is_dense、uint64 data 字节数、原始 data |
| 3 Livox | uint64 timebase、uint32 point_num、uint8 lidar_id、3 个 uint8 rsvd、uint64 实际点数；每点 uint32 offset_time、float32 xyz、uint8 reflectivity/tag/line |
| 4 图像 | uint32 rows/cols，随后逐行紧密 BGR8 字节（每像素 3 字节，不保存内存 padding） |
| 5 GNSS | 无公共 header；uint32 week、float64 tow、uint8 fix_type/valid_fix/diff_soln/carr_soln/num_sv，随后 11 个 float64：latitude/longitude/altitude/height_msl/h_acc/v_acc/p_dop/vel_n/vel_e/vel_d/vel_acc |

PointCloud2 的标准 datatype 1–8、自定义字段名/多元素 count、组织布局、行尾填充、大小端和所有 data
字节均保留；不会过渡成 PointXYZI。PCL header.stamp 按 PCL 约定为微秒，
**完整 ROS 纳秒 header 时间在 Event.header_time 和 CloudSample.timestamp 中**，不能用前者代替。
Livox point_num 必须等于实际点数，否则拒绝发布；空点云仍可保存。
图像使用 `cv_bridge::toCvCopy(message, "bgr8")`，支持原始/压缩消息，再无损保存解码像素；
原 JPEG/PNG 压缩字节、原图像 encoding/step 不作为中立 BGR 图像 API 的组成部分。

## 独立 CMake 目标与依赖

统一构建入口已按依赖顺序构建并安装以下目标：

| 目标 | 源文件 | 依赖 |
|---|---|---|
| `fast_livo_io` | `io/src/dataset.cpp`、`io/src/sample_codec.cpp` | C++17、yaml-cpp、OpenCV core、PCL headers（含 Eigen）；不需要 ROS 或 PCL I/O |
| `fast_livo_offline` | `apps/offline/main.cpp` | `fast_livo_io`、现有 Engine/core 库；POSIX poll/unistd；不链接 ROS |
| `fast_livo_bag_converter` | `apps/bag_converter/main.cpp`、`conversion.cpp` | `fast_livo_io`、rosbag、cv_bridge、sensor_msgs、gnss_comm、livox_ros_driver |
| `dataset_test` | `io/test/dataset_test.cpp` | `fast_livo_io` |
| `bag_converter_test` | `io/test/bag_converter_test.cpp`、`apps/bag_converter/conversion.cpp` | 同转换器，另需 OpenCV imgcodecs |
| 可选 `image_transport_equivalence_test` | `io/test/image_transport_equivalence_test.cpp` | ROS1 rosbag/roscpp/cv_bridge、OpenCV core/imgproc；运行需 compressed_image_transport 和真实 HH 首帧 |

公共 include 路径：`io/include`、`core/include`、PCL/OpenCV 的 include 路径。
私有 include 由相邻 `src/binary.h`、`src/sample_codec.h` 提供，无需安装。
ROS1 的 `gnss_comm`、`livox_ros_driver` 生成消息必须来自 underlay；
**不依赖旧 `include/livox_ros_driver`，不加入旧算法 include 目录，不把 ROS 导出给 io/offline。**

## 可独立执行的验证命令

版本 2 的全字段/损坏检测、真实多块 bag 转换测试均已接入 CTest，
支持重复执行，不重建镜像、不接触 `/data`。统一构建自动执行 I/O 测试，
Noetic 另外构建并执行 ROS1 转换器测试：

```sh
# 宿主机，已完成依赖准备和编译后
./script/env/build.sh fast-livo2 ros1-noetic

# 容器内单独重跑；Noetic 的 CMake 3.16 不支持 --test-dir
cd /catkin_ws/build/io
ctest --output-on-failure
cd /catkin_ws/build/converter
ctest --output-on-failure

# 指定新目录时保留 fixture，可进一步验证 CLI；不会覆盖已有路径
./bag_converter_test /results/io-check/new-ros-fixture
```

覆盖内容：五种中立样本全部字段、标准 PointField 全 datatype、自定义 count、
组织大端点云与行 padding 原字节、纳秒精度与 header 时间倒退、非连续 Mat、
完整 PVT、损坏/截断/版本/长度/空指针/重复序号、未完成发布与禁止覆盖。
真实生成的多块 LZ4 ROS1 fixture 还覆盖记录时间倒退/重复/为 ROS 最小时间时与原生 View 一致的消费顺序、
同记录时间戳的稳定顺序及独立物理序号、
零 header 时间、
原始 mono8 行 padding 与压缩 PNG 的 cv_bridge BGR8 等价性、
IMU 姿态/三组协方差、未知类型拒绝，以及全话题序号空洞。

`bag_converter_test` 不带参数时使用唯一临时目录并在成功后清理；
显式指定新目录时保留自行生成的小型 fixture，便于继续验证 CLI；不会修改输入 bag。
可用生成的 `wrong.bag` 验证默认转换必然非零退出；用
`--gnss-topic /missing` 验证缺少必需传感器会在创建输出前报错。
上述定向测试**不是**完整 FAST-LIVO2 数值验收的替代品。
真实 HH 已转换为 IMU 55,523、Livox 2,742、BGR8 图像 2,742、GNSS 2,713 条事件，
并完成完整离线前后端回放；各环境数值差异与最终验收状态见项目 README。

### 真实 HH 首帧的 image_transport 边界核验

`image_transport_equivalence_test.cpp` 只读取第一条 `/left_camera/image/compressed`，
实际发布给隔离的 `image_transport republish compressed → raw`，再将收到的 Image 转为 BGR8，
与 converter 使用的 `cv_bridge::toCvCopy(CompressedImage, "bgr8")` 逐通道比较。
不是两次调用同一个压缩转换函数互相验证。超时 15 秒报错，不回放整包、不需要修改源数据。

已在独立 `tritium-ros1:noetic` 容器核验真实 HH 首帧：

```text
compressed_format: rgb8; jpeg compressed bgr8
republished_encoding: rgb8
width: 2448
height: 2048
different_channel_values: 0
max_absolute_error: 0
red_blue_swapped_max_error: 111
source_header_sequence: 6000
republished_header_sequence: 0
header_stamp_equal: 1
header_frame_equal: 1
```

结论仅限该真实首帧及当前 Noetic 插件：两条链路的 BGR 像素完全一致，不存在首帧红蓝交换，
不需要修改 converter。中间 raw 的 encoding 为 rgb8 并不意味着最终 BGR 错误；
第二次 cv_bridge 转换将其恢复到同样的 BGR 像素。ROS 发布会重编号 header.seq，
但 header 时间和 frame 保持一致；中立格式保留原 bag header.seq，不据此调度。
这不能证明整条 HH 轨迹数值一致，也不能据此把后半段差异归因于 Eigen/PCL。
本次实际输出位于 `.result/fast-livo2/image-transport-first-frame-20261113/first-frame-comparison.txt`。

运行该检查时使用独立容器、只读挂载数据与源码，输出写 `.result`；
容器内先启动 roscore，再启动：

```sh
rosrun image_transport republish compressed in:=/equivalence/image raw out:=/equivalence/raw
image_transport_equivalence_test /data/HH-LVGO-01_compressed.bag
```

测试程序使用 `/equivalence/image/compressed` 和 `/equivalence/raw`，不可接到用户正在运行的 ROS master。
设置独立 `ROS_MASTER_URI` 和 `ROS_HOSTNAME`，用容器生命周期停止所有辅助进程。
