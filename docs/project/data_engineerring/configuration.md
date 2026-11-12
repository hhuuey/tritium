# 配置与数据版本

`src/dagster_pipeline/configuration.py` 统一管理 Case 登记、强制重算等编排配置模型、
配置目录定位及算法配置合成与渲染。运行环境配置由 `runtime.py` 管理。
源 YAML 仍保存在仓库根目录的 `config/`，打包时安装到 `dagster_pipeline/config/`。
算法负责人维护 Hydra 源 YAML，编排负责人维护 资产、runtime 和 Case 配置。
render 默认将全部配置安装到 `<runtime_root>/build/config/`，请求只传递最终算法配置文件的绝对路径。

## 配置目录

```text
config/
├── algorithm/
│   ├── sensor_hardware_7v.yaml
│   ├── sensor_hardware_11v.yaml
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

| 位置 | 内容 | 负责人 |
| --- | --- | --- |
| `algorithm/sensor_hardware_{7v,11v}.yaml` | 两种布局的参考 LiDAR、LiDAR、相机通道及强度 ring 规则 | 算法负责人共同维护 |
| `algorithm/semantic_common.yaml` | 语义类别 ID/名称/RGB | segmentation / Vector |
| `algorithm/ingestion/` | qfile 同步、按通道解码、NuScenes 和 GT 下载配置 | ingestion |
| `algorithm/calib_localization/` | 统一标定、定位配置 | 标定与定位 |
| `algorithm/segmentation/` | 分割、精修、训练和分割报告配置 | segmentation |
| `algorithm/raster_map/` | 栅格地图融合、裁剪及内部报告配置 | raster map |
| `algorithm/vector_map/` | 矢量地图推理、合并、导出和训练配置 | vector map |
| `dagster/assets.yaml` |资产算法入口、数据版本和配置路径 | 编排与对应算法负责人 |
| `dagster/runtime.yaml` | 产物根、实例根、外部程序、资源池和服务端口 | 编排 |
| `dagster/cases.yaml` | qfile URI 列表、Case 登记和外部模型 | ingestion、编排 |

## Case 登记

Case 配置统一使用 `dagster/cases.yaml`，直接在 `qfile_uris` 中登记数据：

```yaml
data_engineering_resource: /debug/data_engineering_resource
qfile_uris:
  - /mnt/qfiles/case-001.qfile
  - s3://qfiles/case-002.qfile
segmentation_model:
  model_uri: segmentation_resource/sam3.pt
  model_version: v1.0
  source: raw
vector_model:
  model_uri: vector_map_resource/model.ckpt
  model_version: v1.0
  source: raw
```

`qfile_uris` 必须是非空列表，单个 Case 也使用列表。每项可以是本地路径或远端 URI；
qfile 本地相对路径以源 Case YAML 所在目录解析。必填 `data_engineering_resource` 为本地资源根，
可使用绝对路径或相对 Case YAML 的目录，不接受 URI。`segmentation_model`、`vector_model` 和可选的
`sensor_calibration_models` 在同一 YAML 中配置，供列表内所有 Case 共用。所有 `model_uri`
必须是资源根下的非空相对路径，不接受绝对路径、URI、`~` 或含 `..` 的路径。
render 将资源根归一为绝对路径并保留相对 `model_uri`；登记时才拼出绝对本地模型路径写入 manifest。
配置加载、render 和登记均不检查模型文件存在；模型读取和格式检查属于实际消费算法。
每个 qfile 仍独立登记 partition 和 `case_manifest`，其真实 `payload/case.json` 保存 Case 身份、单个 `qfile_uri` 和模型配置。
登记返回统一 `ArtifactRef`，不创建模型绑定资产、不复制或同步模型；算法直接读取 manifest 中的本地模型路径。
当前五个矢量占位入口不读取 `vector_model`，该登记字段仍保留在 manifest 中。

修改配置并 render 后，统一执行：

```bash
uv run dagster-pipeline register-case \
  --config build/config/dagster/cases.yaml
```

## 公共硬件与资产覆盖

render 首先用 `--vehicle` 选择 `algorithm/sensor_hardware_7v.yaml` 或
`sensor_hardware_11v.yaml`，省略时使用 7v。7v 的公共默认值为：

```yaml
ref_channel: front_left_up
lidar_channels:
  - front_left_up
  - front_right_up
  - front_left_down
  - front_right_down
  - front_top
camera_channels:
  - camera_0
  - camera_1
  - camera_6
  - camera_7
intensity_ring_allowlist:
  front_left_up: [[0, 120]]
  front_right_up: [[0, 120]]
```

11v 的参考 LiDAR 为 `top_down`，LiDAR 为 `front_left`、`front_right`、`rear_left`、
`rear_right`、`top_down`，相机为 `camera_0`、`camera_1`、`camera_7`、`camera_9`。
布局名称是配置标识，不按名称推断相机数量。

| 布局 | `intensity_ring_allowlist` 默认值 |
| --- | --- |
| 7v | `front_left_up`、`front_right_up` 各保留闭区间 0～120，其余通道全部保留 |
| 11v | `{}`，全部通道保留全部 ring |

`ref_channel`、`lidar_channels`、`camera_channels` 注入全部资产；
`intensity_ring_allowlist` 仅注入 `case_raster_map`，其他资产不接收此字段。
输出的 `algorithm/sensor_hardware.yaml` 仍保留完整四字段布局快照。

全部 18 份算法配置先合并选中的三个传感器默认值，地图另合入 ring 规则，再应用资产参数；资产 YAML 不再声明
`/sensor_hardware`，最后保留 `_self_`：

```yaml
# @package _global_
defaults:
  - _self_

