# CozyProcessPool 最小复用说明

本文档给出基于 `cozy.engineer_base.process` 的最小用法，目标是统一多进程任务调度方式。

## 1. 适用场景

- 任务可并行执行，且彼此独立。
- 每个任务需要独立超时控制。
- 需要按完成顺序处理结果，而不是按提交顺序等待。
- 需要动态补任务，或达到条件后提前停止。

## 2. 核心对象

- `TaskInfo`: 任务描述，包含 `task_id`、执行函数、超时、参数、metadata。
- `TaskResult`: 任务结果，包含 `status`、`result`、`error`、`timed_out`、`duration_seconds`。
- `CozyProcessPool`: 调度器，负责拉起子进程、超时终止、回收结果。

## 3. 最小示例

```python
import time

from cozy import eb


def worker_init(log_dir: str):
    eb.huey_log_init(
        enable_console_output=True,
        enable_file_output=bool(log_dir),
        output_dir=log_dir or None,
    )


def run_one(sample_id: str, sleep_seconds: float) -> dict:
    time.sleep(sleep_seconds)
    return {'sample_id': sample_id, 'ok': True}


def run_demo() -> list[dict]:
    tasks = [
        eb.TaskInfo(
            task_id='task_1',
            function=run_one,
            timeout_seconds=5,
            args=('a', 0.5),
            metadata={'sample_id': 'a'},
        ),
        eb.TaskInfo(
            task_id='task_2',
            function=run_one,
            timeout_seconds=1,
            args=('b', 2.0),
            metadata={'sample_id': 'b'},
        ),
    ]

    pool = eb.CozyProcessPool(
        tasks=tasks,
        max_process_num=2,
        initializer=worker_init,
        initargs=('',),
    )

    outputs = []
    for task_result in pool.iter_results():
        outputs.append({
            'task_id': task_result.task_id,
            'status': task_result.status,
            'timed_out': task_result.timed_out,
            'duration_seconds': task_result.duration_seconds,
            'error': task_result.error,
            'result': task_result.result,
            'metadata': task_result.metadata,
        })
    return outputs
```

## 4. 动态补任务与提前停止

```python
from cozy import eb


def run_dynamic(task_iter, stop_after_success: int = 10):
    pool = eb.CozyProcessPool(tasks=[], max_process_num=8)

    success_count = 0
    task_iter = iter(task_iter)

    while pool.available_slots() > 0:
        try:
            pool.add_task(next(task_iter))
        except StopIteration:
            break

    for task_result in pool.iter_results():
        if task_result.status == 'success':
            success_count += 1

        if success_count >= stop_after_success:
            pool.shutdown(cancel_pending=True, terminate_active=True)
            break

        while pool.available_slots() > 0:
            try:
                pool.add_task(next(task_iter))
            except StopIteration:
                break
```

## 5. 约束约定

- `task_id` 必须全局唯一。
- 任务函数必须可被多进程序列化（避免闭包和局部函数）。
- 不要在业务任务函数内部重复实现 signal 超时逻辑，统一使用 `TaskInfo.timeout_seconds`。
- `metadata` 仅放轻量信息，避免传递大对象。

## 6. 推荐项目接入方式

- 项目层负责: 构造 `TaskInfo`、定义成功条件、后处理与停止策略。
- cozy 层负责: 并发槽位、超时终止、结果回收、worker 初始化。
