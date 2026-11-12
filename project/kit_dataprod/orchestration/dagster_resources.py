from collections.abc import Mapping, Sequence
import re
from pathlib import Path
from typing import Callable

import dagster as dg
from dagster._core.errors import DagsterPipesExecutionError

from dagster_pipeline.orchestration.asset_diagnostics import (
    AssetExecutionError,
    ProcessExecutionError,
)
from asset_protocol import AssetRequest
from dagster_pipeline.orchestration.artifact_store import ArtifactStore
from dagster_pipeline.orchestration.runner import AssetExecution, SubprocessAssetRunner
from dagster_pipeline.orchestration.serialization import (
    read_json_value,
    write_json_value,
)
from dagster_pipeline.orchestration.asset_registry import (
    AssetExecutionSpec,
    resolve_command,
)
from dagster_pipeline.orchestration.execution_models import ProcessInfo

DEFAULT_ARTIFACT_ROOT = ".artifacts"
DEFAULT_MANIFEST_ROOT = f"{DEFAULT_ARTIFACT_ROOT}/manifests"


class JsonManifestIOManager(dg.ConfigurableIOManager):
    """按 Case 和资产分组保存轻量引用，内部 op 保留执行标识路径。"""

    base_dir: str = DEFAULT_MANIFEST_ROOT
    artifact_root: str = DEFAULT_ARTIFACT_ROOT

    def handle_output(self, context: dg.OutputContext, obj: object) -> None:
        path = self._path_for_context(context)
        write_json_value(path, obj, artifact_root=Path(self.artifact_root))
        context.add_output_metadata({"manifest_path": dg.MetadataValue.path(path)})

    def load_input(self, context: dg.InputContext) -> object:
        return read_json_value(
            self._path_for_context(context), artifact_root=Path(self.artifact_root)
        )

    def _path_for_context(self, context: dg.OutputContext | dg.InputContext) -> Path:
        upstream = (
            context.upstream_output if isinstance(context, dg.InputContext) else context
        )
        if upstream is None:
            raise ValueError("Input manifest requires an upstream output context")
        root = Path(self.base_dir).expanduser().resolve()
        if context.has_asset_key or upstream.has_asset_key:
            asset_context = context if context.has_asset_key else upstream
            identifier = asset_context.get_asset_identifier()
            if len(asset_context.asset_key.path) != 1 or len(identifier) != 2:
                raise ValueError(
                    "Asset manifest requires a flat asset key and one case partition"
                )
            asset_name, case_id = identifier
            group_name = upstream.asset_spec.group_name
            parts = [case_id, group_name, asset_name]
            if any(
                not part or part in {".", ".."} or "/" in part or "\\" in part
                for part in parts
            ):
                raise ValueError("Manifest identity must contain single path components")
            path = root / case_id / group_name / f"{asset_name}.json"
            if not path.resolve().is_relative_to(root):
                raise ValueError("Manifest path must be inside base_dir")
            return path
        identifier = upstream.get_identifier()
        path = root.joinpath(*identifier)
        return path.parent / f"{path.name}.json"


class AssetRunnerResource(dg.ConfigurableResource):
    """将 Dagster context、Pipes 和失败信息接入通用资产执行器。"""

    artifact_root: str = DEFAULT_ARTIFACT_ROOT
    runtime_root: str | None = None

    def resolve_command(self, spec: AssetExecutionSpec) -> Sequence[str]:
        return resolve_command(spec)

    def run(
        self,
        *,
        context: dg.AssetExecutionContext | dg.OpExecutionContext,
        pipes_subprocess_client: dg.PipesSubprocessClient,
        request: AssetRequest,
        group_name: str,
        command: Sequence[str] | Callable[[], Sequence[str]],
        force_recompute: bool = False,
        env: Mapping[str, str] | None = None,
    ) -> AssetExecution:
        runner = SubprocessAssetRunner(ArtifactStore(Path(self.artifact_root)))

        def execute(
            full_command: Sequence[str], command_env: Mapping[str, str] | None
        ) -> None:
            try:
                pipes_subprocess_client.run(
                    context=context,
                    command=full_command,
                    env=command_env,
                ).get_results(implicit_materializations=False)
            except DagsterPipesExecutionError as error:
                # Pipes 不暴露 returncode，只在适配层识别其确定的错误消息。
                match = re.fullmatch(
                    r"External execution process failed with code (-?\d+)",
                    str(error).strip(),
                )
                raise ProcessExecutionError(
                    ProcessInfo(
                        exit_code=int(match.group(1)) if match else None,
                        exception_type=type(error).__name__,
                        message=str(error) or None,
                    )
                ) from error

        op_context = (
            context.op_execution_context
            if isinstance(context, dg.AssetExecutionContext) else context
        )
        step_key = None
        try:
            step_key = op_context.get_step_execution_context().step.key
        except (AttributeError, RuntimeError):
            pass
        try:
            result = runner.run(
                request,
                command,
                group_name=group_name,
                force_recompute=force_recompute,
                env=env,
                executor=execute,
                run_id=getattr(op_context, "run_id", None),
                step_key=step_key,
                logger=getattr(op_context, "log", None),
            )
        except AssetExecutionError as error:
            metadata: dict[str, object] = {
                "error_category": error.error.category,
                "error_code": error.error.code,
                "error_details": dg.MetadataValue.json(error.error.details),
                "process": dg.MetadataValue.json(error.process.to_dict()),
                "identity": dg.MetadataValue.json({
                    "protocol_version": request.protocol_version,
                    "asset_name": request.asset_name,
                    "group_name": group_name,
                    "case_id": request.case_id,
                    "data_version": request.data_version,
                    "scene_id": request.scene_id,
                }),
            }
            if error.failure_path is not None:
                directory = error.failure_path.parent
                metadata.update({
                    "failure_path": dg.MetadataValue.path(error.failure_path),
                    "request_path": dg.MetadataValue.path(directory / "_request.json"),
                    "result_path": dg.MetadataValue.path(directory / "_result.json"),
                })
            raise dg.Failure(
                description=str(error),
                metadata=metadata,
                allow_retries=False,
            ) from error
        context.log.info(f"asset_name={request.asset_name} cache_hit={result.cache_hit}")
        return result
