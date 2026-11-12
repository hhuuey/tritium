import multiprocessing as mp
import time
import traceback

from collections import deque
from dataclasses import dataclass, field
from multiprocessing.process import BaseProcess
from queue import Empty
from typing import Any, Callable, Iterator, Optional


@dataclass(frozen=True)
class TaskInfo:
    task_id: str
    function: Callable[..., Any]
    timeout_seconds: Optional[float]
    heartbeat_interval_seconds: Optional[float] = None
    args: tuple[Any, ...] = ()
    kwargs: dict[str, Any] = field(default_factory=dict)
    metadata: dict[str, Any] = field(default_factory=dict)


@dataclass(frozen=True)
class TaskHeartbeat:
    task_id: str
    elapsed_seconds: float
    timeout_seconds: Optional[float]
    metadata: dict[str, Any] = field(default_factory=dict)


@dataclass(frozen=True)
class TaskResult:
    task_id: str
    status: str
    result: Any
    error: Optional[str]
    traceback: Optional[str]
    timed_out: bool
    exitcode: Optional[int]
    duration_seconds: float
    metadata: dict[str, Any] = field(default_factory=dict)


@dataclass
class _RunningTask:
    task: TaskInfo
    process: BaseProcess
    result_queue: mp.Queue
    start_time: float
    heartbeat_interval_seconds: Optional[float]
    next_heartbeat_time: Optional[float]


def _execute_task(
    task_id: str,
    function: Callable[..., Any],
    args: tuple[Any, ...],
    kwargs: dict[str, Any],
    metadata: dict[str, Any],
    result_queue: mp.Queue,
    initializer: Optional[Callable[..., Any]],
    initargs: tuple[Any, ...],
) -> None:
    try:
        if initializer is not None:
            initializer(*initargs)
        result = function(*args, **kwargs)
        result_queue.put({
            'task_id': task_id,
            'status': 'success',
            'result': result,
            'error': None,
            'traceback': None,
            'timed_out': False,
            'metadata': metadata,
        })
    except Exception as exc:
        result_queue.put({
            'task_id': task_id,
            'status': 'error',
            'result': None,
            'error': str(exc),
            'traceback': traceback.format_exc(),
            'timed_out': False,
            'metadata': metadata,
        })


