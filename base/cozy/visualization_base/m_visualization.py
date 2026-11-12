import numpy as np
import folium
import open3d as o3d
from functools import partial
import os
from common.m_lidar_io import read_kitti_bin_xyz, xyz_to_o3d_pcd
from common.m_calib_comon import to_transform_matrix, euler_to_rotation_matrix

def create_windows_to_show_pcd(pcds):
    vis = o3d.visualization.Visualizer()
    vis.create_window()
    for pcd in pcds:
        vis.add_geometry(pcd)
    mesh_frame = o3d.geometry.TriangleMesh.create_coordinate_frame(size=5)
    vis.add_geometry(mesh_frame)
    vis.run()
    vis.destroy_window()

def create_windows_to_manual_calib(pcds):
    def change_pcd_index(vis, pcd_index):
        global pcd_base
        pcd_base = pcd_index

    def movement(vis, index, add, increments):
        global pcd_base
        change_var = [0., 0., 0., 0., 0., 0.]  # x,y,z,r,p,y
        mapping = {True: 1, False: -1}
        change_var[index] = mapping[add] * increments

        t = change_var[0:3]
        r = change_var[3:6]

        # vis.remove_geometry(pcd1)
        p = np.array(pcd_base.points)
        p = np.column_stack((p, np.ones(p.shape[0])))
        transform_matrix = to_transform_matrix(euler_to_rotation_matrix(r), t)
        p = np.dot(p, transform_matrix.T)
        pcd_base.points = o3d.utility.Vector3dVector(p[:, 0:3])

        vis.update_geometry(pcd_base)
        vis.poll_events()

    vis = o3d.visualization.VisualizerWithKeyCallback()
    vis.create_window()
    key_mapping = {
        'W': (0, True, translate_increment),
        'S': (0, False, translate_increment),
        'E': (1, True, translate_increment),
        'D': (1, False, translate_increment),
        'R': (2, True, translate_increment),
        'F': (2, False, translate_increment),
        'T': (3, True, rotate_increment),
        'G': (3, False, rotate_increment),
        'Y': (4, True, rotate_increment),
        'H': (4, False, rotate_increment),
        'U': (5, True, rotate_increment),
        'J': (5, False, rotate_increment),
    }

    for i, pcd in enumerate(pcds):
        key_mapping[str(i)] = (pcd,)

    for key, value in key_mapping.items():
        if len(value) == 3:
            movement_part = partial(movement, index=value[0], add=value[1], increments=value[2])
            vis.register_key_callback(ord(key), movement_part)
        elif len(value) == 1:
            change_pcd_index_part = partial(change_pcd_index, pcd_index=value[0])
            vis.register_key_callback(ord(key), change_pcd_index_part)
        else:
            raise ValueError("Invalid key mapping")
    for pcd in pcds:
        vis.add_geometry(pcd)
    mesh_frame = o3d.geometry.TriangleMesh.create_coordinate_frame(size=5)
    vis.add_geometry(mesh_frame)
    vis.run()
    vis.destroy_window()


def create_windows_to_pick_pts(pcds):
    def print_pick_pts_coordinate(vis, pts):
        global last_pick_pts_num
        # 每帧都更新点云的颜色
        all_pick_pts_num = len(vis.get_picked_points())
        if all_pick_pts_num == last_pick_pts_num:
            return
        last_pick_pts_num = all_pick_pts_num
        if len(vis.get_picked_points()) != 0:
            print(pts[vis.get_picked_points()[-1]])
        else:
            print("empty")
    # 创建 VisualizerWithEditing 对象
    vis = o3d.visualization.VisualizerWithEditing()
    # 添加点云
    vis.create_window(window_name="Open3D", visible=True)
    merge_pcd = o3d.open3d.geometry.PointCloud()
    for i in pcds:
        merge_pcd += i
    vis.add_geometry(merge_pcd)
    print_pick_pts_coordinate_part = partial(print_pick_pts_coordinate, pts=np.array(merge_pcd.points))
    vis.register_animation_callback(print_pick_pts_coordinate_part)
    vis.run()
    vis.destroy_window()

    pick_idx = vis.get_picked_points()
    pick_pts = np.array(merge_pcd.points)[pick_idx]
    print("summary:\n{}".format(pick_pts))
    return pick_pts

