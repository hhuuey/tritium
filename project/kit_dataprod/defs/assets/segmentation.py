from pathlib import Path

import dagster as dg

from dagster_pipeline.configuration import AssetExecutionConfig
from dagster_pipeline.defs.assets.common import (
    AssetGroup,
    execute_asset,
    scene_artifact_index,
)
from dagster_pipeline.defs.partitions import case_partitions
from asset_protocol import ArtifactRef
from dagster_pipeline.orchestration.dagster_resources import AssetRunnerResource
from dagster_pipeline.orchestration.dynamic_graph import mapping_key_for_scene
from dagster_pipeline.orchestration.nuscenes import load_nuscenes_scene_ids


@dg.op(out=dg.DynamicOut(str))
def emit_segmentation_scenes(nuscenes_dataset: ArtifactRef):
    for scene in load_nuscenes_scene_ids(nuscenes_dataset):
        yield dg.DynamicOutput(scene, mapping_key=mapping_key_for_scene(scene))


@dg.op(pool="gpu_segmentation")
def scene_camera_masks(
    context: dg.OpExecutionContext,
    scene: str,
    nuscenes_dataset: ArtifactRef,
    case_manifest: ArtifactRef,
    config: AssetExecutionConfig,
    asset_runner: AssetRunnerResource,
    pipes_subprocess_client: dg.PipesSubprocessClient,
) -> dg.Output[ArtifactRef]:
    """保存各 scene 的相机语义 mask 及其索引。"""
    return dg.Output(
        execute_asset(
            context=context,
            asset_runner=asset_runner,
            pipes_subprocess_client=pipes_subprocess_client,
            case_id=nuscenes_dataset.case_id,
            scene_id=scene,
            config=config,
            inputs={
                "nuscenes_dataset": [nuscenes_dataset],
                "case_manifest": [case_manifest],
            },
        )
    )


@dg.op
def collect_segmentation_results(
    context: dg.OpExecutionContext,
    artifacts: list[ArtifactRef],
    nuscenes_dataset: ArtifactRef,
    asset_runner: AssetRunnerResource,
) -> ArtifactRef:
    return scene_artifact_index(
        context,
        nuscenes_dataset,
        artifacts,
        artifact_root=Path(asset_runner.artifact_root),
        runtime_root=Path(asset_runner.runtime_root) if asset_runner.runtime_root else None,
    )


@dg.graph_multi_asset(
    outs={
        "scene_camera_masks": dg.AssetOut(
            description="保存各 scene 的相机语义 mask 及其索引。",
            owners=["team:perception"],
            kinds={"segmentation", "scene-mapped"},
            code_version="v1.0",
        ),
    },
    partitions_def=case_partitions,
    group_name=AssetGroup.SEGMENTATION_PRODUCTION.value,
)
def scene_camera_masks_graph(
    nuscenes_dataset: ArtifactRef, case_manifest: ArtifactRef
):
    """保存各 scene 的相机语义 mask 及其索引。"""
    scenes = emit_segmentation_scenes(nuscenes_dataset)
    masks = scenes.map(
        lambda scene: scene_camera_masks(scene, nuscenes_dataset, case_manifest)
    )
    index = collect_segmentation_results(masks.collect(), nuscenes_dataset)
    return {"scene_camera_masks": index}
