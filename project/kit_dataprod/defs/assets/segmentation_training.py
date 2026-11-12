import dagster as dg

from dagster_pipeline.configuration import AssetExecutionConfig
from dagster_pipeline.defs.assets.common import (
    AssetGroup,
    execute_asset,
)
from dagster_pipeline.defs.partitions import case_partitions
from asset_protocol import ArtifactRef
from dagster_pipeline.orchestration.publications import read_scene_collection
from dagster_pipeline.orchestration.dagster_resources import AssetRunnerResource


@dg.asset(
    partitions_def=case_partitions,
    group_name=AssetGroup.SEGMENTATION_TRAINING.value,
    owners=["team:perception"],
    kinds={"labels", "refine"},
    code_version="v1.0",
)
def refined_segmentation_labels(
    context: dg.AssetExecutionContext,
    nuscenes_dataset: ArtifactRef,
    scene_camera_masks: ArtifactRef,
    config: AssetExecutionConfig,
    asset_runner: AssetRunnerResource,
    pipes_subprocess_client: dg.PipesSubprocessClient,
) -> dg.Output[ArtifactRef]:
    """保存相机分割 mask 对应的精修标签条目。"""
    return dg.Output(
        execute_asset(
            context=context,
            asset_runner=asset_runner,
            pipes_subprocess_client=pipes_subprocess_client,
            case_id=nuscenes_dataset.case_id,
            inputs={
                "nuscenes_dataset": [nuscenes_dataset],
                "scene_camera_masks": read_scene_collection(scene_camera_masks),
            },
            config=config,
        )
    )


@dg.asset(
    partitions_def=case_partitions,
    group_name=AssetGroup.SEGMENTATION_TRAINING.value,
    owners=["team:perception"],
    kinds={"dataset", "refine"},
    code_version="v1.0",
)
def segmentation_training_dataset(
    context: dg.AssetExecutionContext,
    nuscenes_dataset: ArtifactRef,
    refined_segmentation_labels: ArtifactRef,
    config: AssetExecutionConfig,
    asset_runner: AssetRunnerResource,
    pipes_subprocess_client: dg.PipesSubprocessClient,
) -> dg.Output[ArtifactRef]:
    """保存图片、mask 与精修标签对应的训练数据清单。"""
    return dg.Output(
        execute_asset(
            context=context,
            asset_runner=asset_runner,
            pipes_subprocess_client=pipes_subprocess_client,
            case_id=nuscenes_dataset.case_id,
            inputs={
                "nuscenes_dataset": [nuscenes_dataset],
                "refined_segmentation_labels": [refined_segmentation_labels],
            },
            config=config,
        )
    )


@dg.asset(
    partitions_def=case_partitions,
    group_name=AssetGroup.SEGMENTATION_TRAINING.value,
    owners=["team:perception"],
    kinds={"model", "refine"},
    pool="gpu_training",
    code_version="v1.0",
)
def segmentation_model_checkpoint(
    context: dg.AssetExecutionContext,
    case_manifest: ArtifactRef,
    segmentation_training_dataset: ArtifactRef,
    config: AssetExecutionConfig,
    asset_runner: AssetRunnerResource,
    pipes_subprocess_client: dg.PipesSubprocessClient,
) -> dg.Output[ArtifactRef]:
    """保存分割训练链生成的模型 checkpoint 与元数据。"""
    artifact = execute_asset(
        context=context,
        asset_runner=asset_runner,
        pipes_subprocess_client=pipes_subprocess_client,
        case_id=segmentation_training_dataset.case_id,
        config=config,
        inputs={
            "case_manifest": [case_manifest],
            "segmentation_training_dataset": [segmentation_training_dataset],
        },
    )
    return dg.Output(artifact)
