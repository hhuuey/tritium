from dataclasses import dataclass, field
from typing import List
import numpy as np

g_header_index = 0


@dataclass
class LocHeader:
    seq: int = 0
    stamp: float = 0.0  # 时间戳，单位秒
    frame_id: str = "base_link"


@dataclass
class Point:
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0


@dataclass
class Vector:
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0


@dataclass
class Quaternion:
    w: float = 1.0
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0

    @staticmethod
    def from_rotation_matrix(R: np.ndarray):
        assert R.shape == (3, 3)
        trace = np.trace(R)
        if trace > 0:
            S = np.sqrt(trace + 1.0) * 2  # S=4*qw
            qw = 0.25 * S
            qx = (R[2, 1] - R[1, 2]) / S
            qy = (R[0, 2] - R[2, 0]) / S
            qz = (R[1, 0] - R[0, 1]) / S
        elif (R[0, 0] > R[1, 1]) and (R[0, 0] > R[2, 2]):
            S = np.sqrt(1.0 + R[0, 0] - R[1, 1] - R[2, 2]) * 2  # S=4*qx
            qw = (R[2, 1] - R[1, 2]) / S
            qx = 0.25 * S
            qy = (R[0, 1] + R[1, 0]) / S
            qz = (R[0, 2] + R[2, 0]) / S
        elif R[1, 1] > R[2, 2]:
            S = np.sqrt(1.0 + R[1, 1] - R[0, 0] - R[2, 2]) * 2  # S=4*qy
            qw = (R[0, 2] - R[2, 0]) / S
            qx = (R[0, 1] + R[1, 0]) / S
            qy = 0.25 * S
            qz = (R[1, 2] + R[2, 1]) / S
        else:
            S = np.sqrt(1.0 + R[2, 2] - R[0, 0] - R[1, 1]) * 2  # S=4*qz
            qw = (R[1, 0] - R[0, 1]) / S
            qx = (R[0, 2] + R[2, 0]) / S
            qy = (R[1, 2] + R[2, 1]) / S
            qz = 0.25 * S
        return Quaternion(qw, qx, qy, qz)

    def to_rotation_matrix(self) -> np.ndarray:
        R = np.array([[
            1 - 2 * (self.y**2 + self.z**2), 2 * (self.x * self.y - self.z * self.w),
            2 * (self.x * self.z + self.y * self.w)
        ],
                      [
                          2 * (self.x * self.y + self.z * self.w), 1 - 2 * (self.x**2 + self.z**2),
                          2 * (self.y * self.z - self.x * self.w)
                      ],
                      [
                          2 * (self.x * self.z - self.y * self.w), 2 * (self.y * self.z + self.x * self.w),
                          1 - 2 * (self.x**2 + self.y**2)
                      ]])
        return R

    def normalize(self):
        norm = np.sqrt(self.w**2 + self.x**2 + self.y**2 + self.z**2)
        if norm == 0:
            return Quaternion(1.0, 0.0, 0.0, 0.0)
        return Quaternion(self.w / norm, self.x / norm, self.y / norm, self.z / norm)

    def conjugate(self) -> "Quaternion":
        return Quaternion(self.w, -self.x, -self.y, -self.z)

    def __mul__(self, other):
        """
        Hamilton product:
          - Quaternion * Quaternion -> Quaternion
          - Quaternion * 3-vector (array/list/tuple of length 3) -> rotated 3-vector (np.ndarray)
        """
        if isinstance(other, Quaternion):
            w1, x1, y1, z1 = self.w, self.x, self.y, self.z
            w2, x2, y2, z2 = other.w, other.x, other.y, other.z
            return Quaternion(w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2, w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2,
                              w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2, w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2)
        if isinstance(other, (np.ndarray, list, tuple)):
            v = np.asarray(other, dtype=float)
            if v.shape == (3, ):
                qv = Quaternion(0.0, v[0], v[1], v[2])
                res = self * qv * self.conjugate()
                return np.array([res.x, res.y, res.z])
        return NotImplemented


@dataclass
class Twist:
    linear: Vector = field(default_factory=Vector)
    angular: Vector = field(default_factory=Vector)
    covariance: List[float] = field(default_factory=list)  # 36


@dataclass
class Transform:
    header: LocHeader = field(default_factory=LocHeader)
    child_frame_id: str = ""
    translation: Vector = field(default_factory=Vector)
    rotation: Quaternion = field(default_factory=Quaternion)


@dataclass
class Pose:
    header: LocHeader = field(default_factory=LocHeader)
    position: Vector = field(default_factory=Vector)
    orientation: Quaternion = field(default_factory=Quaternion)
    covariance: List[float] = field(default_factory=list)  # 36

    def to_tuml(self):
        return f"{self.header.stamp} {self.position.x} {self.position.y} {self.position.z} {self.orientation.x} {self.orientation.y} {self.orientation.z} {self.orientation.w}"

    def from_tuml(self, line: str):
        parts = line.strip().split()
        if len(parts) != 8:
            raise ValueError("Invalid TUML format")
        self.header.stamp = float(parts[0])
        self.position = Vector(float(parts[1]), float(parts[2]), float(parts[3]))
        self.orientation = Quaternion(float(parts[7]), float(parts[4]), float(parts[5]), float(parts[6]))
        return self

    @staticmethod
    def from_transform_matrix(timestamp, transform: np.ndarray, frame_id="base_link", index=-1):
        assert transform.shape == (4, 4)
        return Pose.from_rt_matrix(timestamp, transform[:3, :3], transform[:3, 3], frame_id, index)

    def to_transform_matrix(self) -> np.ndarray:
        R = self.orientation.to_rotation_matrix()
        t = np.array([self.position.x, self.position.y, self.position.z]).reshape(3, 1)
        transform = np.eye(4)
        transform[:3, :3] = R
        transform[:3, 3] = t.flatten()
        return transform

    @staticmethod
    def from_rt_matrix(timestamp, r: np.ndarray, t: np.ndarray, frame_id="base_link", index=-1):
        assert r.shape == (3, 3)
        assert t.shape == (3, )
        pose = Pose()
        pose.header.stamp = timestamp
        pose.header.frame_id = frame_id
        if index >= 0:
            pose.header.seq = index
        else:
            global g_header_index
            pose.header.seq = g_header_index
            g_header_index += 1
        pose.position = Vector(t[0], t[1], t[2])
        pose.orientation = Quaternion.from_rotation_matrix(r)
        return pose
