from typing import List, Optional
import bisect
import numpy as np

from .loc_infra import Pose, Vector, Quaternion, LocHeader


def position_interp(p0: Vector, p1: Vector, ratio: float) -> Vector:
    x = p0.x + ratio * (p1.x - p0.x)
    y = p0.y + ratio * (p1.y - p0.y)
    z = p0.z + ratio * (p1.z - p0.z)
    return Vector(x, y, z)


def quaternion_slerp(q1: Quaternion, q2: Quaternion, t: float) -> Quaternion:
    q1_np = np.array([q1.w, q1.x, q1.y, q1.z], dtype=float)
    q2_np = np.array([q2.w, q2.x, q2.y, q2.z], dtype=float)
    dot = np.dot(q1_np, q2_np)
    if dot < 0.0:
        q2_np = -q2_np
        dot = -dot
    dot = np.clip(dot, -1.0, 1.0)
    if dot > 0.9995:
        result = q1_np + t * (q2_np - q1_np)
        result /= np.linalg.norm(result)
        return Quaternion(result[0], result[1], result[2], result[3])
    theta_0 = np.arccos(dot)
    sin_theta_0 = np.sin(theta_0)
    theta = theta_0 * t
    sin_theta = np.sin(theta)
    s0 = np.cos(theta) - dot * sin_theta / sin_theta_0
    s1 = sin_theta / sin_theta_0
    out = (q1_np * s0) + (q2_np * s1)
    return Quaternion(out[0], out[1], out[2], out[3])


class Odom:

    def __init__(self, poses: List[Pose]):
        self.poses = sorted(poses, key=lambda p: p.header.stamp)
        self.timestamps = [p.header.stamp for p in self.poses]

    def get_nearest(self, timestamp: float) -> Optional[Pose]:
        if not self.poses:
            return None
        idx = bisect.bisect_left(self.timestamps, timestamp)
        if idx == 0:
            return self.poses[0]
        if idx >= len(self.poses):
            return self.poses[-1]
        before = self.poses[idx - 1]
        after = self.poses[idx]
        if abs(before.header.stamp - timestamp) < abs(after.header.stamp - timestamp):
            return before
        else:
            return after

    def interpolate(self, timestamp: float) -> Optional[Pose]:
        if not self.poses or len(self.poses) < 2:
            return None
        idx = bisect.bisect_left(self.timestamps, timestamp)
        if idx == 0:
            return self.poses[0]
        if idx >= len(self.poses):
            return self.poses[-1]
        p0 = self.poses[idx - 1]
        p1 = self.poses[idx]
        t0, t1 = p0.header.stamp, p1.header.stamp
        if t1 == t0:
            return p0
        ratio = (timestamp - t0) / (t1 - t0)
        # 位置线性插值
        p_interp = position_interp(p0.position, p1.position, ratio)
        # 姿态slerp插值
        q_interp = quaternion_slerp(p0.orientation, p1.orientation, ratio)
        return Pose(header=LocHeader(stamp=timestamp), position=p_interp, orientation=q_interp)
