# 数据生产流水线

Dagster 编排 qfile 同步、解码、NuScenes 转换、标定、定位、分割、地图合成、
Scene 裁剪、矢量地图、训练及报告。算法通过统一 JSON 请求接收输入引用和配置文件路径，
大文件保存到产物目录，Dagster IOManager 保存轻量引用。
公共协议由 Python/C++ `asset_protocol` 协议层统一读写；编排使用统一的 `asset_name`
构造产物引用，算法通过 `AssetRequest` / `AssetResult` 交换请求、指标和元数据。
依赖方向固定为 `algorithm → protocol` 和 `dagster_pipeline → protocol`，协议层不依赖算法或
Dagster；算法进程也不导入 Dagster 或 Dagster Pipes。`_request.json` 与 `_result.json` 属于算法通信协议；`_SUCCESS.json`、
`_failure.json`、缓存、进程诊断和发布状态属于 Dagster 编排内部模型。
请求/结果协议和模型登记标签为 `v1.0`，项目包版本为 `1.0.0`。
各资产代码版本和数据版本均为 `v1.0`。
`inputs` 使用真实上游资产名作为 key，每个 value 都是 `list[ArtifactRef]`，单输入也放入列表。

## 安装与启动

首次运行可按[单 Case 完整生产教程](docs/tutorial.md)，从白泽容器、数据及模型同步开始，
完成配置、启动、`production_job` 物化、日志排查和算法调参。

要求 Ubuntu/Debian 和 sudo/root 权限。安装脚本会补齐 uv，默认使用 Python 3.12，
所有算法与 Dagster 共用根目录的 uv 环境。安装脚本安装系统依赖、锁定的 Python 依赖，
构建 native sensor_calibration / localization / case_raster_map 并执行自测，
并将算法与编排配置安装到运行根的 `build/config/` 目录，默认位于项目根下。
原生构建自动清除 Conda 的编译器、头文件、库和 CMake 搜索路径，固定使用系统构建工具及
apt 的 OpenCV、PCL、yaml-cpp、fmt、libffi；可直接在 `(base)` 环境执行，无须手动清理。
安装进程清除继承的 `PYTHONPATH`、`PYTHONHOME` 并禁用用户级 Python 包；安装验收仅加载 pytest
内置插件，忽略外部插件和 pytest 参数，避免 ROS Foxy 等旧环境影响项目的 Python 3.12。
依赖路径、版本或 CMake 配置内容变化时自动重建缓存，依赖未变时保留增量编译。
原生 JSON 统一使用 nlohmann/json 3.11.2+，公共协议由 Valijson 1.0+ 校验；
优先使用满足版本的 apt 包。Ubuntu 20.04 等仓库缺包或版本过旧时，安装脚本自动下载
固定 nlohmann/json 3.11.2、Valijson 1.0，校验 SHA256 后安装到项目 `.deps/json/`。
这两个库仅含头文件，本地依赖与 CMake 路径一致，复用前校验文件，内容变化使 native 缓存失效。
地图校正表计数使用非负 int64 范围。[项目长期约束](AGENTS.md) 记录依赖与安装要求。

