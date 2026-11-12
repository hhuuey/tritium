import numpy as np


# q: 四元数 [x, y, z, w]
# R: 旋转矩阵 3x3 numpy array
def q_to_r(q: list) -> np.ndarray:
    x, y, z, w = q
    R = np.array([[1 - 2 * (y**2 + z**2), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                  [2 * (x * y + z * w), 1 - 2 * (x**2 + z**2), 2 * (y * z - x * w)],
                  [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x**2 + y**2)]])
    return R


# R: 旋转矩阵 3x3 numpy array
# q: 四元数 [x, y, z, w]
# Shepperd Method
def r_to_q(R: np.ndarray) -> list:
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
    return [float(qx), float(qy), float(qz), float(qw)]


# rpy: 旋转向量 roll, pitch, yaw
# R: 旋转矩阵 3x3 numpy array
def rpy_to_r(rpy: list) -> np.ndarray:
    roll, pitch, yaw = rpy
    # 绕 x, y, z 轴的旋转矩阵
    Rx = np.array([[1, 0, 0], [0, np.cos(roll), -np.sin(roll)], [0, np.sin(roll), np.cos(roll)]])
    Ry = np.array([[np.cos(pitch), 0, np.sin(pitch)], [0, 1, 0], [-np.sin(pitch), 0, np.cos(pitch)]])
    Rz = np.array([[np.cos(yaw), -np.sin(yaw), 0], [np.sin(yaw), np.cos(yaw), 0], [0, 0, 1]])
    # 顺序为 roll->pitch->yaw, 外旋
    # 也可是 yaw->pitch->roll, 内旋
    R = Rz @ Ry @ Rx
    return R


# R: 旋转矩阵 3x3 numpy array
# rpy: 旋转向量 roll, pitch, yaw
def r_to_rpy(R: np.ndarray) -> list:
    assert R.shape == (3, 3)
    sy = np.sqrt(R[0, 0] * R[0, 0] + R[1, 0] * R[1, 0])
    singular = sy < 1e-6
    if not singular:
        roll = np.arctan2(R[2, 1], R[2, 2])
        pitch = np.arctan2(-R[2, 0], sy)
        yaw = np.arctan2(R[1, 0], R[0, 0])
    else:
        roll = np.arctan2(-R[1, 2], R[1, 1])
        pitch = np.arctan2(-R[2, 0], sy)
        yaw = 0
    return [roll, pitch, yaw]


# rpy: 旋转向量 roll, pitch, yaw
# q: 四元数 [x, y, z, w]
def rpy_to_q(rpy: list) -> list:
    R = rpy_to_r(rpy)
    q = r_to_q(R)
    return q


# q: 四元数 [x, y, z, w]
# rpy: 旋转向量 roll, pitch, yaw
def q_to_rpy(q: list) -> list:
    R = q_to_r(q)
    rpy = r_to_rpy(R)
    return rpy
