from __future__ import annotations

import os
from collections.abc import Mapping
from pathlib import Path, PurePosixPath
from tempfile import TemporaryDirectory
from urllib.parse import unquote, urlsplit

import dagster as dg
import yaml


class AssetExecutionConfig(dg.Config):
    """执行选项；算法参数由 YAML 配置提供。"""

    force_recompute: bool = False


class CaseManifestConfig(dg.Config):
    """Case 登记使用的 qfile 数据源与模型配置。"""

    qfile_uri: str
    segmentation_model: dict
    vector_model: dict
    sensor_calibration_models: dict | None = None


class ConfigurationError(ValueError):
    pass


def default_config_root() -> Path:
    configured = os.getenv("DAGSTER_PIPELINE_CONFIG_ROOT")
    if configured:
        return Path(configured).expanduser().resolve()

    source_root = Path(__file__).resolve().parents[2] / "config"
    if source_root.is_dir():
        return source_root

    packaged_root = Path(__file__).resolve().parent / "config"
    if packaged_root.is_dir():
        return packaged_root

    raise ConfigurationError(
        "configuration root was not found; set DAGSTER_PIPELINE_CONFIG_ROOT"
    )


def default_runtime_root(environ: Mapping[str, str] | None = None) -> Path:
    values = os.environ if environ is None else environ
    return Path(values.get("DAGSTER_PIPELINE_RUNTIME_ROOT", str(Path.cwd()))).expanduser().resolve()


def default_artifact_root(environ: Mapping[str, str] | None = None) -> Path:
    values = os.environ if environ is None else environ
    runtime_root = default_runtime_root(values)
    root = Path(values.get("DAGSTER_PIPELINE_ARTIFACT_ROOT", ".artifacts")).expanduser()
    return (root if root.is_absolute() else runtime_root / root).resolve()


def rendered_config_root(runtime_root: Path | None = None) -> Path:
    return (runtime_root or default_runtime_root()).expanduser().resolve() / "build/config"


def _compose(path: Path, algorithm_root: Path, *, hardware: dict | None = None) -> dict:
    from hydra import compose
    from omegaconf import OmegaConf

    name = path.relative_to(algorithm_root).with_suffix("").as_posix()
    config = compose(config_name=name)
    if hardware is not None:
        # 先选车型，再合成算法局部覆盖，插值使用最终传感器布局。
        local = OmegaConf.to_container(config, resolve=False)
        hardware = dict(hardware)
        if name != "raster_map/case_raster_map":
            hardware.pop("intensity_ring_allowlist", None)
        config = OmegaConf.merge(hardware, config)
        if "intensity_ring_allowlist" in local:
            # 白名单整体替换，允许 {} 清空，避免残留其他车型的雷达名称。
            config["intensity_ring_allowlist"] = local["intensity_ring_allowlist"]
    config = OmegaConf.to_container(config, resolve=True)
    for field in ("bpe_path", "dataconvert"):
        if config.get(field) and config[field] != "-":
            config[field] = _external_uri(config[field], path.parent)
    normalization = config.get("intensity_normalization")
    if (
        isinstance(normalization, dict)
        and normalization.get("calibration_path")
        and normalization["calibration_path"] != "-"
    ):
        normalization["calibration_path"] = _external_uri(normalization["calibration_path"], path.parent)
    return config


def _hardware_path(algorithm_root: Path, vehicle: str) -> Path:
    if vehicle not in ("7v", "11v"):
        raise ConfigurationError(f"unsupported vehicle {vehicle!r}; choose 7v or 11v")
    return algorithm_root / f"sensor_hardware_{vehicle}.yaml"


def _external_uri(value: str, directory: Path) -> str:
    if urlsplit(value).scheme:
        return value
    path = Path(value).expanduser()
    return str((path if path.is_absolute() else directory / path).resolve())


def _yaml_bytes(payload: dict) -> bytes:
    return yaml.safe_dump(payload, sort_keys=False, allow_unicode=True).encode("utf-8")


def case_id_from_qfile_uri(qfile_uri: str) -> str:
    return unquote(PurePosixPath(urlsplit(qfile_uri).path).name)


