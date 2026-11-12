from collections.abc import Iterable
from dataclasses import dataclass

import dagster as dg

from dagster_pipeline.defs.assets.common import AssetGroup


@dataclass(frozen=True)
class JobSpec:
    name: str
    selection: dg.AssetSelection
    description: str


def build_jobs(job_specs: Iterable[JobSpec]) -> tuple:
    return tuple(
        dg.define_asset_job(
            name=spec.name,
            selection=spec.selection,
            description=spec.description,
            config={},
        )
        for spec in job_specs
    )


case_manifest = dg.AssetSelection.assets("case_manifest")


def module_asset_selection(group_name: str) -> dg.AssetSelection:
    group_assets = dg.AssetSelection.groups(group_name)
    return (
        group_assets | group_assets.upstream()
    ).required_multi_asset_neighbors() - case_manifest


def assets_with_upstream(*asset_keys: str) -> dg.AssetSelection:
    assets = dg.AssetSelection.assets(*asset_keys)
    return (assets | assets.upstream()).required_multi_asset_neighbors() - case_manifest


JOB_SPECS = (
    JobSpec(
        name="module_ingest_job",
        selection=module_asset_selection(AssetGroup.INGESTION.value),
        description="同步已登记 case 的 qfile，解码传感器数据并转换为 NuScenes 数据集。",
    ),
    JobSpec(
        name="module_localization_job",
        selection=assets_with_upstream("localization"),
        description="准备上游资产，并使用 GT 点云与矢量标签运行标定和定位。",
    ),
    JobSpec(
        name="production_job",
        selection=(
            module_asset_selection(AssetGroup.VECTOR_MAP.value)
            | dg.AssetSelection.checks_for_assets("vector_map_export")
        ),
        description="准备全部上游资产；矢量地图入口直接成功，不生成业务文件。",
    ),
    JobSpec(
        name="module_segmentation_job",
        selection=(
            module_asset_selection(AssetGroup.SEGMENTATION_PRODUCTION.value)
            | dg.AssetSelection.checks_for_assets("scene_camera_masks")
        ),
        description="准备上游数据与模型，生成各 scene 的相机语义 mask。",
    ),
    JobSpec(
        name="train_segmentation_job",
        selection=(
            module_asset_selection(AssetGroup.SEGMENTATION_TRAINING.value)
            | dg.AssetSelection.checks_for_assets("scene_camera_masks")
        ),
        description="生成分割产物与训练数据，产出分割模型 checkpoint。",
    ),
    JobSpec(
        name="train_vector_map_job",
        selection=(
            module_asset_selection(AssetGroup.VECTOR_MAP_TRAINING.value)
            | dg.AssetSelection.checks_for_assets("scene_camera_masks")
        ),
        description="复用公共栅格地图和 GT 矢量标签；矢量训练入口直接成功，不生成业务文件。",
    ),
)
