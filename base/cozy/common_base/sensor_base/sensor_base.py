from dataclasses import dataclass, field
from enum import Enum
from typing import Any, Optional, List

from cozy.net_base import OneToManyRM


class SensorType(Enum):
    UNDEFINED = 'UNDEFINED'
    EGO = 'EGO'
    CAMERA = 'CAMERA'
    LIDAR = 'LIDAR'
    RADAR = 'RADAR'
    ULTRASONIC = 'ULTRASONIC'
    IMU = 'IMU'
    LIDAR4D = 'LIDAR4D'


@dataclass
class Sensor:
    sensor_id: str = ''
    sensor_type: SensorType = SensorType.UNDEFINED


@dataclass
class SensorData:
    sensor_data_id: str = ''
    timestamp: float = 0.0
    data: Any = None
    file_path: str = ''

    def load(self):
        raise NotImplementedError("Subclasses must implement load(self, source)")


@dataclass
class FrameRecord:
    sequence: int = 0
    p_sensor_data_id: str = ''  # 组帧主传感器数据ID
    timestamp: float = 0.0


@dataclass
class SensorDataManager:
    sensors: dict[str, Sensor] = field(default_factory=dict)
    sensor_datas: dict[str, SensorData] = field(default_factory=dict)
    frames: dict[int, FrameRecord] = field(default_factory=dict)
    p_sensor_id: str = 'img_cam0'  # 组帧主传感器ID
    frame2sensordata_map: OneToManyRM = field(default_factory=OneToManyRM)
    sensor2sensordata_map: OneToManyRM = field(default_factory=OneToManyRM)

    def add_sensor(self, sensor: Sensor):
        self.sensors[sensor.sensor_id] = sensor

    def add_sensor_data(self, sensor_data: SensorData, frame_index: int, sensor_id: str):
        self.sensor_datas[sensor_data.sensor_data_id] = sensor_data
        self.frame2sensordata_map.attach(frame_index, sensor_data.sensor_data_id)
        self.sensor2sensordata_map.attach(sensor_id, sensor_data.sensor_data_id)

    def add_frame_record(self, frame_record: FrameRecord):
        self.frames[frame_record.sequence] = frame_record

    def get_sensor_by_type(self, sensor_type: SensorType) -> dict[str, Sensor]:
        return {sid: sensor for sid, sensor in self.sensors.items() if sensor.sensor_type == sensor_type}

    def get_sensor_data_by_type(self, sensor_type: SensorType) -> dict[str, SensorData]:
        sensor_ids = {sid for sid, sensor in self.sensors.items() if sensor.sensor_type == sensor_type}

        return {
            sdid: sd
            for sdid, sd in self.sensor_datas.items() if self.sensor2sensordata_map.get_inv(sdid) in sensor_ids
        }

    def get_frame_sensor_datas_by_index(self, frame_index: int) -> dict[str, SensorData]:
        return {
            sdid: sd
            for sdid, sd in self.sensor_datas.items() if self.frame2sensordata_map.get_inv(sdid) == frame_index
        }

    def get_sensor_id_by_sensor_data_id(self, sensor_data_id: str) -> Optional[str]:
        return self.sensor2sensordata_map.get_inv(sensor_data_id)

    def get_frame_index_by_sensor_data_id(self, sensor_data_id: str) -> Optional[int]:
        return self.frame2sensordata_map.get_inv(sensor_data_id)

    def get_frame_indexes(self) -> List[int]:
        return list(self.frames.keys())