passthrough: true
pose_iterations: 5
voxel_size: 0.3
whitelist:
  enabled: false
```

同名资产参数覆盖公共配置，列表整体替换。例如分割资产可以设置自己的
`camera_channels: [camera_0, camera_1]`。每个资产维护一份参数文件，
共享硬件选择；没有其他参数的资产也继承硬件配置。`intensity_ring_allowlist` 的局部覆盖
整块替换公共规则，填写 `{}` 关闭筛选，不与车型规则逐通道合并。
缩小地图 `lidar_channels` 时须同步清空或重设 ring 规则，不能保留已移除通道；
算法严格拒绝未知通道，render 不自动过滤规则。

`algorithm/semantic_common.yaml` 中的 `semantic_classes` 定义 `class_id`、`name`、RGB `color`。
当前只有 `scene_camera_masks`、`case_raster_map` 读取该目录，
这两份配置的 `defaults` 在 `_self_` 之前增加 `/semantic_common`；资产同名参数仍优先。

| 项目类别 ID | 名称 | RGB | 色彩约定 |
| --- | --- | --- | --- |
| 0 | background | `[0, 0, 0]` | Cityscapes unlabeled 黑色 |
| 1 | road | `[128, 64, 128]` | Cityscapes road 紫色 |
| 2 | lane_marking | `[255, 255, 255]` | 项目扩展白色 |
| 4 | curb | `[255, 128, 0]` | 项目扩展亮橙色 |

背景与路面颜色参考 [Cityscapes 官方色表](https://github.com/mcordts/cityscapesScripts/blob/master/cityscapesscripts/helpers/labels.py)。
当前 `curb` 类别也包含护栏、围栏和柱等边界物体，使用亮橙色以便与紫色路面、白色标线区分。
Cityscapes 不单列 lane/curb；项目 ID 保持 `0/1/2/4`，不采用 Cityscapes 的类别 ID。
分割 `classes` 用 `class_id` 引用目录，保留 prompts、优先级、面积和阈值等推理策略。
地图 `road_class_ids` 选择类别，C++ 合成从公共目录生成对应色表；Python 裁剪使用上游
地图内的展示色表。公共 ID、名称必须唯一，ID 和 RGB 分量属于 uint8，背景 ID 0 必须存在。

`whitelist.enabled: false` 分别保留在每份资产 YAML 中，不属于硬件或语义公共配置。
当前未实现白名单，算法和编排均不根据该字段跳过或短路执行；它也不控制参数字段校验。

NuScenes 使用 `ref_channel` 同步组帧；定位将它作为参考 LiDAR，其他参与通道取
`lidar_channels` 中除参考通道外的项；分割使用 `camera_channels`；地图融合使用
LiDAR 和相机通道列表。解码从最终 YAML 的通道列表生成 `decode_manifest.json`，
NuScenes 独立读取自己最终 YAML 的 `camera_channels`、`lidar_channels` 选择传感器，
保留所选通道的原始 profile calibration。解码清单只声明可用通道和文件数，不覆盖 NuScenes 配置。

相机通道必须为 `camera_<N>`，LiDAR 为不带 `lidar/` 的裸方向名；列表元素必须唯一，
不接受 `all`、逗号、路径分隔符或空字符串。一种模态允许 `[]`，两种模态同时为空时失败。
NuScenes 所选通道必须已解码，`ref_channel` 必须属于其所选通道；缺少所选通道的
calibration、媒体或 odom 输出时失败。解码可以提供更大的通道集合；未选中的通道不参与
标定读取、媒体检查、同步、复制或标准表生成，其空外参不会影响 NuScenes 转换。
例如在 `algorithm/ingestion/nuscenes_dataset.yaml` 局部覆盖：

```yaml
camera_channels: [camera_0, camera_1]
lidar_channels: [front_left_up]
ref_channel: front_left_up
start_time: null
end_time: null
```

即使解码还包含 `camera_11`，这里也只转换两路相机和一路 LiDAR。公共硬件配置仍提供默认值，
修改选择后重新 render。`start_time`、`end_time` 使用绝对 Unix 秒，与解码媒体文件名时间戳
同单位，支持小数；`null`、空串或纯空白表示该边界不限。双空不裁剪，单边可独立限制；
非空值必须有限，且 `start_time <= end_time`。

NuScenes 先对全部所选通道按闭区间筛帧，再从 `start_frame_index`（默认 `1`）开始按
`frequency` 同步，并沿用 Scene 的时间与距离切分。同步和复制不使用区间外媒体；没有参考帧
或区间内无法满足同步要求时失败。双空的全量行为仍保留频率与 Scene 规则。所选解码通道的
全量文件计数、非空检查不受裁剪影响，odom 保留全量供插值。

源配置为 `config/algorithm/ingestion/nuscenes_dataset.yaml`，render 后使用
`build/config/algorithm/ingestion/nuscenes_dataset.yaml`。仅改变 NuScenes 通道或时间范围时无需重跑解码；按现有缓存规则更新
`nuscenes_dataset` 的显式 `data_version`，或为它及需要更新的下游逐一设置 `force_recompute`。

两个 GT 资产 YAML 的 `resource_download` 提供下载缓存读取根、锁目录和命令超时：

```yaml
resource_download:
  module_cache_root: /data/qomolo_resource/integration/module
  lock_root: /tmp/data-engineering-gt-resource-locks
  timeout_seconds: 900
