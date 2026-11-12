from .sensor_base import FrameRecord, SensorDataManager, SensorType
from .sensor_ego import EgoRecord
from .sensor_camera import CamRecord, ImgRecord
from .sensor_lidar4d import Lidar4DRecord, Lidar4DPCDRecord
from .sensor_transform import TFRecord, TFTransformManager
from typing import Dict, TypeAlias

CamRecords: TypeAlias = Dict[str, CamRecord]
ImgRecords: TypeAlias = Dict[str, ImgRecord]
Lidar4DRecords: TypeAlias = Dict[str, Lidar4DRecord]
Lidar4DPCDRecords: TypeAlias = Dict[str, Lidar4DPCDRecord]
TFRecords: TypeAlias = Dict[str, TFRecord]
FrameRecords: TypeAlias = Dict[int, FrameRecord]

__all__ = [
    'CamRecord', 'ImgRecord', 'Lidar4DRecord', 'Lidar4DPCDRecord', 'TFRecord', 'FrameRecord', 'CamRecords',
    'ImgRecords', 'Lidar4DRecords', 'Lidar4DPCDRecords', 'TFRecords', 'FrameRecords', 'EgoRecord', 'TFTransformManager', 'SensorDataManager', 'SensorType'
]
