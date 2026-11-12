from typing import List
from .sensor_base import Sensor, SensorType, SensorData
from dataclasses import dataclass, field


@dataclass
class Lidar4DRecord(Sensor):
    # 固定为 LIDAR4D，不允许通过构造函数修改
    sensor_type: SensorType = field(default=SensorType.LIDAR4D, init=False)
    range: List[float] = field(default_factory=lambda: [0.0, 0.0, 0.0, 0.0, 0.0, 0.0])  # 激光雷达4D的测量范围 [x_min, x_max, y_min, y_max, z_min, z_max]


@dataclass
class Lidar4DPCDRecord(SensorData):

    def load(self):
        # 这里可以实现4D激光雷达数据的加载逻辑，例如从文件路径加载点云数据
        if self.file_path:
            # 假设使用某个库加载4D点云数据
            pass
        return
