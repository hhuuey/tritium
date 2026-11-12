from importlib import import_module
from importlib.util import find_spec
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from . import mcap
    from . import open3d
    from . import opencv
    from . import opengl
    from . import qgis


def _has_spec(module_name: str) -> bool:
    try:
        return find_spec(module_name) is not None
    except ModuleNotFoundError:
        return False


def _has_all_specs(module_names: tuple[str, ...]) -> bool:
    return all(_has_spec(module_name) for module_name in module_names)


_lazy_imports = {
    'mcap': 'mcap',
    'open3d': 'open3d',
    'opencv': 'opencv',
    'opengl': 'opengl',
    'qgis': 'qgis',
}

__all__ = [
    'mcap',
    'open3d',
    'opencv',
    'opengl',
    'qgis',
]

if _has_all_specs(('cv2', 'numpy', 'PIL')):
    _lazy_imports.update({
        'draw_text_with_outline': 'opencv',
        'draw_unicode_text_with_outline': 'opencv',
        'get_default_cjk_font_path': 'opencv',
    })
    __all__.extend([
        'draw_text_with_outline',
        'draw_unicode_text_with_outline',
        'get_default_cjk_font_path',
    ])

if _has_all_specs(('OpenGL', 'OpenGL.GL', 'OpenGL.GLUT', 'OpenGL.GLU', 'numpy')):
    _lazy_imports.update({
        'draw_axes': 'opengl',
        'run_odom_slerp_demo': 'opengl',
    })
    __all__.extend([
        'draw_axes',
        'run_odom_slerp_demo',
    ])

if _has_all_specs(('qgis', 'qgis.core')):
    _lazy_imports.update({
        'NodeInfo': 'qgis',
        'project_qgis': 'qgis',
    })
    __all__.extend([
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
