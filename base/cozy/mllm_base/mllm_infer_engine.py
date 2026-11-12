import json
import math

from dataclasses import dataclass
from pathlib import Path
from typing import TYPE_CHECKING, Any, Optional

from cozy.engineer_base import LoggerLevel
from cozy.engineer_base import huey_log
from pydantic import BaseModel

from .mllm_retry_manager import MLLMRetryItem, MLLMRetryManager
from .mllm_verify import MLLMOutputValidator

if TYPE_CHECKING:
    from .mllm_dataset import MLLMDataset

MODULE_NAME = 'cozy.mllm_base.mllm_infer_engine'


@dataclass(frozen=True)
class MLLMEngineConfig:
    model_pt_path: str
    output_datastructure: type[BaseModel]
    output_path: str
    max_retries: int = 3
    batch_size: int = 20
    max_new_tokens: int = 300
    local_files_only: bool = True
    device_map: str = 'auto'
    torch_dtype: Optional[Any] = None
    use_lora: bool = False
    lora_path: Optional[str] = None


class MLLMInferEngine:

    def __init__(self, config: MLLMEngineConfig, dataset: 'MLLMDataset'):
        from .mllm_infer import MLLMInfer

        self.config = config
        self.dataset = dataset
        self.infer = MLLMInfer(
            model_pt_path=config.model_pt_path,
            max_new_tokens=config.max_new_tokens,
            local_files_only=config.local_files_only,
            device_map=config.device_map,
            torch_dtype=config.torch_dtype,
            use_lora=config.use_lora,
            lora_path=config.lora_path,
        )
        self.validator = MLLMOutputValidator(config.output_datastructure)
        self.retry_manager = MLLMRetryManager(config.max_retries)
        self._results_by_id: dict[str, dict[str, Any]] = {}
        self._sample_order: list[str] = []

    def run(self) -> list[dict[str, Any]]:
        dataloader = self.dataset.create_dataloader(batch_size=self.config.batch_size)
        dataset_size = len(self.dataset)
        total_batch_count = math.ceil(dataset_size / self.config.batch_size) if dataset_size > 0 else 0
        huey_log(
            LoggerLevel.INFO,
            f'开始推理, dataset_size={dataset_size}, batch_size={self.config.batch_size}',
            module_name=MODULE_NAME,
        )

        for batch_index, batch in enumerate(dataloader, start=1):
            huey_log(
                LoggerLevel.INFO,
                self._build_batch_log_text(
                    batch_index=batch_index,
                    total_batch_count=total_batch_count,
                    batch=batch,
                    is_retry_batch=False,
                ),
                module_name=MODULE_NAME,
            )
            self._handle_batch(batch)

        retry_batch_index = 0
        while self.retry_manager.has_pending():
            pending_count = self.retry_manager.pending_count()
            total_retry_batch_count = math.ceil(pending_count / self.config.batch_size)
            retry_items = self.retry_manager.pop_retry_batch(self.config.batch_size)
            retry_samples = [retry_item.sample for retry_item in retry_items]
            retry_batch = self.dataset.collate_fn(retry_samples)
            retry_batch_index += 1
            huey_log(
                LoggerLevel.INFO,
                self._build_batch_log_text(
                    batch_index=retry_batch_index,
                    total_batch_count=total_retry_batch_count,
                    batch=retry_batch,
                    is_retry_batch=True,
                ),
                module_name=MODULE_NAME,
            )
            self._handle_batch(retry_batch)

        self._write_results()
        success_count = sum(1 for item in self.results() if item.get('status') == 'success')
        failed_count = len(self.results()) - success_count
        huey_log(
            LoggerLevel.INFO,
            f'推理结束, success={success_count}, failed={failed_count}',
            module_name=MODULE_NAME,
        )
        return self.results()

    def results(self) -> list[dict[str, Any]]:
        return [self._results_by_id[sample_id] for sample_id in self._sample_order if sample_id in self._results_by_id]

    def _handle_batch(self, batch: dict[str, Any]) -> None:
        for failed_item in batch.get('failed_samples', []):
            sample = failed_item['sample']
            reason = failed_item['error']
            self._register_sample(sample)
            huey_log(
                LoggerLevel.WARNING,
                f'sample_id={sample["sample_id"]} 预处理失败: {reason}',
                module_name=MODULE_NAME,
            )
            self._submit_or_finalize(sample, reason=reason, raw_response=None)

        samples = batch.get('samples', [])
        if not samples:
            return

        for sample in samples:
            self._register_sample(sample)

        try:
            responses = self.infer.infer_batch(batch=batch)
        except Exception as exc:
            huey_log(LoggerLevel.ERROR, f'infer_batch failed: {exc}', module_name=MODULE_NAME)
            for sample in samples:
                self._submit_or_finalize(sample, reason=f'infer_batch failed: {exc}', raw_response=None)
            return

        if len(responses) != len(samples):
            huey_log(LoggerLevel.ERROR, 'infer_batch response count mismatch', module_name=MODULE_NAME)
            for sample in samples:
                self._submit_or_finalize(sample, reason='infer_batch response count mismatch', raw_response=None)
            return

        for sample, response in zip(samples, responses):
            validation_result = self.validator.validate(response)
            if validation_result.is_valid:
                save_error = self._save_sample_result(sample, validation_result.parsed_dict or {}, response)
                if save_error:
                    huey_log(
                        LoggerLevel.WARNING,
                        f'sample_id={sample["sample_id"]} 结果保存失败: {save_error}',
                        module_name=MODULE_NAME,
                    )
                self._results_by_id[str(sample['sample_id'])] = {
                    'sample_id': str(sample['sample_id']),
                    'status': 'success',
                    'result': validation_result.parsed_dict,
                    'raw_response': response,
                    'errors': [],
                    'retry_count': self.retry_manager.get_retry_count(str(sample['sample_id'])),
                    'save_error': save_error,
                }
                continue

            reason = '\n'.join(validation_result.errors)
            huey_log(
                LoggerLevel.WARNING,
                f'sample_id={sample["sample_id"]} 输出校验失败: {reason}',
                module_name=MODULE_NAME,
            )
            self._submit_or_finalize(sample, reason=reason, raw_response=response)

    def _build_batch_log_text(
        self,
        batch_index: int,
        total_batch_count: int,
        batch: dict[str, Any],
        is_retry_batch: bool,
    ) -> str:
        batch_name = 'retry_batch' if is_retry_batch else 'batch'
        sample_count = len(batch.get('samples', []))
        failed_sample_count = len(batch.get('failed_samples', []))
        if total_batch_count > 0:
            progress_text = f'{batch_index}/{total_batch_count}'
        else:
            progress_text = f'{batch_index}/0'
        return (f'处理 {batch_name}[{progress_text}], '
                f'sample_count={sample_count}, failed_samples={failed_sample_count}')

    def _register_sample(self, sample: dict[str, Any]) -> None:
        sample_id = str(sample['sample_id'])
        if sample_id not in self._sample_order:
            self._sample_order.append(sample_id)

    def _submit_or_finalize(self, sample: dict[str, Any], reason: str, raw_response: Optional[str]) -> None:
        submitted = self.retry_manager.submit(sample=sample, reason=reason, raw_response=raw_response)
        if submitted:
            retry_count = self.retry_manager.get_retry_count(str(sample['sample_id']))
            huey_log(
                LoggerLevel.WARNING,
                f'sample_id={sample["sample_id"]} 已加入重试队列, retry_count={retry_count}',
                module_name=MODULE_NAME,
            )
            return

        sample_id = str(sample['sample_id'])
        huey_log(
            LoggerLevel.ERROR,
            f'sample_id={sample_id} 最终失败: {reason}',
            module_name=MODULE_NAME,
        )
        self._results_by_id[sample_id] = {
            'sample_id': sample_id,
            'status': 'failed',
            'result': None,
            'raw_response': raw_response,
            'errors': [reason],
            'retry_count': self.retry_manager.get_retry_count(sample_id),
            'save_error': None,
        }

    def _save_sample_result(self, sample: dict[str, Any], parsed_output: dict[str, Any],
                            raw_response: str) -> Optional[str]:
        try:
            self.dataset.save_sample_result(sample, parsed_output, raw_response)
        except Exception as exc:
            return str(exc)
        return None

    def _write_results(self) -> None:
        output_path = Path(self.config.output_path)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        with output_path.open('w', encoding='utf-8') as file:
            json.dump(self.results(), file, ensure_ascii=False, indent=2)
        huey_log(LoggerLevel.INFO, f'汇总结果已写入: {output_path}', module_name=MODULE_NAME)
