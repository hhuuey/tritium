from importlib import import_module
from importlib.util import find_spec
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from . import m_visualization
    from . import text


def _has_spec(module_name: str) -> bool:
    try:
        return find_spec(module_name) is not None
    except ModuleNotFoundError:
        return False


def _has_all_specs(module_names: tuple[str, ...]) -> bool:
    return all(_has_spec(module_name) for module_name in module_names)


_lazy_imports = {}

__all__ = []

if _has_all_specs(('cv2', 'numpy', 'PIL')):
    _lazy_imports.update({
        'text': 'text',
        'draw_text_with_outline': 'text',
        'draw_unicode_text_with_outline': 'text',
        'get_default_cjk_font_path': 'text',
    })
    __all__.extend([
        'text',
        'draw_text_with_outline',
        'draw_unicode_text_with_outline',
        'get_default_cjk_font_path',
    ])

if _has_all_specs(('numpy', 'open3d', 'folium', 'common', 'common.m_lidar_io', 'common.m_calib_comon')):
    _lazy_imports['m_visualization'] = 'm_visualization'
    __all__.append('m_visualization')


def __getattr__(name):
    if name not in _lazy_imports:
        raise AttributeError(f"module '{__name__}' has no attribute '{name}'")

    module_path = _lazy_imports[name]
    full_module_name = f'.{module_path}'
    module = import_module(full_module_name, package=__name__)
    value = module if name == module_path else getattr(module, name)
    globals()[name] = value
    return value
