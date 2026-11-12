from importlib import import_module
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .mllm_config import MLLMBaseConfig
    from .mllm_dataset import MLLMDataset
    from .mllm_infer import MLLMInfer
    from .mllm_infer_engine import MLLMEngineConfig, MLLMInferEngine
    from .mllm_path_dataset import (
        MLLMImagePathInferSample,
        MLLMPathDataset,
        MLLMPathDatasetConfig,
        build_image_path_infer_sample,
    )
    from .mllm_prompt import generate_feedback_messages, load_prompt_pair, load_prompt_text
    from .mllm_retry_manager import MLLMRetryItem, MLLMRetryManager
    from .mllm_runner import MLLMLocalRunnerConfig, normalize_mllm_samples, run_local_dataset, run_mllm_engine
    from .mllm_verify import MLLMOutputValidator, ValidationResult

_EXPORT_MAP = {
    'MLLMBaseConfig': '.mllm_config',
    'MLLMDataset': '.mllm_dataset',
    'MLLMEngineConfig': '.mllm_infer_engine',
    'MLLMInfer': '.mllm_infer',
    'MLLMInferEngine': '.mllm_infer_engine',
    'MLLMImagePathInferSample': '.mllm_path_dataset',
    'MLLMLocalRunnerConfig': '.mllm_runner',
    'MLLMOutputValidator': '.mllm_verify',
    'MLLMPathDataset': '.mllm_path_dataset',
    'MLLMPathDatasetConfig': '.mllm_path_dataset',
    'MLLMRetryItem': '.mllm_retry_manager',
    'MLLMRetryManager': '.mllm_retry_manager',
    'ValidationResult': '.mllm_verify',
    'build_image_path_infer_sample': '.mllm_path_dataset',
    'generate_feedback_messages': '.mllm_prompt',
    'load_prompt_pair': '.mllm_prompt',
    'load_prompt_text': '.mllm_prompt',
    'normalize_mllm_samples': '.mllm_runner',
    'run_local_dataset': '.mllm_runner',
    'run_mllm_engine': '.mllm_runner',
}

__all__ = list(_EXPORT_MAP)


def __getattr__(name):
    if name not in _EXPORT_MAP:
        raise AttributeError(f"module '{__name__}' has no attribute '{name}'")

    module = import_module(_EXPORT_MAP[name], package=__name__)
    value = getattr(module, name)
    globals()[name] = value
    return value