```

资源名和版本由 qfile 的唯一匹配成功 `main` 提供，`vehicle_type` 不参与拼接。
下载固定使用 `sudo -n qomolo-get m <module> <version> -d`，需要该命令具备非交互执行权限。
`module_cache_root` 只控制查找位置，qomolo-get 没有目的目录参数；自定义读取根时需要
配置 qomolo 的缓存位置或预先放置资源。`metadata.json` 的所有 SHA256 都必须有效。
PCD 校验 `.module_metadata.json` 的 `__project`、`__resource_type` 标签；旧 OSM 包可缺省该文件，
存在时同样严格校验。PCD 先排除 `dynamic_elements` 目录下的文件；候选多于一个时，
再排除文件名包含 `ground` 的文件，忽略大小写且不匹配父目录名称。最终必须恰好剩余一个，
唯一候选本身含 `ground` 时仍可使用；筛后为空或仍有多个时，错误列出原始、排除及剩余候选的包内路径。
OSM 必须唯一，没有 OSM 时选择唯一非元数据 JSON；仅复制选中的地图，分别保存为 `map.pcd`、
`map.osm` 或 `map.json`。选中的文件必须被 checksum 清单覆盖且非空，所有清单条目仍须通过校验。

全部算法默认参数写在 YAML 中，不对算法参数设置字段白名单。
配置合成与 render 只处理 YAML 合成、插值和安装，不进行 raster 业务参数校验。
必需参数缺失时在算法使用处报错，算法保留实际数据、几何和资源要求的业务检查。

### 定位固定地图配准

[localization.yaml](../config/algorithm/calib_localization/localization.yaml) 默认设置：

```yaml
passthrough: true
map_alignment_enabled: false
reference_map_path: null
```

默认透传使用 NuScenes 原始 `ego_pose` 做点云运动去畸变，输出保留源定位的 translation、rotation
数值，跳过 BA、全局评估、辅助地图、debug 点云和优化报告。结果的 `metadata.passthrough`
为布尔值，metrics 同步记录当前模式。设置 `passthrough: false` 才启用定位优化。

启用时同时设置 `passthrough: false` 与 `map_alignment_enabled: true`。
保持 `reference_map_path: null`，
自动使用 `gt_point_cloud_snapshot` 的 `payload/map.pcd`，拒绝另行填写外部地图。
仅填写路径不会开启配准，配准与 passthrough 同时启用会失败。
地图需与 ego pose 同坐标系且初值有重叠；GT vector labels 尚未参与对齐。

`map_alignment_*` 参数控制平面 BA、采样和接受门限，当前默认最大平移修正为 3 米、
最大旋转修正为 10 度。修改地图、启用方式或参数后维护对应定位数据版本，再 render。
同版本下更改地图文件或配置不会自动失效缓存。参数、部分 scene 接受的状态及输出说明见
[Localization 说明](../src/algorithm/calib_localization/localization/README.md#固定地图配准)。

地图默认分辨率 0.02 米；块尺寸 1024，批次 250000 点，工作预算 4096 MiB，
聚合 worker 为 2。四方向裁剪距离默认各 15 米，沿地图坐标轴。

### Raster 相机选择与 LiDAR 点范围

生产和地图训练共用 `case_raster_map.yaml`；以下参数只影响地图合成。
相机和 LiDAR 默认继承 render 选中的 7v 或 11v 布局，
7v 的相机为 `camera_0`、`camera_1`、`camera_6`、`camera_7`；11v 为 `camera_0`、`camera_1`、`camera_7`、`camera_9`。
`camera_channels`、`lidar_channels` 均可在对应 raster YAML 中局部覆盖。
相机与 LiDAR 列表必须非空、无重复，算法执行时依据实际上游数据检查所选通道、
标定和媒体是否可用。配置合成与 render 不检查原始 `sensor_hardware.camera_channels` 的子集关系，
相机优先级和 LiDAR 点范围等业务参数也由地图算法在执行时校验。
局部覆盖不修改解码、定位或分割配置。
已有分割产物可包含更多相机；地图只读取所选相机的图像与 mask。
若需要减少分割计算量，另外修改 `scene_camera_masks.yaml` 的相机集合，确保覆盖地图选择。
下面是局部覆盖示例，选择两个相机并开启 ego 裁剪，不代表当前默认值：

```yaml
camera_channels: [camera_0, camera_1]
camera_priority: [camera_1, camera_0]
lidar_point_filter:
  source_range_enabled: true
  source_range_m: [5.0, 15.0]
  ego_enabled: true
  ego_x_m: [-5.0, 20.0]
  ego_y_m: [-10.0, 10.0]
  ego_z_m: [-2.0, 3.0]
