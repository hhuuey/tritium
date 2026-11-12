from collections import deque
from dataclasses import dataclass
from typing import Any, Optional


@dataclass
class MLLMRetryItem:
    sample: dict[str, Any]
    reason: str
    retry_count: int
    raw_response: Optional[str] = None


class MLLMRetryManager:

    def __init__(self, max_retries: int):
        if max_retries < 0:
            raise ValueError('max_retries must be greater than or equal to 0')

        self.max_retries = max_retries
        self._attempts: dict[str, int] = {}
        self._pending_items: deque[MLLMRetryItem] = deque()

    def submit(self, sample: dict[str, Any], reason: str, raw_response: Optional[str] = None) -> bool:
        sample_id = str(sample['sample_id'])
        current_attempt = self._attempts.get(sample_id, 0)
        if current_attempt >= self.max_retries:
            return False

        next_attempt = current_attempt + 1
        self._attempts[sample_id] = next_attempt
        self._pending_items.append(
            MLLMRetryItem(
                sample=sample,
                reason=reason,
                retry_count=next_attempt,
                raw_response=raw_response,
            ))
        return True

    def has_pending(self) -> bool:
        return bool(self._pending_items)

    def pending_count(self) -> int:
        return len(self._pending_items)

    def pop_retry_batch(self, batch_size: int) -> list[MLLMRetryItem]:
        if batch_size < 1:
            raise ValueError('batch_size must be greater than 0')

        items = []
        while self._pending_items and len(items) < batch_size:
            items.append(self._pending_items.popleft())
        return items

    def get_retry_count(self, sample_id: str) -> int:
        return self._attempts.get(sample_id, 0)
