from dataclasses import dataclass, field

from cozy import lb

import numpy as np


@dataclass
class TFRecord:
    tf_id: str = ''
    source_sensor_id: str = ''
    target_sensor_id: str = ''
    rotation: lb.Quaternion = field(default_factory=lb.Quaternion)
    translation: lb.Vector = field(default_factory=lb.Vector)
    timestamp: float = -1.0

    @staticmethod
    def from_transform_matrix(source_sensor_id: str,
                              target_sensor_id: str,
                              matrix: list[list[float]],
                              timestamp: float = -1.0):
        assert len(matrix) == 4 and all(len(row) == 4 for row in matrix), "Matrix must be 4x4"
        rotation = lb.Quaternion.from_rotation_matrix(
            np.array([
                [matrix[0][0], matrix[0][1], matrix[0][2]],
                [matrix[1][0], matrix[1][1], matrix[1][2]],
                [matrix[2][0], matrix[2][1], matrix[2][2]],
            ]))
        translation = lb.Vector(matrix[0][3], matrix[1][3], matrix[2][3])
        return TFRecord(tf_id=source_sensor_id + "_" + target_sensor_id + "_" + f"{timestamp:.3f}",
                        source_sensor_id=source_sensor_id,
                        target_sensor_id=target_sensor_id,
                        rotation=rotation,
                        translation=translation,
                        timestamp=timestamp)

    def to_transform_matrix(self) -> np.ndarray:
        rot_matrix = self.rotation.to_rotation_matrix()
        translation = np.array([[self.translation.x], [self.translation.y], [self.translation.z]])
        transform_matrix = np.eye(4)
        transform_matrix[0:3, 0:3] = rot_matrix
        transform_matrix[0:3, 3:4] = translation
        return transform_matrix


class TFTransformManager:

    def __init__(self):
        self.tf_records = {}

    def add_tf_record(self, tf_record: TFRecord):
        self.tf_records[tf_record.tf_id] = tf_record

    def get_transform_matrix(self, source_sensor_id: str, target_sensor_id: str, timestamp: float = -1.0) -> np.ndarray:
        tf_id = f"{source_sensor_id}_{target_sensor_id}_{timestamp:.3f}"
        tf_id_rev = f"{target_sensor_id}_{source_sensor_id}_{timestamp:.3f}"
        if tf_id not in self.tf_records and tf_id_rev not in self.tf_records:
            raise ValueError(
                f"Transform matrix from {source_sensor_id} to {target_sensor_id} at timestamp {timestamp} not found.")
        if tf_id in self.tf_records:
            return self.tf_records[tf_id].to_transform_matrix()
        return np.linalg.inv(self.tf_records[tf_id_rev].to_transform_matrix())
