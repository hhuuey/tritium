from importlib import import_module
from importlib.util import find_spec
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from . import m_visualization


def _has_spec(module_name: str) -> bool:
    try:
        return find_spec(module_name) is not None
    except ModuleNotFoundError:
        return False


def _has_all_specs(module_names: tuple[str, ...]) -> bool:
    return all(_has_spec(module_name) for module_name in module_names)


_lazy_imports = {}

__all__ = []

if _has_all_specs(('numpy', 'open3d', 'folium', 'common', 'common.m_lidar_io', 'common.m_calib_comon')):
    _lazy_imports.update({
        'm_visualization': 'm_visualization',
        'create_windows_to_show_pcd': 'm_visualization',
        'create_windows_to_manual_calib': 'm_visualization',
        'create_windows_to_pick_pts': 'm_visualization',
        'FoliumMapVisualization': 'm_visualization',
    })
    __all__.extend([
        'm_visualization',
        'create_windows_to_show_pcd',
        'create_windows_to_manual_calib',
        'create_windows_to_pick_pts',
        'FoliumMapVisualization',
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
