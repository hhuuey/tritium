import logging
import os
import sys
import threading

from datetime import datetime
from enum import Enum
from pathlib import Path
from typing import Optional

LOGGER_NAME = 'cozy.engineer_base'
DEFAULT_MODULE_NAME = 'unknown'
DEFAULT_MAX_FILE_LINES = 200000


class LoggerLevel(str, Enum):
    DEBUG = 'debug'
    INFO = 'info'
    WARNING = 'warning'
    ERROR = 'error'
    CRITICAL = 'critical'
    EXCEPTION = 'exception'


class _HueyFormatter(logging.Formatter):

    def formatTime(self, record, datefmt=None):
        dt = datetime.fromtimestamp(record.created)
        return dt.strftime('%Y-%m-%d %H:%M:%S.') + f'{int(record.msecs):03d}'

    def format(self, record):
        module_name = getattr(record, 'module_name', DEFAULT_MODULE_NAME) or DEFAULT_MODULE_NAME
        level_name = getattr(record, 'level_name', record.levelname.lower())
        message = record.getMessage()
        prefix = f'[{self.formatTime(record)}][{record.process}][{module_name}][{level_name}]'

        if record.exc_info:
            exc_text = self.formatException(record.exc_info)
            return f'{prefix}{message}\n{exc_text}'

        if record.exc_text:
            return f'{prefix}{message}\n{record.exc_text}'

        return f'{prefix}{message}'


class _LineRotatingFileHandler(logging.Handler):

    def __init__(self, output_dir: str, max_lines: int):
        super().__init__()
        self._output_dir = Path(output_dir)
        self._max_lines = max(1, int(max_lines))
        self._lock = threading.RLock()
        self._file_index = 0
        self._line_count = 0
        self._stream = None
        self._current_path: Optional[Path] = None
        self._output_dir.mkdir(parents=True, exist_ok=True)
        self._open_next_file()

    def emit(self, record):
        message = self.format(record)
        line_delta = max(1, message.count('\n') + 1)

        with self._lock:
            if self._stream is None or self._line_count + line_delta > self._max_lines:
                self._open_next_file()

            stream = self._stream
            if stream is None:
                return

            stream.write(message + '\n')
            stream.flush()
            self._line_count += line_delta

    def close(self):
        with self._lock:
            if self._stream is not None:
                self._stream.close()
                self._stream = None
        super().close()

    def _open_next_file(self):
        if self._stream is not None:
            self._stream.close()

        self._file_index += 1
        self._line_count = 0
        self._current_path = self._output_dir / f'run_{self._file_index:04d}.log'
        self._stream = self._current_path.open('a', encoding='utf-8')


class _LoggerState:

    def __init__(self):
        self.lock = threading.RLock()
        self.logger: Optional[logging.Logger] = None
        self.stream_handler: Optional[logging.Handler] = None
        self.file_handler: Optional[logging.Handler] = None
        self.console_enabled = True
        self.file_enabled = False
        self.output_dir: Optional[str] = None
        self.max_file_lines = DEFAULT_MAX_FILE_LINES
        self.initialized = False


_STATE = _LoggerState()


def _build_formatter() -> logging.Formatter:
    return _HueyFormatter()


def _to_logging_level(level: LoggerLevel) -> int:
    if level == LoggerLevel.DEBUG:
        return logging.DEBUG
    if level == LoggerLevel.WARNING:
        return logging.WARNING
    if level == LoggerLevel.ERROR or level == LoggerLevel.EXCEPTION:
        return logging.ERROR
    if level == LoggerLevel.CRITICAL:
        return logging.CRITICAL
    return logging.INFO


def _create_logger() -> logging.Logger:
    logger = logging.getLogger(LOGGER_NAME)
    logger.setLevel(logging.DEBUG)
    logger.propagate = False
    logger.handlers.clear()
    return logger


def _apply_handlers() -> logging.Logger:
    logger = _STATE.logger or _create_logger()
    logger.handlers.clear()

    formatter = _build_formatter()

    if _STATE.console_enabled:
        stream_handler = logging.StreamHandler(sys.stdout)
        stream_handler.setLevel(logging.DEBUG)
        stream_handler.setFormatter(formatter)
        logger.addHandler(stream_handler)
        _STATE.stream_handler = stream_handler
    else:
        _STATE.stream_handler = None

    if _STATE.file_enabled and _STATE.output_dir:
        file_handler = _LineRotatingFileHandler(_STATE.output_dir, _STATE.max_file_lines)
        file_handler.setLevel(logging.DEBUG)
        file_handler.setFormatter(formatter)
        logger.addHandler(file_handler)
        _STATE.file_handler = file_handler
    else:
        _STATE.file_handler = None

    _STATE.logger = logger
    _STATE.initialized = True
    return logger


def _reset_handlers():
    if _STATE.stream_handler is not None:
        _STATE.stream_handler.close()
        _STATE.stream_handler = None

    if _STATE.file_handler is not None:
        _STATE.file_handler.close()
        _STATE.file_handler = None


def huey_log_init(
    enable_console_output: bool = True,
    enable_file_output: bool = False,
    output_dir: Optional[str] = None,
    max_file_lines: int = DEFAULT_MAX_FILE_LINES,
) -> logging.Logger:
    with _STATE.lock:
        normalized_output_dir = None
        if output_dir:
            normalized_output_dir = os.path.abspath(output_dir)

        _reset_handlers()
        _STATE.console_enabled = bool(enable_console_output)
        _STATE.file_enabled = bool(enable_file_output) and bool(normalized_output_dir)
        _STATE.output_dir = normalized_output_dir
        _STATE.max_file_lines = max(1, int(max_file_lines))
        logger = _apply_handlers()

    return logger


def _get_logger() -> logging.Logger:
    with _STATE.lock:
        if _STATE.logger is None or not _STATE.initialized:
            return huey_log_init()
        return _STATE.logger


def huey_log(
    level: LoggerLevel,
    msg,
    module_name: str = DEFAULT_MODULE_NAME,
) -> None:
    logger = _get_logger()
    log_level = _to_logging_level(level)

    extra = {
        'level_name': level.value,
        'module_name': module_name or DEFAULT_MODULE_NAME,
    }

    exc_info = level == LoggerLevel.EXCEPTION
    logger.log(log_level, str(msg), extra=extra, exc_info=exc_info)


__all__ = [
    'LoggerLevel',
    'huey_log',
    'huey_log_init',
]
