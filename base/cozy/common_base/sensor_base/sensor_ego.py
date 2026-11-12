from dataclasses import dataclass
from .sensor_base import Sensor, SensorType

@dataclass
class EgoRecord(Sensor):
    sensor_type: SensorType = SensorType.EGO
    wheel_base: float = 0.0  # 轴距