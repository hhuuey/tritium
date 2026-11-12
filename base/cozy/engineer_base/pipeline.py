import os
import time

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from typing import Any, Dict

from .logger import LoggerLevel, huey_log, huey_log_init
from .summary import reset_summary, set_summary, write_summary


@dataclass
class PipelineRunResult:
    exit_code: int = 0
    summary_updates: Dict[str, Any] = field(default_factory=dict)


class BasePipeline(ABC):
    mode_name = 'base'
    module_name = 'pipeline'

    def __init__(self, save_dir: str, version: str = '', summary_file_name: str = 'summary.json'):
        self.save_dir = os.path.abspath(save_dir)
        self.version = version
        self.summary_file_name = summary_file_name

    def run(self) -> int:
        os.makedirs(self.save_dir, exist_ok=True)
        huey_log_init(
            enable_console_output=True,
            enable_file_output=bool(self.save_dir),
            output_dir=self.save_dir or None,
            max_file_lines=200000,
        )
        reset_summary()
        started_at = time.time()
        result = PipelineRunResult(exit_code=1)

        try:
            huey_log(
                LoggerLevel.INFO,
                f'Starting {self.mode_name} pipeline. save_dir={self.save_dir}',
                module_name=self.module_name,
            )
            result = self.execute()
            if result.summary_updates:
                set_summary(**result.summary_updates)
            return int(result.exit_code)
        finally:
            total_seconds = f'{time.time() - started_at:.2f}'
            summary_payload = {
                'mode': self.mode_name,
                'save_dir': self.save_dir,
                'total_pipeline_time': total_seconds,
            }
            if self.version:
                summary_payload['version'] = self.version
            set_summary(**summary_payload)
            write_summary(os.path.join(self.save_dir, self.summary_file_name))
            huey_log(
                LoggerLevel.INFO,
                f'Finished {self.mode_name} pipeline in {total_seconds}s',
                module_name=self.module_name,
            )

    @abstractmethod
    def execute(self) -> PipelineRunResult:
        raise NotImplementedError


__all__ = ['BasePipeline', 'PipelineRunResult']