```

`camera_priority: null` 按 `camera_channels` 的配置顺序选择；显式列表必须恰好包含
全部已选相机各一次，`[]`、遗漏、重复和额外相机均报错。列表前面的相机优先；对每个点，
第一个投影有效且 mask 属于 `road_class_ids` 的相机提供 RGB 与语义。未命中有效道路类别
时继续尝试后续相机；已选相机缺少图像、mask 或标定时失败，不以优先级掩盖缺失数据。

`lidar_point_filter` 统一作用于所有参与地图的 LiDAR，不支持逐雷达规则。
项目地图 YAML 默认 `source_range_enabled: true`、`source_range_m: [5.0, 15.0]`，
保留 5～15 米采集距离。将开关设为 `false` 可保留区间配置并跳过距离筛选；显式 `null`
区间也关闭该条件。距离开关缺省按 `true` 处理，有区间时生效。
`ego_enabled` 默认 `false`，XYZ 三项默认均为 `null`。
开关为 `false` 时，即使填写 XYZ 范围也跳过 ego 坐标转换与裁剪；设为 `true` 且至少
有一个非空 XYZ 区间时才执行。两个开关只接受布尔值，不能填写 `null`、数字或字符串。
单项区间缺省或 `null` 关闭对应条件；standalone 配置缺省整个块关闭全部条件，
不会由 native 补上项目 YAML 的 5～15 米范围，缺省 ego 开关按 `false` 处理。
显式值为包含端点的 `[min, max]` 米制区间，端点必须是有限数值且 `min < max`；
采集距离下界不得为负。未知字段、非 mapping 的块、字符串和布尔端点均报错。
距离条件与开关启用的 XYZ 条件取交集；全部关闭时保留原有点集。

`source_range_m` 使用 PCD 中保留的真实采集距离，启用该条件时字段缺失或非法值失败，
不从去畸变 XYZ 重算。关闭距离筛选不会额外要求该 PCD 字段，但启用强度归一化仍
独立要求真实、正的 `source_range_m`。`ego_x_m`、`ego_y_m`、`ego_z_m` 使用统一的
`sample.timestamp` 时刻优化 ego 坐标系，轴定义沿用输入标定与 pose。对于当前 LiDAR
去畸变点 `p_L`，使用完整姿态与外参变换：

```text
p_E(sample) = inverse(T_W_E(sample.timestamp)) * T_W_E(lidar.timestamp) * T_E_L * p_L
```

同一 sample 的多个 LiDAR 使用同一个 ego 参考时刻，不使用各自 LiDAR 帧时刻的 ego
作为筛选坐标。点范围筛选先于相机投影、强度校正拟合及应用；被排除点不参与纹理、
强度、高度、语义四层或原始强度对照，规则在关闭强度校正时仍生效。
该筛选与世界坐标 Scene 裁剪、只影响强度贡献的 ring 白名单分别配置。

范围筛选同时影响强度归一化的输入观测。自动 `fit_and_apply` 在校正观测采样前
筛选点，因此开关或区间变化会改变拟合样本，可能改变 ring、传感器、距离响应的支持覆盖与拟合系数；
仅使用 5～15 米数据拟合时，距离项可能因支持不足而回退。固定表 `apply` 的系数及
同一保留点的校正增益不变，但参与地图的点集和融合结果会变化。距离筛选开关不控制
归一化模型中的距离响应项；四层地图及拟合／应用继续共用同一范围规则。

`case_map.json.sensor_selection` 保存实际 LiDAR、相机集合与有效相机优先级；
`case_map.json.lidar_point_filter` 保存四项区间、`source_range_enabled`、`ego_enabled` 及
`coordinate_system: ego`、
`reference_timestamp: sample.timestamp`。结果 metrics 的 `lidar_point_filter.channels`
以通道名为 key，`summary` 提供汇总；各项记录 `input_points`、`range_kept_points`、
`ego_kept_points` 和 `projected_points`，分别为原始输入、采集距离条件保留、再经过
ego 范围条件保留，以及相机／语义投影后实际入图的点数。

地图 YAML 默认在读取点云、图像和 mask 前筛选 sample：

```yaml
frame_sampling:
  enabled: true
  min_translation_m: 0.05
intensity_robust_fusion: true
```

依据优化车辆位姿的世界 XY 位置，首帧保留；相对上一保留帧移动达到 5cm 才保留，
跨 Scene 不重置。垂直抖动、等待和原地旋转不补帧；保留帧使用全部配置传感器。
校正与原始对照及纹理、强度、高度、语义四层共用保留列表，跳过帧不读取业务文件。
Scene 中心仍按完整轨迹计算。Case 清单和质量报告记录各 Scene 的采样数量与 token。

`intensity_normalization.enabled: true`、`mode: fit_and_apply` 默认每 Case 自动拟合
传感器、ring 和距离的联合对数强度模型，使用当前选择的 LiDAR 通道。
自动拟合未配置身份时使用 Case＋通道隔离标识；固定表 `apply` 通过 `calibration_path`
加载，必须显式配置真实 `sensor_id`（单雷达）或独立硬件 `sensor_ids`（多雷达）。
输入 PCD 必须有真实 `ring/source_range_m`，采集距离不从去畸变 XYZ 重算。

标定网格默认 10cm，每格至少 3 帧、2 个 ring；使用可靠路面并剔除混合、标线和异常点。
`range_knots: 6` 是对数距离节点数，`iterations: 8` 是 Huber 拟合轮数；
`max_calibration_points: 200000` 为每传感器观测上限，采样和模型仍受内存预算限制。
ring 支持至少 20 个独立重复格，传感器连接至少 20 个共享格；距离至少 100 个重复格、
每节点至少 20 格且跨度至少 1.5 倍。按完整格划分 80% 拟合、20% 留出；不可识别或留出
一致性变差时禁用距离项，支持不足的已知 ring 保留原值，报告回退原因。
范围外不外推距离响应，总增益限制 0.5～2.0，不做精确入射角校正。
错误身份、损坏表、非法值和未知 ring 明确失败。主地图使用校正浮点强度，
`intensity_raw/` 保留按相同规则融合的完整原始对照。
完整参数、固定表接入和限制见[地图模块说明](../src/algorithm/raster_map/case_raster_map/README.md)。

默认稳健融合按每格／传感器／帧／ring 中位数 → 帧内 ring 等权均值 → 帧代表值 Huber
融合。`intensity_robust_fusion: false` 改用点级均值，供效果消融；精确点级 P90 始终保留。
纹理、语义和高度保持各自原有汇总方式，使用同一份抽帧列表，不进行空间模糊。

地图的强度 ring 白名单继承所选硬件配置，CLAHE 保留地图自身设置。以下为 7v 的有效默认值：

```yaml
intensity_ring_allowlist:
  front_left_up: [[0, 120]]
  front_right_up: [[0, 120]]
