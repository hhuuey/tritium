"""Protocol operations; generated field definitions live in generated.py."""
from __future__ import annotations

from pathlib import Path
from typing import Any

from asset_protocol.generated import ArtifactRef, AssetError, AssetIdentity, AssetRequest, AssetResult
from asset_protocol.io import _decode_json, _encode_json, _write_json_atomic, read_json
from asset_protocol.validation import IDENTITY_FIELDS, from_dict, to_dict, validate


def identity_of(model: AssetIdentity | AssetRequest | AssetResult) -> AssetIdentity:
    return from_dict(AssetIdentity, {name: getattr(model, name) for name in IDENTITY_FIELDS})


def read_request_identity(path: Path) -> AssetIdentity:
    value = read_json(path)
    if type(value) is not dict:
        raise ValueError("Request identity must be a JSON object")
    return from_dict(AssetIdentity, {name: value[name] for name in IDENTITY_FIELDS if name in value})


def read_request(path: Path) -> AssetRequest:
    return from_dict(AssetRequest, _decode_json(Path(path).read_bytes()))


def write_request(path: Path, request: AssetRequest) -> None:
    _write_json_atomic(path, _encode_json(to_dict(request)))


def read_result(path: Path) -> AssetResult:
    return from_dict(AssetResult, _decode_json(Path(path).read_bytes()))


def write_result(path: Path, result: AssetResult) -> None:
    _write_json_atomic(path, _encode_json(to_dict(result)))


def input_refs(request: AssetRequest, asset_name: str) -> tuple[ArtifactRef, ...]:
    validate(request)
    if asset_name not in request.inputs:
        raise ValueError(f"{request.asset_name} requires references for input asset {asset_name!r}")
    return tuple(request.inputs[asset_name])


def input_ref(request: AssetRequest, asset_name: str) -> ArtifactRef:
    references = input_refs(request, asset_name)
    if len(references) != 1:
        raise ValueError(f"{request.asset_name} requires exactly one reference for input asset {asset_name!r}")
    return references[0]


def input_key(request: AssetRequest, *asset_names: str) -> str:
    validate(request)
    matches = [name for name in asset_names if name in request.inputs]
    if len(matches) != 1:
        raise ValueError(f"{request.asset_name} requires exactly one input asset from {asset_names!r}")
    return matches[0]


def success_result(identity: AssetIdentity | AssetRequest | AssetResult, *,
                   metrics: dict[str, Any] | None = None, metadata: dict[str, Any] | None = None) -> AssetResult:
    return from_dict(AssetResult, {**to_dict(identity_of(identity)), "status": "success", "error": None,
        "metrics": {} if metrics is None else metrics, "metadata": {} if metadata is None else metadata})


def failure_result(identity: AssetIdentity | AssetRequest | AssetResult, error: AssetError, *,
                   metrics: dict[str, Any] | None = None, metadata: dict[str, Any] | None = None) -> AssetResult:
    return from_dict(AssetResult, {**to_dict(identity_of(identity)), "status": "failed", "error": to_dict(error),
        "metrics": {} if metrics is None else metrics, "metadata": {} if metadata is None else metadata})


def validate_result_identity(request: AssetIdentity | AssetRequest, result: AssetResult) -> None:
    validate(result)
    if identity_of(request) != identity_of(result):
        raise ValueError("Asset result identity does not match the request")
