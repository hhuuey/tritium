from typing import List
import numpy as np
from dataclasses import dataclass, field

quaternion_eps = 1e-8


@dataclass
class Vector:
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0

    @staticmethod
    def from_numpy(array: np.ndarray) -> "Vector":
        if array.shape != (3, ):
            raise ValueError("Input array must be of shape (3,)")
        return Vector(array[0], array[1], array[2])

    @staticmethod
    def from_list(lst: List[float]) -> "Vector":
        if len(lst) != 3:
            raise ValueError("Input list must have exactly three elements.")
        return Vector(lst[0], lst[1], lst[2])

    def to_numpy(self) -> np.ndarray:
        return np.array([self.x, self.y, self.z])

    def __add__(self, other: "Vector") -> "Vector":
        if not isinstance(other, Vector):
            raise TypeError("Addition is only supported between two Vectors.")
        return Vector(self.x + other.x, self.y + other.y, self.z + other.z)

    def __sub__(self, other: "Vector") -> "Vector":
        if not isinstance(other, Vector):
            raise TypeError("Subtraction is only supported between two Vectors.")
        return Vector(self.x - other.x, self.y - other.y, self.z - other.z)

    def __truediv__(self, scalar: float) -> "Vector":
        if not isinstance(scalar, (int, float)):
            raise TypeError("Division is only supported between a Vector and a scalar.")
        if scalar == 0:
            raise ZeroDivisionError("Division by zero is not allowed.")
        return Vector(self.x / scalar, self.y / scalar, self.z / scalar)

    def norm(self) -> float:
        return np.sqrt(self.x**2 + self.y**2 + self.z**2)  # Euclidean norm

    def normalize(self):
        norm = self.norm()
        if norm == 0:
            return Vector(0.0, 0.0, 0.0)
        return self / norm

    def __mul__(self, scalar: float) -> "Vector":
        if not isinstance(scalar, (int, float)):
            raise TypeError("Multiplication is only supported between a Vector and a scalar.")
        return Vector(self.x * scalar, self.y * scalar, self.z * scalar)

    def dot(self, other: "Vector") -> float:
        if not isinstance(other, Vector):
            raise TypeError("Dot product is only supported between two Vectors.")
        return self.x * other.x + self.y * other.y + self.z * other.z

    def skew(self) -> np.ndarray:
        return np.array([[0, -self.z, self.y], [self.z, 0, -self.x], [-self.y, self.x, 0]])

    def cross(self, other: "Vector") -> "Vector":
        if not isinstance(other, Vector):
            raise TypeError("Cross product is only supported between two Vectors.")
        # (y1*z2 - z1*y2, z1*x2 - x1*z2, x1*y2 - y1*x2)
        return Vector.from_numpy(self.skew() @ other.to_numpy())


@dataclass
class AngleAxis:
    angle: float = 0.0
    axis: Vector = field(default_factory=lambda: Vector(1.0, 0.0, 0.0))

    def rotate(self, vector3d: np.ndarray):
        v = np.asarray(vector3d, dtype=float)
        if v.shape == (3, ):
            k = np.asarray(self.axis, dtype=float)
            k = k / np.linalg.norm(k)
            cos_theta = np.cos(self.angle)
            sin_theta = np.sin(self.angle)
            # rodrigues formula
            # v' = v * cosθ + (k × v) * sinθ + k * (k · v) * (1 − cosθ)
            rotated_v = v * cos_theta + np.cross(k, v) * sin_theta + k * np.dot(k, v) * (1 - cos_theta)
            return rotated_v
        raise ValueError("Input must be a 3D vector.")


# Hamilton Quaternion Representation
@dataclass
class HQuaternion:
    w: float = 1.0
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0

    # 四元数乘法
    def __mul__(self, other: "HQuaternion") -> "HQuaternion":
        if not isinstance(other, HQuaternion):
            raise TypeError("Multiplication is only supported between two HamiltonQuaternions.")

        w1, x1, y1, z1 = self.w, self.x, self.y, self.z
        w2, x2, y2, z2 = other.w, other.x, other.y, other.z
        return HQuaternion(w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2, w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2,
                           w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2, w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2)

    def conjugate(self) -> "HQuaternion":
        return HQuaternion(self.w, -self.x, -self.y, -self.z)

    def norm(self) -> float:
        return np.sqrt(self.w**2 + self.x**2 + self.y**2 + self.z**2)

    def normalize(self):
        norm = self.norm()
        if norm == 0:
            self = HQuaternion(1.0, 0.0, 0.0, 0.0)
        else:
            self = HQuaternion(self.w / norm, self.x / norm, self.y / norm, self.z / norm)
        return self

    def exp(self) -> "HQuaternion":
        v_norm = self.norm()
        exp_w = np.exp(self.w)
        if v_norm < quaternion_eps:
            return HQuaternion(exp_w, 0.0, 0.0, 0.0)
        coeff = exp_w * np.sin(v_norm) / v_norm
        return HQuaternion(exp_w * np.cos(v_norm), coeff * self.x, coeff * self.y, coeff * self.z)

    def log(self) -> "HQuaternion":
        v_norm = self.norm()
        if v_norm < quaternion_eps:
            return HQuaternion(np.log(v_norm), 0.0, 0.0, 0.0)
        theta = np.arccos(self.w / v_norm)
        coeff = theta / v_norm
        return HQuaternion(np.log(v_norm), coeff * self.x, coeff * self.y, coeff * self.z)


@dataclass
class RQuaternion(HQuaternion):

    def __init__(self, w: float = 1.0, x: float = 0.0, y: float = 0.0, z: float = 0.0):
        super().__init__(w, x, y, z)
        self = self.normalize()

    def get_angle(self) -> float:
        return 2 * np.arccos(self.w)  # theta

    def get_axis(self) -> List[float]:
        s = np.sqrt(1 - self.w**2)  # sin(theta/2)
        if s < quaternion_eps:
            return [self.x, self.y, self.z]
        else:
            return [self.x / s, self.y / s, self.z / s]

    # 角轴表示
    def get_angle_axis(self) -> AngleAxis:
        angle = self.get_angle()
        axis = self.get_axis()
        return AngleAxis(angle, Vector.from_list(axis))

    # 双边乘
    def rotate(self, vector3d):
        v = np.asarray(vector3d, dtype=float)
        if v.shape == (3, ):
            qv = RQuaternion(0.0, v[0], v[1], v[2])
            res = self * qv * self.conjugate()
            return np.array([res.x, res.y, res.z])
        raise ValueError("Input must be a 3D vector.")

    # 时间导数
    def derivative(self, omega: np.ndarray):
        v = np.asarray(omega, dtype=float)
        if v.shape == (3, ):
            qv = RQuaternion(0.0, v[0], v[1], v[2])
            res = self * qv
            return np.array([-2 * res.x, -2 * res.y, -2 * res.z])
        raise ValueError("Input must be a 3D vector.")

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
        return RQuaternion(qw, qx, qy, qz)

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
