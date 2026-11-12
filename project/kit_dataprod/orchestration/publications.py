"""Publish local manifest and Scene collection payloads using normal asset identities."""

from __future__ import annotations

from collections.abc import Callable, Iterator
from contextlib import contextmanager
from dataclasses import replace
import os
from pathlib import Path
from typing import Any
from urllib.parse import unquote, urlsplit

from asset_protocol import (
    ArtifactRef, AssetRequest, from_dict, success_result, to_dict,
    write_request, write_result,
)

from dagster_pipeline.orchestration.artifact_lock import artifact_execution_locks
from dagster_pipeline.orchestration.artifact_store import ArtifactStore
from dagster_pipeline.orchestration.asset_diagnostics import write_failure_summary
from dagster_pipeline.orchestration.execution_models import (
    AssetVersion, OrchestrationError, ProcessInfo, SuccessMarker,
)
from dagster_pipeline.orchestration.json_io import read_json, write_json_atomic
from dagster_pipeline.orchestration.nuscenes import load_nuscenes_scene_ids


@contextmanager
def _publication_locks(
    store: ArtifactStore, request: AssetRequest, group_name: str,
) -> Iterator[tuple[Path, SuccessMarker]]:
    directory = store.artifact_directory(
        case_id=request.case_id, group_name=group_name,
        asset_name=request.asset_name, data_version=request.data_version,
    )
    inputs: set[Path] = set()
    while True:
        with artifact_execution_locks(directory, tuple(inputs)):
            try:
                resolved = set(store.managed_input_directories(request))
                if not resolved.issubset(inputs):
                    inputs = resolved
                    continue
                marker = store.success_marker(request, group_name=group_name)
                resolved.update(store.marker_input_directories(marker))
                if directory in resolved:
                    raise ValueError("A publication cannot depend on itself")
                if resolved != inputs:
                    inputs = resolved
                    continue
                store.validate_upstream_versions(marker)
                yield directory, marker
            except Exception as error:
                (directory / "_SUCCESS.json").unlink(missing_ok=True)
                write_failure_summary(
                    directory / "_failure.json", request, group_name=group_name,
                    error=OrchestrationError(
                        "input", "PUBLICATION_FAILED", str(error) or type(error).__name__,
                    ),
                    process=ProcessInfo(), run_id=None, step_key=None,
                )
                raise
            except BaseException:
                (directory / "_SUCCESS.json").unlink(missing_ok=True)
                raise
            return


def _publish(
    store: ArtifactStore, request: AssetRequest, *, group_name: str,
    filename: str, document: Callable[[], dict[str, Any]],
    metadata: dict[str, Any], reuse: bool,
) -> ArtifactRef:
    with _publication_locks(store, request, group_name) as (directory, marker):
        payload = document()
        path = directory / "payload" / filename
        if reuse:
            cached = store.find_cached(request, group_name=group_name, expected_marker=marker)
            if cached is not None:
                try:
                    if read_json(path) == payload and cached.metadata == metadata:
                        return cached
                except (OSError, ValueError):
                    pass
        (directory / "_SUCCESS.json").unlink(missing_ok=True)
        (directory / "_failure.json").unlink(missing_ok=True)
        # Never remove directory: it may also contain independently cached Scenes.
        write_json_atomic(path, payload)
        write_request(directory / "_request.json", request)
        result = success_result(request, metadata=metadata)
        write_result(directory / "_result.json", result)
        store.complete(request, result, group_name=group_name, expected_marker=marker)
        return store.reference(request, group_name=group_name, metadata=metadata)


def publish_case_manifest(
    *, artifact_root: Path, case_id: str, payload: dict[str, Any],
) -> ArtifactRef:
    store = ArtifactStore(artifact_root)
    directory = store.artifact_directory(
        case_id=case_id, group_name="ingestion", asset_name="case_manifest", data_version="v1.0",
    )
    request = AssetRequest(
        "case_manifest", case_id, "v1.0", str(directory / "payload"),
        str(directory / "payload" / "case.json"),
    )

    def document() -> dict[str, Any]:
        if payload.get("case_id") != case_id:
            raise ValueError("case_manifest payload case_id does not match its reference")
        return payload

    return _publish(
        store, request, group_name="ingestion", filename="case.json", document=document,
        metadata={}, reuse=True,
    )


def _scene_references(
    reference: ArtifactRef, artifacts: list[ArtifactRef], *, expected: list[str],
) -> list[ArtifactRef]:
    scenes: set[str] = set()
    for artifact in artifacts:
        to_dict(artifact)
        if (artifact.asset_name, artifact.case_id, artifact.data_version) != (
            reference.asset_name, reference.case_id, reference.data_version,
        ):
            raise ValueError("Scene artifact identity does not match the collection")
        if not artifact.scene_id or artifact.scene_id in scenes:
            raise ValueError("Scene artifacts require unique, nonempty scene IDs")
        scenes.add(artifact.scene_id)
    if scenes != set(expected):
        raise ValueError(f"Scene collection is incomplete: expected {sorted(expected)}, got {sorted(scenes)}")
    return sorted(artifacts, key=lambda item: item.scene_id)


