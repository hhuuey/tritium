from tkinter import N
from pyparsing import Opt
from .bbox import BBox2D, BBox3D
from .det_match import OneToManyRM, OneToOneRM
from dataclasses import dataclass, field
from typing import Optional, Type, Dict, Any, Set
from enum import Enum


class DetCategory(Enum):
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


def add_attribute(name: str, enums: Optional[Type[Enum]] = None):

    def _decorator(cls):
        ann = getattr(cls, "__annotations__", None)
        if ann is None:
            cls.__annotations__ = {}
        cls.__annotations__[name] = Attribution  # add type annotation for dataclass
        setattr(cls, name, field(default_factory=lambda: Attribution(name=name, enums=enums)))
        return cls

    return _decorator


@dataclass
@add_attribute('category', enums=DetCategory)
class DetRecord:
    det_id: str
    score: float = 0.0
    sensor_data_ids: list[str] = field(default_factory=list)  # 检测结果信息源，关联对应的sensor data id列表

    def set(self, attr_name, attr_value, attr_score=1.0):
        if hasattr(self, attr_name):
            attr = getattr(self, attr_name)
            if isinstance(attr, Attribution):
                attr.type = attr_value
                attr.score = attr_score
            else:
                setattr(self, attr_name, attr_value)
        else:
            raise AttributeError(f"{self.__class__.__name__} has no attribute '{attr_name}'")


@dataclass
class Det2DRecord(DetRecord):
    bbox: BBox2D = field(default_factory=BBox2D)


@dataclass
class Det3DRecord(DetRecord):
    bbox: BBox3D = field(default_factory=BBox3D)


@dataclass
class Det2dManager:
    det_2d_datas: Dict[str, Any] = field(default_factory=dict)
    category2det2d_map: OneToManyRM = field(default_factory=OneToManyRM)  # key: category, value: set of det_ids
    img2det2d_map: OneToManyRM = field(default_factory=OneToManyRM)  # key: img_id, value: set of det_ids
    frame2det2d_map: OneToManyRM = field(default_factory=OneToManyRM)

    def add_img2det2d_record(self, img_id: str, det_id: str):
        self.img2det2d_map.attach(img_id, det_id)

    def add_frame2det2d_record(self, frame_index: int, det_id: str):
        self.frame2det2d_map.attach(frame_index, det_id)

    def add_det_2d_record(self, det_2d_record: Any, frame_index: Optional[int], img_id: Optional[str] = None):
        self.det_2d_datas[det_2d_record.det_id] = det_2d_record

        category = det_2d_record.category.type
        self.category2det2d_map.attach(category, det_2d_record.det_id)

        if frame_index is not None:
            self.add_frame2det2d_record(frame_index, det_2d_record.det_id)

        if img_id is not None:
            self.add_img2det2d_record(img_id, det_2d_record.det_id)

    def get_objs_by_ids(self, ids: Set[str]) -> Dict[str, Any]:
        return {det_id: self.det_2d_datas[det_id] for det_id in ids}

    def get_obj_ids(self,
                    category: Optional[Any] = None,
                    img_id: Optional[str] = None,
                    frame_index: Optional[int] = None) -> Set[str]:
        det_ids_sets = []

        if category is not None:
            det_ids_cat = self.category2det2d_map.get(category)
            det_ids_sets.append(det_ids_cat)

        if img_id is not None:
            det_ids_img = self.img2det2d_map.get(img_id)
            det_ids_sets.append(det_ids_img)

        if frame_index is not None:
            det_ids_frame = self.frame2det2d_map.get(frame_index)
            det_ids_sets.append(det_ids_frame)

        if not det_ids_sets:
            return set(self.det_2d_datas.keys())

        # 取交集
        det_ids = set.intersection(*det_ids_sets)
        return det_ids

    def get_objs(self,
                 category: Optional[Any] = None,
                 img_id: Optional[str] = None,
                 frame_index: Optional[int] = None) -> Dict[str, Any]:
        det_ids = self.get_obj_ids(category, img_id, frame_index)
        return self.get_objs_by_ids(det_ids)
