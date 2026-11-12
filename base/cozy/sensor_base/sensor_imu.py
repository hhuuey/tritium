from dataclasses import dataclass
from .sensor_base import Sensor, SensorType, SensorData


@dataclass
class IMUSensor(Sensor):
    sensor_type: SensorType = SensorType.IMU
    sampling_rate_hz: float = 100.0  # 采样率，单位Hz


@dataclass
class IMURecord(SensorData):
    sensor_type: SensorType = SensorType.IMU
    acceleration_x: float = 0.0  # 加速度-前后方向，单位m/s²
    acceleration_y: float = 0.0  # 加速度-左右方向，单位m/s²
    acceleration_z: float = 0.0  # 加速度-垂直方向，单位m/s²
    angular_velocity_x: float = 0.0  # 角速度-绕前后轴旋转，单位rad/s
    angular_velocity_y: float = 0.0  # 角速度-绕左右轴旋转，单位rad/s
    angular_velocity_z: float = 0.0  # 角速度-绕垂直轴旋转，单位rad/s
