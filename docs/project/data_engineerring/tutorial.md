# 单 Case 生产教程

本文从一个真实 Case 开始，演示在白泽 Docker 中准备数据和模型、配置 Dagster、运行
`production_job`，以及查看结果、定位失败和调整算法参数。关联变更：
[QP-53850](changes/qp-53850-pipeline-config-and-install.md)。

示例 Case 为：

```text
cnwxijk-qthd-58-2026-07-29T101300.000Z-2026-07-29T101700.000Z
```

当前生产链包含真实的数据解码、NuScenes 转换、相机分割、定位去畸变和栅格建图。
标定默认透传原始标定，定位默认使用原轨迹去畸变；矢量地图的 Scene 推理、Case 合并和
导出目前均为直接成功的占位入口。**生产成功不代表已生成矢量地图。**

## 1. 启动白泽 Docker，确认挂载

通过白泽平台启动项目使用的开发容器，启用 NVIDIA GPU，配置以下挂载和访问条件：

- 宿主机的一处可写目录映射到容器 `/debug`，用于保存原始数据、模型和运行结果。
- 宿主机项目 `/home/ubuntu/code/data_engineering` 映射到容器 `/debug/data_engineering`。
  若项目本来就在上述 `/debug` 挂载中，无需重复挂载。
- 开放容器的 `3000` 端口，通过白泽端口转发或宿主机端口映射访问 Dagster UI。
- 容器可以安装系统依赖、访问项目依赖源，并具备 GT 资源下载所需的权限和网络。

镜像名称和容器启动入口使用当前白泽环境的实际配置。下文不假设通用的 Docker 镜像或
启动命令。**命令均按 Bash 编写**；宿主机终端和容器终端分别执行，不能混用路径。

| 用途 | 宿主机 | 容器内 |
| --- | --- | --- |
| 项目 | `/home/ubuntu/code/data_engineering` | `/debug/data_engineering` |
| 数据、模型根 | 实际映射到 `/debug` 的目录 | `/debug` |
| 原始 Case | 上述目录的 `jk-mini-dataset-raw/<case>` | `/debug/jk-mini-dataset-raw/<case>` |
| 模型资源 | 上述目录的 `data_engineering_resource/` | `/debug/data_engineering_resource/` |
| Dagster 状态及产物 | 项目目录内，受项目挂载保留 | `/debug/data_engineering/.dagster_home`、`.artifacts` |

## 2. 在宿主机同步 Case 和资源目录

以下 `/mnt` 源路径在**宿主机**访问。先输入实际映射到容器 `/debug` 的目录，不要直接把
宿主机项目目录当作该根目录，除非挂载确实如此。

```bash
set -euo pipefail
read -r -p '请输入宿主机实际映射到容器 /debug 的绝对目录：' HOST_DEBUG_DIR
[[ "$HOST_DEBUG_DIR" == /* && -d "$HOST_DEBUG_DIR" && -w "$HOST_DEBUG_DIR" ]]

CASE_ID=cnwxijk-qthd-58-2026-07-29T101300.000Z-2026-07-29T101700.000Z
CASE_SOURCE=/mnt/.wellspiking/ai-infra-data-a989a8e9f2b94758952a60500ba8bb7b/2025030400001/ce4b8976aeb63e863dbe7a6af36ded8f
RESOURCE_SOURCE=/mnt/spiking-storage/.shared/haihui.huang/data_engineering_resource

mkdir -p "$HOST_DEBUG_DIR/jk-mini-dataset-raw" \
  "$HOST_DEBUG_DIR/data_engineering_resource"

rsync -Pa "$CASE_SOURCE/$CASE_ID" "$HOST_DEBUG_DIR/jk-mini-dataset-raw/"
rsync -Pa "$RESOURCE_SOURCE/" "$HOST_DEBUG_DIR/data_engineering_resource/"

test -s "$HOST_DEBUG_DIR/jk-mini-dataset-raw/$CASE_ID/info.json"
ls -lh "$HOST_DEBUG_DIR/data_engineering_resource/segmentation_resource/sam3.pt" \
  "$HOST_DEBUG_DIR/data_engineering_resource/calibration_resource/dav2_orin_518x644_fp16.engine" \
  "$HOST_DEBUG_DIR/data_engineering_resource/calibration_resource/ORBvoc.bin"
```

