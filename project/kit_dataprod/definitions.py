from pathlib import Path

import dagster as dg

from dagster_pipeline.defs.jobs import JOB_SPECS, build_jobs


@dg.definitions
def defs() -> dg.Definitions:
    base_definitions = dg.load_from_defs_folder(
        path_within_project=Path(__file__).parent
    )
    jobs = build_jobs(JOB_SPECS)
    return dg.Definitions.merge(base_definitions, dg.Definitions(jobs=jobs))
