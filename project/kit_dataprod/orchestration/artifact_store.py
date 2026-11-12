from __future__ import annotations

from copy import deepcopy
from dataclasses import dataclass
from pathlib import Path
from typing import Any
from urllib.parse import unquote, urlsplit

from asset_protocol import (
    PROTOCOL_VERSION, ArtifactRef, AssetIdentity, AssetRequest, AssetResult,
    read_result, validate, validate_result_identity,
)
from dagster_pipeline.orchestration.execution_models import (
    AssetVersion, ExecutionIdentity, SuccessMarker,
)


@dataclass(frozen=True)
class ArtifactStore:
    """保存产物；缓存只比较当前身份和上游的明文版本清单。"""

    root: Path

    def artifact_directory(
        self,
        *,
        case_id: str,
        group_name: str,
        asset_name: str,
        data_version: str,
        scene_id: str | None = None,
    ) -> Path:
        parts = [case_id, group_name, asset_name, data_version]
        if scene_id is not None:
            parts.append(scene_id)
        if any(not value or value in {".", ".."} or "/" in value or "\\" in value for value in parts):
            raise ValueError("Artifact identity must contain single path components")
        root = self.root.expanduser().resolve()
        directory = root.joinpath(*parts).resolve()
        if directory == root or not directory.is_relative_to(root):
            raise ValueError("Artifact directory must be inside artifact_root")
        return directory

    def success_marker(self, request: AssetRequest, *, group_name: str) -> SuccessMarker:
        """只读直接上游成功标记，合并其已保存的祖先版本，不读取旧请求。"""
        if request.protocol_version != PROTOCOL_VERSION:
            raise ValueError(f"Unsupported asset protocol: {request.protocol_version!r}")
        validate(request)
        versions: set[AssetVersion] = set()
        for references in request.inputs.values():
            for reference in references:
                location = self._managed_input(reference)
                version = AssetVersion(
                    reference.asset_name, reference.case_id, reference.scene_id,
                    reference.data_version, location[1] if location else None,
                )
                versions.add(version)
                if location is not None:
                    upstream, _ = self._read_published(location[0], self._version_identity(version))
                    versions.update(upstream.upstream_versions)
        current = AssetVersion(
            request.asset_name, request.case_id, request.scene_id, request.data_version, group_name,
        )
        if current in versions:
            raise ValueError(f"Asset {request.asset_name!r} cannot depend on itself")
        return SuccessMarker(
            ExecutionIdentity(
                request.protocol_version, group_name, request.asset_name,
                request.case_id, request.data_version, request.scene_id,
            ),
            tuple(sorted(versions, key=AssetVersion.sort_key)),
        )

    def managed_input_directories(self, request: AssetRequest) -> tuple[Path, ...]:
        """在读取上游标记前，定位需要共享锁的直接输入目录。"""
        validate(request)
        return tuple(sorted({
            location[0]
            for references in request.inputs.values()
            for reference in references
            if (location := self._managed_input(reference)) is not None
        }))

    def marker_input_directories(self, marker: SuccessMarker) -> tuple[Path, ...]:
        """直接按版本清单定位受管祖先，不递归扫描请求。"""
        return tuple(sorted({
            self.artifact_directory(
                case_id=version.case_id, group_name=version.group_name,
                asset_name=version.asset_name, data_version=version.data_version,
                scene_id=version.scene_id,
            )
            for version in marker.upstream_versions if version.group_name is not None
        }))

    def validate_upstream_versions(self, marker: SuccessMarker) -> None:
        """持有输入锁后检查祖先产物是否已成功发布，不比较配置或文件内容。"""
        for version in marker.upstream_versions:
            if version.group_name is not None:
                identity = self._version_identity(version)
                self._read_published(self.artifact_directory(**self._directory_fields(identity)), identity)

    def find_cached(
        self, request: AssetRequest, *, group_name: str,
        expected_marker: SuccessMarker | None = None,
    ) -> ArtifactRef | None:
        expected_marker = expected_marker or self.success_marker(request, group_name=group_name)
        self.validate_upstream_versions(expected_marker)
        directory = self.artifact_directory(**self._directory_fields(expected_marker.identity))
        try:
            marker, result = self._read_published(directory, expected_marker.identity)
            if marker != expected_marker:
                return None
        except (OSError, ValueError, TypeError, AttributeError):
            return None
        return self.reference(request, group_name=group_name, metadata=result.metadata)

    def reference(
        self, request: AssetRequest, *, group_name: str,
        metadata: dict[str, Any] | None = None,
    ) -> ArtifactRef:
        directory = self.artifact_directory(
            case_id=request.case_id, group_name=group_name, asset_name=request.asset_name,
            data_version=request.data_version, scene_id=request.scene_id,
        )
        return ArtifactRef(
            asset_name=request.asset_name, uri=str(directory / "payload"),
            data_version=request.data_version, case_id=request.case_id,
            scene_id=request.scene_id, metadata={} if metadata is None else deepcopy(metadata),
        )

    def complete(
        self, request: AssetRequest, result: AssetResult, *, group_name: str,
        expected_marker: SuccessMarker | None = None,
    ) -> None:
        validate_result_identity(request, result)
        if result.status != "success" or result.error is not None:
            raise ValueError("Only successful asset results can be published")
        try:
            current = self.success_marker(request, group_name=group_name)
            self.validate_upstream_versions(current)
        except (OSError, ValueError, TypeError) as error:
            if expected_marker is None:
                raise
            raise RuntimeError(f"Upstream versions changed while executing {request.asset_name!r}") from error
        if expected_marker is not None and current != expected_marker:
            raise RuntimeError(f"Upstream versions changed while executing {request.asset_name!r}")
        directory = self.artifact_directory(**self._directory_fields(current.identity))
        if not (directory / "payload").is_dir():
            raise FileNotFoundError(f"Cannot complete {request.asset_name}; payload is not a directory")
        current.write(directory / "_SUCCESS.json")

    def _managed_input(self, reference: ArtifactRef) -> tuple[Path, str] | None:
        """识别当前产物根内的规范 payload，外部引用只使用其版本。"""
        parsed = urlsplit(reference.uri)
        if parsed.scheme not in {"", "file"} or parsed.netloc not in {"", "localhost"}:
            return None
        path = Path(unquote(parsed.path if parsed.scheme == "file" else reference.uri)).expanduser().absolute()
        root = self.root.expanduser().absolute()
        if not path.is_relative_to(root) and not path.is_relative_to(root.resolve()):
            return None
        path, root = path.resolve(), root.resolve()
        if not path.is_relative_to(root):
            raise ValueError(f"Managed input URI escapes artifact_root: {reference.uri}")
        if path.name != "payload":
            return None
        parts = path.relative_to(root).parts
        expected = [reference.case_id, "", reference.asset_name, reference.data_version]
        if reference.scene_id is not None:
            expected.append(reference.scene_id)
        expected.append("payload")
        if len(parts) != len(expected):
            raise ValueError(f"Managed input URI has an invalid layout: {reference.uri}")
        expected[1] = parts[1]
        if list(parts) != expected:
            raise ValueError(f"Managed input URI has an invalid layout: {reference.uri}")
        return path.parent, parts[1]

    @staticmethod
    def _version_identity(version: AssetVersion) -> ExecutionIdentity:
        return ExecutionIdentity(
            PROTOCOL_VERSION, version.group_name, version.asset_name,
            version.case_id, version.data_version, version.scene_id,
        )

    @staticmethod
    def _directory_fields(identity: ExecutionIdentity) -> dict[str, Any]:
        return {
            "case_id": identity.case_id, "group_name": identity.group_name,
            "asset_name": identity.asset_name, "data_version": identity.data_version,
            "scene_id": identity.scene_id,
        }

    @staticmethod
    def _read_published(directory: Path, identity: ExecutionIdentity) -> tuple[SuccessMarker, AssetResult]:
        if (directory / "_failure.json").exists() or not (directory / "payload").is_dir():
            raise ValueError(f"Managed input payload is missing or failed: {directory}")
        try:
            marker = SuccessMarker.read(directory / "_SUCCESS.json")
            result = read_result(directory / "_result.json")
            if marker.identity != identity:
                raise ValueError("success marker identity mismatch")
            validate_result_identity(AssetIdentity(
                asset_name=identity.asset_name, case_id=identity.case_id,
                data_version=identity.data_version, scene_id=identity.scene_id,
                protocol_version=identity.protocol_version,
            ), result)
            if result.status != "success":
                raise ValueError("result is not successful")
        except (OSError, ValueError, TypeError, AttributeError) as error:
            raise ValueError(f"Managed input has no valid success marker/result: {directory}: {error}") from error
        return marker, result
