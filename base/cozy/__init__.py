# 类型提示导入 - 仅用于类型检查，不影响运行时
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from . import map_base as mb
    from . import loc_base as lb
    from . import mllm_base as mlb
    from . import net_base as nb
    from . import engineer_base as eb
    from . import visualization_base as vb
    from . import phigent_base as pb

# 懒加载配置 - 只在需要时才导入模块
_lazy_imports = {
    'mb': 'map_base',
    'lb': 'loc_base',
    'mlb': 'mllm_base',
    'nb': 'net_base',
    'eb': 'engineer_base',
    'vb': 'visualization_base',
    'pb': 'phigent_base',
}

__all__ = ['mb', 'lb', 'mlb', 'nb', 'vb', 'eb', 'pb']


def __getattr__(name):
    """懒加载：只在实际使用时才导入模块"""
    if name in _lazy_imports:
        from importlib import import_module
        module_path = _lazy_imports[name]

        # 直接导入完整路径（从 cozy 包下）
        full_module_name = f'.{module_path}'
        module = import_module(full_module_name, package=__name__)

        # 缓存已导入的模块
        globals()[name] = module
        return module

    raise AttributeError(f"module '{__name__}' has no attribute '{name}'")
