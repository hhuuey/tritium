from importlib import import_module
from importlib.util import find_spec
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from . import reader
    from . import writer_foxglove
    from . import writer_json


def _has_spec(module_name: str) -> bool:
    try:
        return find_spec(module_name) is not None
    except ModuleNotFoundError:
        return False


def _has_all_specs(module_names: tuple[str, ...]) -> bool:
    return all(_has_spec(module_name) for module_name in module_names)


_lazy_imports = {}

__all__ = []

if _has_all_specs(('mcap', 'mcap.reader', 'mcap_protobuf', 'mcap_protobuf.decoder')):
    _lazy_imports['reader'] = 'reader'
    _lazy_imports['main'] = 'reader'
    __all__.extend(['reader', 'main'])

if _has_all_specs(('mcap', 'mcap.writer', 'attr')):
    _lazy_imports['writer_json'] = 'writer_json'
    __all__.append('writer_json')

if _has_all_specs((
        'mcap_protobuf',
        'mcap_protobuf.writer',
        'foxglove_schemas_protobuf',
        'google.protobuf',
        'numpy',
        'open3d',
)):
    _lazy_imports['writer_foxglove'] = 'writer_foxglove'
    __all__.append('writer_foxglove')


def __getattr__(name):
    if name not in _lazy_imports:
        raise AttributeError(f"module '{__name__}' has no attribute '{name}'")

    module_path = _lazy_imports[name]
    full_module_name = f'.{module_path}'
    module = import_module(full_module_name, package=__name__)
    value = module if name == module_path else getattr(module, name)
    globals()[name] = value
    return value
