from importlib import import_module
from importlib.util import find_spec
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from . import loc_visual
    from .loc_visual import draw_axes, run_odom_slerp_demo


def _has_spec(module_name: str) -> bool:
    try:
        return find_spec(module_name) is not None
    except ModuleNotFoundError:
        return False


def _has_all_specs(module_names: tuple[str, ...]) -> bool:
    return all(_has_spec(module_name) for module_name in module_names)


_lazy_imports = {}

__all__ = []

if _has_all_specs(('OpenGL', 'OpenGL.GL', 'OpenGL.GLUT', 'OpenGL.GLU', 'numpy')):
    _lazy_imports.update({
        'loc_visual': 'loc_visual',
        'draw_axes': 'loc_visual',
        'run_odom_slerp_demo': 'loc_visual',
    })
    __all__.extend(['loc_visual', 'draw_axes', 'run_odom_slerp_demo'])


def __getattr__(name):
    if name not in _lazy_imports:
        raise AttributeError(f"module '{__name__}' has no attribute '{name}'")

    module_path = _lazy_imports[name]
    full_module_name = f'.{module_path}'
    module = import_module(full_module_name, package=__name__)
    value = module if name == module_path else getattr(module, name)
    globals()[name] = value
    return value
