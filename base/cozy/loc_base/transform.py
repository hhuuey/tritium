import numpy as np


def rpy2rot(roll, pitch, yaw, dtype=float):
    R = np.zeros((3, 3), dtype=dtype)
    cr = np.cos(roll)
    sr = np.sin(roll)
    cp = np.cos(pitch)
    sp = np.sin(pitch)
    cy = np.cos(yaw)
    sy = np.sin(yaw)
    R[0][0] = cy * cp
    R[0][1] = cy * sp * sr - sy * cr
    R[0][2] = cy * sp * cr + sy * sr
    R[1][0] = sy * cp
    R[1][1] = sy * sp * sr + cy * cr
    R[1][2] = sy * sp * cr - cy * sr
    R[2][0] = -sp
    R[2][1] = cp * sr
    R[2][2] = cp * cr
    R = np.asmatrix(R)
    return R


def xyzrpy2tm(xyz, rpy, is_2d=False):
    homo_m = np.eye(4, dtype=np.double)
    if is_2d:
        R = rpy2rot(0, 0, rpy[2])
        homo_m[:2, 3] = xyz[:2]
    else:
        R = rpy2rot(rpy[0], rpy[1], rpy[2])
        homo_m[:3, 3] = xyz[:3]
    homo_m[:3, :3] = R
    return homo_m