`-P` 显示进度并保留部分传输文件，`-a` 保留目录结构及适用的文件属性。传输中断后可以
重跑相同命令。Case 的源路径**没有尾部 `/`**，所以复制完整 Case 目录；如果源写成
`.../$CASE_ID/`，含义变为复制该目录的内容，需要将目标明确写为对应 Case 目录。
不要把 Case 的内容散放在 `jk-mini-dataset-raw/` 根目录。资源源路径带尾部 `/`，
所以将共享资源目录的完整内容复制到目标 `data_engineering_resource/`，保留其内部结构。
容器内需要得到：

```text
/debug/data_engineering_resource/
├── calibration_resource/
│   ├── dav2_orin_518x644_fp16.engine
│   └── ORBvoc.bin
└── segmentation_resource/
    └── sam3.pt
```

## 3. 在容器内检查资源

此后命令如未特别标明，均在**容器内**执行。

```bash
cd /debug/data_engineering
CASE_ID=cnwxijk-qthd-58-2026-07-29T101300.000Z-2026-07-29T101700.000Z

test -s "/debug/jk-mini-dataset-raw/$CASE_ID/info.json"
test -r /debug/data_engineering_resource/segmentation_resource/sam3.pt
test -r /debug/data_engineering_resource/calibration_resource/dav2_orin_518x644_fp16.engine
test -r /debug/data_engineering_resource/calibration_resource/ORBvoc.bin
test -r /opt/qomolo/baize/config/player_tools/dataconvert
nvidia-smi
```

若容器内找不到文件，先核对挂载和复制层级。`dataconvert` 是白泽工具，默认路径见
`config/algorithm/ingestion/decoded_sensor_data.yaml`；实际安装位置不同需修改该配置后 render。
安装脚本不会下载上述模型。TensorRT engine 还需兼容实际部署的 GPU 和 TensorRT 版本。

完整生产同时依赖 GT 点云和 GT 矢量标签。算法从 Case 根目录 `info.json` 中选取与录制
项目匹配的固定地图版本；缓存缺失或不可用时，使用非交互命令
`sudo -n qomolo-get m <资源模块名> <固定版本> -d` 下载，再检查资源清单和内容。
因此容器必须具备相应工具、资源权限及免交互执行权限。默认资源缓存位于
`/data/qomolo_resource/integration/module`。它与这里手动同步的三个模型不是同一类资源。
标定、定位开启透传也仍要求两种 GT 上游成功。

## 4. 编辑源 Case 配置

在宿主机编辑 `/home/ubuntu/code/data_engineering/config/dagster/cases.yaml`，或在容器内
编辑同一挂载文件 `/debug/data_engineering/config/dagster/cases.yaml`。
首次演示只保留本 Case，内容如下。`qfile_uris` 和资源根填写**容器内路径**，
各 `model_uri` 填相对资源根的路径：

