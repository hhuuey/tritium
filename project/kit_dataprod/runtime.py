from __future__ import annotations

import os
import shlex
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path

import dagster as dg
import yaml

from dagster_pipeline.configuration import default_artifact_root, default_runtime_root, rendered_config_root

POOL_NAMES = (
    "nas_io",
    "cpu_heavy",
    "gpu_calibration",
    "case_raster_map",
    "scene_raster_maps",
    "gpu_segmentation",
    "gpu_vector_map",
    "gpu_training",
)


@dataclass(frozen=True)
class RuntimeConfig:
    environment: str
    config_root: Path
    runtime_root: Path
    dagster_home: Path
    artifact_root: Path
    manifest_root: Path
    grpc_timeout_seconds: int
    host: str
    port: int
    pools: dict[str, int | None]

    def environment_variables(self) -> dict[str, str]:
        return {
            "DAGSTER_PIPELINE_ENVIRONMENT": self.environment,
            "DAGSTER_HOME": str(self.dagster_home),
            "DAGSTER_PIPELINE_ARTIFACT_ROOT": str(self.artifact_root),
            "DAGSTER_PIPELINE_MANIFEST_ROOT": str(self.manifest_root),
            "DAGSTER_PIPELINE_RUNTIME_ROOT": str(self.runtime_root),
            "DAGSTER_GRPC_TIMEOUT_SECONDS": str(self.grpc_timeout_seconds),
            "DAGSTER_PIPELINE_DAGSTER_HOST": self.host,
            "DAGSTER_PIPELINE_DAGSTER_PORT": str(self.port),
        }

    def shell_exports(self) -> str:
        return "\n".join(
            f"export {name}={shlex.quote(value)}"
            for name, value in self.environment_variables().items()
        )


def load_runtime_config(
    *,
    config_root: Path | None = None,
    environ: Mapping[str, str] | None = None,
) -> RuntimeConfig:
    environment_values = os.environ if environ is None else environ
    runtime_root = default_runtime_root(environment_values)
    bootstrap_artifact_root = default_artifact_root(environment_values)
    root = (
        Path(config_root).expanduser().resolve()
        if config_root is not None
        else rendered_config_root(runtime_root)
    )
    path = root / "dagster" / "runtime.yaml"
    try:
        content = path.read_text(encoding="utf-8")
    except FileNotFoundError:
        if config_root is not None:
            raise
        raise FileNotFoundError(
            f"缺少安装的 runtime 配置：{path}；请先执行 dagster-pipeline render-configs。"
        ) from None
    payload = yaml.safe_load(content)
    environment = payload.get("environment")
    if not isinstance(environment, str) or not environment.strip() or any(
        character in environment for character in ("\0", "\n", "\r")
    ):
        raise ValueError("runtime.yaml environment must be a nonblank single-line string")
    paths = payload["paths"]
    dagster = payload["dagster"]
    raw_pools = payload["pools"]

    pools = {
        name: _pool_limit(
            environment_values.get(
                f"DAGSTER_PIPELINE_POOL_{name.upper()}_LIMIT", raw_pools[name]
            ),
        )
        for name in POOL_NAMES
    }
    if pools["nas_io"] != 1:
        raise ValueError("nas_io pool limit must be 1")
    grpc_timeout_seconds = int(
        environment_values.get(
            "DAGSTER_GRPC_TIMEOUT_SECONDS", dagster["grpc_timeout_seconds"]
        ),
    )
    port = int(
        environment_values.get("DAGSTER_PIPELINE_DAGSTER_PORT", dagster["port"]),
    )
    artifact_root = (
        _runtime_path(
            environment_values.get(
                "DAGSTER_PIPELINE_ARTIFACT_ROOT", paths["artifact_root"]
            ),
            runtime_root,
        )
        if config_root is not None
        else bootstrap_artifact_root
    )
    if "DAGSTER_PIPELINE_MANIFEST_ROOT" in environment_values:
        manifest_root = _runtime_path(
            environment_values["DAGSTER_PIPELINE_MANIFEST_ROOT"],
            runtime_root,
        )
    elif "DAGSTER_PIPELINE_ARTIFACT_ROOT" in environment_values:
        manifest_root = artifact_root / "manifests"
    else:
        manifest_root = _runtime_path(
            paths["manifest_root"],
            runtime_root,
        )

    return RuntimeConfig(
        environment=environment,
        config_root=root,
        runtime_root=runtime_root,
        dagster_home=_runtime_path(
            environment_values.get("DAGSTER_HOME", paths["dagster_home"]),
            runtime_root,
        ),
        artifact_root=artifact_root,
        manifest_root=manifest_root,
        grpc_timeout_seconds=grpc_timeout_seconds,
        host=str(
            environment_values.get("DAGSTER_PIPELINE_DAGSTER_HOST", dagster["host"])
        ),
        port=port,
        pools=pools,
    )


def apply_pool_limits(instance: dg.DagsterInstance, config: RuntimeConfig) -> None:
    for pool_name, limit in config.pools.items():
        if limit is None:
            instance.event_log_storage.delete_concurrency_limit(pool_name)
        else:
            instance.event_log_storage.set_concurrency_slots(pool_name, limit)


def _pool_limit(value: object) -> int | None:
    if value is None or str(value).lower() in {"none", "unlimited"}:
        return None
    return int(value)


def _runtime_path(value: str, project_root: Path) -> Path:
    path = Path(value).expanduser()
    return (path if path.is_absolute() else project_root / path).resolve()
