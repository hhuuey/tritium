"""执行调用者提供的命令并检查协议结果，不查询登记或管理产物缓存。"""

from __future__ import annotations

import os
from collections.abc import Callable, Mapping, Sequence
from pathlib import Path
import subprocess
from typing import Any

from dagster_pipeline.orchestration.asset_diagnostics import (
    AssetExecutionError,
    process_info_from_exception,
    process_info_from_return,
)
from dagster_pipeline.orchestration.execution_models import (
    OrchestrationError,
    ProcessInfo,
)
from asset_protocol import AssetRequest, AssetResult, read_result, validate_result_identity, write_request

CommandExecutor = Callable[[Sequence[str], Mapping[str, str] | None], Any]
Command = Sequence[str] | Callable[[], Sequence[str]]


def invoke_asset(
    request: AssetRequest,
    directory: Path,
    command: Command,
    env: Mapping[str, str] | None,
    executor: CommandExecutor | None,
) -> tuple[AssetResult, ProcessInfo]:
    result_path = directory / "_result.json"
    try:
        actual_command = command() if callable(command) else command
        if not actual_command:
            raise ValueError("Asset command must not be empty")
        request_path = directory / "_request.json"
        write_request(request_path, request)
        full_command = [*actual_command, "--request", str(request_path), "--result", str(result_path)]
    except Exception as error:
        raise AssetExecutionError(
            OrchestrationError(
                "process", "PROCESS_START_FAILED", "Failed to prepare asset process",
                {"exception": f"{type(error).__name__}: {error}"},
            ),
            process=process_info_from_exception(error),
        ) from error
    start_failed = False
    execution_error: Exception | None = None
    try:
        if executor is None:
            completed = subprocess.run(
                full_command, check=False, env={**os.environ, **(env or {})}, text=True
            )
        else:
            completed = executor(full_command, env)
        process = process_info_from_return(completed)
    except Exception as error:
        # 取消异常继承 BaseException，直接传播，不写成算法失败。
        process = process_info_from_exception(error)
        start_failed = isinstance(error, OSError)
        execution_error = error
    result, result_error = _read_result(request, result_path)
    try:
        return _classify_result(result, result_error, process, start_failed=start_failed), process
    except AssetExecutionError as error:
        if execution_error is not None:
            raise error from execution_error
        if result_error is not None:
            raise error from result_error
        raise


def _read_result(
    request: AssetRequest, path: Path
) -> tuple[AssetResult | None, Exception | None]:
    try:
        result = read_result(path)
        validate_result_identity(request, result)
        return result, None
    except Exception as error:
        return None, error


def _classify_result(
    result: AssetResult | None,
    result_error: Exception | None,
    process: ProcessInfo,
    *,
    start_failed: bool,
) -> AssetResult:
    abnormal = process.exit_code not in (None, 0) or process.exception_type is not None
    if result is not None:
        if result.status == "failed":
            # 外部协议错误保留原 category/code/message/details，进程信息单独保存。
            raise AssetExecutionError(
                OrchestrationError.from_protocol_error(result.error),
                process=process,
                result=result,
            )
        if abnormal:
            raise AssetExecutionError(
                OrchestrationError(
                    "process", "PROCESS_FAILED", "Process failed after writing a success result",
                    {"result_status": result.status, "process": process.to_dict()},
                ),
                process=process,
                result=result,
            )
        return result
    if start_failed:
        code, message, category = "PROCESS_START_FAILED", "Failed to start asset process", "process"
    elif abnormal:
        code, message, category = "PROCESS_FAILED", "Asset process failed", "process"
    elif isinstance(result_error, FileNotFoundError):
        code, message, category = "RESULT_MISSING", "Asset result is missing", "protocol"
    else:
        code, message, category = "RESULT_INVALID", "Asset result is invalid", "protocol"
    details: dict[str, Any] = {"process": process.to_dict()}
    if result_error is not None:
        details["result_error"] = f"{type(result_error).__name__}: {result_error}"
    if process.message:
        message = f"{message}: {process.message}"
    raise AssetExecutionError(
        OrchestrationError(category, code, message, details), process=process
    )
