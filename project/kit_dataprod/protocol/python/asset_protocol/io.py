"""只依赖标准库的严格 JSON 读写和持久化原子写入。"""

from __future__ import annotations

import json
import math
import os
from pathlib import Path
import tempfile
from typing import Any

JsonValue = None | bool | int | float | str | list["JsonValue"] | dict[str, "JsonValue"]


def validate_json(value: Any, location: str = "document", _ancestors: set[int] | None = None) -> None:
    if _ancestors is None:
        _ancestors = set()
    if value is None or type(value) is bool:
        return
    if type(value) is int:
        if not -(2**63) <= value <= 2**64 - 1:
            raise ValueError(f"JSON integer is outside the supported range at {location}")
        return
    if type(value) is float:
        if not math.isfinite(value):
            raise ValueError(f"Non-finite JSON number at {location}")
        return
    if isinstance(value, str):
        try:
            value.encode("utf-8", errors="strict")
        except UnicodeError as error:
            raise ValueError(f"Invalid UTF-8 JSON string at {location}") from error
        return
    if type(value) is list:
        if id(value) in _ancestors:
            raise ValueError(f"JSON value contains a cycle at {location}")
        _ancestors.add(id(value))
        try:
            for index, item in enumerate(value):
                validate_json(item, f"{location}[{index}]", _ancestors)
        except RecursionError as error:
            raise ValueError("JSON nesting is too deep") from error
        finally:
            _ancestors.remove(id(value))
        return
    if type(value) is dict:
        if id(value) in _ancestors:
            raise ValueError(f"JSON value contains a cycle at {location}")
        _ancestors.add(id(value))
        try:
            for key, item in value.items():
                if not isinstance(key, str):
                    raise ValueError(f"JSON object key must be a string at {location}")
                if "\0" in key:
                    raise ValueError(f"NUL is not supported in JSON object keys at {location}")
                validate_json(key, f"{location}.key", _ancestors)
                validate_json(item, f"{location}.{key}", _ancestors)
        except RecursionError as error:
            raise ValueError("JSON nesting is too deep") from error
        finally:
            _ancestors.remove(id(value))
        return
    raise ValueError(f"Unsupported JSON value at {location}: {type(value).__name__}")


def _object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    value: dict[str, Any] = {}
    for key, item in pairs:
        if key in value:
            raise ValueError(f"Duplicate JSON object key: {key}")
        value[key] = item
    return value


def _constant(value: str) -> None:
    raise ValueError(f"Non-finite JSON number: {value}")


def _decode_json(document: str | bytes) -> JsonValue:
    """Parse tokens; protocol model conversion validates the resulting values."""
    if isinstance(document, bytes):
        document = document.decode("utf-8", errors="strict")
    if not isinstance(document, str):
        raise TypeError("JSON document must be text or UTF-8 bytes")
    document.encode("utf-8", errors="strict")
    return json.loads(document, object_pairs_hook=_object, parse_constant=_constant)


def decode_json(document: str | bytes) -> JsonValue:
    value = _decode_json(document)
    validate_json(value)
    return value


def _encode_json(value: JsonValue) -> str:
    return json.dumps(value, allow_nan=False, ensure_ascii=False, indent=2, sort_keys=True) + "\n"


def encode_json(value: JsonValue) -> str:
    validate_json(value)
    return _encode_json(value)


def read_json(path: Path) -> JsonValue:
    return decode_json(Path(path).read_bytes())


def write_json_atomic(path: Path, value: JsonValue) -> None:
    _write_json_atomic(path, encode_json(value))


def _write_json_atomic(path: Path, content: str) -> None:
    """Persist JSON text already validated by the caller."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", dir=path.parent,
            prefix=f".{path.name}.", suffix=".tmp", delete=False,
        ) as output:
            temporary_path = Path(output.name)
            output.write(content)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary_path, path)
        temporary_path = None
        descriptor = os.open(path.parent, os.O_RDONLY | getattr(os, "O_DIRECTORY", 0))
        try:
            os.fsync(descriptor)
        finally:
            os.close(descriptor)
    finally:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)