```yaml
data_engineering_resource: /debug/data_engineering_resource
qfile_uris:
  - /debug/jk-mini-dataset-raw/cnwxijk-qthd-58-2026-07-29T101300.000Z-2026-07-29T101700.000Z
segmentation_model:
  model_uri: segmentation_resource/sam3.pt
  model_version: v1.0
  source: raw
vector_model:
  model_uri: vector_map_resource/model.ckpt
  model_version: v1.0
  source: raw
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

`vector_model` 是当前登记结构的必填配置，上面的路径用于占位；当前矢量入口不读取该文件，
无需额外同步或创建它。SAM3 权重用于实际分割。两个标定模型在关闭标定透传、执行相机标定时
读取；默认透传跳过模型加载。所有模型均只读本地路径，不会由 Dagster 自动同步。

`data_engineering_resource` 必填，可写容器内绝对目录，或相对源 `cases.yaml` 的目录，
不接受 URI。所有 `model_uri` 必须是非空相对路径，不接受绝对路径、URI、`~` 或 `..`。
render 将资源根转成绝对路径，`build/config/dagster/cases.yaml` 中仍保留相对 `model_uri`；
登记时才拼出绝对模型路径写入 `case_manifest/payload/case.json`，算法继续按绝对本地路径读取。
配置渲染和登记不检查模型文件存在；实际消费模型时才检查可读性与格式。

一份 Case YAML 的模型配置用于列表中全部 Case。Case ID 是 qfile 路径的最后一级目录名，
因此 UI partition 会显示完整的录制目录名。

## 5. 首次安装、渲染配置

在容器中、项目根目录执行。安装需要 apt 权限和下载依赖的网络；脚本使用系统 C/C++
编译器及系统 OpenCV、PCL、yaml-cpp，自动准备 Python 环境，构建 native 程序并运行自测，
最后渲染全部配置。第一次运行可能耗时较长，保存日志便于定位失败。

```bash
(
  set -euo pipefail
  cd /debug/data_engineering
  mkdir -p build/logs
  ./script/install.sh 2>&1 | tee "build/logs/install-$(date +%Y%m%d-%H%M%S).log"

  # 若 uv 由安装脚本下载，它位于此处；脚本不能修改调用它的终端环境。
  export PATH="$PWD/.deps/uv:$PATH"
  export DAGSTER_PIPELINE_RUNTIME_ROOT="$PWD"

  test -x build/sensor_calibration/sensor_calibration
  test -x build/localization/localization
  test -x build/case_raster_map/case_raster_map
  test -f build/config/dagster/cases.yaml
  uv run dg check defs
)
```

代码块在子 shell 内运行，任何一步失败即停止本块；环境变量只在本块生效。
只有安装成功后才继续。不要把安装日志中某个警告当作最终结果，应查看失败命令和非零退出。
默认构建包含相机标定支持；模型已经准备好不代表可以绕过实际依赖安装。

配置分为两层：

| 位置 | 用途 |
| --- | --- |
| `config/dagster/{runtime,cases,assets}.yaml` | 编辑运行参数、Case 登记、资产程序与版本 |
| `config/algorithm/` | 编辑各算法参数；保留 Hydra 的 `defaults` 和 `# @package _global_` |
| `build/config/dagster/`、`build/config/algorithm/` | render 后运行实际读取的配置 |

本教程按默认 7v 布局演示。安装脚本也默认渲染 7v。若数据使用 11v，在安装成功后、
启动服务前执行 `uv run dagster-pipeline render-configs --vehicle 11v`，检查生成文件中的
参考通道为 `top_down`。

后续只修改 YAML 时不需要重复安装。在项目根目录执行：

```bash
cd /debug/data_engineering
export PATH="$PWD/.deps/uv:$PATH"
export DAGSTER_PIPELINE_RUNTIME_ROOT="$PWD"
uv run dagster-pipeline render-configs
cat build/config/dagster/cases.yaml
cat build/config/algorithm/ingestion/nuscenes_dataset.yaml
```

render 会合成公共配置和局部覆盖、展开相对路径，并安装最终普通 YAML。它会覆盖相应生成
配置，因此长期修改应写在源 `config/`。**`run.sh` 只读 `build/config/`，不会自动 render。**
源配置改变但未 render，运行仍使用旧参数；直接改生成文件也会被下一次 render 覆盖。
`--vehicle` 不记忆上次选择：11v 环境在本教程每一处重新 render（包括重新登记、调参）时
都必须带 `--vehicle 11v`；无参数会恢复 7v。切换布局后仍须按第 11 节处理成功缓存。

## 6. 启动服务并登记 Case

```bash
cd /debug/data_engineering
export PATH="$PWD/.deps/uv:$PATH"
export DAGSTER_PIPELINE_RUNTIME_ROOT="$PWD"
set -o pipefail
mkdir -p build/logs
./script/run.sh 2>&1 | tee "build/logs/dagster-$(date +%Y%m%d-%H%M%S).log"
```