intensity_clahe:
  enabled: true
  tile_size_m: 2.0
  clip_limit: 2.0
```

7v 上述两通道保留 0～120，排除 121 及以上；其他通道未设白名单，保留全部。
11v 默认 `intensity_ring_allowlist: {}`，不继承 7v 通道名，也不按相似名称推断范围。
可在 `case_raster_map.yaml` 中整块覆盖规则，显式 `{}` 关闭筛选；阈值可按设备和场景调整。缺省通道或 `null` 保留全部；显式值必须是非空 uint16 闭区间列表，未知通道与非法范围报错。
筛选先于强度拟合、校正及融合，关闭校正时仍生效，启用的通道须提供真实 ring。
高度、RGB、语义与几何覆盖仍使用点范围筛选后完整的观测，不受 ring 白名单影响。
固定表只要求保留 ring 有对应系数。
`intensity_ring_filter_report.json` 提供通道／ring 的原始输入点数、通过点范围筛选后
再被 ring 白名单排除的点数，以及实际入图的强度点数，
Case 清单保存规则与报告路径，Dagster metrics 提供汇总。

强度有效性使用独立 `intensity_valid`，Scene 输出 `intensity_valid.npy`；无保留强度
与真实零强度可区分，训练清单的 `intensity_valid_mask` 指向该掩码。

强度主预览 `intensity_map_viz.png` 使用有效格的最终 `intensity`，地图默认如下：

```yaml
intensity_viz_mode: percentile
intensity_viz_percentiles: [1.0, 99.0]
intensity_viz_sigma: 2.0
intensity_viz_range: [0.0, 255.0]
intensity_viz_gamma: 0.6
```

`percentile` 使用全 Case 有效 `intensity` 的上下分位；`sigma` 使用均值 ± k 倍总体
标准差并限制在实际 min/max；`fixed` 使用配置范围。范围映射到 0～1 后计算
`normalized ** intensity_viz_gamma`，gamma 小于 1 提亮、等于 1 线性显示。
无观测格为黑色，常量有效格为中灰。`intensity_map_p90_viz.png` 显示 P90 诊断，复用
主图范围；原始对照、Case/Scene 主预览也使用同一范围和 gamma，局部不重新拉伸。
显示不改变 `intensity/intensity_p90` 数值；`intensity_map.png` 将融合强度按 0～255 clamp。

CLAHE 开启时增加两张 `intensity_map_clahe_viz.png`、`intensity_map_p90_clahe_viz.png`。
使用上述统一显示编码后，仅由强度有效像素拟合局部表；局部格锚定世界零点。
稳健／最终强度、P90、原始对照和 Scene 共用 Case 的 `intensity_clahe_luts.npy`，
清单记录实际格大小与 SHA-256。少于 256 个有效像素、常量格或空格用恒等映射。
CLAHE 可能增强残留条纹，建议固定抽帧和基础显示范围，对比原方案、仅白名单、
仅 CLAHE、两项同时启用，同时检查标线和强度缺测覆盖。

修改相机集合、优先级、点范围、抽帧、白名单、校正、融合或显示参数后重新 render，
按 [Dagster 缓存规则](../src/dagster_pipeline/README.md#缓存规则)维护数据版本或显式重算。
校正表和支持诊断属于算法业务文件，不属于协议层的配置或协议。

## 传感器标定

`sensor_calibration.yaml` 默认继承 render 选中的 7v 或 11v 布局。`ref_channel` 是选中 LiDAR 的参考通道，
`lidar_channels` 的其余项参与从激光求解，`camera_channels` 控制相机求解；
传感器选择与解码、NuScenes、定位使用相同布局。
不再从输入表推断额外传感器。选中的任意传感器缺失都会使整批失败；关闭透传后，几何退化或求解失败也会使整批失败。

标定配置默认 `passthrough: true`。透传时读取 NuScenes 输入版本的 `calibrated_sensor`、`sensor`
两表，将原始标定记录及其字段值原样保留在输出 `calibrated_sensor.json` 的 `records` 中。
仍校验 Case manifest、NuScenes 和两种 GT 的输入身份、目录，以及两表的结构和传感器关联；
跳过帧读取、窗口选择、LiDAR/相机求解、相机模型和 backend 加载。
`calibration_report.json` 与结果 `metadata.passthrough` 使用布尔值记录透传，metrics 同步记录。
需要实际执行标定时显式设置 `passthrough: false`；此时选中的相机通道还需要 manifest 中登记的本地模型。

相机模型由 Case YAML 的 `sensor_calibration_models` 统一登记。`depth_engine` 必须是
兼容部署 GPU/TensorRT 的米制 Depth Anything 2 TensorRT engine，`vocabulary` 是 ORB
词袋二进制。两者填写 `model_uri`、`model_version`、`source`，不包含摘要字段。
`model_uri` 相对 `data_engineering_resource` 解析，登记时转成绝对本地路径；
关闭透传并执行相机标定时必须是本机可读文件，
或包含 `model.json` 和安全相对 `checkpoint` 路径的本地模型 payload。
Dagster 传入统一 `case_manifest` 引用，native 读取 `case.json` 并校验 Case 身份；
算法 YAML 只保留 `passthrough`、传感器选择、`gpu_device` 与求解参数。透传或纯 LiDAR Case 可省略整个标定模型配置。

```yaml
data_engineering_resource: /debug/data_engineering_resource
sensor_calibration_models:
  depth_engine:
    model_uri: calibration_resource/dav2_orin_518x644_fp16.engine
    model_version: depth-anything2-metric-v1
    source: raw
  vocabulary:
    model_uri: calibration_resource/ORBvoc.bin
    model_version: orb-vocabulary-v1
    source: raw
