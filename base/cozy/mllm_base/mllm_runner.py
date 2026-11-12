from dataclasses import dataclass
from typing import Any, Callable, Optional

from .mllm_infer_engine import MLLMEngineConfig, MLLMInferEngine

LocalResultBuilder = Callable[[dict[str, Any]], dict[str, Any]]
LocalResultSaver = Callable[[dict[str, Any], dict[str, Any], str], None]


@dataclass(frozen=True)
class MLLMLocalRunnerConfig:
    local_test: bool = False
    result_builder: Optional[LocalResultBuilder] = None
    result_saver: Optional[LocalResultSaver] = None


def normalize_mllm_samples(samples: list[dict[str, Any]]) -> list[dict[str, Any]]:
    return [dict(sample, sample_id=str(sample['sample_id'])) for sample in samples]


def run_local_dataset(
    samples: list[dict[str, Any]],
    config: MLLMLocalRunnerConfig,
) -> list[dict[str, Any]]:
    if config.result_builder is None:
        raise ValueError('result_builder is required when local_test is enabled')

    results = []
    for sample in normalize_mllm_samples(samples):
        parsed_output = config.result_builder(sample)
        if config.result_saver is not None:
            config.result_saver(sample, parsed_output, '')
        results.append({
            'sample_id': str(sample['sample_id']),
            'status': 'success',
            'result': parsed_output,
            'raw_response': '',
            'errors': [],
            'retry_count': 0,
            'save_error': None,
        })
    return results


def run_mllm_engine(
    samples: list[dict[str, Any]],
    engine_config: MLLMEngineConfig,
    dataset: Optional[Any] = None,
    local_config: Optional[MLLMLocalRunnerConfig] = None,
) -> list[dict[str, Any]]:
    normalized_samples = normalize_mllm_samples(samples)
    if not normalized_samples:
        return []

    runner_config = local_config or MLLMLocalRunnerConfig()
    if runner_config.local_test:
        return run_local_dataset(normalized_samples, runner_config)

    if dataset is None:
        raise ValueError('dataset is required when local_test is disabled')

    engine = MLLMInferEngine(config=engine_config, dataset=dataset)
    return engine.run()
