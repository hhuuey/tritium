import dagster as dg
from dagster_pipeline.configuration import AssetExecutionConfig
from dagster_pipeline.defs.assets.common import AssetGroup, execute_asset
from dagster_pipeline.defs.partitions import case_partitions
from asset_protocol import ArtifactRef
from dagster_pipeline.orchestration.dagster_resources import AssetRunnerResource


@dg.asset(
    partitions_def=case_partitions,
    group_name=AssetGroup.CALIB_LOCALIZATION.value,
    kinds={"calibration", "json", "ground-truth"},
    code_version="v1.0",
    pool="gpu_calibration",
)
def sensor_calibration(
    context: dg.AssetExecutionContext,
    nuscenes_dataset: ArtifactRef,
    gt_point_cloud_snapshot: ArtifactRef,
    gt_vector_labels_snapshot: ArtifactRef,
    case_manifest: ArtifactRef,
    config: AssetExecutionConfig,
    asset_runner: AssetRunnerResource,
    pipes_subprocess_client: dg.PipesSubprocessClient,
) -> dg.Output[ArtifactRef]:
    """消费两种 GT 依赖并求解传感器外参；当前不应用 GT 对齐。"""
    inputs = {
        "nuscenes_dataset": [nuscenes_dataset],
        "gt_point_cloud_snapshot": [gt_point_cloud_snapshot],
        "gt_vector_labels_snapshot": [gt_vector_labels_snapshot],
        "case_manifest": [case_manifest],
    }
    return dg.Output(
        execute_asset(
            context=context,
            asset_runner=asset_runner,
            pipes_subprocess_client=pipes_subprocess_client,
            case_id=nuscenes_dataset.case_id,
            inputs=inputs,
            config=config,
        )
    )


@dg.asset(
    partitions_def=case_partitions,
    group_name=AssetGroup.CALIB_LOCALIZATION.value,
    kinds={"localization", "point-cloud", "ground-truth"},
    code_version="v1.0",
    pool="cpu_heavy",
)
def localization(
    context: dg.AssetExecutionContext,
    nuscenes_dataset: ArtifactRef,
    sensor_calibration: ArtifactRef,
    gt_point_cloud_snapshot: ArtifactRef,
    gt_vector_labels_snapshot: ArtifactRef,
    config: AssetExecutionConfig,
    asset_runner: AssetRunnerResource,
    pipes_subprocess_client: dg.PipesSubprocessClient,
) -> dg.Output[ArtifactRef]:
    """消费标定与两种 GT 输入，保存优化位姿和各 scene 的去畸变点云。"""
    return dg.Output(
        execute_asset(
            context=context,
            asset_runner=asset_runner,
            pipes_subprocess_client=pipes_subprocess_client,
            case_id=nuscenes_dataset.case_id,
            inputs={
                "nuscenes_dataset": [nuscenes_dataset],
                "sensor_calibration": [sensor_calibration],
                "gt_point_cloud_snapshot": [gt_point_cloud_snapshot],
                "gt_vector_labels_snapshot": [gt_vector_labels_snapshot],
            },
            config=config,
        )
    )