```

修改源 Case YAML 后执行 `uv run dagster-pipeline render-configs`，
再通过 `register-case` 重新登记列表中的 Case，最后启动对应 job。
登记校验配置形状与资源根内路径约束，文件可稍后挂载；模型可读性和格式在实际执行相机标定时检查，
缓存命中时不重新检查。`install.sh` 不下载或转换模型。
`camera_channels: []` 可运行纯 LiDAR，但 `DAGSTER_PIPELINE_CALIBRATE_WITH_CAMERA=OFF`
仅控制编译，不自动修改硬件布局。

求解参数保持默认 `min_frames: 16`、`max_frames: 35`、`pose_iterations: 5`、
`extrinsic_iterations: 5`、`extrinsic_inner_iterations: 10`、`voxel_size: 4.0`、
`eigen_threshold: 10.0`、`max_frame_gap_seconds: 0.5`、`camera_observation_distance_m: 10.0`。
`camera_ground_normal_source` 默认 `depth_preferred`，可显式选择 `map_preferred`。
`lidar_installation_modes` 指定从 LiDAR 安装方式；`scene_ids: []` 使用全 Case 候选 scene。
`enable_body_mask` 默认 true，`enable_camera_evidence` 与 `enable_visualization` 默认 false，
`visualization_frames` 默认 3。更多输入条件见 [标定说明](../src/algorithm/calib_localization/sensor_calibration/README.md)。

资产登记的 `sensor_calibration.executable` 默认指向 `build/sensor_calibration/sensor_calibration`。
runtime 的 `pools.gpu_calibration: 1` 限制标定并发。
`module_localization_job`、`production_job` 和地图训练共用标定及定位，
两者都要求 GT 点云与矢量标签；当前标定不执行 GT 几何对齐。

## 资产登记与 render

源 `dagster/assets.yaml` 登记入口、版本和相对于该文件的配置路径：

```yaml
sensor_calibration:
  executable: build/sensor_calibration/sensor_calibration
  data_version: v1.0
  config_path: ../algorithm/calib_localization/sensor_calibration.yaml
localization:
  executable: build/localization/localization
  data_version: v1.0
  config_path: ../algorithm/calib_localization/localization.yaml
