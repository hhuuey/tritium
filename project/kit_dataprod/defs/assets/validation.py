import dagster as dg

from dagster_pipeline.defs.partitions import case_partitions
from dagster_pipeline.orchestration.artifact_resolver import resolve_artifact_path
from asset_protocol import ArtifactRef
from dagster_pipeline.orchestration.publications import read_scene_collection
from dagster_pipeline.orchestration.nuscenes import load_nuscenes_scene_ids


@dg.asset_check(
    asset=dg.AssetKey("scene_camera_masks"),
    additional_ins={"nuscenes_dataset": dg.AssetIn()},
    partitions_def=case_partitions,
)
def segmentation_scene_coverage_check(
    scene_camera_masks: ArtifactRef,
    nuscenes_dataset: ArtifactRef,
) -> dg.AssetCheckResult:
    expected = set(load_nuscenes_scene_ids(nuscenes_dataset))
    actual = {artifact.scene_id for artifact in read_scene_collection(scene_camera_masks)}
    return dg.AssetCheckResult(
        passed=expected == actual,
        metadata={
            "expected_scene_count": len(expected),
            "actual_scene_count": len(actual),
            "missing_scenes": sorted(expected - actual),
        },
    )


@dg.asset_check(asset=dg.AssetKey("vector_map_export"), partitions_def=case_partitions)
def final_label_payload_check(
    vector_map_export: ArtifactRef,
) -> dg.AssetCheckResult:
    return dg.AssetCheckResult(
        passed=resolve_artifact_path(vector_map_export, strict=False).is_dir(),
        metadata={
            "artifact_uri": vector_map_export.uri,
            "data_version": vector_map_export.data_version,
        },
    )