该命令前台运行，保留这个终端。脚本会加载已渲染 runtime、检查 native 文件、应用并发池、
从已渲染 `cases.yaml` 登记动态 partitions，并发布每个 Case 的 `case_manifest`，然后启动 UI。
登记会产生独立的 manifest 物化记录，这是正常现象。

默认 runtime 是 `local`，监听 `0.0.0.0:3000`，状态保存在项目 `.dagster_home/`，产物位于
`.artifacts/`。通过白泽提供的 3000 端口访问入口打开 UI；若做了宿主机端口映射，应使用
宿主机对应地址和端口，浏览器中的 `localhost` 指浏览器所在机器。

若修改了 Case 的 qfile 或模型配置，可停止后 render，再运行 `run.sh` 自动重新登记。
只把已运行服务的登记更新到新配置时，也可以在第二个容器终端执行：

```bash
(
  set -euo pipefail
  cd /debug/data_engineering
  export PATH="$PWD/.deps/uv:$PATH"
  export DAGSTER_PIPELINE_RUNTIME_ROOT="$PWD"
  uv run dagster-pipeline render-configs
  runtime_exports=$(uv run dagster-pipeline runtime-env)
  eval "$runtime_exports"
  printf 'DAGSTER_HOME=%s\n' "$DAGSTER_HOME"
  uv run dagster-pipeline register-case --config build/config/dagster/cases.yaml
)
```

`runtime-env` 确保手动登记与服务使用相同 `DAGSTER_HOME` 和产物根；不要在另一空实例登记后
期待当前 UI 自动出现 partition。列表中移除一个 Case 不等于删除其历史 partition 或产物。
已有运行执行期间不要改写它正在使用的配置；修改 runtime 或资产版本后停止并重启服务。

## 7. 在 UI 物化 `production_job`

1. 打开 Dagster UI，进入项目 code location 的 Jobs，选择 `production_job`。
2. 进入 Launchpad，选择 partition：
   `cnwxijk-qthd-58-2026-07-29T101300.000Z-2026-07-29T101700.000Z`。
3. 首次执行使用默认 Run Config `{}`，不把算法参数粘贴到 Launchpad。
4. 点击 Launch Run，进入该 Run 详情，查看依赖图、各 Step 状态及日志。
5. 等待 Run 成功，并查看资产物化记录和最终检查；若失败，先定位第一个失败 Step。

生产链大致如下，实际并行顺序由依赖和并发池决定：

```text
已登记 case_manifest → qfile_snapshot → decoded_sensor_data → nuscenes_dataset
                         ├→ gt_point_cloud_snapshot ─┐
                         └→ gt_vector_labels_snapshot┤
case_manifest + nuscenes_dataset + 两种 GT → sensor_calibration → localization
case_manifest + nuscenes_dataset → scene_camera_masks（逐 Scene）
NuScenes + 标定 + 定位 + masks → case_raster_map → scene_raster_maps（逐 Scene）
→ scene_vector_maps（占位）→ case_vector_map（占位）→ vector_map_export（占位）
```

逐 Scene 节点会显示动态展开的 Step；集合节点等待预期 Scene 全部成功后发布索引。
绿色矢量节点表示协议执行和发布完成，当前不会生成矢量地图文件。
首次执行后，相同版本再次启动生产会复用成功缓存；它不等于每次重新跑所有算法。

## 8. 看日志和结构化失败

先看 UI 的 Run → 失败 Step，阅读事件中的错误及该 Step 的 stdout/stderr。算法输出由
编排捕获，启动终端同时包含服务、注册和代码加载日志；UI 适合按 Run、Step 定位。
宿主机或白泽平台自己的容器日志属于另一层，不替代资产执行记录。

容器内跟踪保存的启动日志、查找本 Case 失败文件：

```bash
cd /debug/data_engineering
ls -lt build/logs
# 以下选择最近一次保存的服务日志。
LATEST_LOG=$(ls -t build/logs/dagster-*.log | head -n 1)
tail -n 100 -f "$LATEST_LOG"
```

`Ctrl+C` 只结束这里的 `tail`。在另一个终端检查产物：

