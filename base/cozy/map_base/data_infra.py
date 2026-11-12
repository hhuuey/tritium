from dataclasses import dataclass
from dataclasses_json import dataclass_json
from typing import Type


def map_dataclass(cls: Type) -> Type:
    """Apply @dataclass and @dataclass_json in one decorator."""
    cls = dataclass(cls)
    cls = dataclass_json(cls)
    return cls


g_geo_eps_ = 1e-8
EARTH_RADIUS = 6378137.0  # WGS84 地球半径（单位：米）
