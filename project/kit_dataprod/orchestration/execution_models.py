"""Dagster 编排层的执行、失败和诊断模型。"""

from __future__ import annotations

from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any

from dagster_pipeline.orchestration.json_io import read_json, validate_json, write_json_atomic


@dataclass(frozen=True)
class ExecutionIdentity:
    """当前资产执行的身份，与成功和失败记录共用。"""

    protocol_version: str
    group_name: str
    asset_name: str
    case_id: str
    data_version: str
    scene_id: str | None

    def __post_init__(self) -> None:
        for name, value in self.to_dict().items():
            if name != "scene_id" or value is not None:
                _nonempty_string(value, f"execution identity.{name}")

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)

    @classmethod
    def from_dict(cls, value: object) -> "ExecutionIdentity":
        return cls(**_object(value, {
            "protocol_version", "group_name", "asset_name", "case_id", "data_version", "scene_id",
        }, "execution identity"))


@dataclass(frozen=True)
class AssetVersion:
    """上游资产的明文版本；外部引用没有产物分组。"""

    asset_name: str
    case_id: str
    scene_id: str | None
    data_version: str
    group_name: str | None

    def __post_init__(self) -> None:
        for name, value in self.to_dict().items():
            if name not in {"scene_id", "group_name"} or value is not None:
                _nonempty_string(value, f"asset version.{name}")

    def sort_key(self) -> tuple[str, str, str, str, str]:
        return (
            self.case_id, self.group_name or "", self.asset_name,
            self.scene_id or "", self.data_version,
        )

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)

    @classmethod
    def from_dict(cls, value: object) -> "AssetVersion":
        return cls(**_object(value, {
            "asset_name", "case_id", "scene_id", "data_version", "group_name",
        }, "asset version"))


@dataclass(frozen=True)
class SuccessMarker:
    """成功标记只记录当前身份和去重排序后的上游版本。"""

    identity: ExecutionIdentity
    upstream_versions: tuple[AssetVersion, ...] = ()

    def __post_init__(self) -> None:
        if not isinstance(self.identity, ExecutionIdentity):
            raise TypeError("success marker identity must be an ExecutionIdentity")
        if type(self.upstream_versions) is not tuple or not all(
            isinstance(version, AssetVersion) for version in self.upstream_versions
        ):
            raise TypeError("success marker upstream_versions must be a tuple of AssetVersion")
        if self.upstream_versions != tuple(sorted(set(self.upstream_versions), key=AssetVersion.sort_key)):
            raise ValueError("success marker upstream_versions must be unique and sorted")

    def to_dict(self) -> dict[str, Any]:
        return {
            "identity": self.identity.to_dict(),
            "upstream_versions": [version.to_dict() for version in self.upstream_versions],
        }

    def write(self, path: Path) -> None:
        write_json_atomic(path, self.to_dict())

    @classmethod
    def from_dict(cls, value: object) -> "SuccessMarker":
        fields = _object(value, {"identity", "upstream_versions"}, "success marker")
        versions = fields["upstream_versions"]
        if type(versions) is not list:
            raise ValueError("success marker upstream_versions must be a list")
        return cls(
            ExecutionIdentity.from_dict(fields["identity"]),
            tuple(AssetVersion.from_dict(version) for version in versions),
        )

    @classmethod
    def read(cls, path: Path) -> "SuccessMarker":
        return cls.from_dict(read_json(path))


@dataclass(frozen=True)
class ProcessInfo:
    """只记录已知的退出信息，避免把 Pipes 异常误报成带 returncode。"""

    exit_code: int | None = None
    exception_type: str | None = None
    message: str | None = None

    def __post_init__(self) -> None:
        if self.exit_code is not None and type(self.exit_code) is not int:
            raise TypeError("process exit_code must be an integer")
        for label in ("exception_type", "message"):
            value = getattr(self, label)
            if value is not None:
                _nonempty_string(value, f"process {label}")

    def to_dict(self) -> dict[str, Any]:
        value: dict[str, Any] = {}
        if self.exit_code is not None:
            value["exit_code"] = self.exit_code
            if self.exit_code < 0:
                value["signal"] = -self.exit_code
        if self.exception_type:
            value["exception_type"] = self.exception_type
        if self.message:
            value["message"] = self.message
        validate_json(value)
        return value

    @classmethod
    def from_dict(cls, value: object) -> "ProcessInfo":
        if type(value) is not dict:
            raise ValueError("process info must be an object")
        fields = set(value)
        if not fields <= {"exit_code", "signal", "exception_type", "message"}:
            raise ValueError("process info has unknown fields")
        restored = cls(
            exit_code=value.get("exit_code"),
            exception_type=value.get("exception_type"),
            message=value.get("message"),
        )
        if value != restored.to_dict():
            raise ValueError("process info fields do not match known exit information")
        return restored


