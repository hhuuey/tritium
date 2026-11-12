from dataclasses import dataclass
from typing import Any, Callable, Optional

from .mllm_dataset import MLLMDataset

PromptBuilder = Callable[[dict[str, Any]], str]
ResultSaver = Callable[[dict[str, Any], dict[str, Any], str], None]


@dataclass(frozen=True)
class MLLMPathDatasetConfig:
    model_pt_path: str
    batch_size: int = 1
    local_files_only: bool = True
    prompt_text: str = ''
    system_prompt: str = ''
    prompt_builder: Optional[PromptBuilder] = None
    system_prompt_builder: Optional[PromptBuilder] = None
    result_saver: Optional[ResultSaver] = None


@dataclass(frozen=True)
class MLLMImagePathInferSample:
    sample_id: str
    image_path: str
    resize_image_path: str = ''
    target_image_path: str = ''
    result_save_dir: str = ''
    sample_fields: Optional[dict[str, Any]] = None
    result_extra_fields: Optional[dict[str, Any]] = None


def build_image_path_infer_sample(
    sample: MLLMImagePathInferSample,
    only_crop_image: bool = False,
) -> dict[str, Any]:
    target_image_path = sample.target_image_path.strip() or sample.image_path.strip()
    resize_image_path = sample.resize_image_path.strip()
    image_paths = [target_image_path]
    if resize_image_path and not only_crop_image:
        image_paths.append(resize_image_path)

    payload = {
        'sample_id': str(sample.sample_id),
        'image_path': target_image_path,
        'image_paths': image_paths,
        'target_image_path': target_image_path,
        'target_resize_image_path': resize_image_path,
        'result_save_dir': sample.result_save_dir,
    }
    if sample.sample_fields:
        payload.update(sample.sample_fields)
    if sample.result_extra_fields:
        payload['result_extra_fields'] = dict(sample.result_extra_fields)
    return payload


class MLLMPathDataset(MLLMDataset):

    def __init__(self, samples: list[dict[str, Any]], config: MLLMPathDatasetConfig):
        super().__init__(config)
        self.samples = [dict(sample) for sample in samples]

    def __len__(self) -> int:
        return len(self.samples)

    def build_sample(self, index: int) -> dict[str, Any]:
        sample = dict(self.samples[index])
        sample['sample_id'] = str(sample['sample_id'])
        sample['images'] = self._resolve_image_paths(sample)

        prompt_text = self.config.prompt_text
        if self.config.prompt_builder is not None:
            prompt_text = self.config.prompt_builder(sample)

        system_prompt = self.config.system_prompt
        if self.config.system_prompt_builder is not None:
            system_prompt = self.config.system_prompt_builder(sample)

        if prompt_text:
            sample['prompt_text'] = prompt_text
        if system_prompt:
            sample['system_prompt'] = system_prompt
        return sample

    def save_sample_result(
        self,
        sample: dict[str, Any],
        parsed_output: dict[str, Any],
        raw_response: str,
    ) -> None:
        if self.config.result_saver is None:
            super().save_sample_result(sample, parsed_output, raw_response)
            return

        self.config.result_saver(sample, parsed_output, raw_response)

    def _resolve_image_paths(self, sample: dict[str, Any]) -> list[str]:
        raw_image_paths = sample.get('image_paths')
        if isinstance(raw_image_paths, list) and raw_image_paths:
            return [str(path) for path in raw_image_paths if str(path)]

        image_path = str(sample.get('image_path', '')).strip()
        if not image_path:
            return []
        return [image_path]
