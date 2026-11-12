from cozy.sensor_base.camera_model.pinhole_cam import PinholeCam
from cozy.sensor_base.camera_model.equidistant_cam import EquidistantCam
from cozy.sensor_base.camera_model.camera_model_common import intrinsic_form_matrix
import cv2
import numpy as np


if __name__ == '__main__':
    test_fisheye = True
    cam_info = [752, 480]
    cam_intrinsic = [712.557492, 714.825860, 370.075592, 244.759309]
    distortion = [-0.473, 0.273, -0.001, 0.001, 0.0]
    distortion_equidistant = [0.01, 0.001, 0.000003, 0.00000001]
    pts = [2.0, -1.0, 4.0]
    rvec = np.array([[0], [0], [0]], dtype=np.float64)
    tvec = np.array([[0], [0], [0]], dtype=np.float64)

    if test_fisheye:
        cam = EquidistantCam(cam_info, cam_intrinsic, distortion_equidistant)
    else:
        cam = PinholeCam(cam_info, cam_intrinsic, distortion)

    uv = cam.project(pts)
    pts_lift = cam.lift(uv)
    if test_fisheye:
        uv_cv2, _ = cv2.fisheye.projectPoints(np.array(pts, dtype=np.float64).reshape(-1, 1, 3), rvec, tvec,
                                              intrinsic_form_matrix(cam_intrinsic).reshape(3, 3),
                                              np.array(distortion_equidistant, dtype=np.float64))
        pts_lift_cv2 = cv2.fisheye.undistortPoints(np.array(uv, dtype=np.float64).reshape(-1, 1, 2), intrinsic_form_matrix(cam_intrinsic).reshape(3, 3),
                                                   np.array(distortion_equidistant, dtype=np.float64))
    else:
        uv_cv2, _ = cv2.projectPoints(np.array(pts), rvec, tvec, intrinsic_form_matrix(cam_intrinsic),
                                      np.array(distortion))
        pts_lift_cv2 = cv2.undistortPoints(uv, intrinsic_form_matrix(cam_intrinsic), np.array(distortion))

    print("pts: {}".format(pts))

    print("my project: {}".format(uv))
    print("cv project: {}".format(uv_cv2))

    print("my lift: {}".format(pts_lift))
    print("cv lift: {}".format(pts_lift_cv2))