```bash
(
  set -euo pipefail
  cd /debug/data_engineering
  export PATH="$PWD/.deps/uv:$PATH"
  export DAGSTER_PIPELINE_RUNTIME_ROOT="$PWD"
  runtime_exports=$(uv run dagster-pipeline runtime-env)
  eval "$runtime_exports"
  CASE_ID=cnwxijk-qthd-58-2026-07-29T101300.000Z-2026-07-29T101700.000Z
  CASE_ARTIFACT="$DAGSTER_PIPELINE_ARTIFACT_ROOT/$CASE_ID"
  find "$CASE_ARTIFACT" -name _failure.json -print

  uv run python -m json.tool \
    "$CASE_ARTIFACT/ingestion/nuscenes_dataset/v1.0/_request.json"
)
```

普通算法执行目录内的文件含义：

| 文件 | 阅读目的 |
| --- | --- |
| `_request.json` | 确认执行身份、所用 `config_path`、输入引用和输出路径 |
| `_result.json` | 看算法 `status`、`error`、metrics 和 metadata；启动失败时可能未生成 |
| `_failure.json` | 看编排保存的失败原因和进程诊断，按实际结构追踪关联文件 |
| `_SUCCESS.json` | 当前版本及上游版本的成功标记，用于缓存判断 |
| `payload/` | 实际算法产物，失败时可能留有诊断文件，不能据目录存在判断成功 |

将上例文件名换为 `_result.json` 或 `_failure.json`，即可使用 `python -m json.tool`
格式化阅读。不要只看最终下游“输入缺失”：上游失败才是根因。成功重算会更新当前目录的
状态，历史执行过程仍以 Dagster Run 记录为准。

## 9. 查看生成结果

默认数据版本为 `v1.0`。未修改 runtime 时，目录位于
`/debug/data_engineering/.artifacts/<完整 Case ID>/`：

```text
ingestion/
  case_manifest/v1.0/payload/case.json
  qfile_snapshot/v1.0/payload/...
  decoded_sensor_data/v1.0/payload/decode_manifest.json
  nuscenes_dataset/v1.0/payload/
    v1.0/                         # NuScenes 标准 JSON 表
    ...                           # 表中 filename 引用的媒体
ground_truth/
  gt_point_cloud_snapshot/v1.0/payload/map.pcd
  gt_vector_labels_snapshot/v1.0/payload/map.osm（或 map.json）
calib_localization/
  sensor_calibration/v1.0/payload/calibrated_sensor.json
  sensor_calibration/v1.0/payload/calibration_report.json
  localization/v1.0/payload/
    ego_pose.json
    lidar_undistort.json
    data/<scene>/lidar_undistort/<channel>/*.pcd
segmentation_production/scene_camera_masks/v1.0/
  payload/index.json
  <scene>/payload/mask.json
  <scene>/payload/masks/...
  <scene>/payload/validation/...
raster_map/
  case_raster_map/v1.0/payload/
    case_map.json
    texture_map.png / intensity_map_viz.png / semantic_map.png
    report.json / report.html
    raw_tiles/...
  scene_raster_maps/v1.0/
    payload/index.json
    <scene>/payload/...
vector_map/...
```

NuScenes 标准表包括 `scene.json`、`sample.json`、`sample_data.json`、`sensor.json`、
`calibrated_sensor.json` 和 `ego_pose.json`。先从 `scene.json` 确认实际 Scene 数量，
不要根据 Case 时长直接推断数量。

标定透传状态查看 `calibration_report.json` 及该资产 `_result.json` 的
`metadata.passthrough`；透传只保留原始标定，不执行几何求解。定位 `passthrough: true`
仍生成去畸变点云，沿用原轨迹，不执行常规优化；默认 `map_alignment_enabled: false`。

分割优先浏览 `validation/` 下的原图叠加图，再检查 `validation_report.json`。
栅格优先查看 `report.html`、`texture_map.png` 和 `intensity_map_viz.png`；原始数值由
`case_map.json` 及 `raw_tiles/` 表达。报告文件是算法内部产物，不是独立 Dagster 资产。
Scene 集合 `payload/index.json` 记录相对索引及子引用，子目录是实际 Scene token，
不要把集合 payload 当作某个 Scene 的地图。