```

Python 资产用 `module` 指定入口，native 资产用 `executable` 直接指定程序路径，两者必须恰好选择一个。
`executable` 的相对路径基于项目运行根目录（`DAGSTER_PIPELINE_RUNTIME_ROOT`，默认当前目录），
加载规格时转换为绝对路径；`config_path` 仍相对于 `assets.yaml` 所在目录。
启动脚本会检查登记的 native 程序是否可执行；单次资产执行在缓存未命中后才启动程序。
地图合成资产的 `executable` 默认是 `build/case_raster_map/case_raster_map`，
配置路径为 `../algorithm/raster_map/case_raster_map.yaml`。
`dataconvert` 属于解码算法内部工具，在 `algorithm/ingestion/decoded_sensor_data.yaml` 中直接配置，
相对路径在 render 时基于源算法 YAML 的目录归一化；独立调用算法时相对于请求的配置文件目录。
runtime 仅保存运行目录、服务和并发池设置，程序路径不经过环境变量转发。
分割、地图和导出的验证报告由各生产算法写入自身 payload；检查使用生产算法配置，
不登记独立报告入口或报告 YAML。

定位的 `undistort_workers: 2` 和 `undistort_memory_budget_mb: 4096` 控制所有去畸变
worker 的共同预算，独立输出未压缩 binary PCD；`passthrough: true` 仍执行去畸变。
该预算不覆盖 BA、全量参考地图加载和配准点云。
地图使用 C++ 读取、投影、精确栅格聚合和完整 PNG 导出，默认工作预算 4096 MiB、
聚合并发 2，分段耗时和内存/磁盘指标写入 AssetResult。
Case、Scene、输入数据和输出路径来自请求，不写入算法 YAML。

显式生成最终配置：

```bash
uv run dagster-pipeline render-configs                  # 默认 7v
uv run dagster-pipeline render-configs --vehicle 11v    # 选择 11v
uv run dagster-pipeline render-configs --config-root /path/to/source/config
uv run dagster-pipeline render-configs --output-directory /path/to/export/config
```

`--vehicle` 仅接受 `7v`、`11v`，每次调用独立选择，不写入配置或环境变量、不记忆上次选择。
安装脚本默认渲染 7v；11v 环境安装后及以后每次 render 都必须显式传 `--vehicle 11v`。
切换布局后按既有缓存规则维护受影响资产的 `data_version`，或逐节点 `force_recompute`，
render 本身不使成功缓存失效。

`--config-root` 指定整个源 config 树，包含 algorithm 和 dagster 两个子目录。
默认源目录由 `DAGSTER_PIPELINE_CONFIG_ROOT` 或仓库 / wheel 的 config 定位。
默认目标为 `<runtime_root>/build/config/`；运行根由 `DAGSTER_PIPELINE_RUNTIME_ROOT` 指定，默认项目根，
配置位置不随 `DAGSTER_PIPELINE_ARTIFACT_ROOT` 改变。
自定义导出目录用于分发；使用前将完整内容安装到运行环境的 `<runtime_root>/build/config/`。

render 以 algorithm 子目录为 Hydra 根，合成全部已登记资产，展开 defaults 和插值，
选中的布局统一输出为 algorithm/sensor_hardware.yaml，不导出两个布局源文件；
另输出 semantic_common.yaml 及五类普通算法 YAML，同时安装 dagster 子树中的资产、runtime、
Case 配置。全部配置合成成功后才写出文件。
成功 render 后清理 `algorithm/` 与 `dagster/` 内不再生成的配置和空目录；
其他操作备注保留，合成失败时保留已有配置。
最终目录保留源配置的分类结构，仅公共硬件文件归一为 `sensor_hardware.yaml`；
全部输出仍为 23 份 YAML，安装后的资产配置路径为 `../algorithm/...`。

可选 `bpe_path`、地图的 `intensity_normalization.calibration_path`、Case 中的 `qfile_uris`
和 `data_engineering_resource` 若为相对路径，在 render 时按对应源文件所在目录展开为绝对路径。
Case 的 `model_uri` 保留相对资源根的路径，登记时才展开；资源根和模型 URI 不接受远端 URI。
qfile 的非本地 URI 保持原值，资源文件本身不复制。

运行时资产注册表只读取 `<runtime_root>/build/config/dagster/assets.yaml`。
AssetRequest 的顶层 `config_path` 指向安装后的绝对路径，Python 使用 PyYAML，
C++ 使用 yaml-cpp，算法运行时不合成配置。

`script/install.sh` 读取源 `dagster/runtime.yaml`，在依赖和 native 安装完成后将配置安装到
`<runtime_root>/build/config/`。
`script/run.sh`、`script/stop.sh`、`script/reset.sh` 均读取已安装的 `dagster/runtime.yaml`；
run 从已安装 Case 配置登记数据，不读取源目录、不自动 render。
配置缺失时提示先执行 render，不应用资源池或登记 Case。
修改源 YAML 或资产版本后需要手动重新 render。

## Job 与强制重算

六个 job 为 `module_ingest_job`、`module_localization_job`、
`module_segmentation_job`、`production_job`、`train_segmentation_job`、`train_vector_map_job`。
它们默认配置为 `{}`，无需导出 job YAML。Launchpad 只设置强制重算等编排选项。

`module_localization_job` 选择 `sensor_calibration → localization` 及上游。两个资产都要求 GT PCD 和 GT vector labels，
任一缺失都会失败。生产和地图训练共用 `case_raster_map → scene_raster_maps`；
矢量训练保留 GT 矢量标签依赖。五个矢量生产、训练入口当前直接返回成功，不读取业务输入或配置内容，
也不生成业务文件；没有额外开关。上游依赖、协议与发布检查继续生效。
算法参数不出现在 asset 的配置模型中。

资产强制重算示例：

```yaml
ops:
  localization:
    config:
      force_recompute: true
```

graph asset 使用实际嵌套 op 路径。例如 Scene 裁剪：

```yaml
ops:
  scene_raster_maps_graph:
    ops:
      scene_raster_maps:
        config:
          force_recompute: true
