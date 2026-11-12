from __future__ import annotations

import fcntl
from collections.abc import Iterator, Sequence
from contextlib import ExitStack, contextmanager
from pathlib import Path
from typing import BinaryIO


def execution_lock_path(directory: Path) -> Path:
    return directory.with_name(f".{directory.name}.execution.lock")


@contextmanager
def artifact_execution_locks(
    output_directory: Path, input_directories: Sequence[Path]
) -> Iterator[None]:
    """锁定当前输出，并在执行期间保持受管输入不被覆盖。"""

    output_lock = execution_lock_path(output_directory)
    locks = {execution_lock_path(directory): True for directory in input_directories}
    locks[output_lock] = False
    with ExitStack() as stack:
        for path, shared in sorted(locks.items()):
            stack.enter_context(_file_lock(path, shared=shared))
        yield


@contextmanager
def _file_lock(path: Path, *, shared: bool) -> Iterator[None]:
    path.parent.mkdir(parents=True, exist_ok=True)
    stream: BinaryIO
    with path.open("a+b") as stream:
        operation = fcntl.LOCK_SH if shared else fcntl.LOCK_EX
        fcntl.flock(stream.fileno(), operation)
        try:
            yield
        finally:
            fcntl.flock(stream.fileno(), fcntl.LOCK_UN)
