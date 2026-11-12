from camera_model_common import intrinsic_form_matrix
import numpy as np


# projection: pinhole[fx, fy, cx, cy]
# distortion: rad_tan[k1, k2, p1, p2, k3]
class PinholeCam:
    def __init__(self, cam_wh, intrinsic, distortion=None):
        assert distortion is None or len(distortion) == 5
        self.w = cam_wh[0]
        self.h = cam_wh[1]
        self.K = intrinsic_form_matrix(intrinsic)
        self.K_inv = np.linalg.inv(self.K)
        self.distortion_ = distortion

    def lift(self, uv):
        xy = np.dot(self.K_inv, np.append(np.array(uv), 1.))[:-1]

        # iterate find undistorted points
        pts_u = self.undistortion(xy)

        # return pts_u / np.linalg.norm(pts_u)
        return pts_u / pts_u[2]

    def project(self, xyz):
        assert abs(xyz[2]) > 1e-6

        # distortion
        xy1 = np.array(xyz) / xyz[2]
        dx_dy = self.distortion(xy1[:-1])
        xy1[0] += dx_dy[0]
        xy1[1] += dx_dy[1]

        # projection
        uv_1 = np.dot(self.K, xy1)
        return uv_1[:-1]

    # return 畸变的相对变化量
    # xy in z=1 plan 相机坐标系
    def distortion(self, xy):
        if self.distortion_ is not None:
            x_u = xy[0]
            y_u = xy[1]
            k1, k2, p1, p2, k3 = self.distortion_
            x2 = x_u * x_u
            y2 = y_u * y_u
            x_y = x_u * y_u
            r2 = x2 + y2
            r4 = r2 * r2
            r6 = r4 * r2
            ratio = k1 * r2 + k2 * r4 + k3 * r6
            dx = x_u * ratio + 2 * p1 * x_y + p2 * (r2 + 2 * x2)
            dy = y_u * ratio + p1 * (r2 + 2 * y2) + 2 * p2 * x_y
        else:
            dx = 0.
            dy = 0.
        return [dx, dy]

    # xy in z=1 plan
    def undistortion(self, xy):
        if self.distortion_ is None:
            return np.append(xy, 1.)
        # Recursive distortion model
        n = 8
        mx_d = xy[0]
        my_d = xy[1]
        d_u = self.distortion(xy)

        # Approximate value
        x_u = mx_d - d_u[0]
        y_u = my_d - d_u[1]

        for i in range(n):
            d_u = self.distortion([x_u, y_u])
            x_u = mx_d - d_u[0]
            y_u = my_d - d_u[1]

        return np.array([x_u, y_u, 1.])
