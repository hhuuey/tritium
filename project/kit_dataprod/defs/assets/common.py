from __future__ import annotations

from enum import Enum
from pathlib import Path

import dagster as dg

from dagster_pipeline.configuration import AssetExecutionConfig
from dagster_pipeline.orchestration.artifact_store import ArtifactStore
from asset_protocol import ArtifactRef, AssetRequest
from dagster_pipeline.orchestration.publications import publish_scene_collection
from dagster_pipeline.orchestration.dagster_resources import AssetRunnerResource
from dagster_pipeline.orchestration.asset_registry import get_asset_execution_spec


class AssetGroup(str, Enum):
    """资产展示分组与产物目录使用同一组名称。"""

    INGESTION = "ingestion"
    GROUND_TRUTH = "ground_truth"
    CALIB_LOCALIZATION = "calib_localization"
    SEGMENTATION_PRODUCTION = "segmentation_production"
    SEGMENTATION_TRAINING = "segmentation_training"
    RASTER_MAP = "raster_map"
    VECTOR_MAP = "vector_map"
    VECTOR_MAP_TRAINING = "vector_map_training"


def execute_asset(
    *,
    context: dg.AssetExecutionContext | dg.OpExecutionContext,
    asset_runner: AssetRunnerResource,
    pipes_subprocess_client: dg.PipesSubprocessClient,
    case_id: str,
    inputs: dict[str, list[ArtifactRef] | None],
    config: AssetExecutionConfig,
    scene_id: str | None = None,
) -> ArtifactRef:
    op_context = (
        context.op_execution_context
        if isinstance(context, dg.AssetExecutionContext)
        else context
    )
    asset_name = op_context.op.name
    asset_key = dg.AssetKey(asset_name)
    if asset_key not in context.assets_def.keys:
        raise ValueError(f"执行 op {asset_name!r} 必须使用它所生产的资产名称")
    group_name = AssetGroup(context.assets_def.group_names_by_key[asset_key]).value
    spec = get_asset_execution_spec(
        asset_name,
        runtime_root=Path(asset_runner.runtime_root) if asset_runner.runtime_root else None,
    )
    output = ArtifactStore(Path(asset_runner.artifact_root)).artifact_directory(
        case_id=case_id,
        group_name=group_name,
        asset_name=asset_name,
        data_version=spec.data_version,
        scene_id=scene_id,
    )
    execution = asset_runner.run(
        context=context,
        pipes_subprocess_client=pipes_subprocess_client,
        group_name=group_name,
        request=AssetRequest(
            asset_name=asset_name,
            case_id=case_id,
            scene_id=scene_id,
            data_version=spec.data_version,
            output_uri=str(output / "payload"),
            inputs={key: value for key, value in inputs.items() if value is not None},
            config_path=str(spec.config_path),
        ),
        command=lambda: asset_runner.resolve_command(spec),
        force_recompute=config.force_recompute,
    )
    context.add_output_metadata(
        {
            "asset_name": asset_name,
            "group_name": group_name,
            "artifact_uri": dg.MetadataValue.path(execution.artifact.uri),
            "data_version": spec.data_version,
            "cache_hit": execution.cache_hit,
            **execution.metrics,
        }
    )
    return execution.artifact



def scene_artifact_index(
    context: dg.OpExecutionContext,
    nuscenes_dataset: ArtifactRef,
    artifacts: list[ArtifactRef] | tuple[ArtifactRef, ...],
    *,
    artifact_root: Path,
    runtime_root: Path | None = None,
) -> ArtifactRef:
    asset_key = context.asset_key_for_output()
    asset_name = asset_key.to_user_string()
    return publish_scene_collection(
        artifact_root=artifact_root,
        group_name=context.assets_def.group_names_by_key[asset_key],
        asset_name=asset_name,
        data_version=get_asset_execution_spec(
            asset_name, runtime_root=runtime_root
        ).data_version,
        nuscenes_dataset=nuscenes_dataset,
        artifacts=list(artifacts),
    )
