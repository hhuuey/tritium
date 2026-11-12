from .bbox import BBox2D, BBox3D

from enum import Enum
from typing import Optional
from dataclasses import dataclass, field

from typing import Type


class ObjCategory(Enum):
    TRAFFIC_LIGHT = "TRAFFIC_LIGHT"
    TRAFFIC_LIGHT_BULB = "TRAFFIC_LIGHT_BULB"
    REGULAR_VEHICLE = "REGULAR_VEHICLE"
    PEDESTRIAN = "PEDESTRIAN"
    BOLLARD = "BOLLARD"
    CONSTRUCTION_CONE = "CONSTRUCTION_CONE"
    CONSTRUCTION_BARREL = "CONSTRUCTION_BARREL"
    STOP_SIGN = "STOP_SIGN"
    BICYCLE = "BICYCLE"
    LARGE_VEHICLE = "LARGE_VEHICLE"
    UNKNOWN = "UNKNOWN"


@dataclass
class Attribution:
    name: str
    type: Optional[Enum] = None
    score: float = 0.0
    enums: Optional[Type[Enum]] = None


@dataclass
class NObj:
    id: str = ''
    score: float = -1.0
    category: Attribution = field(default_factory=lambda: Attribution(name="category", enums=ObjCategory))


@dataclass
class NObj3D(NObj):
    bbox: Optional[BBox3D] = None


@dataclass
class NObj2D(NObj):
    bbox: Optional[BBox2D] = None


if __name__ == "__main__":

    @define_obj(ObjCategory.TRAFFIC_LIGHT, is_3d=True)
    class TrafficLightObj:
        light_state: str = "unknown"
        bulb_count: int = 3

    @define_obj(ObjCategory.TRAFFIC_LIGHT_BULB, is_3d=False)
    class TrafficLightBulbObj:
        color: str = "red"
        brightness: float = 1.0

    traffic_light = TrafficLightObj(
        id="tl_001",
        light_state="green",
        bulb_count=3,
    )

    bulb = TrafficLightBulbObj(
        id="bulb_001",
        color="red",
    )

    print(traffic_light.category)  # 输出: ObjCategory.TRAFFIC_LIGHT
