from __future__ import annotations

import logging
import shutil
from collections.abc import Mapping
from copy import deepcopy
from dataclasses import dataclass, replace
from pathlib import Path
from typing import Any

from dagster_pipeline.orchestration.asset_diagnostics import (
    AssetExecutionError,
    write_failure_summary,
)
from dagster_pipeline.orchestration.asset_process import (
    Command,
    CommandExecutor,
    invoke_asset,
)
from dagster_pipeline.orchestration.artifact_lock import artifact_execution_locks
from dagster_pipeline.orchestration.artifact_store import ArtifactStore
from asset_protocol import AssetRequest, ArtifactRef, PROTOCOL_VERSION, validate
from dagster_pipeline.orchestration.execution_models import (
    OrchestrationError,
    ProcessInfo,
    SuccessMarker,
)


@dataclass(frozen=True)
class AssetExecution:
    artifact: ArtifactRef
    cache_hit: bool
    metrics: dict[str, Any]


@dataclass(frozen=True)
class SubprocessAssetRunner:
    """在规范资产目录中执行算法，按执行身份复用缓存。"""

    artifact_store: ArtifactStore

    def run(
        self,
        request: AssetRequest,
        command: Command,
        *,
        group_name: str,
        force_recompute: bool = False,
        env: Mapping[str, str] | None = None,
        executor: CommandExecutor | None = None,
        run_id: str | None = None,
        step_key: str | None = None,
        logger: Any = None,
    ) -> AssetExecution:
        if request.protocol_version != PROTOCOL_VERSION:
            raise ValueError(f"Unsupported asset protocol: {request.protocol_version!r}")
        validate(request)
        request = deepcopy(request)
        directory = self.artifact_store.artifact_directory(
            case_id=request.case_id,
            group_name=group_name,
            asset_name=request.asset_name,
            data_version=request.data_version,
            scene_id=request.scene_id,
        )
        request = replace(request, output_uri=str(directory / "payload"))
        directory.parent.mkdir(parents=True, exist_ok=True)
        input_directories: set[Path] = set()
        while True:
            with artifact_execution_locks(directory, tuple(input_directories)):
                try:
                    resolved = set(self.artifact_store.managed_input_directories(request))
                    if directory in resolved:
                        raise ValueError(f"Asset {request.asset_name!r} cannot depend on itself")
                    # 先锁直接输入，再读取其成功标记中保存的祖先版本。
                    if not resolved.issubset(input_directories):
                        input_directories = resolved
                        continue
                    expected_marker = self.artifact_store.success_marker(request, group_name=group_name)
                    resolved.update(self.artifact_store.marker_input_directories(expected_marker))
                    if directory in resolved:
                        raise ValueError(f"Asset {request.asset_name!r} cannot depend on itself")
                    if resolved != input_directories:
                        input_directories = resolved
                        continue
                    self.artifact_store.validate_upstream_versions(expected_marker)
                    return self._run_locked(
                        request=request,
                        directory=directory,
                        command=command,
                        group_name=group_name,
                        force_recompute=force_recompute,
                        env=env,
                        executor=executor,
                        expected_marker=expected_marker,
                    )
                except Exception as error:
                    wrapped = (
                        error if isinstance(error, AssetExecutionError) else AssetExecutionError(
                            OrchestrationError(
                                "input", "INPUT_INVALID", str(error) or type(error).__name__,
                                {"exception_type": type(error).__name__},
                            )
                        )
                    )
                    # 摘要也必须在输出锁内保存，避免覆盖另一执行刚完成的状态。
                    self._record_failure(
                        directory, request, group_name, wrapped, run_id, step_key, logger
                    )
                    if wrapped is error:
                        raise
                    raise wrapped from error

    def _run_locked(
        self,
        *,
        request: AssetRequest,
        directory: Path,
        command: Command,
        group_name: str,
        force_recompute: bool,
        env: Mapping[str, str] | None,
        executor: CommandExecutor | None,
        expected_marker: SuccessMarker,
    ) -> AssetExecution:
        if not force_recompute:
            cached = self.artifact_store.find_cached(
                request,
                group_name=group_name,
                expected_marker=expected_marker,
            )
            if cached is not None:
                return AssetExecution(cached, True, {"cache_hit": 1})
        try:
            (directory / "_SUCCESS.json").unlink(missing_ok=True)
            if directory.exists():
                shutil.rmtree(directory)
            directory.mkdir(parents=True)
        except Exception as error:
            raise AssetExecutionError(
                OrchestrationError("resource", "ARTIFACT_PREPARE_FAILED", str(error), {})
            ) from error
        process = ProcessInfo()
        try:
            result, process = invoke_asset(request, directory, command, env, executor)
            self.artifact_store.complete(
                request,
                result,
                group_name=group_name,
                expected_marker=expected_marker,
            )
        except AssetExecutionError:
            self._remove_success(directory)
            raise
        except Exception as error:
            self._remove_success(directory)
            changed = str(error).startswith("Upstream versions changed while executing")
            raise AssetExecutionError(
                OrchestrationError(
                    "input" if changed else "resource",
                    "INPUT_CHANGED" if changed else "ARTIFACT_PUBLISH_FAILED",
                    str(error) or type(error).__name__,
                    {"exception_type": type(error).__name__},
                ),
                process=process,
            ) from error
        except BaseException:
            # 取消只清理 marker，保持原始 BaseException，不产生算法失败摘要。
            self._remove_success(directory)
            raise
        return AssetExecution(
            self.artifact_store.reference(
                request, group_name=group_name, metadata=result.metadata
            ),
            False,
            {**result.metrics, "cache_hit": 0},
        )

    @staticmethod
    def _remove_success(directory: Path, logger: Any = None) -> None:
        try:
            (directory / "_SUCCESS.json").unlink(missing_ok=True)
        except Exception:
            target = logger if logger is not None else logging.getLogger(__name__)
            try:
                target.error("Failed to remove success marker %s", directory, exc_info=True)
            except Exception:
                logging.getLogger(__name__).error(
                    "Failed to remove success marker %s", directory, exc_info=True
                )

    def _record_failure(
        self,
        directory: Path,
        request: AssetRequest,
        group_name: str,
        error: AssetExecutionError,
        run_id: str | None,
        step_key: str | None,
        logger: Any,
    ) -> None:
        failure_path = directory / "_failure.json"
        error.failure_path = failure_path
        error.args = (f"{error.error.code}: {error.error.message}; failure={failure_path}",)
        write_failure_summary(
            failure_path,
            request,
            group_name=group_name,
            error=error.error,
            process=error.process,
            run_id=run_id,
            step_key=step_key,
            logger=logger,
        )
