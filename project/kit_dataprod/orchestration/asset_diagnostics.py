"""资产执行失败的结构化诊断与进程信息。"""

from __future__ import annotations

from datetime import datetime, timezone
import logging
from pathlib import Path
from typing import Any

from asset_protocol import AssetRequest, AssetResult

from dagster_pipeline.orchestration.execution_models import (
    ExecutionIdentity,
    FailureSummary,
    OrchestrationError,
    ProcessInfo,
)


class ProcessExecutionError(RuntimeError):
    """执行适配器提供的进程异常，通用层不依赖具体执行框架。"""

    def __init__(self, process: ProcessInfo) -> None:
        self.process = process
        super().__init__(process.message or "Process execution failed")


class AssetExecutionError(RuntimeError):
    """携带编排错误、进程信息和失败摘要路径的统一执行异常。"""

    def __init__(
        self,
        error: OrchestrationError,
        *,
        process: ProcessInfo | None = None,
        failure_path: Path | None = None,
        result: AssetResult | None = None,
    ) -> None:
        self.error = error
        self.process = process or ProcessInfo()
        self.failure_path = failure_path
        self.result = result
        suffix = f"; failure={failure_path}" if failure_path else ""
        super().__init__(f"{self.error.code}: {self.error.message}{suffix}")


def process_info_from_exception(error: BaseException) -> ProcessInfo:
    """读取适配器提供的信息或通用异常的 returncode。"""
    if isinstance(error, ProcessExecutionError):
        return error.process
    exit_code = getattr(error, "returncode", None)
    if not isinstance(exit_code, int):
        exit_code = None
    return ProcessInfo(
        exit_code=exit_code,
        exception_type=type(error).__name__,
        message=str(error) or None,
    )


def process_info_from_return(value: Any) -> ProcessInfo:
    exit_code = getattr(value, "returncode", value if isinstance(value, int) else None)
    return ProcessInfo(exit_code=exit_code if isinstance(exit_code, int) else None)


def write_failure_summary(
    path: Path,
    request: AssetRequest,
    *,
    group_name: str,
    error: OrchestrationError,
    process: ProcessInfo,
    run_id: str | None,
    step_key: str | None,
    logger: Any = None,
) -> None:
    """原子写入失败摘要；写摘要失败时只记录日志，不覆盖原始异常。"""
    summary = FailureSummary(
        identity=ExecutionIdentity(
            protocol_version=request.protocol_version,
            group_name=group_name,
            asset_name=request.asset_name,
            case_id=request.case_id,
            data_version=request.data_version,
            scene_id=request.scene_id,
        ),
        time=datetime.now(timezone.utc).isoformat(),
        error=error,
        process=process,
        run_id=run_id,
        step_key=step_key,
    )
    try:
        summary.write(path)
    except Exception:
        target = logger if logger is not None else logging.getLogger(__name__)
        try:
            target.error("Failed to write asset failure summary %s", path, exc_info=True)
        except Exception:
            logging.getLogger(__name__).error(
                "Failed to write asset failure summary %s", path, exc_info=True
            )
