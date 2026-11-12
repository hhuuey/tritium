from pathlib import Path

import dagster as dg

from dagster_pipeline.configuration import CaseManifestConfig, AssetExecutionConfig
from dagster_pipeline.defs.assets.common import (
    AssetGroup,
    execute_asset,
)
from dagster_pipeline.defs.partitions import case_partitions
from asset_protocol import ArtifactRef
from dagster_pipeline.orchestration.publications import publish_case_manifest
from dagster_pipeline.orchestration.dagster_resources import AssetRunnerResource


@dg.asset(
    partitions_def=case_partitions,
    group_name=AssetGroup.INGESTION.value,
    owners=["team:data-platform"],
    kinds={"manifest", "qfile"},
    code_version="v1.0",
)
def case_manifest(
    context: dg.AssetExecutionContext,
    config: CaseManifestConfig,
    asset_runner: AssetRunnerResource,
) -> dg.MaterializeResult[ArtifactRef]:
    """登记 Case 数据源与模型路径，发布真实的 manifest payload。"""
    artifact = publish_case_manifest(
        artifact_root=Path(asset_runner.artifact_root),
        case_id=context.partition_key,
        payload={"case_id": context.partition_key, **config.model_dump()},
    )
    return dg.MaterializeResult(
        value=artifact,
        metadata={"case_id": artifact.case_id, "qfile_uri": config.qfile_uri},
    )


@dg.asset(
    partitions_def=case_partitions,
    group_name=AssetGroup.INGESTION.value,
    owners=["team:data-platform"],
    kinds={"rsync", "qfile"},
    pool="nas_io",
    code_version="v1.0",
)
def qfile_snapshot(
    context: dg.AssetExecutionContext,
    case_manifest: ArtifactRef,
    config: AssetExecutionConfig,
    asset_runner: AssetRunnerResource,
    pipes_subprocess_client: dg.PipesSubprocessClient,
) -> dg.Output[ArtifactRef]:
    """同步并保存当前 case 的 qfile 源目录快照。"""
    return dg.Output(
        execute_asset(
            context=context,
            asset_runner=asset_runner,
            pipes_subprocess_client=pipes_subprocess_client,
            case_id=case_manifest.case_id,
            config=config,
            inputs={"case_manifest": [case_manifest]},
        )
    )


@dg.asset(
    partitions_def=case_partitions,
    group_name=AssetGroup.INGESTION.value,
    owners=["team:data-platform"],
    kinds={"dataconvert"},
    pool="nas_io",
    code_version="v1.0",
)
def decoded_sensor_data(
    context: dg.AssetExecutionContext,
    qfile_snapshot: ArtifactRef,
    config: AssetExecutionConfig,
    asset_runner: AssetRunnerResource,
    pipes_subprocess_client: dg.PipesSubprocessClient,
) -> dg.Output[ArtifactRef]:
    """解码 qfile 中的点云、图像、里程计及标定数据。"""
    return dg.Output(
        execute_asset(
            context=context,
            asset_runner=asset_runner,
            pipes_subprocess_client=pipes_subprocess_client,
            case_id=qfile_snapshot.case_id,
            inputs={"qfile_snapshot": [qfile_snapshot]},
            config=config,
        )
    )


@dg.asset(
    partitions_def=case_partitions,
    group_name=AssetGroup.INGESTION.value,
    owners=["team:data-platform"],
    kinds={"nuscenes", "dataset"},
    pool="cpu_heavy",
    code_version="v1.0",
)
def nuscenes_dataset(
    context: dg.AssetExecutionContext,
    decoded_sensor_data: ArtifactRef,
    config: AssetExecutionConfig,
    asset_runner: AssetRunnerResource,
    pipes_subprocess_client: dg.PipesSubprocessClient,
) -> dg.Output[ArtifactRef]:
    """保存 NuScenes 标准表与对应传感器媒体数据。"""
    return dg.Output(
        execute_asset(
            context=context,
            asset_runner=asset_runner,
            pipes_subprocess_client=pipes_subprocess_client,
            case_id=decoded_sensor_data.case_id,
            inputs={"decoded_sensor_data": [decoded_sensor_data]},
            config=config,
        )
    )