## 10. 修改算法配置

编辑源文件中的对应字段，保留顶部 Hydra 声明和其余参数，不要用下列片段整文件替换。
修改后统一 render；已有成功缓存还需按下一节重算。

| 需求 | 源配置 | 常用参数 |
| --- | --- | --- |
| 公共传感器布局 | `config/algorithm/sensor_hardware_7v.yaml` 或 `sensor_hardware_11v.yaml` | `camera_channels`、`lidar_channels`、`ref_channel`、`intensity_ring_allowlist` |
| 解码工具及独立通道 | `config/algorithm/ingestion/decoded_sensor_data.yaml` | `dataconvert`、局部通道覆盖 |
| NuScenes 通道、时间和 Scene | `config/algorithm/ingestion/nuscenes_dataset.yaml` | 见下文 |
| 标定 | `config/algorithm/calib_localization/sensor_calibration.yaml` | `passthrough`、求解窗口及迭代参数 |
| 定位与去畸变 | `config/algorithm/calib_localization/localization.yaml` | `passthrough`、`map_alignment_enabled`、`undistort_workers` |
| 分割 | `config/algorithm/segmentation/scene_camera_masks.yaml` | `classes`、`image_resolution`、`confidence_threshold` |
| Case 栅格建图 | `config/algorithm/raster_map/case_raster_map.yaml` | `resolution_m`、通道、强度显示和过滤 |
| Scene 地图裁剪 | `config/algorithm/raster_map/scene_raster_maps.yaml` | `top_m`、`bottom_m`、`left_m`、`right_m` |

默认 7v 公共相机为 `camera_0`、`camera_1`、`camera_6`、`camera_7`；LiDAR 为
`front_left_up`、`front_right_up`、`front_left_down`、`front_right_down`、`front_top`，
参考通道为 `front_left_up`。11v 的参考通道为 `top_down`，LiDAR 为 `front_left`、
`front_right`、`rear_left`、`rear_right`、`top_down`，相机为 `camera_0`、`camera_1`、
`camera_7`、`camera_9`。布局名不代表相机数量。
render 先选公共布局，再应用资产局部覆盖；栅格地图也继承所选相机。修改公共配置会影响整个链路。

NuScenes 可以独立选择解码结果的子集，例如在自身 YAML 中增加或调整：

```yaml
camera_channels: [camera_0, camera_1]
lidar_channels: [front_left_up, front_right_up]
ref_channel: front_left_up
start_time: null
end_time: null
frequency: 5.0
start_frame_index: 1
scene_duration: 20.0
scene_distance: 20.0
```

`decode_manifest.json` 只用于核对可用通道，不能替 NuScenes 决定选择。
未选通道不检查外参、媒体或同步；选中通道必须已解码，参考通道必须在选择内。
缩小 NuScenes 通道后，标定、定位、分割、栅格地图等下游的选择也必须匹配可用数据；
仅修改 NuScenes 不会自动改写其他算法的配置。若仍保留默认四相机，下游不能消费只有两相机的数据。

时间是**绝对 Unix 秒**，支持小数，包含两端。两项为空时不裁剪，只填一项表示单侧限制；
“全量”指不按时间裁剪，仍会按 `frequency` 采样。该 Case 的 `Z` 表示 UTC，例如：

```yaml
# 2026-07-29 10:13:30 UTC 至 10:14:30 UTC，共一分钟。
start_time: 1785320010.0
end_time: 1785320070.0
```

转换先筛选所有选中通道的时间范围，再从 `start_frame_index` 开始同步、采样并切 Scene。
默认参考时间跳变阈值 `main_sensor_jump_time: 0.3` 秒，同步阈值 `sync_time: 0.1` 秒。
Scene 在时长和累计平面路程**同时严格超过** `20` 秒、`20` 米时结束；不足的尾段并入前一
Scene，整段不足则保留一个 Scene。因此低速或静止片段不会每 20 秒机械切分。