class CozyProcessPool:

    def __init__(
            self,
            tasks: Optional[list[TaskInfo]],
            max_process_num: int,
            initializer: Optional[Callable[..., Any]] = None,
            initargs: tuple[Any, ...] = (),
            on_task_heartbeat: Optional[Callable[[TaskHeartbeat], None]] = None,
    ):
        if max_process_num < 1:
            raise ValueError('max_process_num must be greater than 0')

        if tasks is None:
            tasks = []

        task_ids = [task.task_id for task in tasks]
        if len(task_ids) != len(set(task_ids)):
            raise ValueError('task_id must be unique')

        self._context = mp.get_context('spawn')
        self._pending_tasks = deque(tasks)
        self._task_order = task_ids
        self._results: dict[str, TaskResult] = {}
        self._completed_buffer: deque[TaskResult] = deque()
        self._active_tasks: dict[str, _RunningTask] = {}
        self._known_task_ids = set(task_ids)
        self._started = False
        self.max_process_num = max_process_num
        self._initializer = initializer
        self._initargs = initargs
        self._on_task_heartbeat = on_task_heartbeat

    def available_slots(self) -> int:
        return max(0, self.max_process_num - len(self._active_tasks))

    def add_task(self, task: TaskInfo) -> None:
        if task.task_id in self._known_task_ids:
            raise ValueError(f'task_id already exists: {task.task_id}')

        self._pending_tasks.append(task)
        self._task_order.append(task.task_id)
        self._known_task_ids.add(task.task_id)

        if self._started:
            self._start_pending_tasks()

    def cancel_pending(self) -> int:
        canceled = len(self._pending_tasks)
        self._pending_tasks.clear()
        return canceled

    def shutdown(self, cancel_pending: bool = False, terminate_active: bool = False) -> None:
        if cancel_pending:
            self.cancel_pending()

        if not terminate_active:
            return

        terminated_task_ids = []
        for task_id, running_task in self._active_tasks.items():
            self._terminate_process(running_task.process)
            task_result = self._build_timeout_result(running_task.task, time.monotonic() - running_task.start_time)
            self._results[task_id] = task_result
            self._completed_buffer.append(task_result)
            self._close_queue(running_task.result_queue)
            terminated_task_ids.append(task_id)

        for task_id in terminated_task_ids:
            self._active_tasks.pop(task_id)

    def start(self) -> None:
        if self._started:
            return

        self._started = True
        self._start_pending_tasks()

    def wait(self) -> None:
        for _ in self.iter_results():
            continue

    def iter_results(self, poll_interval_seconds: float = 0.05) -> Iterator[TaskResult]:
        if not self._started:
            self.start()

        while self._pending_tasks or self._active_tasks or self._completed_buffer:
            self._start_pending_tasks()
            self._collect_finished_tasks_to_buffer()

            while self._completed_buffer:
                yield self._completed_buffer.popleft()

            if self._active_tasks and not self._completed_buffer:
                time.sleep(poll_interval_seconds)

    def results(self) -> list[dict[str, Any]]:
        if self._started:
            self._start_pending_tasks()
            self._collect_finished_tasks_to_buffer()

        return [dict(self._results[task_id].__dict__) for task_id in self._task_order if task_id in self._results]

    def _start_pending_tasks(self) -> None:
        while self._pending_tasks and len(self._active_tasks) < self.max_process_num:
            task = self._pending_tasks.popleft()
            result_queue = self._context.Queue()
            process = self._context.Process(
                target=_execute_task,
                args=(
                    task.task_id,
                    task.function,
                    task.args,
                    task.kwargs,
                    task.metadata,
                    result_queue,
                    self._initializer,
                    self._initargs,
                ),
            )
            process.start()
            heartbeat_interval_seconds = self._normalize_heartbeat_interval(task.heartbeat_interval_seconds)
            next_heartbeat_time = None
            if heartbeat_interval_seconds is not None:
                next_heartbeat_time = time.monotonic() + heartbeat_interval_seconds
            self._active_tasks[task.task_id] = _RunningTask(
                task=task,
                process=process,
                result_queue=result_queue,
                start_time=time.monotonic(),
                heartbeat_interval_seconds=heartbeat_interval_seconds,
                next_heartbeat_time=next_heartbeat_time,
            )

    def _collect_finished_tasks_to_buffer(self) -> None:
        completed_task_ids = []

        for task_id, running_task in self._active_tasks.items():
            now = time.monotonic()
            if self._is_timed_out(running_task, now):
                self._terminate_process(running_task.process)
                task_result = self._build_timeout_result(running_task.task, now - running_task.start_time)
                self._results[task_id] = task_result
                self._completed_buffer.append(task_result)
                self._close_queue(running_task.result_queue)
                completed_task_ids.append(task_id)
                continue

            self._emit_heartbeat_if_needed(task_id, running_task, now)

            if running_task.process.is_alive():
                continue

            running_task.process.join()
            task_result = self._build_result(running_task)
            self._results[task_id] = task_result
            self._completed_buffer.append(task_result)
            self._close_queue(running_task.result_queue)
            completed_task_ids.append(task_id)

        for task_id in completed_task_ids:
            self._active_tasks.pop(task_id)

    def _is_timed_out(self, running_task: _RunningTask, now: float) -> bool:
        timeout_seconds = running_task.task.timeout_seconds
        if timeout_seconds is None:
            return False

        return now - running_task.start_time > timeout_seconds

    def _emit_heartbeat_if_needed(self, task_id: str, running_task: _RunningTask, now: float) -> None:
        del task_id
        if self._on_task_heartbeat is None:
            return
        heartbeat_interval_seconds = running_task.heartbeat_interval_seconds
        if heartbeat_interval_seconds is None:
            return
        next_heartbeat_time = running_task.next_heartbeat_time
        if next_heartbeat_time is None:
            running_task.next_heartbeat_time = now + heartbeat_interval_seconds
            return
        if now < next_heartbeat_time:
            return

        elapsed_seconds = max(0.0, now - running_task.start_time)
        event = TaskHeartbeat(
            task_id=running_task.task.task_id,
            elapsed_seconds=elapsed_seconds,
            timeout_seconds=running_task.task.timeout_seconds,
            metadata=running_task.task.metadata,
        )
        try:
            self._on_task_heartbeat(event)
        except Exception:
            pass

        while running_task.next_heartbeat_time is not None and running_task.next_heartbeat_time <= now:
            running_task.next_heartbeat_time += heartbeat_interval_seconds

    def _normalize_heartbeat_interval(self, heartbeat_interval_seconds: Optional[float]) -> Optional[float]:
        if heartbeat_interval_seconds is None:
            return None
        if heartbeat_interval_seconds <= 0:
            return None
        return float(heartbeat_interval_seconds)

    def _build_result(self, running_task: _RunningTask) -> TaskResult:
        duration_seconds = max(0.0, time.monotonic() - running_task.start_time)

        try:
            result = running_task.result_queue.get_nowait()
        except Empty:
            return TaskResult(
                task_id=running_task.task.task_id,
                status='error',
                result=None,
                error='process exited without returning a result',
                traceback=None,
                timed_out=False,
                exitcode=running_task.process.exitcode,
                duration_seconds=duration_seconds,
                metadata=running_task.task.metadata,
            )

        return TaskResult(
            task_id=running_task.task.task_id,
            status=result.get('status', 'error'),
            result=result.get('result'),
            error=result.get('error'),
            traceback=result.get('traceback'),
            timed_out=bool(result.get('timed_out', False)),
            exitcode=running_task.process.exitcode,
            duration_seconds=duration_seconds,
            metadata=result.get('metadata', running_task.task.metadata),
        )

    def _build_timeout_result(self, task: TaskInfo, duration_seconds: float) -> TaskResult:
        return TaskResult(
            task_id=task.task_id,
            status='timeout',
            result=None,
            error=f'task exceeded {task.timeout_seconds} seconds',
            traceback=None,
            timed_out=True,
            exitcode=None,
            duration_seconds=max(0.0, duration_seconds),
            metadata=task.metadata,
        )

    def _terminate_process(self, process: BaseProcess) -> None:
        process.terminate()
        process.join(timeout=1)
        if process.is_alive():
            process.kill()
            process.join()

    def _close_queue(self, result_queue: mp.Queue) -> None:
        result_queue.close()
        result_queue.join_thread()


__all__ = ['CozyProcessPool', 'TaskHeartbeat', 'TaskInfo', 'TaskResult']
