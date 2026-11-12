from .mllm_processor import build_multimodal_inputs, load_mllm_processor
import importlib
import os
from typing import Any, Optional
from cozy.engineer_base import LoggerLevel, huey_log
import torch
import transformers

MODULE_NAME = 'cozy.mllm_base.mllm_infer'


class MLLMInfer:

    def __init__(
        self,
        model_pt_path: str,
        max_new_tokens: int = 300,
        local_files_only: bool = True,
        device_map: str = 'auto',
        torch_dtype: Optional[Any] = None,
        use_lora: bool = False,
        lora_path: Optional[str] = None,
    ):

        if torch_dtype is None:
            torch_dtype = torch.bfloat16

        self.processor = load_mllm_processor(
            model_pt_path,
            local_files_only,
        )

        model_kwargs = {
            'local_files_only': local_files_only,
            'device_map': device_map,
        }
        if torch_dtype is not None:
            model_kwargs['torch_dtype'] = torch_dtype

        model_cls = getattr(transformers, 'AutoModelForImageTextToText')  # AutoModelForVision2Seq
        self.model = model_cls.from_pretrained(model_pt_path, **model_kwargs)
        if use_lora:
            huey_log(
                LoggerLevel.INFO,
                f'use_lora=True, start loading LoRA adapter from: {lora_path}',
                module_name=MODULE_NAME,
            )
            self.model = self._load_lora_adapter(
                model=self.model,
                lora_path=lora_path,
                local_files_only=local_files_only,
            )
            huey_log(
                LoggerLevel.INFO,
                'LoRA adapter loaded successfully.',
                module_name=MODULE_NAME,
            )
        self.max_new_tokens = max_new_tokens

    @staticmethod
    def _load_lora_adapter(model: Any, lora_path: Optional[str], local_files_only: bool) -> Any:
        if not lora_path:
            raise ValueError('use_lora is True, but lora_path is empty')

        resolved_lora_path = os.path.abspath(os.path.expanduser(lora_path))
        if not os.path.exists(resolved_lora_path):
            raise FileNotFoundError(f'LoRA path not found: {resolved_lora_path}')

        peft_module = importlib.import_module('peft')
        peft_model_cls = getattr(peft_module, 'PeftModel')

        return peft_model_cls.from_pretrained(
            model,
            resolved_lora_path,
            local_files_only=local_files_only,
        )

    def infer(self, messages: list[dict[str, Any]]) -> str:
        responses = self.infer_batch(messages_batch=[messages])
        return responses[0] if responses else ''

    def infer_batch(
        self,
        batch: Optional[dict[str, Any]] = None,
        messages_batch: Optional[list[list[dict[str, Any]]]] = None,
    ) -> list[str]:
        if batch is not None and batch.get('inputs') is not None:
            inputs = batch['inputs'].to(self.model.device)
            input_lengths = batch.get('input_lengths') or inputs['attention_mask'].sum(dim=1).tolist()
        elif messages_batch is not None:
            inputs = build_multimodal_inputs(
                processor=self.processor,
                messages_batch=messages_batch,
            ).to(self.model.device)
            input_lengths = inputs['attention_mask'].sum(dim=1).tolist()
        else:
            raise ValueError('infer_batch requires either batch inputs or messages_batch')

        outputs = self.model.generate(
            **inputs,
            max_new_tokens=self.max_new_tokens,
            do_sample=False,
            pad_token_id=self.processor.tokenizer.pad_token_id,
        )

        responses = []
        for index in range(len(outputs)):
            generated_tokens = outputs[index][int(input_lengths[index]):]
            responses.append(self.processor.decode(generated_tokens, skip_special_tokens=True))
        return responses