def publish_scene_collection(
    *, artifact_root: Path, group_name: str, asset_name: str, data_version: str,
    nuscenes_dataset: ArtifactRef, artifacts: list[ArtifactRef],
) -> ArtifactRef:
    store = ArtifactStore(artifact_root)
    directory = store.artifact_directory(
        case_id=nuscenes_dataset.case_id, group_name=group_name,
        asset_name=asset_name, data_version=data_version,
    )
    request = AssetRequest(
        asset_name, nuscenes_dataset.case_id, data_version, str(directory / "payload"),
        str(directory / "payload" / "index.json"),
        inputs={"nuscenes_dataset": [nuscenes_dataset], asset_name: list(artifacts)},
    )
    reference = store.reference(request, group_name=group_name)
    metadata: dict[str, Any] = {}

    def document() -> dict[str, Any]:
        expected = sorted(load_nuscenes_scene_ids(nuscenes_dataset))
        metadata["scene_ids"] = expected
        ordered = _scene_references(reference, artifacts, expected=expected)
        entries = []
        for child in ordered:
            expected_path = store.artifact_directory(
                case_id=child.case_id, group_name=group_name, asset_name=child.asset_name,
                data_version=child.data_version, scene_id=child.scene_id,
            ) / "payload"
            if _local_path(child.uri) != expected_path.resolve():
                raise ValueError("Scene collection requires published children in its asset directory")
            entries.append(to_dict(replace(
                child, uri=Path(os.path.relpath(expected_path, directory / "payload")).as_posix(),
            )))
        return {"artifacts": entries}

    return _publish(
        store, request, group_name=group_name, filename="index.json", document=document,
        metadata=metadata, reuse=True,
    )


def _local_path(uri: str) -> Path:
    parsed = urlsplit(uri)
    if parsed.scheme not in {"", "file"} or parsed.netloc not in {"", "localhost"}:
        raise ValueError("Collection payloads must be local")
    if parsed.query or parsed.fragment:
        raise ValueError("Collection payload URI cannot contain query or fragment")
    path = Path(unquote(parsed.path) if parsed.scheme else uri)
    if not path.is_absolute():
        raise ValueError("Collection payload path must be absolute")
    return path.resolve()


def read_scene_collection(reference: ArtifactRef) -> list[ArtifactRef]:
    to_dict(reference)
    if reference.scene_id is not None:
        raise ValueError("Scene collection reference must have scene_id=null")
    payload = _local_path(reference.uri)
    if payload.name != "payload" or len(payload.parents) < 5:
        raise ValueError("Scene collection must use a managed payload directory")
    store = ArtifactStore(payload.parents[4])
    location = store._managed_input(reference)
    if location is None:
        raise ValueError("Scene collection must use a managed payload directory")
    directory, group = location
    inputs: set[Path] = set()
    while True:
        with artifact_execution_locks(directory, tuple(inputs)):
            identity = store._version_identity(AssetVersion(
                reference.asset_name, reference.case_id, None, reference.data_version, group,
            ))
            marker, result = store._read_published(directory, identity)
            resolved = set(store.marker_input_directories(marker))
            if resolved != inputs:
                inputs = resolved
                continue
            store.validate_upstream_versions(marker)
            document = read_json(payload / "index.json")
            if not isinstance(document, dict) or set(document) != {"artifacts"} or not isinstance(document["artifacts"], list):
                raise ValueError("Scene index requires an artifacts list")
            children = []
            for value in document["artifacts"]:
                child = from_dict(ArtifactRef, value)
                path = Path(child.uri)
                if path.is_absolute() or urlsplit(child.uri).scheme:
                    raise ValueError("Scene index paths must be relative to its payload")
                child = replace(child, uri=str((payload / path).resolve()))
                version = AssetVersion(
                    child.asset_name, child.case_id, child.scene_id, child.data_version, group,
                )
                if version not in marker.upstream_versions:
                    raise ValueError("Scene index child is not recorded in its publication")
                expected_path = store.artifact_directory(
                    case_id=reference.case_id, group_name=group, asset_name=reference.asset_name,
                    data_version=reference.data_version, scene_id=child.scene_id,
                ) / "payload"
                if _local_path(child.uri) != expected_path.resolve():
                    raise ValueError("Scene index path does not match its child identity")
                children.append(child)
            expected = result.metadata.get("scene_ids")
            if not isinstance(expected, list) or not all(isinstance(item, str) for item in expected) or len(set(expected)) != len(expected):
                raise ValueError("Scene collection metadata requires unique scene_ids")
            return _scene_references(reference, children, expected=expected)