@dataclass(frozen=True)
class OrchestrationError:
    """编排层错误，与算法 AssetResult 中的协议错误分开。"""

    category: str
    code: str
    message: str
    details: dict[str, Any] = field(default_factory=dict)

    def __post_init__(self) -> None:
        for label in ("category", "code", "message"):
            _nonempty_string(getattr(self, label), f"orchestration error {label}")
        if type(self.details) is not dict:
            raise TypeError("orchestration error details must be a dict")
        validate_json(self.details)

    def to_dict(self) -> dict[str, Any]:
        return {
            "category": self.category,
            "code": self.code,
            "message": self.message,
            "details": self.details,
        }

    @classmethod
    def from_protocol_error(cls, error: Any) -> "OrchestrationError":
        """复制算法协议错误，避免把协议类型存进编排模型。"""
        return cls(error.category, error.code, error.message, dict(error.details))

    @classmethod
    def from_dict(cls, value: object) -> "OrchestrationError":
        fields = _object(value, {"category", "code", "message", "details"}, "orchestration error")
        return cls(**fields)


@dataclass(frozen=True)
class FailureSummary:
    """Dagster 自有的 `_failure.json` 文档模型。"""

    identity: ExecutionIdentity
    time: str
    error: OrchestrationError
    process: ProcessInfo
    run_id: str | None
    step_key: str | None

    def __post_init__(self) -> None:
        if not isinstance(self.identity, ExecutionIdentity):
            raise TypeError("failure summary identity must be an ExecutionIdentity")
        _nonempty_string(self.time, "failure summary time")
        if not isinstance(self.error, OrchestrationError):
            raise TypeError("failure summary error must be an OrchestrationError")
        if not isinstance(self.process, ProcessInfo):
            raise TypeError("failure summary process must be ProcessInfo")
        for label in ("run_id", "step_key"):
            value = getattr(self, label)
            if value is not None:
                _nonempty_string(value, f"failure summary {label}")

    def to_dict(self) -> dict[str, Any]:
        return {
            "identity": self.identity.to_dict(),
            "time": self.time,
            "error": self.error.to_dict(),
            "process": self.process.to_dict(),
            "run_id": self.run_id,
            "step_key": self.step_key,
        }

    def write(self, path: Path) -> None:
        write_json_atomic(path, self.to_dict())

    @classmethod
    def from_dict(cls, value: object) -> "FailureSummary":
        fields = _object(
            value, {"identity", "time", "error", "process", "run_id", "step_key"},
            "failure summary",
        )
        return cls(
            identity=ExecutionIdentity.from_dict(fields["identity"]),
            time=fields["time"],
            error=OrchestrationError.from_dict(fields["error"]),
            process=ProcessInfo.from_dict(fields["process"]),
            run_id=fields["run_id"],
            step_key=fields["step_key"],
        )

    @classmethod
    def read(cls, path: Path) -> "FailureSummary":
        return cls.from_dict(read_json(path))


def _nonempty_string(value: object, label: str) -> None:
    if not isinstance(value, str) or not value:
        raise TypeError(f"{label} must be a non-empty string")
    validate_json(value)


def _object(value: object, expected: set[str], label: str) -> dict[str, Any]:
    if type(value) is not dict:
        raise ValueError(f"{label} must be an object")
    actual = set(value)
    if actual != expected:
        raise ValueError(
            f"{label} fields mismatch; missing={sorted(expected - actual)}, "
            f"unknown={sorted(actual - expected)}"
        )
    return value
