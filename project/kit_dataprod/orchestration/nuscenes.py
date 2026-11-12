from asset_protocol import ArtifactRef


def load_nuscenes_scene_ids(nuscenes: ArtifactRef) -> list[str]:
    scene_ids = nuscenes.metadata.get("scene_ids")
    if not isinstance(scene_ids, list) or not all(
        isinstance(scene_id, str) and scene_id for scene_id in scene_ids
    ):
        raise ValueError("NuScenes artifact metadata requires a scene_ids list")
    if len(set(scene_ids)) != len(scene_ids):
        raise ValueError("NuScenes artifact metadata contains duplicate scene_ids")
    return list(scene_ids)
