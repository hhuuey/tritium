import json
import re

from dataclasses import dataclass, field
from typing import Any, Optional

from pydantic import BaseModel, ValidationError


@dataclass
class ValidationResult:
    is_valid: bool
    raw_response: str
    parsed_output: Optional[BaseModel] = None
    parsed_dict: Optional[dict[str, Any]] = None
    errors: list[str] = field(default_factory=list)


class MLLMOutputValidator:

    def __init__(self, output_datastructure: type[BaseModel]):
        if not issubclass(output_datastructure, BaseModel):
            raise TypeError('output_datastructure must inherit from pydantic.BaseModel')

        self.output_datastructure = output_datastructure

    def extract_json(self, response: str) -> tuple[Optional[str], Optional[str]]:
        response = self._strip_thinking_blocks(response)
        code_block_patterns = [r'```(?:json)?\s*\n([\s\S]*?)\n```', r'```([\s\S]*?)```']

        for pattern in code_block_patterns:
            matches = re.findall(pattern, response, re.DOTALL)
            for match in matches:
                cleaned = match.strip()
                if cleaned.startswith('{') and cleaned.endswith('}'):
                    try:
                        json.loads(cleaned)
                        return cleaned, 'code_block'
                    except json.JSONDecodeError:
                        continue

        json_pattern = r'\{[^{}]*(?:\{[^{}]*\}[^{}]*)*\}'
        matches = re.findall(json_pattern, response, re.DOTALL)
        for match in matches:
            try:
                json.loads(match)
                return match, 'direct'
            except json.JSONDecodeError:
                continue

        cleaned = response.strip()
        if cleaned.startswith('{') and cleaned.endswith('}'):
            try:
                json.loads(cleaned)
                return cleaned, 'full_response'
            except json.JSONDecodeError:
                return None, None

        return None, None

    @staticmethod
    def _strip_thinking_blocks(response: str) -> str:
        # Qwen3.5 may emit reasoning blocks wrapped by <think>...</think>.
        return re.sub(r'<think>[\s\S]*?</think>', '', response, flags=re.DOTALL).strip()

    def validate(self, response: str) -> ValidationResult:
        json_str, _ = self.extract_json(response)
        if not json_str:
            return ValidationResult(
                is_valid=False,
                raw_response=response,
                errors=['未找到有效的 JSON 对象，请输出标准 JSON。'],
            )

        try:
            parsed = json.loads(json_str)
        except json.JSONDecodeError as exc:
            return ValidationResult(
                is_valid=False,
                raw_response=response,
                errors=[f'JSON 解析失败: {exc}'],
            )

        try:
            parsed_output = self._model_validate(parsed)
        except ValidationError as exc:
            return ValidationResult(
                is_valid=False,
                raw_response=response,
                errors=self._format_validation_errors(exc),
            )

        parsed_dict = self._model_dump(parsed_output)
        return ValidationResult(
            is_valid=True,
            raw_response=response,
            parsed_output=parsed_output,
            parsed_dict=parsed_dict,
        )

    def format_feedback(self, validation_result: ValidationResult) -> str:
        if validation_result.is_valid:
            return ''

        feedback_lines = ['输出格式验证失败，请重新生成。', '问题如下:']
        for index, error in enumerate(validation_result.errors, start=1):
            feedback_lines.append(f'{index}. {error}')

        feedback_lines.append('')
        feedback_lines.append('请输出一个能够通过以下 Pydantic 数据结构校验的 JSON:')
        feedback_lines.append(self._model_schema_text())
        return '\n'.join(feedback_lines)

    def _format_validation_errors(self, exc: ValidationError) -> list[str]:
        formatted_errors = []
        for error in exc.errors():
            location = '.'.join(str(item) for item in error.get('loc', []))
            message = error.get('msg', 'unknown validation error')
            if location:
                formatted_errors.append(f'{location}: {message}')
            else:
                formatted_errors.append(message)
        return formatted_errors

    def _model_schema_text(self) -> str:
        if hasattr(self.output_datastructure, 'model_json_schema'):
            return json.dumps(self.output_datastructure.model_json_schema(), ensure_ascii=False, indent=2)
        return self.output_datastructure.schema_json(indent=2, ensure_ascii=False)

    def _model_validate(self, parsed: dict[str, Any]) -> BaseModel:
        if hasattr(self.output_datastructure, 'model_validate'):
            return self.output_datastructure.model_validate(parsed)
        return self.output_datastructure.parse_obj(parsed)

    def _model_dump(self, parsed_output: BaseModel) -> dict[str, Any]:
        if hasattr(parsed_output, 'model_dump'):
            return parsed_output.model_dump()
        return parsed_output.dict()
