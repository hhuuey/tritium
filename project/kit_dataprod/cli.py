from __future__ import annotations

import argparse
import os
from pathlib import Path
from typing import Sequence

import dagster as dg

from dagster_pipeline.configuration import (
    CaseManifestConfig,
    case_id_from_qfile_uri,
    default_artifact_root,
    load_case_registration_config,
    render_configs,
)
from dagster_pipeline.defs.assets.ingestion import case_manifest
from dagster_pipeline.orchestration.dagster_resources import (
    AssetRunnerResource,
    JsonManifestIOManager,
)
from dagster_pipeline.runtime import apply_pool_limits, load_runtime_config


def register_partition(
    instance: dg.DagsterInstance, partition_name: str, partition_key: str
) -> None:
    if not instance.has_dynamic_partition(partition_name, partition_key):
        instance.add_dynamic_partitions(partition_name, [partition_key])


def load_case_registrations(path: Path) -> list[dict]:
    payload = load_case_registration_config(path)
    root = Path(payload["data_engineering_resource"])

    def resolve_model(model: dict) -> dict:
        return {**model, "model_uri": str((root / model["model_uri"]).resolve())}

    # manifest 使用绝对本地路径；配置文件保留资源根与相对路径，便于部署。
    models = {key: resolve_model(payload[key]) for key in ("segmentation_model", "vector_model")}
    calibration = payload.get("sensor_calibration_models")
    models["sensor_calibration_models"] = (
        {role: resolve_model(model) for role, model in calibration.items()}
        if calibration is not None else None
    )
    return [{"qfile_uri": uri, **models} for uri in payload["qfile_uris"]]


def register_case(
    instance: dg.DagsterInstance,
    *,
    qfile_uri: str,
    segmentation_model: dict,
    vector_model: dict,
    sensor_calibration_models: dict | None = None,
    manifest_root: Path,
    artifact_root: Path | None = None,
) -> str:
    case_id = case_id_from_qfile_uri(qfile_uri)
    register_partition(instance, "cases", case_id)

    config = CaseManifestConfig(
        qfile_uri=qfile_uri,
        segmentation_model=segmentation_model,
        vector_model=vector_model,
        sensor_calibration_models=sensor_calibration_models,
    )
    result = dg.materialize(
        [case_manifest],
        partition_key=case_id,
        instance=instance,
        resources={
            "asset_runner": AssetRunnerResource(
                artifact_root=str(artifact_root or default_artifact_root()),
            ),
            "io_manager": JsonManifestIOManager(
                base_dir=str(manifest_root),
                artifact_root=str(artifact_root or default_artifact_root()),
            ),
        },
        run_config={"ops": {"case_manifest": {"config": config.model_dump()}}},
    )
    if not result.success:
        raise RuntimeError(f"Failed to register {case_id}")
    return case_id


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="dagster-pipeline")
    commands = parser.add_subparsers(dest="command", required=True)
    registration = commands.add_parser("register-case")
    registration.add_argument("--config", type=Path, required=True)
    render = commands.add_parser("render-configs")
    render.add_argument("--output-directory", type=Path)
    render.add_argument("--config-root", type=Path)
    render.add_argument("--vehicle", choices=("7v", "11v"), default="7v", help="选择硬件配置，默认 7v")
    export = commands.add_parser("export-asset-protocol")
    export.add_argument("--output-directory", type=Path, required=True)
    for command in ("runtime-env", "apply-runtime"):
        runtime = commands.add_parser(command)
        runtime.add_argument("--config-root", type=Path)
    return parser


def main(argv: Sequence[str] | None = None) -> None:
    args = build_parser().parse_args(argv)
    if args.command == "export-asset-protocol":
        from asset_protocol.export import export_asset_protocol

        directory, archive = export_asset_protocol(args.output_directory)
        print(directory)
        print(archive)
        return
    if args.command in {"runtime-env", "apply-runtime"}:
        runtime = load_runtime_config(config_root=args.config_root)
        if args.command == "runtime-env":
            print(runtime.shell_exports())
        else:
            os.environ.update(runtime.environment_variables())
            with dg.DagsterInstance.get() as instance:
                apply_pool_limits(instance, runtime)
            for pool, limit in runtime.pools.items():
                print(f"{pool}={limit if limit is not None else 'unlimited'}")
        return
    artifact_root = default_artifact_root()
    if args.command == "render-configs":
        for path in render_configs(
            output_directory=args.output_directory,
            config_root=args.config_root,
            vehicle=args.vehicle,
        ):
            print(path)
        return
    manifest_root = Path(
        os.getenv("DAGSTER_PIPELINE_MANIFEST_ROOT", str(artifact_root / "manifests"))
    )
    registrations = load_case_registrations(args.config)
    with dg.DagsterInstance.get() as instance:
        for registration in registrations:
            print(
                register_case(
                    instance,
                    manifest_root=manifest_root,
                    artifact_root=artifact_root,
                    **registration,
                )
            )
