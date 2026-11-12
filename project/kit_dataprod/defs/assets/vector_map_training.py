import dagster as dg

from dagster_pipeline.configuration import AssetExecutionConfig
from dagster_pipeline.defs.assets.common import AssetGroup, execute_asset
from dagster_pipeline.defs.partitions import case_partitions
from asset_protocol import ArtifactRef
from dagster_pipeline.orchestration.publications import read_scene_collection
from dagster_pipeline.orchestration.dagster_resources import AssetRunnerResource


@dg.asset(
    partitions_def=case_partitions,
    group_name=AssetGroup.VECTOR_MAP_TRAINING.value,
    owners=["team:mapping"],
    kinds={"training-list", "vector-map"},
    code_version="v1.0",
)
def vector_map_training_dataset(
    context: dg.AssetExecutionContext,
    scene_raster_maps: ArtifactRef,
    gt_vector_labels_snapshot: ArtifactRef,
    config: AssetExecutionConfig,
    asset_runner: AssetRunnerResource,
    pipes_subprocess_client: dg.PipesSubprocessClient,
) -> dg.Output[ArtifactRef]:
    """矢量地图训练数据占位入口，直接成功并发布空 payload。"""
    return dg.Output(
        execute_asset(
            context=context,
            asset_runner=asset_runner,
            pipes_subprocess_client=pipes_subprocess_client,
            case_id=scene_raster_maps.case_id,
            config=config,
            inputs={
                "scene_raster_maps": read_scene_collection(scene_raster_maps),
                "gt_vector_labels_snapshot": [gt_vector_labels_snapshot],
            },
        )
    )


@dg.asset(
    partitions_def=case_partitions,
    group_name=AssetGroup.VECTOR_MAP_TRAINING.value,
    owners=["team:mapping"],
    kinds={"model", "training"},
    pool="gpu_training",
    code_version="v1.0",
)
def vector_map_model_checkpoint(
    context: dg.AssetExecutionContext,
    vector_map_training_dataset: ArtifactRef,
    config: AssetExecutionConfig,
    asset_runner: AssetRunnerResource,
    pipes_subprocess_client: dg.PipesSubprocessClient,
) -> dg.Output[ArtifactRef]:
    """矢量地图训练占位入口，直接成功并发布空 payload。"""
    artifact = execute_asset(
        context=context,
        asset_runner=asset_runner,
        pipes_subprocess_client=pipes_subprocess_client,
        case_id=vector_map_training_dataset.case_id,
        inputs={"vector_map_training_dataset": [vector_map_training_dataset]},
        config=config,
    )
    return dg.Output(artifact)