map_color_options = {
    "red",
    "darkred",
    "lightred",
    "orange",
    "beige",
    "green",
    "darkgreen",
    "lightgreen",
    "darkblue",
    "cadetblue",
    "lightblue",
    "purple",
    "darkpurple",
    "pink",
    "white",
    "gray",
    "lightgray",
    "black",
}

class FoliumMapVisualization:
    def __init__(self):
        # 创建地图
        self.map = None
        self.map_html_path = None
        self.label_map = {-1: "blue"}
        self.label_number = 0
        self.color_list = list(map_color_options)

    def add_marker(self, lat, lon, description, label=-1):
        if self.map == None:
            self.map = folium.Map(
                # 经纬度 [纬度,经度]
                location=[lat, lon],
                # tiles ='http://webrd0{s}.is.autonavi.com/appmaptile?lang=zh_cn&size=1&scale=1&style=8&x={x}&y={y}&z={z}',
                tiles='https://webrd02.is.autonavi.com/appmaptile?lang=zh_en&size=1&scale=1&style=8&x={x}&y={y}&z={z}',
                attr='高德-中英文对照',
                # 初始地图比例尺大小
                zoom_start=12,
            )
            self.map.add_child(folium.LatLngPopup())

        if label not in self.label_map.keys():
            self.label_map[label] = self.color_list[self.label_number]
            self.label_number = (self.label_number + 1) % len(self.color_list)

        folium.Marker(
            location=[float(lat), float(lon)],
            popup=description,
            icon=folium.Icon(icon='cloud', color=self.label_map[label])
        ).add_to(self.map)
        return self

    def save_html(self, map_html_path):
        # 启用纬度/经度弹出窗口，点击地图可查询经纬度
        # 保存地图为HTML
        self.map.save(map_html_path)
        self.__replace_map_css_resource__(map_html_path)

    def __replace_map_css_resource__(self, map_html_path):
        # 打开文件，读取内容
        with open(map_html_path, 'r', encoding='utf-8') as file:
            content = file.read()
        # 替换文本
        modified_content = content.replace("https://netdna.bootstrapcdn.com/bootstrap/3.0.0/css/bootstrap.min.css",
                                           os.path.join(os.path.dirname(os.path.realpath(__file__)), 'lib', 'bootstrap.min.css'))
        # 写回文件
        with open(map_html_path, 'w', encoding='utf-8') as file:
            file.write(modified_content)


if __name__ == "__main__":
    np.set_printoptions(precision=3, suppress=True)
    translate_increment = 0.02
    rotate_increment = 0.1
    last_pick_pts_num = 0
    pcd0 = xyz_to_o3d_pcd(read_kitti_bin_xyz('D://software/tritium_code/dataset/sequences/00/velodyne/000000.bin'), [0., 0., 0.])
    pcd1 = xyz_to_o3d_pcd(read_kitti_bin_xyz('D://software/tritium_code/dataset/sequences/00/velodyne/000001.bin'), [1., 0., 0.])
    pcd2 = xyz_to_o3d_pcd(read_kitti_bin_xyz('D://software/tritium_code/dataset/sequences/00/velodyne/000002.bin'), [0., 1., 0.])
    pcd3 = xyz_to_o3d_pcd(read_kitti_bin_xyz('D://software/tritium_code/dataset/sequences/00/velodyne/000003.bin'), [0., 0., 1.])
    pcd_base = pcd0

    _ = create_windows_to_pick_pts([pcd0, pcd1, pcd2, pcd3])

    create_windows_to_manual_calib([pcd0, pcd1, pcd2, pcd3])

    folium_map = FoliumMapVisualization()

    folium_map.add_marker(22.5876728, 113.4134872, 'test1')
    folium_map.add_marker(22.6876728, 113.4134872, 'test2')
    folium_map.save_html('./tmp.html')
    exit(0)