```

强制重算的范围和下游版本传播见 [Dagster 缓存规则](../src/dagster_pipeline/README.md#缓存规则)。

## 数据版本

请求与结果协议和 manifest 发布版本为 `v1.0`，项目包版本为 `1.0.0`。
各资产的初始 `code_version` 和 `data_version` 统一为 `v1.0`，后续按资产独立维护。
协议、代码、数据和模型版本独立维护。

资产数据版本由源 `dagster/assets.yaml` 登记，修改后显式 render。
输入引用保留生产者版本，输出使用当前资产自己的版本。生产和地图训练共用 Case 栅格图：
`<case>/raster_map/case_raster_map/v1.0/payload/`；Scene 裁剪按上游引用的数据版本读取地图，
输出使用裁剪资产自己的版本。NuScenes 标准表目录由 NuScenes 引用版本确定。
标定与定位的版本、目录和执行独立；定位拥有自己的 data 目录，生产和地图训练共用产物。

缓存命中条件、源内容或 channel 变更后的版本维护、上游传播、执行锁和离线复用，
统一见 [Dagster 缓存规则](../src/dagster_pipeline/README.md#缓存规则)。

训练资产统一返回指向其真实 payload 的 `ArtifactRef`。分割训练继续写入 model.ckpt 和 model.json，
后者保存名称、模型版本、来源、安全相对 checkpoint 路径及实际状态，不计算模型摘要。
两个矢量训练入口只创建空 payload，不生成训练列表、checkpoint 或模型元数据。
已有同版本缓存仍可复用；需要实际执行当前入口并清理旧业务文件时，对相应节点设置 force_recompute，
需要更新的下游节点也逐一设置。
manifest 不进行内容哈希、不拼接模型版本，也不增加版本字段。
修改模型路径、内容或配置中的 model_version 后，重新登记 manifest 并不会自动使下游缓存失效；
必须维护相关资产 data_version，或对受影响节点及其下游逐一设置 force_recompute。
GT 包完整性与地图校正等业务已有的摘要校验继续由各算法负责。

## 分组目录与命名

AssetGroup 在 src/dagster_pipeline/defs/assets/common.py 集中定义八个分组：
ingestion、ground_truth、calib_localization、segmentation_production、segmentation_training、
raster_map、vector_map 和 vector_map_training。各资产模块与 jobs 导入枚举，
执行 helper 从资产定义获取分组，向存储和 runner 传普通字符串。
assets.yaml 不另登记分组。

```text
artifact_root/<case>/<group>/<asset>/<data_version>/[scene]/payload/
manifest_root/<case>/<group>/<asset>.json
```

资产 key、生产 op、注册表键和协议 asset_name 使用同一名称。
case 表示整个案例的产物，scene 表示单场景，复数表示场景产物集合；
标定、定位与栅格地图使用统一名称，checkpoint 资产名称表示训练产出。
资产输出全部使用 ArtifactRef；动态 Scene 内部传字符串 ID，集合用 payload/index.json 记录子引用。
19 个资产对应 18 个算法执行规格，保留 6 个 job。
请求/结果的 `protocol_version: v1.0` 管理算法通信结构；成功标记属于编排内部模型。
结果成功时 `status=success` 且 `error=null`；失败时必须包含固定类别
`input`、`resource`、`algorithm`、`process` 或 `protocol` 的结构化错误。

## Runtime 配置

`config/dagster/runtime.yaml` 中的路径基于 `DAGSTER_PIPELINE_RUNTIME_ROOT` 解析，脚本默认设为仓库根。
部署 wheel 或独立目录时显式配置该根。
`environment` 字段标识运行环境，默认 `local`，通过 `DAGSTER_PIPELINE_ENVIRONMENT` 导出，
用于脚本检查目录与进程的环境归属。所有环境固定使用 `runtime.yaml`。
`DAGSTER_PIPELINE_CONFIG_ROOT` 仅用于选择源 config 树和 render，不由 runtime 导出，
不影响 run / stop / reset 的配置位置。

运行脚本先从 `<runtime_root>/build/config/dagster/runtime.yaml` 读取运行参数。
更换运行根时设置 `DAGSTER_PIPELINE_RUNTIME_ROOT` 并在该根 render；
`DAGSTER_PIPELINE_ARTIFACT_ROOT` 仅覆盖产物位置，不参与定位配置。

```bash
export DAGSTER_PIPELINE_CONFIG_ROOT=/etc/dagster-pipeline/config
export DAGSTER_PIPELINE_RUNTIME_ROOT=/srv/dagster-pipeline
export DAGSTER_PIPELINE_ARTIFACT_ROOT=/srv/dagster-pipeline/artifacts

# 源 dagster/runtime.yaml 的 environment、paths 及 cases.yaml 已配置好。
./script/install.sh
./script/run.sh
./script/stop.sh
```

`DAGSTER_PIPELINE_ARTIFACT_ROOT` 指定实际产物根，
`DAGSTER_PIPELINE_MANIFEST_ROOT` 指定 IO 记录根，默认位于产物根的 manifests 子目录。
IOManager 用当前产物根恢复相对 URI，外部 URI 保持原值。

`grpc_timeout_seconds` 只限制 Dagster 控制面调用，不能作为算法执行超时；资产、op 和 job
不配置自动重试，失败后通过 Dagster 手动重跑。手动强制重算会删除当前 data_version
目录中的历史 `_SUCCESS.json` 与 `_failure.json`，Dagster run 记录保留执行历史。
Scene 子步骤仅重算指定 Scene；集合发布只更新索引及集合状态，不删除其他 Scene 的成功产物。

七个 pool 在 runtime YAML 中指定。qfile 快照、解码和两种 GT 快照都使用 `nas_io`，
并发数固定为 1；runtime YAML 或环境变量将其设为其他值或无限制时，加载配置会报错。
其他 pool 的正整数表示并发上限，null 表示不设置上限。
`DAGSTER_PIPELINE_POOL_<POOL>_LIMIT` 可覆盖单项，如 `DAGSTER_PIPELINE_POOL_GPU_SEGMENTATION_LIMIT=1`。
地图合成默认并发 1，裁剪默认并发 2。

run.sh 持有实例生命周期锁并记录 PID 和目录归属。
`script/stop.sh` 停止对应实例，`script/reset.sh` 清理实例数据、业务产物和 IO 记录，
保留 `<runtime_root>/build/config/` 及 native 构建结果，重置后可以直接再次启动。
