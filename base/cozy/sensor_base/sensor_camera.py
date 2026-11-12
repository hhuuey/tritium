from .sensor_base import Sensor, SensorType, SensorData
import cv2
import numpy as np
from dataclasses import dataclass, field


@dataclass
class CamRecord(Sensor):
    # 固定为 CAMERA，不允许通过构造函数修改
    sensor_type: SensorType = field(default=SensorType.CAMERA, init=False)
    cam2img: np.ndarray = field(default_factory=lambda: np.eye(3))  # 3x3 相机内参矩阵
    width: int = 0  # 图像宽度
    height: int = 0  # 图像高度


@dataclass
class ImgRecord(SensorData):

    def load(self):
        if self.file_path:
            self.data = cv2.imread(self.file_path)
        return self.data