标定与定位 YAML 各自的 `passthrough: true` 默认开启。关闭标定透传才会进行实际求解，
关闭定位透传才会进入常规优化；两者是独立开关，各自配置如下：

```yaml
passthrough: true
```

需要定位地图配准时，在定位 YAML 同时设置 `passthrough: false`、
`map_alignment_enabled: true` 和 `reference_map_path: null`。配准使用 GT 快照
`map.pcd`，不允许外部地图覆盖，是否接受配准以实际结果为准。

栅格默认 `resolution_m: 0.02` 米、所选布局的四路相机、`intensity_viz_gamma: 0.6`。
ring 白名单也随布局选择：7v 的 `front_left_up`、`front_right_up` 保留 0～120，其他通道
全部保留；11v 默认 `{}`，全部保留。不要把 7v 通道名的规则复制到 11v。
在 `case_raster_map.yaml` 中填写 `intensity_ring_allowlist` 会整块替换车型规则，
显式 `{}` 可关闭筛选。缩小地图 LiDAR 列表时也要清空或重设规则，
已移除通道的规则不会被自动过滤，会由算法报错。CLAHE 保持地图源文件设置。Scene 默认上下左右各 `15.0` 米。降低分辨率值会增加内存和磁盘需求。
例如要将 Case 地图分辨率改为 4 厘米，在 `case_raster_map.yaml` 修改：

```yaml
resolution_m: 0.04
```

要扩大 Scene 裁剪范围，在 `scene_raster_maps.yaml` 修改，以下单位均为米：

```yaml
top_m: 20.0
bottom_m: 20.0
left_m: 20.0
right_m: 20.0
```

调参含义及完整配置见[配置说明](configuration.md)和[栅格算法说明](../src/algorithm/raster_map/case_raster_map/README.md)。

## 11. 配置改动后正确重跑

缓存比较显式资产版本及祖先版本，**不会对 YAML、模型或源文件变化自动做内容检测**。
只 render、重新登记，或在 UI 再点一次 Launch，不保证重新执行已成功算法。

长期发布不同结果时，在源 `config/dagster/assets.yaml` 维护受影响资产的 `data_version`，
再 render 并运行该资产及其下游。版本变化会进入下游祖先版本清单；初始版本为 `v1.0`。
只有 NuScenes 局部时间或通道改变且原解码数据满足要求时，可保留解码版本和缓存，
维护 NuScenes 版本并执行后续链路；改变实际解码选择则需维护解码版本。

临时同版本重算，在 Launchpad 配置 `force_recompute`。普通资产示例：

```yaml
ops:
  localization:
    config:
      force_recompute: true
```

Scene 资产是 graph，需要嵌套到真正执行算法的 op：

```yaml
ops:
  scene_raster_maps_graph:
    ops:
      scene_raster_maps:
        config:
          force_recompute: true
```

`force_recompute` 只作用于指定操作，不自动强制下游。同版本强制重算会更新该资产自己的
payload 和状态，Scene 集合重建保留子目录。要更新整条受影响链路，应逐个强制下游，
或使用前述显式版本方式。模型路径变化还需 render 后重新登记 manifest，再重算实际消费
该模型的资产及其下游；仅改 `model_version` 不自动失效。

以本 Case **只改时间范围**为例，保留所有通道及默认版本，完成一次调参与重跑：

1. 等当前 Run 结束，在第二个容器终端初始化环境并停止服务：

```bash
cd /debug/data_engineering
export PATH="$PWD/.deps/uv:$PATH"
export DAGSTER_PIPELINE_RUNTIME_ROOT="$PWD"
./script/stop.sh
```

2. 编辑源 `config/algorithm/ingestion/nuscenes_dataset.yaml`，将时间改为上面的两个秒值。
3. 在项目根执行 `uv run dagster-pipeline render-configs`，检查生成 YAML 的时间是否正确。
4. 用第 6 节命令重新启动服务，选择同一 `production_job` 和完整 Case partition。
5. 在 Launchpad 使用以下配置，重算 NuScenes 及全部受影响下游，复用 qfile、解码与 GT：

