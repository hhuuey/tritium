from __future__ import annotations

from pathlib import Path
from typing import Any
from urllib.parse import unquote, urlsplit

from asset_protocol import ArtifactRef, from_dict, to_dict

from dagster_pipeline.orchestration.json_io import (
    JsonValue,
    decode_json,
    encode_json,
    read_json,
    write_json_atomic,
)


def write_json_value(path: Path, value: Any, *, artifact_root: Path) -> None:
    write_json_atomic(path, _encode(value, Path(artifact_root).expanduser().resolve()))


def read_json_value(path: Path, *, artifact_root: Path) -> Any:
    return _decode(read_json(path), Path(artifact_root).expanduser().resolve())


def _encode(value: Any, root: Path) -> Any:
    if isinstance(value, ArtifactRef):
        payload = to_dict(value)
        payload["uri"] = _encode_uri(payload["uri"], root)
        # Extension data is opaque JSON; its keys are never serialization tags.
        return {"__type__": "ArtifactRef", "value": payload}
    if isinstance(value, list):
        return [_encode(item, root) for item in value]
    if isinstance(value, dict):
        return {key: _encode(item, root) for key, item in value.items()}
    return value


def _encode_uri(value: str | None, root: Path) -> Any:
    if value is None:
        return None
    parsed = urlsplit(value)
    if parsed.scheme not in {"", "file"} or parsed.netloc not in {"", "localhost"}:
        return value
    path = Path(unquote(parsed.path if parsed.scheme == "file" else value)).expanduser()
    if path.is_absolute():
        path = path.resolve()
        if path.is_relative_to(root):
            return {"__managed_uri__": path.relative_to(root).as_posix()}
    return value


def _decode(value: Any, root: Path) -> Any:
    if isinstance(value, list):
        return [_decode(item, root) for item in value]
    if not isinstance(value, dict):
        return value
    if "__managed_uri__" in value:
        return str((root / value["__managed_uri__"]).resolve())
    if "__type__" in value:
        if value["__type__"] == "ArtifactRef":
            payload = dict(value["value"])
            payload["uri"] = _decode(payload["uri"], root)
            return from_dict(ArtifactRef, payload)
        raise ValueError(f"Unsupported serialized type: {value['__type__']!r}")
    return {key: _decode(item, root) for key, item in value.items()}
