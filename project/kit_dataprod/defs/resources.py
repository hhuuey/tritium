import os
from pathlib import Path

import dagster as dg

from dagster_pipeline.configuration import default_artifact_root, default_runtime_root
from dagster_pipeline.orchestration.dagster_resources import (
    JsonManifestIOManager,
    AssetRunnerResource,
)


@dg.definitions
def resources() -> dg.Definitions:
    artifact_root = str(default_artifact_root())
    manifest_root = os.getenv(
        "DAGSTER_PIPELINE_MANIFEST_ROOT",
        str(Path(artifact_root) / "manifests"),
    )
    return dg.Definitions(
        resources={
            "io_manager": JsonManifestIOManager(
                base_dir=manifest_root, artifact_root=artifact_root
            ),
            "pipes_subprocess_client": dg.PipesSubprocessClient(),
            "asset_runner": AssetRunnerResource(
                artifact_root=artifact_root,
                runtime_root=str(default_runtime_root()),
            ),
        }
    )
