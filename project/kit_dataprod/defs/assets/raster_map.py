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
from dagster_pipeline.orchestration.nuscenes import load_nuscenes_scene_ids


@dg.asset(
    partitions_def=case_partitions,
    group_name=AssetGroup.RASTER_MAP.value,
    owners=["team:mapping"],
    kinds={"map", "reconstruction"},
    code_version="v1.0",
    pool="case_raster_map",
)
def case_raster_map(
    context: dg.AssetExecutionContext,
    scene_camera_masks: ArtifactRef,
    nuscenes_dataset: ArtifactRef,
    sensor_calibration: ArtifactRef,
    localization: ArtifactRef,
    config: AssetExecutionConfig,
    asset_runner: AssetRunnerResource,
    pipes_subprocess_client: dg.PipesSubprocessClient,
) -> dg.Output[ArtifactRef]:
    """将当前 case 的全部 scene 合成为统一道路栅格地图。"""
    return dg.Output(
        execute_asset(
            context=context,
            asset_runner=asset_runner,
            pipes_subprocess_client=pipes_subprocess_client,
            case_id=nuscenes_dataset.case_id,
            config=config,
            inputs={
                "nuscenes_dataset": [nuscenes_dataset],
                "scene_camera_masks": read_scene_collection(scene_camera_masks),
                "sensor_calibration": [sensor_calibration],
                "localization": [localization],
            },
        )
    )


@dg.op(out=dg.DynamicOut(str))
def emit_map_crop_scenes(nuscenes_dataset: ArtifactRef):
    for scene in load_nuscenes_scene_ids(nuscenes_dataset):
        yield dg.DynamicOutput(scene, mapping_key=mapping_key_for_scene(scene))


@dg.op(pool="scene_raster_maps")
def scene_raster_maps(
    context: dg.OpExecutionContext,
    scene: str,
    case_raster_map: ArtifactRef,
    config: AssetExecutionConfig,
    asset_runner: AssetRunnerResource,
    pipes_subprocess_client: dg.PipesSubprocessClient,
) -> dg.Output[ArtifactRef]:
    """保存按 scene 中心裁剪的多图层栅格地图及其索引。"""
    return dg.Output(
        execute_asset(
            context=context,
            asset_runner=asset_runner,
            pipes_subprocess_client=pipes_subprocess_client,
            case_id=case_raster_map.case_id,
            scene_id=scene,
            inputs={"case_raster_map": [case_raster_map]},
            config=config,
        )
    )


@dg.op
def collect_map_crops(
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
        "scene_raster_maps": dg.AssetOut(
            description="保存按 scene 中心裁剪的多图层栅格地图及其索引。",
            owners=["team:mapping"],
            kinds={"map", "scene-mapped"},
            code_version="v1.0",
        )
    },
    partitions_def=case_partitions,
    group_name=AssetGroup.RASTER_MAP.value,
)
def scene_raster_maps_graph(
    nuscenes_dataset: ArtifactRef, case_raster_map: ArtifactRef
) -> dict[str, ArtifactRef]:
    """保存按 scene 中心裁剪的多图层栅格地图及其索引。"""
    scenes = emit_map_crop_scenes(nuscenes_dataset)
    maps = scenes.map(lambda scene: scene_raster_maps(scene, case_raster_map))
    return {"scene_raster_maps": collect_map_crops(maps.collect(), nuscenes_dataset)}
