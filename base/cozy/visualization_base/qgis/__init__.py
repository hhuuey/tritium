from importlib import import_module
from importlib.util import find_spec
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from . import base
    from . import map_visual
    from .base import NodeInfo, project_qgis
    from .map_visual import from_geojson, png_to_geotiff, read_elem_from_geojson, to_geojson, write_elem_to_geojson


def _has_spec(module_name: str) -> bool:
    try:
        return find_spec(module_name) is not None
    except ModuleNotFoundError:
        return False


def _has_all_specs(module_names: tuple[str, ...]) -> bool:
    return all(_has_spec(module_name) for module_name in module_names)


_lazy_imports = {
    'map_visual': 'map_visual',
    'to_geojson': 'map_visual',
    'from_geojson': 'map_visual',
    'write_elem_to_geojson': 'map_visual',
    'read_elem_from_geojson': 'map_visual',
}

__all__ = [
    'map_visual',
    'to_geojson',
    'from_geojson',
    'write_elem_to_geojson',
    'read_elem_from_geojson',
]

if _has_all_specs(('cv2', 'numpy', 'rasterio')):
    _lazy_imports['png_to_geotiff'] = 'map_visual'
    __all__.append('png_to_geotiff')

if _has_all_specs(('qgis', 'qgis.core')):
    _lazy_imports.update({
        'base': 'base',
        'NodeInfo': 'base',
        'project_qgis': 'base',
    })

    __all__.extend([
        'base',
        'NodeInfo',
        'project_qgis',
    ])


def __getattr__(name):
    if name not in _lazy_imports:
        raise AttributeError(f"module '{__name__}' has no attribute '{name}'")

    module_path = _lazy_imports[name]
    full_module_name = f'.{module_path}'
    module = import_module(full_module_name, package=__name__)
    value = module if name == module_path else getattr(module, name)
    globals()[name] = value
    return value
