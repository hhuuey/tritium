from pathlib import Path

import dagster as dg

from asset_protocol import ArtifactRef
from dagster_pipeline.configuration import AssetExecutionConfig
from dagster_pipeline.defs.assets.common import (
    AssetGroup,
    execute_asset,
    scene_artifact_index,
)
from dagster_pipeline.defs.partitions import case_partitions
from dagster_pipeline.orchestration.publications import read_scene_collection
from dagster_pipeline.orchestration.dagster_resources import AssetRunnerResource
from dagster_pipeline.orchestration.dynamic_graph import mapping_key_for_scene


@dg.op(out=dg.DynamicOut(str))
def emit_scene_maps(scene_raster_maps: ArtifactRef):
    for artifact in read_scene_collection(scene_raster_maps):
        yield dg.DynamicOutput(
            artifact.scene_id, mapping_key=mapping_key_for_scene(artifact.scene_id)
        )


@dg.op(pool="gpu_vector_map")
def scene_vector_maps(
    context: dg.OpExecutionContext,
    scene: str,
    scene_raster_maps: ArtifactRef,
    case_manifest: ArtifactRef,
    config: AssetExecutionConfig,
    asset_runner: AssetRunnerResource,
    pipes_subprocess_client: dg.PipesSubprocessClient,
) -> dg.Output[ArtifactRef]:
    """Scene 矢量地图占位入口，直接成功并发布空 payload。"""
    scene_map = next(
        artifact for artifact in read_scene_collection(scene_raster_maps)
        if artifact.scene_id == scene
    )
    return dg.Output(
        execute_asset(
            context=context,
            asset_runner=asset_runner,
            pipes_subprocess_client=pipes_subprocess_client,
            case_id=scene_map.case_id,
            scene_id=scene_map.scene_id,
            config=config,
            inputs={
                "scene_raster_maps": [scene_map],
                "case_manifest": [case_manifest],
            },
        )
    )


@dg.op
def collect_vector_labels(
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
        "scene_vector_maps": dg.AssetOut(
            description="逐 Scene 直接成功，发布空子产物和集合索引。",
            owners=["team:mapping"],
            kinds={"vector-map", "scene-mapped"},
            code_version="v1.0",
        ),
    },
    partitions_def=case_partitions,
    group_name=AssetGroup.VECTOR_MAP.value,
)
def scene_vector_maps_graph(
    scene_raster_maps: ArtifactRef,
    nuscenes_dataset: ArtifactRef,
    case_manifest: ArtifactRef,
):
    """逐 Scene 直接成功，发布空子产物和集合索引。"""
    maps = emit_scene_maps(scene_raster_maps)
    labels = maps.map(lambda scene: scene_vector_maps(scene, scene_raster_maps, case_manifest))
    index = collect_vector_labels(labels.collect(), nuscenes_dataset)
    return {"scene_vector_maps": index}


@dg.asset(
    partitions_def=case_partitions,
    group_name=AssetGroup.VECTOR_MAP.value,
    owners=["team:mapping"],
    kinds={"vector-map", "merge"},
    code_version="v1.0",
)
def case_vector_map(
    context: dg.AssetExecutionContext,
    scene_vector_maps: ArtifactRef,
    nuscenes_dataset: ArtifactRef,
    config: AssetExecutionConfig,
    asset_runner: AssetRunnerResource,
    pipes_subprocess_client: dg.PipesSubprocessClient,
) -> dg.Output[ArtifactRef]:
    """Case 矢量地图占位入口，直接成功并发布空 payload。"""
    return dg.Output(
        execute_asset(
            context=context,
            asset_runner=asset_runner,
            pipes_subprocess_client=pipes_subprocess_client,
            case_id=nuscenes_dataset.case_id,
            inputs={"scene_vector_maps": read_scene_collection(scene_vector_maps)},
            config=config,
        )
    )


@dg.asset(
    partitions_def=case_partitions,
    group_name=AssetGroup.VECTOR_MAP.value,
    owners=["team:mapping"],
    kinds={"vector-map", "conversion"},
    code_version="v1.0",
)
def vector_map_export(
    context: dg.AssetExecutionContext,
    case_vector_map: ArtifactRef,
    config: AssetExecutionConfig,
    asset_runner: AssetRunnerResource,
    pipes_subprocess_client: dg.PipesSubprocessClient,
) -> dg.Output[ArtifactRef]:
    """矢量地图导出占位入口，直接成功并发布空 payload。"""
    return dg.Output(
        execute_asset(
            context=context,
            asset_runner=asset_runner,
            pipes_subprocess_client=pipes_subprocess_client,
            case_id=case_vector_map.case_id,
            inputs={"case_vector_map": [case_vector_map]},
            config=config,
        )
    )
