from cozy.sensor_base.camera_model.camera_model_common import intrinsic_form_matrix
import numpy as np


# projection: pinhole[mu, mv, u0, v0]
# distortion: rad_tan[k2, k3, k4, k5]
class EquidistantCam:
    def __init__(self, cam_wh, intrinsic, distortion=None):
        assert distortion is None or len(distortion) == 4
        self.w = cam_wh[0]
        self.h = cam_wh[1]
        self.K = intrinsic_form_matrix(intrinsic)
        self.K_inv = np.linalg.inv(self.K)
        self.distortion_ = distortion

    def project(self, xyz):
        assert abs(xyz[2]) > 1e-6

        # distortion
        xy1 = np.array(xyz) / xyz[2]
        r = np.sqrt(xy1[0] * xy1[0] + xy1[1] * xy1[1])
        theta = np.arctan(r)
        theta_2 = theta * theta
        theta_4 = theta_2 * theta_2
        theta_6 = theta_4 * theta_2
        theta_8 = theta_4 * theta_4

        if self.distortion_ is not None:
            theta_d = theta * (1 + self.distortion_[0] * theta_2 + self.distortion_[1] * theta_4 +
                               self.distortion_[2] * theta_6 + self.distortion_[3] * theta_8)
        else:
            theta_d = theta

        ratio = theta_d / r
        x_d = xy1[0] * ratio
        y_d = xy1[1] * ratio
        uv_1 = np.dot(self.K, np.array((x_d, y_d, 1.)))

        return uv_1[:-1]

    def lift(self, uv):
        xy = np.dot(self.K_inv, np.append(np.array(uv), 1.))[:-1]
        theta, phi = self.backproject_symmetric(xy)

        p = np.array([0., 0., 0.])
        p[0] = np.sin(theta) * np.cos(phi)
        p[1] = np.sin(theta) * np.sin(phi)
        p[2] = np.cos(theta)

        return p / np.linalg.norm(p[2])

    def backproject_symmetric(self, p_u):
        tol = 1e-10
        p_u_norm = np.linalg.norm(p_u)

        if p_u_norm < 1e-10:
            phi = 0.0
        else:
            phi = np.arctan2(p_u[1], p_u[0])

        npow = 9
        if self.distortion_[3] < 1e-20:
            npow -= 2
        if self.distortion_[2] < 1e-20:
            npow -= 2
        if self.distortion_[1] < 1e-20:
            npow -= 2
        if self.distortion_[0] < 1e-20:
            npow -= 2

        coeffs = np.zeros((npow + 1, 1))
        coeffs[0] = -p_u_norm
        coeffs[1] = 1.0

        if npow >= 3:
            coeffs[3] = self.distortion_[0]
        if npow >= 5:
            coeffs[5] = self.distortion_[1]
        if npow >= 7:
            coeffs[7] = self.distortion_[2]
        if npow >= 9:
            coeffs[9] = self.distortion_[3]

        if npow == 1:
            theta = p_u_norm
        else:
            A = np.zeros((npow, npow))
            A[1:, :-1] = np.eye(npow - 1)
            A[:, -1] = -coeffs[:npow, 0] / coeffs[npow]
            eigval, _ = np.linalg.eig(A)
            thetas = []
            for t in eigval.real:
                if abs(t.imag) > tol:
                    continue

                if t < -tol:
                    continue
                elif t < 0.0:
                    t = 0.0

                thetas.append(t)

            if not thetas:
                theta = p_u_norm
            else:
                theta = min(thetas)

        return theta, phi