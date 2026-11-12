"""加载资产执行规格，并基于已有规格解析启动命令。"""

from __future__ import annotations

import sys
from dataclasses import dataclass
from pathlib import Path

import yaml

from dagster_pipeline.configuration import ConfigurationError, default_runtime_root, rendered_config_root


@dataclass(frozen=True)
class AssetExecutionSpec:
    asset_name: str
    data_version: str
    config_path: Path
    module: str | None = None
    executable: Path | None = None

    def __post_init__(self) -> None:
        if (self.module is None) == (self.executable is None):
            raise ConfigurationError(f"{self.asset_name}: specify exactly one of module or executable")
        if self.module is not None and (
            not isinstance(self.module, str) or not self.module.strip() or "\0" in self.module
        ):
            raise ConfigurationError(f"{self.asset_name}: module must be a nonblank string without NUL")


def load_asset_execution_specs(
    path: Path | None = None, *, runtime_root: Path | None = None,
) -> dict[str, AssetExecutionSpec]:
    path = (
        Path(path)
        if path is not None
        else rendered_config_root(runtime_root) / "dagster/assets.yaml"
    ).resolve()
    document = yaml.safe_load(path.read_text(encoding="utf-8"))
    if not isinstance(document, dict):
        raise ConfigurationError(f"{path}: assets must be a mapping")
    root = (runtime_root or default_runtime_root()).expanduser().resolve()
    specs: dict[str, AssetExecutionSpec] = {}
    for asset_name, fields in document.items():
        fields = dict(fields)
        fields["config_path"] = (path.parent / fields["config_path"]).resolve()
        if "executable" in fields:
            value = fields["executable"]
            if not isinstance(value, str) or not value.strip() or "\0" in value:
                raise ConfigurationError(f"{asset_name}: executable must be a nonblank path without NUL")
            executable = Path(value).expanduser()
            fields["executable"] = (executable if executable.is_absolute() else root / executable).resolve()
        specs[asset_name] = AssetExecutionSpec(asset_name=asset_name, **fields)
    return specs


def get_asset_execution_spec(
    asset_name: str, *, runtime_root: Path | None = None
) -> AssetExecutionSpec:
    return load_asset_execution_specs(runtime_root=runtime_root)[asset_name]


def resolve_command(spec: AssetExecutionSpec) -> tuple[str, ...]:
    """从已加载的执行规格解析命令，不重新读取登记文件。"""
    if spec.module:
        return (sys.executable, "-m", spec.module)
    return (str(spec.executable),)
