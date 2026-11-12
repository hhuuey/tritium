from .mllm_processor import build_multimodal_inputs, load_mllm_processor

import json

from pathlib import Path
from typing import Any, Optional
import numpy


class MLLMDataset:

    def __init__(self, config: Any):
        self.config = config
        self.processor = load_mllm_processor(
            config.model_pt_path,
            getattr(config, 'local_files_only', True),
        )

    def build_sample(self, index: int) -> dict[str, Any]:
        raise NotImplementedError('subclass must implement build_sample')

    def __getitem__(self, index: int) -> dict[str, Any]:
        sample = dict(self.build_sample(index))
        return self._normalize_sample(sample)

    def _normalize_sample(self, sample: dict[str, Any]) -> dict[str, Any]:
        if 'sample_id' not in sample:
            raise ValueError('sample must contain sample_id')

        if 'messages' not in sample:
            prompt_text = sample.get('prompt_text', '')
            image_paths = sample.get('images', [])
            system_prompt = sample.get('system_prompt')
            sample['messages'] = self.build_messages(prompt_text, image_paths, system_prompt)

        return sample

    def build_messages(
        self,
        prompt_text: str,
        image_paths: list[str],
        system_prompt: Optional[str] = None,
    ) -> list[dict[str, Any]]:
        messages = []
        if system_prompt:
            messages.append({
                'role': 'system',
                'content': [{
                    'type': 'text',
                    'text': system_prompt
                }],
            })

        user_content = []
        for image_path in image_paths:
            user_content.append({'type': 'image', 'image': image_path})
        if prompt_text:
            user_content.append({'type': 'text', 'text': prompt_text})

        messages.append({'role': 'user', 'content': user_content})
        return messages

    def collate_fn(self, batch: list[dict[str, Any]]) -> dict[str, Any]:
        samples = []
        failed_samples = []
        messages_batch = []

        for sample in batch:
            try:
                normalized_sample = self._normalize_sample(dict(sample))
                samples.append(normalized_sample)
                messages_batch.append(normalized_sample['messages'])
            except Exception as exc:
                failed_samples.append({'sample': dict(sample), 'error': str(exc)})

        if not samples:
            return {
                'samples': [],
                'failed_samples': failed_samples,
                'inputs': None,
                'input_lengths': [],
            }

        try:
            inputs = build_multimodal_inputs(
                processor=self.processor,
                messages_batch=messages_batch,
            )
        except Exception as exc:
            failed_samples.extend({'sample': sample, 'error': str(exc)} for sample in samples)
            return {
                'samples': [],
                'failed_samples': failed_samples,
                'inputs': None,
                'input_lengths': [],
            }

        return {
            'samples': samples,
            'failed_samples': failed_samples,
            'inputs': inputs,
            'input_lengths': inputs['attention_mask'].sum(dim=1).tolist(),
        }

    def create_dataloader(
        self,
        batch_size: Optional[int] = None,
        shuffle: bool = False,
        num_workers: int = 0,
    ) -> Any:
        from torch.utils.data import DataLoader

        return DataLoader(
            self,
            batch_size=batch_size or getattr(self.config, 'batch_size', 1),
            shuffle=shuffle,
            num_workers=num_workers,
            collate_fn=self.collate_fn,
        )

    def save_sample_result(
        self,
        sample: dict[str, Any],
        parsed_output: dict[str, Any],
        raw_response: str,
    ) -> None:
        sample_output_path = sample.get('sample_output_path')
        if not sample_output_path:
            return

        output_path = Path(sample_output_path)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        with output_path.open('w', encoding='utf-8') as file:
            json.dump(
                {
                    'sample_id': sample['sample_id'],
                    'result': parsed_output,
                    'raw_response': raw_response,
                },
                file,
                ensure_ascii=False,
                indent=2,
            )