标定默认包含相机求解器。安装脚本下载并校验 Sophus 1.22.10，与系统 fmt 一起安装到 `.deps/`；
已有 Sophus 缓存不匹配时自动重建，避免继续引用 Conda 的 fmt。
缺少 CUDA/TensorRT 开发包时配置 NVIDIA 官方 apt 仓库，安装 CUDA 12.8 与 TensorRT 10.8 的匹配组合；
TensorRT 开发包、头文件和运行库在同一 apt 事务中全部指定相同版本，避免混选仓库中的新版本。
NVIDIA apt 下载及首次仓库密钥包下载默认使用云桌面可访问的
`http://proxy-qomolo.wellspiking.ai:32389`，APT 命令参数会覆盖已有 NVIDIA 代理设置。
可通过 `DAGSTER_PIPELINE_NVIDIA_PROXY` 指定其他代理，设为 `DIRECT` 时直连；
APT 代理仅用于 NVIDIA，Ubuntu 与内部源沿用系统配置；uv/Sophus/JSON 公网工具下载先尝试当前
出口，uv 同步或下载失败后使用同一代理重试。出口地址见 [内部代理说明](https://docs.wellspiking.ai/infra/proxy.html)。
安装需要 sudo/root 权限；相机运行还需要可用的 NVIDIA GPU/驱动、匹配该 TensorRT 的深度 engine 与 ORB 词袋。
已有依赖可通过 `DAGSTER_PIPELINE_SOPHUS_DIR`、`DAGSTER_PIPELINE_CUDA_ROOT`、
`DAGSTER_PIPELINE_TENSORRT_ROOT` 指定。仅部署 LiDAR 时设置 `DAGSTER_PIPELINE_CALIBRATE_WITH_CAMERA=OFF`，
并将硬件配置的 `camera_channels` 设为空。`DAGSTER_PIPELINE_SKIP_SYSTEM_DEPS=1` 仅用于依赖已齐全的机器。

```bash
./script/install.sh
./script/run.sh
```

`config/dagster/` 平铺三份配置：`runtime.yaml` 管理运行环境，`cases.yaml` 登记数据和模型，
`assets.yaml` 登记资产执行入口和版本。
启动、停止和重置统一读取 `build/config/dagster/` 中安装后的配置，不读取源目录。
在 Case YAML 的 `qfile_uris` 列表中直接填写一个或多个 qfile 路径或 URI，模型配置由列表中的 Case 共用。
Case 配置必填本地资源根 `data_engineering_resource`，各 `model_uri` 填其下的相对路径；
标准目录包含 `calibration_resource/` 和 `segmentation_resource/`。资源根可绝对或相对 Case YAML，
模型路径不接受绝对路径、URI、`~` 或 `..`。render 将根转为绝对路径并保留相对模型路径，
登记时才展开绝对路径写入 manifest，不检查文件存在、不复制资源。
GT 地图从 qfile 根目录 `info.json` 的录制历史取得精确资源版本。模型版本及来源信息在 Case 配置中登记。
正常 qfile 解码需要可用的 dataconvert；真实分割需要 SAM3 权重和 GPU。

解码从自己的最终配置读取 `camera_channels` 和 `lidar_channels`，公共默认值来自
选中的 `config/algorithm/sensor_hardware_7v.yaml`（默认）或 `sensor_hardware_11v.yaml`，
只请求配置列出的通道。相机填 `camera_<N>`，LiDAR 填裸方向名；`[]` 跳过一种模态，
两者不能同时为空。修改源配置后运行 `dagster-pipeline render-configs` 安装最终 YAML。
缺失配置通道或输出为空时失败，`decode_manifest.json` 记录选择、文件数和步骤耗时，
NuScenes 从自己的最终配置独立选择已解码通道的子集，仅处理所选通道；`ref_channel` 必须属于
该子集。解码清单不覆盖 NuScenes 配置，未选相机的空外参不影响转换；配置示例见
[公共硬件与资产覆盖](docs/configuration.md#公共硬件与资产覆盖)。
NuScenes 的可选 `start_time`、`end_time` 按绝对 Unix 秒限定闭区间，双空不裁剪；
裁剪后从 `start_frame_index`（从 1 开始，默认 1）起，在前 `initial_sync_search_frames` 个原始参考帧中
搜索首个全部所选传感器在 `sync_time` 内同步的起点。窗口默认 100，必须为正整数，包含第 N 帧且在频率降采样前计数；
失败候选不消耗媒体，设为 1 时指定起始帧必须同步成功。以找到的起点为锚点按 `frequency` 采样并切分 Scene；
后续采样帧同步缺失立即失败，窗口内无可同步起点也失败，参考时间戳跳变及里程计覆盖检查仍执行。
修改 `config/algorithm/ingestion/nuscenes_dataset.yaml` 后运行 `uv run dagster-pipeline render-configs`，
并重算 `nuscenes_dataset` 及受影响下游，可对这些节点逐一设置 `force_recompute`。
里程计读取 `odom/localization_odom/` 下全部分段 TXT，
合并后按时间戳排序供插值使用；重复时间戳保留，不因此报错。

GT 快照使用 `period_histories.maps` 的唯一成功 `main`，按项目及存在的 `ads_type` 过滤，
不拼接车型、不回退候选或 latest。下载调用 `sudo -n qomolo-get m <module> <version> -d`，
按资源版本加锁，并校验包内 SHA256 和存在的模块标签后保存 `map.pcd`、`map.osm` 或 `map.json`。
PCD 先排除 `dynamic_elements` 目录；候选多于一个时再排除文件名含 `ground` 的文件（忽略大小写），
最终必须唯一。唯一 ground PCD 可正常使用，父目录名称不参与 ground 筛选。
两个 payload 都包含 `resource_manifest.json`；下载、校验、复制失败时不发布成功标记。

登记配置中的全部 Case：

```bash
uv run dagster-pipeline register-case --config build/config/dagster/cases.yaml
```

登记读取控制信息，数据和模型可在运行真实算法前挂载。Case ID 是 qfile 文件名。
所有 job 使用已登记的 Case partition。

## Job

| Job | 范围 |
| --- | --- |
| `module_ingest_job` | 同步、解码和 NuScenes 转换 |
| `module_localization_job` | 两种 GT、`sensor_calibration → localization` 及上游 |
| `module_segmentation_job` | 分割及报告 |
| `production_job` | 标定、定位、分割、栅格地图及矢量占位入口 |
| `train_segmentation_job` | 分割精修、训练数据及模型 checkpoint |
| `train_vector_map_job` | 共用 GT／栅格链及两个矢量训练占位入口 |

`sensor_calibration` 和 `localization` 都依赖 `gt_point_cloud_snapshot` 与
`gt_vector_labels_snapshot`，任一缺失都会明确失败。生产和地图训练共用
`sensor_calibration → localization → case_raster_map → scene_raster_maps`。

完整生产先选择 7v 或 11v 硬件布局，检查相应 `sensor_hardware_<vehicle>.yaml` 的参考 LiDAR 与传感器通道，
然后运行安装脚本并启动。默认标定透传不需要标定模型；如果将标定 `passthrough` 设为 `false` 并选择相机通道，
再在 `config/dagster/cases.yaml` 的 `sensor_calibration_models` 中登记 ORB 词袋和
米制 Depth Anything 2 TensorRT engine（本地路径及版本）。
在 Dagster 的 Jobs 页面选择 `production_job`，选择已登记的 Case partition 后 Launch Run。
依赖图自动执行同步、解码、NuScenes、标定、定位，并结合相机分割生成栅格地图、裁剪和对应报告；
矢量生产与训练入口当前直接返回成功，不生成业务文件。
标定由 `gpu_calibration` 池限制为一次一个 Case；关闭透传后，任何选中传感器求解失败都会阻止该 Case 的下游生产。
调试标定/定位可运行 `module_localization_job`；生产和地图训练使用同一份标定、定位与栅格地图结果。
模型由算法直接读取 `case_manifest` 的本地路径，不设模型绑定资产、不下载模型、不计算模型摘要。
模型路径、文件内容或登记版本变化不会自动使缓存失效；维护消费资产的数据版本并 render，
或对受影响资产及其下游分别设置 `force_recompute`。缓存命中时不重新检查模型文件。

标定与定位默认均为 `passthrough: true`。标定将 NuScenes 原始 `calibrated_sensor.json` 的全部记录
原样保留在输出的 `records` 中，跳过帧读取、窗口选择、LiDAR/相机求解和相机模型、backend 加载，
仍校验输入身份、目录和标定表。报告及结果 `metadata.passthrough` 标记透传，metrics 同步记录。
将标定配置设为 `passthrough: false` 才执行实际外参求解。
定位使用原始 `ego_pose` 做点云运动去畸变，保留原始定位数值并跳过优化，结果 `metadata.passthrough`
记录当前模式；`map_alignment_enabled` 默认 `false`。
启用固定地图配准时，在 `localization.yaml` 设置 `passthrough: false` 与 `map_alignment_enabled: true`；
保持 `reference_map_path: null`，自动读取 GT 快照的 `map.pcd`，拒绝外部地图覆盖。
修改后维护对应数据版本并 render。参数、接受门限、部分 scene 对齐状态及输出见
[Localization 地图配准](src/algorithm/calib_localization/localization/README.md#固定地图配准)。

## 资产名称与分组

资产名描述产物，执行算法的 op、登记键、协议 `asset_name` 和产物目录使用同一名称。
`case_` 表示整个 Case 的产物，`scene_` 表示按 Scene 处理；集合使用复数，
例如 `case_raster_map → scene_raster_maps → scene_vector_maps → case_vector_map → vector_map_export`。
分割集合为 `scene_camera_masks`，标定、定位和栅格地图统一使用无后缀资产名。
训练终点为 `segmentation_model_checkpoint` 和 `vector_map_model_checkpoint`。
分割、标定及栅格地图的验证报告保存在所属算法的 payload 中，随同一次执行生成，不设独立资产或报告配置。

`src/dagster_pipeline/defs/assets/common.py` 中的 `AssetGroup` 枚举统一定义
`ingestion`、`ground_truth`、`calib_localization`、`segmentation_production`、`segmentation_training`、
`raster_map`、`vector_map`、`vector_map_training` 八个分组。各模块和 job selection 导入枚举，
执行时从 Dagster 资产定义获取分组，磁盘目录采用相同名称。
负责 Scene 发出、收集等辅助步骤的 op 保留动作名称，组合图使用 `_graph` 后缀。

## 算法配置与版本

`config/dagster/assets.yaml` 按资产名登记执行入口、数据版本和算法 YAML 路径。
全部执行资产的数据版本为 `v1.0`。
源配置按负责人分为 `config/algorithm/ingestion/`、
`config/algorithm/calib_localization/`、`config/algorithm/segmentation/` 、`config/algorithm/raster_map/` 和 `config/algorithm/vector_map/`。
`config/algorithm/sensor_hardware_7v.yaml` 和 `sensor_hardware_11v.yaml` 分别提供两种硬件布局。
render 先选择布局（默认 7v，`--vehicle 11v` 选择 11v），将 `ref_channel`、`lidar_channels`、
`camera_channels` 作为全部 18 份算法配置的公共默认值。布局还包含 `intensity_ring_allowlist`，
仅注入 `case_raster_map` 配置。`semantic_common.yaml` 提供语义类别 ID/名称/RGB，仅由
`scene_camera_masks` 和 `case_raster_map` 继承。各资产可以覆盖同名参数。
算法项目和执行模块以资产名称命名，配置文件统一为 `<asset_name>.yaml`。
定位实现使用 `localization` 项目与可执行文件；栅格地图合成实现使用 `case_raster_map` 项目与可执行文件。资产 key 描述产物。
每份资产 YAML 自行保留 `whitelist.enabled: false`；当前未实现白名单，该字段不控制执行。
分割与地图共用背景黑、路面紫、路面标记白、路沿亮橙色表。背景和路面匹配
[Cityscapes 官方色表](https://github.com/mcordts/cityscapesScripts/blob/master/cityscapesscripts/helpers/labels.py)。
路沿使用项目扩展亮橙色 `[255, 128, 0]`，便于与紫色路面、白色标线区分；当前类别也包含护栏、围栏和柱等边界物体。
白色车道线为项目扩展；Cityscapes 不单列 lane/curb，项目类别 ID 保持 `0/1/2/4`。
`qfile_snapshot`、`decoded_sensor_data` 与两个 GT 快照共用 `nas_io`，默认容量为 1。
Case 地图合成由 `src/algorithm/raster_map/case_raster_map/` 中的 C++ 程序执行，
配置为 `config/algorithm/raster_map/case_raster_map.yaml`；Scene 裁剪使用 Python `scene_raster_maps`。
Dagster 编排代码位于 `src/dagster_pipeline/`，总体执行流程、缓存发布机制及 Python 模块调用关系见
[Dagster 编排说明](src/dagster_pipeline/README.md)。

地图默认在读取点云、图像和 mask 前按优化车辆位姿抽帧，距上一保留帧的世界 XY 位移
达到 5cm 才保留，跨 Scene 延续判断；纹理、强度、高度、语义和强度校正共用帧列表。
每个 Case 联合拟合传感器、ring 和真实采集距离响应；支持不足的项回退并报告原因。
未填写硬件 ID 时按 Case 和通道隔离；跨 Case 固定表必须填写真实硬件身份。
强度按 ring 中位数、帧内等权及 Huber 稳健融合，主预览显示 `intensity`，默认全 Case
P1～P99。P90 诊断、完整原始对照及 Scene 裁剪共用基础显示范围；CLAHE 额外输出局部增强图。
生产和地图训练共用地图 YAML，当前默认值为：

| 参数 | 默认值 |
| --- | --- |
| 分辨率 | 0.02 米 |
| 相机 | 继承所选布局；默认 7v 为 `camera_0`、`camera_1`、`camera_6`、`camera_7` |
| 强度 gamma | 0.6 |
| 强度 ring 白名单 | 继承所选布局：7v 的 `front_left_up`、`front_right_up` 保留 0–120，其他通道全部保留；11v 为 `{}`，保留全部 ring |
| CLAHE | 开启，局部格 2 米 |

ring 白名单只影响强度贡献，其他图层继续使用通过点范围筛选的点。地图 YAML 可整块覆盖
`intensity_ring_allowlist`，填写 `{}` 关闭筛选；局部规则不与车型规则逐通道合并。
缩小地图 LiDAR 选择时须同步清空或重设 ring 规则，规则中的未知通道会报错。
配置与使用方式见[地图算法说明](src/algorithm/raster_map/case_raster_map/README.md)。

道路高度面与 QGIS 标注回投

`case_raster_map` 默认从语义 1（道路）和 2（车道线）观测生成平滑道路高度面，排除路沿，
并发布 `useful_height_map.png`（16 位数值图）、`useful_height_status.png`（0 无效、1 原始支持、
2 补洞）和 `useful_height_map_viz.png`（彩色预览）。数值高度按
`Z = offset_m + (code - 1) * scale_m` 解码，`code=0` 是缺测；参数和 SHA-256 保存在
`case_map.json`。默认最多向原始道路观测补 5 米，超过范围或支撑不足的区域保持缺测。
修改高度算法参数后，必须强制重算 `case_raster_map` 及下游 `scene_raster_maps`。

QGIS 手绘矢量的叠加与逐帧相机回投见[标注回投说明](src/algorithm/vector_map/projection/README.md)。
命令需要显式使用 `overlay` 或 `project` 子命令，两者共用 `--gt-dir`，固定读取目录内的
`polygon.geojson` 和 `polyline.geojson`。先按 Case 生成底图叠加：

```bash
uv run python script/project_vector_labels.py overlay \
  --case-id CASE --gt-dir /home/ubuntu/vector_gt --output-dir output
```

输出 `output/basemap_overlay.png`；也可用 `--basemap PNG` 直接指定底图，此时无需 `--case-id`。
按 Case 快速回投：

```bash
uv run python script/project_vector_labels.py project \
  --case-id CASE --gt-dir /home/ubuntu/vector_gt --output-dir output
```

资产根目录默认 `/debug/data_engineering/.artifacts`，可用 `--base-artifact-dir` 修改。
工具分别选择该 Case 的栅格、NuScenes、标定、定位资产的最高数值版本（`v1.10` 高于 `v1.9`，
忽略修改时间）；最新版不完整会报错，不回退旧版。GT 目录直接包含两个 GeoJSON。
未传底图时优先使用地图登记的 CLAHE 主强度图，其次 P90；NuScenes 表目录自动发现，存在歧义时用
`--nuscenes-version` 指定。显式路径优先，仍校验地图与定位来源一致；完整手动示例：

```bash
uv run python script/project_vector_labels.py project \
  --basemap /data/intensity_map_clahe_viz.png \
  --polygon /data/polygon.geojson --polyline /data/polyline.geojson \
  --case-raster-dir /data/case_raster_map \
  --nuscenes-dir /data/nuscenes_dataset \
  --calibration-dir /data/sensor_calibration \
  --localization-dir /data/localization --output-dir output
```

工具会按 `output/<sample_timestamp>_<camera_channel>.png` 保存全部相机帧，并写入
`projection_manifest.json`。高度缺测处断开线段、统计全局及每个 fid 的缺测采样和丢弃长度，
但仍保存没有可见标注的原尺寸相机图。

独立算法可附带协议实现，无需实现请求与结果的公共字段解析：

```bash
uv run dagster-pipeline export-asset-protocol --output-directory dist/asset-protocol-v1.0
```

导出公共 Schema、共享契约样例、自动生成的 Python/C++17 模型、CMake 和成功/失败示例。
两端在读写边界执行 Schema 与严格 JSON 校验。Python 使用 jsonschema，C++ 使用 nlohmann/json 和 Valijson。
修改公共固定字段只需编辑 Schema 后运行 `uv run python script/generate_protocol.py`；
`uv run python script/generate_protocol.py --check` 检查生成物漂移。Node 仅在开发生成时需要。

调用方用 `write_request` 写请求；算法用 `read_request` 得到生成的类型，通过
`input_ref` / `input_refs` 取得输入 payload 引用，读取 `config_path` 并向 `output_uri` 写业务文件，
最后用 `success_result` / `failure_result` 和 `write_result` 返回结果。
调用方用 `read_result` 解析结果，并通过 `validate_result_identity` 核对它属于本次请求。
`AssetResult` 只报告状态、错误、指标和元数据，最终产物引用及成功标记由 Dagster 发布。
完整 JSON、字段必填规则、Python/C++ 接入和异常处理见
[协议层说明](src/protocol/README.md#一次调用如何传递输入与输出)。

源码按三层组织：`src/algorithm/` 为算法，`src/dagster_pipeline/` 为编排，
`src/protocol/` 集中保存公共规范、Python/C++ 协议实现、示例和说明。

修改源 YAML 后显式渲染：

```bash
uv run dagster-pipeline render-configs                  # 默认 7v
uv run dagster-pipeline render-configs --vehicle 11v    # 选择 11v
uv run dagster-pipeline render-configs --config-root /path/to/source/config
uv run dagster-pipeline render-configs --output-directory /path/to/export/config
```

`--vehicle` 只接受 `7v`、`11v`；每次调用独立选择，不保存上次选择。安装脚本默认渲染 7v，
11v 环境安装后及后续每次重新 render 都须带 `--vehicle 11v`，再启动服务。
`--config-root` 指定包含 `algorithm/` 和 `dagster/` 的整个源 config 树。
Hydra 合成 `algorithm/` 中的配置并展开插值，同时安装 `dagster/` 中的资产执行登记、
runtime 和 Case 配置。选中的硬件配置统一输出为 `algorithm/sensor_hardware.yaml`，
不导出两个布局源文件；最终目录如下：

```text
build/config/
├── algorithm/
│   ├── sensor_hardware.yaml
│   ├── semantic_common.yaml
│   ├── ingestion/
│   ├── calib_localization/
│   ├── segmentation/
│   ├── raster_map/
│   └── vector_map/
└── dagster/
    ├── assets.yaml
    ├── runtime.yaml
    └── cases.yaml
```

算法请求的顶层 `config_path` 指向安装后的绝对路径；Python 使用 PyYAML，
C++ 使用 yaml-cpp。启动脚本读取已安装配置，不自动 render。
运行根由 `DAGSTER_PIPELINE_RUNTIME_ROOT` 指定，默认项目根；配置固定从其 `build/config/` 读取，
独立于 `DAGSTER_PIPELINE_ARTIFACT_ROOT`。`DAGSTER_PIPELINE_CONFIG_ROOT` 仅选择源配置；
`--output-directory` 用于显式导出。重置保留 `build/config/` 和 native 构建结果。
Job 默认配置为 `{}`，Launchpad 只控制强制重算和运行资源。

产物路径固定：

```text
.artifacts/<case>/<group_name>/<asset_name>/<data_version>/payload/
.artifacts/<case>/<group_name>/<asset_name>/<data_version>/<scene>/payload/
.artifacts/manifests/<case>/<group_name>/<asset_name>.json
```

执行目录中保存 `_request.json`、`_result.json`、`_SUCCESS.json` 和 `payload/`；失败执行由编排层
在同一目录原子写入 `_failure.json`，不会创建独立诊断目录。
请求和结果使用协议 `v1.0`；缓存由显式资产数据版本管理，完整规则见
[Dagster 缓存说明](src/dagster_pipeline/README.md#缓存规则)。

所有资产统一返回 `ArtifactRef`，URI 指向实际的 `payload/`。登记时写入 `case_manifest` 的
`payload/case.json`；多 Scene 资产写入集合 `payload/index.json`，包含相对路径的子引用，
各 Scene 保持独立缓存，集合具有自己的结果和成功标记。
所有受管资产的输入、输出 URI 都指向 `payload/`。NuScenes 标准表位于
`ingestion/nuscenes_dataset/v1.0/payload/v1.0/`，媒体位于 `payload/samples/`；编排通过执行元数据获取 scene 列表。
`sensor_calibration` 与 `localization` 分别独立执行并缓存。
标定在 payload 中保存 `calibrated_sensor.json`；定位保存 payload 根目录的
`ego_pose.json`、`lidar_undistort.json` 和 `data/<scene>/` 点云，生产和地图训练共用这些产物。
标定尚未应用 GT 几何对齐，如实报告 `alignment_applied: false`。
定位可对齐快照 PCD；`alignment_applied` 仅表示至少一个 scene 接受配准，
`map_alignment_scene_count` 用于区分部分接受。GT vector labels 的几何对齐尚未实现。
`scene_vector_maps`、`case_vector_map`、`vector_map_export`、`vector_map_training_dataset` 和
`vector_map_model_checkpoint` 当前全部直接返回成功，不读取模型、地图或训练输入，不检查业务内容，
也不生成地图、报告、训练列表或 checkpoint。单次算法 payload 为空，结果的 metrics、metadata 均为空；
Scene 集合仍由编排生成 `index.json`。现有上游依赖、协议校验、成功标记和缓存规则继续生效，
成功仅表示入口执行完成。详细说明见[矢量地图入口](src/algorithm/vector_map/README.md)。
分割精修仍复制基础权重，记录 `no-op-copy`。

## 部署与复用产物

1. 配置新环境路径并登记 Case。
2. 复制完整 `<case>/<group_name>/<asset_name>/<version>/` 目录及它引用的产物，保留 payload、执行结果和成功标记。
3. 运行需要的 job，由缓存重新生成当前环境的 IO 引用。

受管 IO 引用和业务清单采用相对路径，搬迁时保留引用目标的目录关系。
无需搬运 IO 记录；外部源数据和模型需要在新环境配置有效位置，复制完成后再运行 job。
产物复用、版本传播和强制重算规则统一见 [Dagster 缓存说明](src/dagster_pipeline/README.md#缓存规则)。

算法结果必须带有 `status` 和 `error`：成功时 `status=success` 且 `error=null`，算法主动
失败时提供结构化 `AssetError`。错误类别固定为 `input`、`resource`、`algorithm`、`process`、
`protocol`；进程启动、退出、缓存、发布等编排失败由 Dagster 的 `OrchestrationError` 写入
`_failure.json`，不会伪装成算法结果。SIGKILL 只记录进程失败，不能据此推断 OOM。资产、op 和 job 不设置 Dagster 自动重试，
失败后由 Dagster 重新发起一次手动运行；`grpc_timeout_seconds` 只限制控制面调用，不是算法执行超时。

## 验证与开发说明

native 执行路径直接配置在 `config/dagster/assets.yaml` 的 `executable`；
解码工具路径配置在 `config/algorithm/ingestion/decoded_sensor_data.yaml` 的 `dataconvert`。
下列 `*_BIN` 变量只为测试指定受测构建，不参与资产启动路径解析。

```bash
DAGSTER_PIPELINE_LOCALIZATION_BIN="$PWD/build/localization/localization" \
DAGSTER_PIPELINE_CASE_RASTER_MAP_BIN="$PWD/build/case_raster_map/case_raster_map" uv run pytest -q tests
uv run dg check defs
ctest --test-dir build/localization --output-on-failure
ctest --test-dir build/case_raster_map --output-on-failure
ctest --test-dir build/sensor_calibration --output-on-failure
```

- [配置与版本](docs/configuration.md)
- [架构及团队职责](docs/architecture.md)
- [算法接口](docs/algorithm-contract.md)
- [QP-53850 流水线配置与安装修复](docs/changes/qp-53850-pipeline-config-and-install.md)
- [QP-51665 数据生产流水线整合](docs/changes/qp-51665-data-pipeline-consolidation.md)
- [Native 标定与诊断](src/algorithm/calib_localization/sensor_calibration/README.md)
- [Case 地图与 Scene 裁剪](src/algorithm/raster_map/case_raster_map/README.md)
- [Native Localization](src/algorithm/calib_localization/localization/README.md)
