# 算法接口

## 公共类型

所有算法输入使用同一种引用，输入键使用真实上游资产名，值统一为引用列表：

```python
@dataclass
class ArtifactRef:
    asset_name: str
    uri: str
    data_version: str
    case_id: str
    scene_id: str | None = None
    metadata: dict = field(default_factory=dict)

@dataclass
class AssetRequest:
    asset_name: str
    case_id: str
    data_version: str
    output_uri: str
    config_path: str
    scene_id: str | None = None
    inputs: dict[str, list[ArtifactRef]] = field(default_factory=dict)
    protocol_version: str = "v1.0"
```

受管 uri 和 output_uri 始终指向资产的 payload/ 目录，asset_name 是实际生产资产。
qfile 地址和本地模型路径保存在 `case_manifest` 的业务 JSON 中；所有资产输出均为真实 payload 引用。公共协议使用生成的 dataclass，
JSON 直接读写，读取时恢复嵌套输入引用。`inputs` 必须是字典，key 必须为非空字符串，
value 必须是列表，列表内每个元素必须是 `ArtifactRef`，且 `ref.asset_name == key`。
通过 `validate(model)` 显式校验内存模型，读取、写入和执行边界自动校验这一形状。协议不设置资产注册表白名单或全局 Case 限制；
跨 Case、Scene 和文件完整性由消费算法按业务要求检查。
公共类型和请求/结果 JSON 读写位于独立的 `asset_protocol` 协议层；Python 使用 json/jsonschema，
C++17 使用 nlohmann/json/Valijson，链接 `asset_protocol::asset_protocol`。算法和编排都直接依赖协议层，
协议层不依赖算法或 Dagster。Dagster 只传递 `ArtifactRef` 和内部调度所需的基础 JSON 值；
缓存、成功标记及进程诊断仍由编排维护。
IOManager 的编排类型恢复及路径迁移位于 `dagster_pipeline.orchestration.serialization`。
算法参数由 Hydra 源配置合成并安装为普通 YAML，请求只传安装后的 config_path。
协议层不解析配置内容或业务文件，也不负责 Dagster 的 `_SUCCESS.json`、`_failure.json`。
Python/C++ 实现集中在 `src/protocol/`，
完整请求 JSON、输入引用读取、结果写入与身份核对见
[协议层接入说明](../src/protocol/README.md#一次调用如何传递输入与输出)。
公共类型通过 quicktype 自动生成；固定字段只维护 Schema，运行 `uv run python script/generate_protocol.py`
重新生成 Python/C++ 协议模型，使用字段的业务逻辑按语义变化调整。metadata、metrics、details 保持开放 JSON 对象，
增加业务键无需生成。读写使用 `read_request`、`write_request`、`read_result`、`write_result`，
转换使用 `from_dict(Model, value)`、`to_dict(model)`，模型自身不含手写字段解析。
公共字段规范以 [JSON Schema](../src/protocol/schema/asset_protocol.v1.schema.json) 为准；
Python 与 C++ 使用同一份 [契约样例](../src/protocol/fixtures/contract.json) 验证接受与拒绝行为。
输入 key 与引用资产名一致、请求与结果身份一致等关系约束由两端实现校验。
算法入口不导入 Dagster 或 Dagster Pipes；编排侧可以使用 Dagster Pipes 启动算法，
但 Pipes、缓存和进程诊断不进入算法协议依赖。
两种语言拒绝错类型、未知协议字段、重复 key、非有限数、非法 UTF-8 和尾随 JSON；
业务 metadata、metrics、details 的字段自由定义。

单个输入写成 `[ref]`，`input_ref(request, asset_name)` 要求对应列表恰好有一个元素。
多个 Scene 的同类产物放在同一个 key 下，`input_refs` 保留列表顺序。空 Scene 汇总用 `[]`，
它表示已汇总但没有产物；缺少 key 表示没有提供该输入，不能用 `[]` 代替缺失的必需单输入。
可选源未提供时省略 key，不写 `null`。不同生产者使用不同 key，标定、定位和栅格地图使用统一资产名。
Scene 身份放在请求及引用的 `scene_id` 中，Case 身份放在 `case_id` 中，不拼接进 key。
Scene 集合本身也返回 `ArtifactRef`，其 `payload/index.json` 保存子引用列表。
编排读取并校验该索引后，将子引用按 Scene ID 顺序组装为算法输入列表。

## 18 个执行资产的输入映射

下表中 `[ref]` 是单引用列表，`[scene_ref, ...]` 是保持收集顺序的场景引用列表。
每个引用的 `asset_name` 都等于该行对应的 key，URI、版本、Case 与 Scene 身份保留生产者值。
下表是编排发送的依赖映射；五个矢量入口保留这些依赖，但当前算法不读取或检查业务输入。

| 执行资产 | inputs 的 key → value |
| --- | --- |
| `qfile_snapshot` | `case_manifest` → `[ref]`，算法从 `case.json` 读取 qfile 地址 |
| `decoded_sensor_data` | `qfile_snapshot` → `[ref]` |
| `nuscenes_dataset` | `decoded_sensor_data` → `[ref]` |
| `gt_point_cloud_snapshot` | `qfile_snapshot` → `[ref]` |
| `gt_vector_labels_snapshot` | `qfile_snapshot` → `[ref]` |
| `sensor_calibration` | `nuscenes_dataset`、`gt_point_cloud_snapshot`、`gt_vector_labels_snapshot`、`case_manifest` → 各 `[ref]` |
| `localization` | `nuscenes_dataset`、`sensor_calibration`、`gt_point_cloud_snapshot`、`gt_vector_labels_snapshot` → 各 `[ref]` |
| `scene_camera_masks` | `nuscenes_dataset`、`case_manifest` → 各 `[ref]` |
| `case_raster_map` | `nuscenes_dataset`、`sensor_calibration`、`localization` → 各 `[ref]`；`scene_camera_masks` → `[scene_ref, ...]` |
| `scene_raster_maps` | `case_raster_map` → `[ref]` |
| `scene_vector_maps` | `scene_raster_maps` → `[当前 Scene 的 ref]`；`case_manifest` → `[ref]` |
| `case_vector_map` | `scene_vector_maps` → `[scene_ref, ...]` |
| `vector_map_export` | `case_vector_map` → `[ref]` |
| `refined_segmentation_labels` | `nuscenes_dataset` → `[ref]`；`scene_camera_masks` → `[scene_ref, ...]` |
| `segmentation_training_dataset` | `nuscenes_dataset`、`refined_segmentation_labels` → 各 `[ref]` |
| `segmentation_model_checkpoint` | `case_manifest`、`segmentation_training_dataset` → 各 `[ref]` |
| `vector_map_training_dataset` | `scene_raster_maps` → `[scene_ref, ...]`；`gt_vector_labels_snapshot` → `[ref]` |
| `vector_map_model_checkpoint` | `vector_map_training_dataset` → `[ref]` |

`case_manifest` 是本地发布资产，引用版本为 `v1.0`、`scene_id=null`，其 payload 包含
`case.json`：`case_id`、单个 `qfile_uri`、`segmentation_model`、`vector_model` 和可选的
`sensor_calibration_models`。模型配置使用 `model_uri`、`model_version`、`source` 等业务字段，不含模型摘要。
源 Case 配置必填本地资源根 `data_engineering_resource`，各 `model_uri` 必须为根内非空相对路径，
不接受绝对路径、URI、`~` 或 `..`。render 将根转绝对路径、保留相对模型路径，
登记时才拼出绝对 `model_uri` 写入 manifest；算法接口继续读取绝对本地路径。
实际使用模型的算法只读取本地文件或本地 payload，并检查 manifest Case 身份、路径可读性及必要结构。
当前矢量占位入口不读取 manifest 中的 `vector_model`。
模型 payload 的 `model.json.checkpoint` 必须为留在 payload 内的安全相对文件路径。
纯 LiDAR 标定可以省略标定模型配置，但仍必须传入 manifest。
两个 GT 快照读取受管 qfile 的 `info.json`；录制地图缺失或无有效唯一 `main` 时明确失败。
Case 发布和 Scene 发出、收集使用本地编排，不增加上述 18 个算法执行规格。
验证报告属于生产算法的 payload，不创建独立请求、资产、版本目录或配置。

## 执行身份与目录

资产 key、生产 op、assets.yaml 注册表键和协议 asset_name 使用同一个名称。
execute_asset 从 Dagster context 获取名称和 AssetGroup 分组，底层执行器单独接收
普通字符串 group_name；分组不写入算法请求，也不需要算法导入 Dagster 定义。
graph 容器名称为 `<asset_name>_graph`，emit、collect 是辅助步骤。

```text
artifact_root/<case>/<group>/<asset>/<data_version>/[scene]/
├── _request.json
├── _result.json
├── _SUCCESS.json   # 仅成功执行
├── _failure.json   # 编排层记录当前失败
└── payload/

manifest_root/<case>/<group>/<asset>.json
```

协议和 manifest 发布版本为 `v1.0`，项目包版本为 `1.0.0`。
各资产的初始 `code_version` 和 `data_version` 统一为 `v1.0`，后续按资产独立维护。
各版本字段含义独立，数据版本由资产注册表维护；协议读取要求 `protocol_version` 为 `v1.0`。

`_request.json`、`_result.json` 是协议层的算法通信协议；`_SUCCESS.json` 和 `_failure.json`
由编排层管理，算法不负责构造它们。成功标记、版本缓存、执行锁与产物复用的完整规则见
[Dagster 缓存说明](../src/dagster_pipeline/README.md#缓存规则)。

## 定位请求示例

```json
{
  "protocol_version": "v1.0",
  "asset_name": "localization",
  "case_id": "case.qfile",
  "scene_id": null,
  "data_version": "v1.0",
  "output_uri": "/srv/.artifacts/case.qfile/calib_localization/localization/v1.0/payload",
  "config_path": "/srv/build/config/algorithm/calib_localization/localization.yaml",
  "inputs": {
    "nuscenes_dataset": [{
      "asset_name": "nuscenes_dataset",
      "uri": "/srv/.artifacts/case.qfile/ingestion/nuscenes_dataset/v1.0/payload",
      "data_version": "v1.0",
      "case_id": "case.qfile",
      "scene_id": null
    }],
    "sensor_calibration": [{
      "asset_name": "sensor_calibration",
      "uri": "/srv/.artifacts/case.qfile/calib_localization/sensor_calibration/v1.0/payload",
      "data_version": "v1.0",
      "case_id": "case.qfile",
      "scene_id": null
    }],
    "gt_point_cloud_snapshot": [{
      "asset_name": "gt_point_cloud_snapshot",
      "uri": "/srv/.artifacts/case.qfile/ground_truth/gt_point_cloud_snapshot/v1.0/payload",
      "data_version": "v1.0",
      "case_id": "case.qfile",
      "scene_id": null
    }],
    "gt_vector_labels_snapshot": [{
      "asset_name": "gt_vector_labels_snapshot",
      "uri": "/srv/.artifacts/case.qfile/ground_truth/gt_vector_labels_snapshot/v1.0/payload",
      "data_version": "v1.0",
      "case_id": "case.qfile",
      "scene_id": null
    }]
  }
}
```

NuScenes 的 dataroot 是引用 uri，标准表目录是 uri/data_version。
该例读取 ingestion/nuscenes_dataset/v1.0/payload/v1.0，定位自身输出版本不影响上游定位。

`sensor_calibration` 使用 native sensor_calibration；`localization` 使用 native Localization。
两者都必须传入 `gt_point_cloud_snapshot`、`gt_vector_labels_snapshot` 两个单引用列表，
任一缺失就失败。生产和地图训练共用标定、定位、Case 地图与 Scene 裁剪产物。
标定与定位默认均为 `passthrough: true`。标定读取 NuScenes 的 `calibrated_sensor`、`sensor` 两表，
将原始标定记录及其字段值原样保留在输出 `calibrated_sensor.json` 的 `records` 中；继续校验输入身份、
目录和两表关联，跳过帧读取、窗口选择、LiDAR/相机求解、相机模型和 backend 加载。
标定业务报告和结果 `metadata.passthrough` 以布尔值记录模式，metrics 同步记录；设置为 `false` 才执行实际外参求解。
定位透传使用 NuScenes 原始 `ego_pose` 做点云运动去畸变，并保留源定位数值、跳过优化，
结果 `metadata.passthrough` 同样记录布尔值。
定位的地图配准由最终 YAML 中的 `map_alignment_enabled` 控制，必须同时关闭 passthrough。
`localization` 自动读取 GT PCD 引用下的 `map.pcd`，`reference_map_path` 保持 null，
拒绝外部地图路径。GT vector labels 尚未参与几何对齐。
标定与定位各自记录自己的对齐状态，不能将某一资产的状态当作整条 GT 链的状态。

## 结果和产物

```json
{
  "protocol_version": "v1.0",
  "asset_name": "localization",
  "case_id": "case.qfile",
  "scene_id": null,
  "data_version": "v1.0",
  "status": "success",
  "error": null,
  "metadata": {"passthrough": true},
  "metrics": {
    "passthrough": 1,
    "alignment_requested": 1,
    "alignment_applied": 0,
    "map_alignment_enabled": 0,
    "map_alignment_scene_count": 0
  }
}
```

结果包含执行身份、`status`、`error`、metrics 和 metadata，不登记 outputs。成功结果必须为
正常进程退出、合法的 `status=success` 且 `error=null`，同时通过 payload 目录和上游版本记录
检查，编排才会发布 `_SUCCESS.json`。失败结果必须提供结构化 `AssetError`，并由编排在当前执行
目录原子写入 `_failure.json`；失败永远不发布成功标记。错误类别固定为 `input`、`resource`、
`algorithm`、`process`、`protocol`。仅观察到 SIGKILL 时记录 process，不能推断 OOM。
算法负责内部文件和业务完整性。metadata.scene_ids 用于动态 Scene 分发，
metadata.model 可发布模型名称、版本、来源及实际状态，不包含摘要；内部 checkpoint 路径保存在模型 payload 的 `model.json`。

失败结果保留相同执行身份，以下为 `status` / `error` 字段示例：

```json
{
  "status": "failed",
  "error": {
    "category": "input",
    "code": "INPUT_NOT_FOUND",
    "message": "必需的输入文件不存在",
    "details": {"asset_name": "decoded_sensor_data"}
  }
}
```

`code`、`message` 必须为非空字符串，`details` 必须为 JSON 对象。数值 metrics 和
StageStats 保持原用途，状态和错误使用独立字段。进程非零退出时，即使残留成功 result
也不能发布成功标记；合法 failed 结果即使进程退出码为 0 也失败。

| 进程结束 | 结果 | 编排判定 |
| --- | --- | --- |
| 正常 | 合法 success | payload / 上游版本记录通过后发布成功标记 |
| 正常 | 合法 failed | 保留算法的结构化错误 |
| 异常 | 合法 failed | 保留算法错误，补充 process 信息 |
| 异常 | 合法 success | `process / PROCESS_FAILED` |
| 正常 | 缺失或无效 | `protocol / RESULT_MISSING` 或 `RESULT_INVALID` |
| 异常 | 缺失或无效 | `process / PROCESS_FAILED` |

进程启动失败使用 `process / PROCESS_START_FAILED`。无效结果包括协议、JSON、状态、错误
结构或执行身份不合法；请求无效则拒绝调用算法，不伪造结果。

| 类别 | 含义 |
| --- | --- |
| `input` | 缺少输入、业务输入条件或受管上游版本记录无效 |
| `resource` | 明确的资源失败、payload 目录或成功标记发布失败 |
| `algorithm` | 算法业务异常或算法主动报告失败 |
| `process` | 进程启动、异常退出或信号终止 |
| `protocol` | 正常退出后的结果缺失、内容无效或执行身份不匹配 |

标定算法读取最终 YAML 的 ref_channel、lidar_channels、camera_channels，选中的传感器必须全部求解成功。
成功时在 payload 保存普通 JSON calibrated_sensor.json（records、alignment_requested、alignment_applied）及
calibration_report.json；完整保留输入记录及非外参字段，仅更新选中传感器的求解外参。
标定要求两份 GT 输入，但尚未执行 GT 几何对齐，alignment_applied 始终为 false。
失败时保留诊断报告并返回 `status: failed` 的协议结果，不发布有效 calibrated_sensor.json；成功标记由 Dagster 创建。
定位的业务文件统一放在 payload：

```text
calib_localization/localization/<version>/payload/
├── ego_pose.json
├── lidar_undistort.json
├── data/<scene>/lidar_undistort.json
├── data/<scene>/...
└── debug/...
```

根层 LiDAR 索引中的 Scene manifest 路径为
data/<scene>/lidar_undistort.json，相对于该索引所在目录解析。
根级索引与结果 metrics 记录 `map_alignment_enabled` 和接受配准的 `map_alignment_scene_count`。
`alignment_requested` 表示提供了必需的 GT 输入；`alignment_applied` 仅在至少一个 scene
接受配准时为真，不表示所有 scene 均已对齐。
开启时 scene 记录接受状态、原因和世界修正；GT labels 对齐仍未实现。详细参数、地图产品
与坐标约定见 [Localization 说明](../src/algorithm/calib_localization/localization/README.md)。

## Python 入口

```python
from algorithm.common.config import load_algorithm_config
from algorithm.common.protocol import AlgorithmResult, input_ref, run_algorithm
from asset_protocol import AssetRequest

def execute(request: AssetRequest, output_directory):
    parameters = load_algorithm_config(request)
    source = input_ref(request, "decoded_sensor_data")
    # 按 source.uri 读取数据并写入 output_directory。
    return AlgorithmResult(metrics={"count": 1})

if __name__ == "__main__":
    run_algorithm(execute)
```

`AlgorithmResult` 默认 `status="success"`、`error=None`。业务失败可返回
`AlgorithmResult(status="failed", error=AssetError(...))`，或抛出
`AlgorithmFailure(code, message, category="algorithm", details={})`。
入口捕获有效请求执行中的异常，原子写失败结果后重新抛出，保留完整 traceback 和非零退出码。
`AssetRequest` 从共享协议层导入，`AlgorithmResult` 从算法执行封装导入。

Case 栅格合成使用 `src/algorithm/raster_map/case_raster_map/` 中的 C++ 实现，
`case_raster_map` 协议入口产生生产和地图训练共用的结果。Scene 裁剪使用 Python
`algorithm.raster_map.scene_raster_maps`，Case 栅格验证在 native 合成程序内部完成。
两个入口按同一 `AssetRequest` / `AssetResult` 交换数据，配置分别登记为
`case_raster_map.yaml` 与 `scene_raster_maps.yaml`。
分割在每个 Scene 的 payload 写入 `validation_report.json`；Case 栅格地图在自身 payload
写入 `report.json` 与 `report.html`。报告路径和状态由同一次算法结果返回，文件完整性检查失败
会使所属资产执行失败。矢量生产与训练入口不生成报告或其他业务文件。

`scene_vector_maps`、`case_vector_map`、`vector_map_export`、`vector_map_training_dataset` 和
`vector_map_model_checkpoint` 的处理函数当前统一返回默认成功的 `AlgorithmResult()`。
它们不读取配置、模型或业务输入，不校验资产业务身份、地图或训练文件；不新增配置开关。
公共框架仍解析协议请求、创建输出 payload 并写入 `status="success"`、`error=null` 的结果，
metrics、metadata 均为 `{}`。非法协议请求或公共执行、发布失败仍按原有机制处理。
成功仅表示入口执行完成；现有资产名称、依赖、分组、版本和缓存机制保持不变。

地图业务入口先在完整 Case 时间顺序上按优化车辆位姿筛选 sample，默认上一保留帧的
世界 XY 位移至少 5cm；跨 Scene 延续，跳过帧不读取点云、图像和 mask。四个图层、
校正拟合和原始对照共用保留帧。Scene 中心仍按完整轨迹计算。

强度校正联合使用传感器、ring 与 `source_range_m`，默认每 Case 拟合 6 个对数距离
节点、8 轮 Huber，支持及留出检查不足时回退并记录原因。采集距离由定位去畸变前
保存，不从变换后 XYZ 反推；固定表要求显式硬件身份，算法自行解析表与 config。

最终 `intensity` 默认按 ring 中位数、帧内等权和 Huber 融合，点级 `intensity_p90`
保留作诊断。主预览显示 `intensity`，使用 P1/P99、gamma 0.6；
P90 诊断及原始对照复用基础显示范围，地图另外输出已启用的 CLAHE 增强图。
这些算法业务语义不改变协议层请求或结果字段，完整约束见
[地图算法说明](../src/algorithm/raster_map/case_raster_map/README.md)。

Python 使用 PyYAML，C++ 使用 yaml-cpp，算法无需调用 Hydra。
源配置位于 `config/algorithm/`，按 ingestion、calib_localization、segmentation、raster_map、vector_map 分类，
render 先选择 sensor_hardware_7v.yaml（默认）或 sensor_hardware_11v.yaml（`--vehicle 11v`），
将其中 `ref_channel`、`lidar_channels`、`camera_channels` 作为全部资产的默认值，
`intensity_ring_allowlist` 仅注入 `case_raster_map`。资产局部参数优先，
ring 规则局部覆盖整块替换，显式 `{}` 关闭筛选。缩小地图 LiDAR 选择须同步调整规则，
未知通道由算法拒绝，不自动过滤。7v 两个上方 LiDAR 保留 0～120，11v 默认全部保留。
scene_camera_masks、case_raster_map 额外继承 semantic_common.yaml，
提供语义类别 ID、名称和 RGB。每份资产自行保留 whitelist.enabled: false；当前未实现白名单，不控制执行。
运行 `dagster-pipeline render-configs` 将全部 defaults 和插值展开为普通 YAML，选中布局输出为
`algorithm/sensor_hardware.yaml`。选择不持久化；11v 每次 render 都须带 `--vehicle 11v`。
可选 bpe_path 的相对值在 render 时按源 YAML 所在目录展开为绝对路径。
Case 的相对 qfile 和 `data_engineering_resource` 在 render 时基于源 Case YAML 展开为绝对路径，
`model_uri` 仍相对资源根保存，登记时才拼成绝对路径；不检查模型文件存在、不复制资源。
配置树内部引用保持相对路径，安装后的资产登记用 `../algorithm/...` 引用普通 YAML。
全部默认参数写入 YAML，必需参数缺失时在使用处报错。
除五个直接成功的矢量占位入口外，各入口按自身业务要求检查输入并执行算法；执行失败返回非零退出码。
算法写 result，由编排检查并发布成功状态；数据版本维护和重算规则统一见
[Dagster 缓存说明](../src/dagster_pipeline/README.md#缓存规则)。
资产、op 和 job 不设置 Dagster 自动重试；手动重算清理当前诊断，不引入 attempt_id 或独立目录。
历史保存在 Dagster run 记录；`grpc_timeout_seconds` 是控制面超时，不限制算法运行时长。

排错先读取当前执行目录的 `_failure.json`，按 `identity` 确认 Case、资产、版本和 Scene，
查看 `error.code`、`message` 与 `details`，再到对应 Dagster run / step 查看完整 traceback。
摘要记录 UTC `time`、结构化 `error`、`process`、`run_id`、`step_key`；只写已知的进程退出码
和异常信息，未知信息保持缺失或 null。诊断摘要写入失败只记录日志，不覆盖原始失败原因。

```bash
uv run python -m algorithm.ingestion.nuscenes_dataset.cli \
  --request /absolute/request.json --result /absolute/result.json

uv run python -m algorithm.ingestion.ground_truth.gt_point_cloud_snapshot \
  --request /absolute/request.json --result /absolute/result.json

uv run python -m algorithm.ingestion.ground_truth.gt_vector_labels_snapshot \
  --request /absolute/request.json --result /absolute/result.json

build/localization/localization \
  --request /absolute/request.json --result /absolute/result.json
```

qfile 快照将源目录内容复制到 payload 根，源目录必须包含 `info.json`。
两个 GT 快照各有独立模块入口，共用录制地图解析与下载器，并校验各自的 `request.asset_name`。
输入为 `qfile_snapshot: [ref]`，只读取 `period_histories.maps` 的成功 `main`，
按 `project_name` 和存在的顶层 `ads_type` 过滤，使用完整 `main.name` 和精确版本。
下载缓存按模块与版本加锁，命令退出为 0 后仍校验文件与 SHA256。
GT payload 保存 `map.pcd`、`map.osm` 或 `map.json`，不做地图格式转换。
`resource_manifest.json` 保存 info SHA256、录制历史、模块/版本、包内源路径、地图 SHA256 和下载缓存命中情况。
PCD 先排除 `dynamic_elements` 目录下的文件；候选多于一个时，再排除文件名包含 `ground` 的文件，
忽略大小写，父目录名称不参与匹配。最终必须恰好剩余一个；唯一 ground PCD 正常使用。
筛后为空或仍有多个时，错误列出原始、排除及剩余候选的包内路径。
OSM 必须唯一，只有无 OSM 时才选择唯一原始 JSON 标签。
包必须通过 `metadata.json` 的所有 SHA256 校验，选中的地图必须被 checksum 清单覆盖且非空；
模块标签存在时严格校验，旧 OSM 包允许缺省模块标签文件。

qfile 解码将选定媒体、odom、原始 profile.yaml、info.json 及 `decode_manifest.json` 写入自己的 payload。
dataconvert 顺序执行 LiDAR、camera、odom 三次；空模态跳过，不传空 channel 参数。
清单保存 camera/LiDAR 列表、每通道非空文件数和每步骤耗时；缺少输出即失败。
NuScenes 从自己的最终 YAML 读取 `camera_channels`、`lidar_channels`，允许只消费解码集合的子集；
解码清单仅用于校验所选通道已解码及其文件数，不能覆盖 NuScenes 配置。
参考通道必须属于 NuScenes 所选集合。只为所选通道读取原始标定、检查媒体、同步、复制和生成表，
未选通道的空外参或媒体不影响转换；按所选模态要求数据目录，始终要求 odom。
`start_time`、`end_time` 为可选绝对 Unix 秒，与解码媒体文件名时间戳同单位；`null`、空串及
纯空白表示不限该边界。双空不裁剪，允许单边限制；非空值必须有限，且起始时间不大于终止时间。
先按闭区间筛选所有所选通道，再从 `start_frame_index`（默认 `1`）按 `frequency` 同步，
最后执行现有 Scene 切分。同步、媒体复制及 sample 不越过边界，无参考帧或同步缺帧时失败。
所选通道全量解码计数及非空检查仍执行，odom 保留全量用于插值；全量并不取消频率和 Scene 规则。
仅调整 NuScenes 通道或时间范围时可复用解码产物，按既有显式版本或逐节点 `force_recompute` 规则更新下游。

## 业务清单与模型

业务 JSON 直接描述实际数据。项目自有路径相对所在 manifest 目录，
跨资产引用可以使用 ..；NuScenes filename 相对 dataroot。
地图块和裁剪内部文件保持其局部目录约束。

case_map.json 保存栅格、稀疏块、PNG、展示编码和 Scene 中心，原始栅格标识为 `numpy-structured-raster-v1.0`。
map_meta.json 保存裁剪窗口、原始图层、主强度预览与 P90 诊断 PNG 路径，并沿用 Case 显示尺度。
栅格地图算法保留实际文件和几何检查；当前矢量占位入口不生成交付地图。
scene_camera_masks 汇总各场景的逐帧相机语义掩码；scene_raster_maps 汇总纹理、
高度、强度和语义栅格地图；scene_vector_maps 汇总各 Scene 的空 payload 引用。
这些集合使用同名 `ArtifactRef`，`scene_id=null`，其 `payload/index.json` 结构为
`{"artifacts": [完整子 ArtifactRef, ...]}`，子 URI 相对索引目录。子 Scene 自己的成功记录与 payload
保留在 `<asset>/<version>/<scene>/`，集合结果及成功记录保存在 `<asset>/<version>/`。
集合发布验证预期 Scene 完整且唯一、身份一致、子产物成功和祖先版本有效；重建集合不删除子目录。

分割训练将 model.ckpt 和 model.json 写入自己的 payload，后者包含模型名称、版本、安全相对 checkpoint 路径、
source 和实际状态，不计算内容摘要。算法在结果 metadata.model 中返回发布信息，编排统一返回
指向 payload 根的 `ArtifactRef`，并按普通资产流程发布成功标记。分割训练仍复制已有 checkpoint，
记录 `no-op-copy`，从 manifest 读取基础模型配置。
训练链分别止于 segmentation_model_checkpoint 和 vector_map_model_checkpoint。
两个矢量训练入口不生成 training_list.json、model.ckpt 或 model.json，普通 payload 为空；
公共编排仍检查它们声明的 GT 和栅格等上游资产状态。

manifest 不做内容哈希或版本拼接。模型路径、内容或配置中的模型版本变化本身不使缓存失效；
通过显式资产 data_version 及祖先版本传播，或逐一强制重算相关节点和下游更新结果。
缓存命中时不再打开模型文件。GT 包校验和地图校正业务已有摘要检查保持各自职责。

## 接入与发布

1. 在所属配置分类中编写算法 YAML，硬件默认值由 renderer 统一注入，不声明 /sensor_hardware；读取语义目录的算法通过 defaults 继承 semantic_common，再由最后的 _self_ 应用自身参数；whitelist.enabled: false 保留在资产 YAML 中。
2. 使用共享协议层实现扁平 `v1.0` 请求和结果入口。
3. 在 assets.yaml 登记入口、data_version 和 config_path。
4. 声明资产依赖，将 IOManager 恢复的引用按真实资产名组装为列表 inputs。
5. 运行 dagster-pipeline render-configs，安装最终配置。
6. 验证实际读写、独立输入版本、版本缓存及跨根目录搬迁。
7. 运行相关测试、native 自测和 dg check defs。
