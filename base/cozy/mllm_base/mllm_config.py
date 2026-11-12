from .mllm_infer_engine import MLLMEngineConfig

from dataclasses import dataclass
from dataclasses import fields
from dataclasses import replace
from typing import Any
from typing import Mapping
from typing import Optional

from pydantic import BaseModel


@dataclass(frozen=True)
class MLLMBaseConfig:
    model_pt_path: str = ''
    output_datastructure: Optional[type[BaseModel]] = None
    output_path: str = ''
    max_retries: int = 3
    batch_size: int = 20
    max_new_tokens: int = 300
    local_files_only: bool = True
    device_map: str = 'auto'
    torch_dtype: Optional[Any] = None

    def with_overrides(
        self,
        overrides: Optional[Mapping[str, Any]] = None,
        **kwargs: Any,
    ) -> 'MLLMBaseConfig':
        merged_overrides = dict(overrides or {})
        merged_overrides.update(kwargs)
        field_names = {field.name for field in fields(self)}
        normalized_overrides = {}

        for key, value in merged_overrides.items():
            if key not in field_names:
                continue
            if value is None:
                continue
            normalized_overrides[key] = value

        if not normalized_overrides:
            return self
        return replace(self, **normalized_overrides)

    def get_missing_required_fields(self, field_names: list[str]) -> list[str]:
        missing_fields = []

        for field_name in field_names:
            field_value = getattr(self, field_name)
            if field_value is None:
                missing_fields.append(field_name)
                continue

            if isinstance(field_value, str) and not field_value.strip():
                missing_fields.append(field_name)

        return missing_fields

    def to_engine_config(self) -> MLLMEngineConfig:
        if self.output_datastructure is None:
            raise ValueError('output_datastructure is required for MLLM inference')

        return MLLMEngineConfig(
            model_pt_path=self.model_pt_path,
            output_datastructure=self.output_datastructure,
            output_path=self.output_path,
            max_retries=self.max_retries,
            batch_size=self.batch_size,
            max_new_tokens=self.max_new_tokens,
            local_files_only=self.local_files_only,
            device_map=self.device_map,
            torch_dtype=self.torch_dtype,
        )
