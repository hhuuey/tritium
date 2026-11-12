import numpy as np
import open3d as o3d


def read_kitti_bin_xyz(lidar_path):
    pointcloud = np.fromfile(str(lidar_path), dtype=np.float32, count=-1).reshape([-1, 4])
    return pointcloud[:, :3]


def read_kitti_bin_xyzi(lidar_path):
    xyzi = np.fromfile(str(lidar_path), dtype=np.float32, count=-1).reshape([-1, 4])
    xyzi[:, 3] = 256 * xyzi[:, 3]
    return xyzi


def xyz_to_o3d_pcd(xyz, color=[1., 0., 0.]):
    pcd = o3d.open3d.geometry.PointCloud()
    pcd.points = o3d.open3d.utility.Vector3dVector(xyz)
    pcd.paint_uniform_color(color)
    return pcd


def xyzi_to_o3d_pcd(xyzi):
    pcd = o3d.open3d.geometry.PointCloud()
    pcd.points = o3d.open3d.utility.Vector3dVector(xyzi[:, :3])
    rgb = np.column_stack([xyzi[:, 3] / 255., np.zeros((xyzi.shape[0], 2))])
    pcd.colors = o3d.utility.Vector3dVector(rgb)
    return pcd


def filter_pts_by_intensity(xyzi):
    return xyzi


if __name__ == "__main__":
    xyzi = read_kitti_bin_xyzi('D://software/tritium_code/dataset/sequences/00/velodyne/000000.bin')
    xyzi = filter_pts_by_intensity(xyzi)

    pcd0 = xyzi_to_o3d_pcd(xyzi)

    create_windows_to_show_pcd([pcd0])
