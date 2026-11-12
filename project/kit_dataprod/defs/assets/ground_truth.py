import dagster as dg

from dagster_pipeline.configuration import AssetExecutionConfig
from dagster_pipeline.defs.assets.common import (
    AssetGroup,
    execute_asset,
)
from dagster_pipeline.defs.partitions import case_partitions
from asset_protocol import ArtifactRef
from dagster_pipeline.orchestration.dagster_resources import AssetRunnerResource


def _snapshot_ground_truth(
    *,
    context: dg.AssetExecutionContext,
    qfile: ArtifactRef,
    config: AssetExecutionConfig,
    asset_runner: AssetRunnerResource,
    pipes_subprocess_client: dg.PipesSubprocessClient,
) -> ArtifactRef:
    return execute_asset(
        context=context,
        asset_runner=asset_runner,
        pipes_subprocess_client=pipes_subprocess_client,
        case_id=qfile.case_id,
        config=config,
        inputs={"qfile_snapshot": [qfile]},
    )


@dg.asset(
    partitions_def=case_partitions,
    group_name=AssetGroup.GROUND_TRUTH.value,
    owners=["team:annotation"],
    kinds={"ground-truth", "pcd", "qomolo-get"},
    pool="nas_io",
    code_version="v1.0",
)
def gt_point_cloud_snapshot(
    context: dg.AssetExecutionContext,
    qfile_snapshot: ArtifactRef,
    config: AssetExecutionConfig,
    asset_runner: AssetRunnerResource,
    pipes_subprocess_client: dg.PipesSubprocessClient,
) -> dg.Output[ArtifactRef]:
    """保存 qfile 记录的精确版本 GT 点云地图。"""
    return dg.Output(
        _snapshot_ground_truth(
            context=context,
            qfile=qfile_snapshot,
            config=config,
            asset_runner=asset_runner,
            pipes_subprocess_client=pipes_subprocess_client,
        )
    )


@dg.asset(
    partitions_def=case_partitions,
    group_name=AssetGroup.GROUND_TRUTH.value,
    owners=["team:annotation"],
    kinds={"ground-truth", "vector-map", "qomolo-get"},
    pool="nas_io",
    code_version="v1.0",
)
def gt_vector_labels_snapshot(
    context: dg.AssetExecutionContext,
    qfile_snapshot: ArtifactRef,
    config: AssetExecutionConfig,
    asset_runner: AssetRunnerResource,
    pipes_subprocess_client: dg.PipesSubprocessClient,
) -> dg.Output[ArtifactRef]:
    """保存 qfile 记录的精确版本 GT OSM 或 JSON 地图。"""
    return dg.Output(
        _snapshot_ground_truth(
            context=context,
            qfile=qfile_snapshot,
            config=config,
            asset_runner=asset_runner,
            pipes_subprocess_client=pipes_subprocess_client,
        )
    )
