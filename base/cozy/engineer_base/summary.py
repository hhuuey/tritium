import json
import os

from typing import Any, Dict, Optional

_SUMMARY: Dict[str, Any] = {"issues": []}


def reset_summary() -> Dict[str, Any]:
    _SUMMARY.clear()
    _SUMMARY["issues"] = []
    return _SUMMARY


def get_summary() -> Dict[str, Any]:
    if "issues" not in _SUMMARY or not isinstance(_SUMMARY["issues"], list):
        _SUMMARY["issues"] = []
    return _SUMMARY


def set_summary(key: Optional[str] = None, value: Any = None, **kwargs: Any) -> Dict[str, Any]:
    if key is not None:
        _SUMMARY[key] = value

    if kwargs:
        _SUMMARY.update(kwargs)

    if "issues" not in _SUMMARY or not isinstance(_SUMMARY["issues"], list):
        _SUMMARY["issues"] = []

    return _SUMMARY


def add_issue(issue: str) -> Dict[str, Any]:
    if "issues" not in _SUMMARY or not isinstance(_SUMMARY["issues"], list):
        _SUMMARY["issues"] = []

    if issue:
        _SUMMARY["issues"].append(str(issue))

    return _SUMMARY


def write_summary(output_file: str) -> Dict[str, Any]:
    output_dir = os.path.dirname(os.path.abspath(output_file))
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)

    if "issues" not in _SUMMARY or not isinstance(_SUMMARY["issues"], list):
        _SUMMARY["issues"] = []

    with open(output_file, "w", encoding="utf-8") as file_obj:
        json.dump(_SUMMARY, file_obj, indent=2, ensure_ascii=False)

    return _SUMMARY


__all__ = [
    "add_issue",
    "get_summary",
    "reset_summary",
    "set_summary",
    "write_summary",
]