```yaml
ops:
  nuscenes_dataset:
    config:
      force_recompute: true
  sensor_calibration:
    config:
      force_recompute: true
  localization:
    config:
      force_recompute: true
  scene_camera_masks_graph:
    ops:
      scene_camera_masks:
        config:
          force_recompute: true
  case_raster_map:
    config:
      force_recompute: true
  scene_raster_maps_graph:
    ops:
      scene_raster_maps:
        config:
          force_recompute: true
  scene_vector_maps_graph:
    ops:
      scene_vector_maps:
        config:
          force_recompute: true
  case_vector_map:
    config:
      force_recompute: true
  vector_map_export:
    config:
      force_recompute: true
```

6. 启动后确认这些 Step 实际执行；检查新 NuScenes 的 `sample.json` 时间戳及 `scene.json`，
   再检查分割集合、定位和地图。NuScenes 标准表时间戳为微秒，核对时换算为秒；算法配置仍用秒。

失败执行没有可复用的成功缓存。修复失败原因后可重新启动生产，之前已成功且未受影响的
上游会被复用。若同时改动成功节点的输入或参数，应按本节显式重算，不能仅依赖失败重试。

## 12. 停止、再次启动和常见问题

在第二个容器终端执行，使用与启动时一致的 runtime 根：

```bash
cd /debug/data_engineering
export DAGSTER_PIPELINE_RUNTIME_ROOT="$PWD"
./script/stop.sh

# 下次运行时再次执行；不会自动重新 render。
./script/run.sh
```

停止服务保留 Dagster 状态、产物和已渲染配置。运行中的任务先在 UI 确认状态并按需终止，
避免把停服务当作任务正常完成。不要用 `reset.sh` 处理普通失败：它会清理实例状态、业务
产物和 IO 记录，是明确需要清空环境时的操作。

| 现象 | 先检查及处理 |
| --- | --- |
| `info.json` 不存在 | 核对宿主/容器挂载、Case 源路径及 rsync 尾部斜杠，`qfile_uris` 必须指向 Case 根 |
| UI 没有 partition | 看启动登记是否成功、生成 cases 是否正确，以及手动登记的 `DAGSTER_HOME` 是否一致 |
| 修改源 YAML 没生效 | 检查 `build/config/` 和 `_request.json.config_path`，先 render，再处理成功缓存 |
| native 程序不可用 | 查看 install 日志末尾失败原因，确认三个 `build/<程序>/<程序>` 可执行；先完成安装 |
| GPU 不可见或分割 OOM | 容器执行 `nvidia-smi`；核对 GPU 挂载、权重和实际依赖，按需降低分割分辨率或 runtime GPU 并发后 render/restart |
| `CALIBRATION_INVALID`、外参为空 | 在失败节点确认选中通道及解码 `profile.yaml`；修复选中通道标定，或在整条下游配置中移除不用的通道 |
| 所选通道未解码 | NuScenes 选择必须是解码选择子集；若需新增通道，改解码配置并重算解码及下游 |
| `TIME_RANGE_EMPTY` 或同步失败 | 确认使用 UTC Unix 秒而非毫秒/相对秒，时间范围有参考帧，所选其他通道也有可同步媒体 |
| GT 下载失败 | 看对应 GT Step 的 `_failure.json`，核对 `info.json` 固定地图记录、资源权限、网络和 `sudo -n qomolo-get`，不要绕过 GT 依赖 |
| 已改模型但仍用旧结果 | render、重新登记；维护消费资产版本，或对消费资产及其下游逐一 force |
| 已有实例在运行 | 使用同一根目录执行 `stop.sh`；不要并发启动两个服务共享该实例 |
| 矢量入口成功但没有地图 | 当前占位实现的预期行为，实际栅格结果在 `raster_map/` 下 |

进一步参考[配置说明](configuration.md)、[编排与缓存](../src/dagster_pipeline/README.md)及
[算法接口](algorithm-contract.md)。本教程中的宿主挂载、GPU、资源权限和数据质量需要在实际
部署环境确认；文档检查不替代该 Case 在目标机器上的完整生产验证。