def load_case_registration_config(path: Path) -> dict:
    """规范登记配置：固定资源根，保留根内相对模型路径。"""
    payload = yaml.safe_load(path.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise ConfigurationError(f"{path}: case registration must be a mapping")
    unexpected = set(payload) - {
        "qfile_uris", "data_engineering_resource", "segmentation_model", "vector_model",
        "sensor_calibration_models",
    }
    if unexpected:
        raise ConfigurationError(
            f"{path}: unsupported case fields: {sorted(unexpected, key=str)}"
        )
    qfiles = payload.get("qfile_uris")
    if not isinstance(qfiles, list) or not qfiles:
        raise ConfigurationError(f"{path}: qfile_uris must be a nonempty list")
    directory = path.resolve().parent
    uris = []
    case_ids = set()
    for number, value in enumerate(qfiles, 1):
        if not isinstance(value, str) or not value.strip() or any(
            character in value for character in ("\0", "\n", "\r")
        ):
            raise ConfigurationError(
                f"{path}: qfile_uris[{number}] must be a nonblank URI without NUL or newlines"
            )
        try:
            uri = _external_uri(value, directory)
            case_id = case_id_from_qfile_uri(uri)
        except ValueError as error:
            raise ConfigurationError(
                f"{path}: invalid qfile URI {value!r}: {error}"
            ) from error
        if not case_id.strip() or case_id in {".", ".."} or any(
            character in case_id for character in ("/", "\\", "\0", "\n", "\r")
        ):
            raise ConfigurationError(f"{path}: qfile URI must identify a case: {value!r}")
        if case_id in case_ids:
            raise ConfigurationError(f"{path}: duplicate case ID {case_id!r} in qfile_uris")
        case_ids.add(case_id)
        uris.append(uri)
    payload["qfile_uris"] = uris
    resource = payload.get("data_engineering_resource")
    if (
        not isinstance(resource, str) or not resource.strip()
        or any(character in resource for character in ("\0", "\n", "\r"))
    ):
        raise ConfigurationError(f"{path}: data_engineering_resource must be a nonblank local directory path")
    if urlsplit(resource).scheme:
        raise ConfigurationError(f"{path}: data_engineering_resource must be a local directory path, not a URI")
    payload["data_engineering_resource"] = _external_uri(resource, directory)

    def normalize_model(model: object, name: str) -> dict:
        if not isinstance(model, dict):
            raise ConfigurationError(f"{path}: {name} must be a mapping")
        unsupported = set(model) - {
            "model_uri", "model_version", "source", "training_release_id"
        }
        if unsupported:
            raise ConfigurationError(f"{path}: {name}: unsupported model fields: {sorted(unsupported, key=str)}")
        for field in ("model_uri", "model_version"):
            value = model.get(field)
            if not isinstance(value, str) or not value.strip():
                raise ConfigurationError(f"{path}: {name}.{field} is required")
            if any(character in value for character in ("\0", "\n", "\r")):
                raise ConfigurationError(f"{path}: {name}.{field} cannot contain NUL or newlines")
        version = model["model_version"]
        if version in {".", ".."} or any(character in version for character in ("/", "\\")):
            raise ConfigurationError(f"{path}: {name}.model_version must be a path component")
        for field in ("source", "training_release_id"):
            if field in model and (
                not isinstance(model[field], str) or not model[field].strip()
                or any(character in model[field] for character in ("\0", "\n", "\r"))
            ):
                raise ConfigurationError(f"{path}: {name}.{field} must be a nonblank string without NUL or newlines")
        uri = model["model_uri"]
        if (
            urlsplit(uri).scheme or Path(uri).is_absolute() or uri.startswith("~")
            or ".." in Path(uri).parts or Path(uri) == Path(".")
        ):
            raise ConfigurationError(f"{path}: {name}.model_uri must be a relative path within data_engineering_resource")
        return model

    for key in ("segmentation_model", "vector_model"):
        payload[key] = normalize_model(payload.get(key), key)
    calibration_models = payload.get("sensor_calibration_models")
    if calibration_models is not None:
        if not isinstance(calibration_models, dict) or set(calibration_models) != {"depth_engine", "vocabulary"}:
            raise ConfigurationError(f"{path}: sensor_calibration_models must contain exactly depth_engine and vocabulary")
        for role in ("depth_engine", "vocabulary"):
            calibration_models[role] = normalize_model(
                calibration_models[role], f"sensor_calibration_models.{role}"
            )
    return payload


def compose_algorithm_config(
    asset_name: str, *, config_root: Path | None = None, vehicle: str = "7v"
) -> dict:
    from hydra import initialize_config_dir

    root = (config_root or default_config_root()).resolve()
    registry_path = root / "dagster/assets.yaml"
    registry = yaml.safe_load(registry_path.read_text(encoding="utf-8"))
    algorithm_root = root / "algorithm"
    source = (registry_path.parent / registry[asset_name]["config_path"]).resolve()
    with initialize_config_dir(config_dir=str(algorithm_root), version_base="1.3"):
        hardware = _compose(_hardware_path(algorithm_root, vehicle), algorithm_root)
        return _compose(source, algorithm_root, hardware=hardware)


def render_configs(
    *, output_directory: Path | None = None, config_root: Path | None = None,
    vehicle: str = "7v",
) -> tuple[Path, ...]:
    from hydra import initialize_config_dir

    root = (config_root or default_config_root()).resolve()
    output = (output_directory or rendered_config_root()).expanduser().resolve()
    registry_path = root / "dagster/assets.yaml"
    registry = yaml.safe_load(registry_path.read_text(encoding="utf-8"))
    algorithm_root = root / "algorithm"
    documents = {}
    with initialize_config_dir(config_dir=str(algorithm_root), version_base="1.3"):
        hardware = _compose(_hardware_path(algorithm_root, vehicle), algorithm_root)
        documents[Path("algorithm/sensor_hardware.yaml")] = _yaml_bytes(hardware)
        common = algorithm_root / "semantic_common.yaml"
        documents[common.relative_to(root)] = _yaml_bytes(_compose(common, algorithm_root))
        for fields in registry.values():
            source = (registry_path.parent / fields["config_path"]).resolve()
            documents[source.relative_to(root)] = _yaml_bytes(
                _compose(source, algorithm_root, hardware=hardware)
            )
    cases_path = root / "dagster/cases.yaml"
    documents[cases_path.relative_to(root)] = _yaml_bytes(
        load_case_registration_config(cases_path)
    )
    runtime_path = root / "dagster/runtime.yaml"
    documents[runtime_path.relative_to(root)] = _yaml_bytes(
        yaml.safe_load(runtime_path.read_text(encoding="utf-8"))
    )
    documents[registry_path.relative_to(root)] = _yaml_bytes(registry)

    output.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory(prefix=".render-", dir=output) as staging:
        for relative, content in documents.items():
            temporary = Path(staging) / relative
            temporary.parent.mkdir(parents=True, exist_ok=True)
            temporary.write_bytes(content)
        for relative in documents:
            target = output / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            (Path(staging) / relative).replace(target)
        for name in ("algorithm", "dagster"):
            managed = output / name
            obsolete = [
                target for target in managed.rglob("*")
                if target.is_file()
                and (
                    target.suffix in {".yaml", ".yml"}
                    or (
                        target.is_relative_to(output / "dagster/cases")
                        and (
                            target.name in {"mini-dataset.txt", "mini-dataset.txt.remote"}
                            or target.name.endswith(".dataset.txt")
                        )
                    )
                )
                and target.relative_to(output) not in documents
            ]
            for target in obsolete:
                target.unlink()
                directory = target.parent
                while directory != managed and not any(directory.iterdir()):
                    directory.rmdir()
                    directory = directory.parent
    return tuple(output / relative for relative in documents)


__all__ = [
    "CaseManifestConfig",
    "ConfigurationError",
    "AssetExecutionConfig",
    "case_id_from_qfile_uri",
    "compose_algorithm_config",
    "default_artifact_root",
    "default_runtime_root",
    "default_config_root",
    "load_case_registration_config",
    "render_configs",
    "rendered_config_root",
]
