"""Schema-driven validation and conversion for generated protocol dataclasses."""
from __future__ import annotations

from copy import deepcopy
from dataclasses import asdict, fields, is_dataclass
from functools import lru_cache
from importlib.resources import files
import json
from types import UnionType
from typing import Any, TypeVar, get_args, get_origin, get_type_hints

from jsonschema import Draft7Validator, FormatChecker, validators
from jsonschema.exceptions import ValidationError

from asset_protocol import generated
from asset_protocol.io import validate_json

SCHEMA = json.loads(files("asset_protocol").joinpath("schema.json").read_text(encoding="utf8"))
DEFINITIONS = SCHEMA["definitions"]
MODEL_TYPES = {name: getattr(generated, name) for name, definition in DEFINITIONS.items() if "properties" in definition}
PROTOCOL_VERSION = DEFINITIONS["AssetIdentity"]["properties"]["protocol_version"]["const"]
IDENTITY_FIELDS = tuple(DEFINITIONS["AssetIdentity"]["properties"])
ERROR_CATEGORIES = frozenset(DEFINITIONS["AssetError"]["properties"]["category"]["enum"])
AssetStatus = str
ErrorCategory = str
T = TypeVar("T")
FORMAT_CHECKER = FormatChecker()
# The wire profile distinguishes integer tokens from floating point tokens,
# matching Valijson strong types and generated Python/C++ integer members.
ProtocolValidator = validators.extend(Draft7Validator, type_checker=Draft7Validator.TYPE_CHECKER.redefine(
    "integer", lambda checker, value: type(value) is int))


@FORMAT_CHECKER.checks("non-blank")
def _non_blank(value):
    return not isinstance(value, str) or bool(value.strip())


@lru_cache(maxsize=None)
def validator(name: str) -> Draft7Validator:
    return ProtocolValidator({"$ref": f"#/definitions/{name}", "definitions": DEFINITIONS}, format_checker=FORMAT_CHECKER)


def validate_payload(name: str, value: Any) -> None:
    validate_json(value)
    try:
        validator(name).validate(value)
    except ValidationError as error:
        location = ".".join(map(str, error.absolute_path)) or name
        raise ValueError(f"Protocol validation at {location}: {error.message}") from error
    if name == "AssetRequest":
        for key, references in value["inputs"].items():
            for reference in references:
                if reference["asset_name"] != key:
                    raise ValueError("ArtifactRef asset_name must match its input key")


def _defaults(value, definition):
    if "$ref" in definition:
        return _defaults(value, DEFINITIONS[definition["$ref"].rsplit("/", 1)[1]])
    if isinstance(value, dict):
        properties = definition.get("properties", {})
        for key, child in properties.items():
            if value.get(key, object()) is None and key not in definition.get("required", []) and "default" not in child:
                value.pop(key)
            if key not in value and "default" in child:
                value[key] = deepcopy(child["default"])
            if key in value:
                value[key] = _defaults(value[key], child)
        additional = definition.get("additionalProperties")
        if isinstance(additional, dict):
            for key in value.keys() - properties.keys():
                value[key] = _defaults(value[key], additional)
    elif isinstance(value, list) and isinstance(definition.get("items"), dict):
        value = [_defaults(item, definition["items"]) for item in value]
    for branch in definition.get("anyOf", []):
        if value is not None and branch.get("type") != "null":
            value = _defaults(value, branch)
    return value


def _restore(annotation, value):
    if value is None or annotation is Any:
        return value
    origin, args = get_origin(annotation), get_args(annotation)
    if origin is UnionType:
        return _restore(next(item for item in args if item is not type(None)), value)
    if origin is list:
        return [_restore(args[0], item) for item in value]
    if origin is dict:
        return {key: _restore(args[1], item) for key, item in value.items()}
    if is_dataclass(annotation):
        annotations = get_type_hints(annotation)
        return annotation(**{field.name: _restore(annotations[field.name], value[field.name])
            for field in fields(annotation) if field.name in value})
    return value


def _typed_payload(value, annotation, location):
    """Check generated member types while building a JSON-shaped value.

    Open JSON remains intact until validate_payload checks cycles and values;
    copying it afterwards avoids asdict recursing into invalid cyclic data.
    """
    if annotation is Any:
        return value
    origin, args = get_origin(annotation), get_args(annotation)
    if origin is UnionType:
        for option in args:
            try:
                return _typed_payload(value, option, location)
            except TypeError:
                pass
        raise TypeError(f"{location} does not match {annotation}")
    if origin in (list, dict):
        if type(value) is not origin:
            raise TypeError(f"{location} must be a {origin.__name__}")
        if origin is dict:
            return {key: _typed_payload(item, args[-1], f"{location}[{key!r}]") for key, item in value.items()}
        return [_typed_payload(item, args[-1], f"{location}[{key!r}]") for key, item in enumerate(value)]
    if is_dataclass(annotation):
        if type(value) is not annotation:
            raise TypeError(f"{location} must be an {annotation.__name__}")
        annotations = get_type_hints(annotation)
        return {field.name: _typed_payload(getattr(value, field.name), annotations[field.name], f"{location}.{field.name}")
                for field in fields(annotation)}
    valid = type(value) in (int, float) if annotation is float else type(value) is annotation
    if not valid:
        raise TypeError(f"{location} must be {annotation.__name__}")
    return value


def from_dict(model_type: type[T], value: dict[str, Any]) -> T:
    if MODEL_TYPES.get(model_type.__name__) is not model_type:
        raise TypeError("Expected a generated protocol model type")
    validate_payload(model_type.__name__, value)
    return _restore(model_type, _defaults(deepcopy(value), DEFINITIONS[model_type.__name__]))


def to_dict(model: T) -> dict[str, Any]:
    if MODEL_TYPES.get(type(model).__name__) is not type(model):
        raise TypeError("Expected a generated protocol model")
    value = _defaults(_typed_payload(model, type(model), type(model).__name__), DEFINITIONS[type(model).__name__])
    validate_payload(type(model).__name__, value)
    return deepcopy(value)


def validate(model: T) -> None:
    to_dict(model)


def validate_asset_status(status: str, error: Any) -> None:
    if is_dataclass(error):
        error = asdict(error)
    value = {"asset_name": "identity", "case_id": "identity", "data_version": "identity",
             "protocol_version": PROTOCOL_VERSION, "status": status, "error": error}
    validate_payload("AssetResult", value)
