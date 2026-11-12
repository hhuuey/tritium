import numpy as np
import cv2



def opencv_cam_test():
    # 创建一张全白的图像
    image_size = (900, 1800)  # h, w
    image = 255 * np.ones((image_size[0], image_size[1], 3), dtype=np.uint8)

    # 在图像上绘制一些点，每个点之间行和列间隔都是50个像素
    point_size = 5
    interval = 50

    # 存储点的坐标
    points = []
    for i in range(0, image_size[0] + 1, interval):
        for j in range(0, image_size[1] + 1, interval):
            cv2.circle(image, (j, i), point_size, (0, 0, 0), -1)
            points.append((j, i))

    # 连接相邻的点（左右和上下）
    for i in range(image_size[0] // interval):
        for j in range(image_size[1] // interval):
            # 左右连接
            cv2.line(image, points[i * (image_size[1] // interval + 1) + j],
                     points[i * (image_size[1] // interval + 1) + j + 1], (0, 0, 0), 1)
            # 上下连接
            cv2.line(image, points[i * (image_size[1] // interval + 1) + j],
                     points[(i + 1) * (image_size[1] // interval + 1) + j], (0, 0, 0), 1)

    # 定义相机的畸变参数
    k1, k2, p1, p2 = 0.15, 0.001, 0.001, 0.001  # 曲率和切向畸变参数

    # 创建相机矩阵
    fx, fy = 1000, 1000  # 焦距
    cx, cy = image_size[1] // 2, image_size[0] // 2  # 中心点坐标
    camera_matrix = np.array([[fx, 0, cx], [0, fy, cy], [0, 0, 1]], dtype=np.float64)

    # 应用桶形畸变和枕形畸变
    distortion_coefficients = np.array([k1, k2, p1, p2], dtype=np.float64)
    image_undistorted = cv2.undistort(image, camera_matrix, distortion_coefficients, camera_matrix)

    # 显示原始图像和畸变后的图像
    cv2.imshow('Original Image', image)
    cv2.imshow('Undistorted Image', image_undistorted)
    cv2.waitKey(0)
    cv2.destroyAllWindows()
    return






