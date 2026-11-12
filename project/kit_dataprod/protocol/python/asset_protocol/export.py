"""从实际共享实现导出可独立使用的 Python/C++ 协议源码包。"""

from __future__ import annotations

from pathlib import Path
import shutil
import tarfile
from tempfile import TemporaryDirectory


def _resources(package: Path) -> tuple[Path, Path]:
    cpp = package / "_cpp"
    resources = package / "_protocol_resources"
    if cpp.is_dir() and resources.is_dir():
        return cpp, resources
    source = package.parents[1]
    cpp = source / "cpp/asset_protocol"
    if not cpp.is_dir() or not (source / "README.md").is_file():
        raise FileNotFoundError("缺少协议层的 C++ 源码或接入说明")
    return cpp, source


def export_asset_protocol(output_directory: Path) -> tuple[Path, Path]:
    """导出到新目录或空目录，避免覆盖调用方已有文件。"""
    output = Path(output_directory).expanduser().resolve()
    if output.exists() and (not output.is_dir() or any(output.iterdir())):
        raise FileExistsError(f"协议导出目录必须不存在或为空：{output}")
    package = Path(__file__).resolve().parent
    cpp, resources = _resources(package)
    archive = output.with_name(output.name + ".tar.gz")
    if archive.exists() or archive.is_symlink():
        raise FileExistsError(f"协议压缩包已存在：{archive}")
    resource_directories = [resources / name for name in ("examples", "schema", "fixtures")]
    for source in (package, cpp, *resource_directories):
        if output.is_relative_to(source):
            raise ValueError("协议导出目录不能位于待复制的源码目录内")
    output.parent.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory(prefix=".asset-protocol-", dir=output.parent) as temporary:
        staged = Path(temporary) / output.name
        python = staged / "python/asset_protocol"
        shutil.copytree(
            package,
            python,
            ignore=shutil.ignore_patterns(
                "__pycache__", "*.pyc", "_cpp", "_protocol_resources", "export.py"
            ),
        )
        (staged / "python/pyproject.toml").write_text(
            '[build-system]\nrequires = ["hatchling"]\n'
            'build-backend = "hatchling.build"\n\n'
            '[project]\nname = "asset-protocol"\nversion = "1.0.0"\n'
            'requires-python = ">=3.10"\ndependencies = ["jsonschema>=4.23,<5"]\n\n'
            '[tool.hatch.build.targets.wheel]\npackages = ["asset_protocol"]\n',
            encoding="utf-8",
        )
        shutil.copytree(cpp, staged / "cpp/asset_protocol")
        for source in resource_directories:
            shutil.copytree(source, staged / source.name)
        shutil.copyfile(resources / "README.md", staged / "README.md")
        temporary_archive = Path(temporary) / archive.name
        with tarfile.open(temporary_archive, "w:gz") as bundle:
            bundle.add(staged, arcname=output.name)
        if output.exists():
            output.rmdir()
        staged.replace(output)
        temporary_archive.replace(archive)
    return output, archive
