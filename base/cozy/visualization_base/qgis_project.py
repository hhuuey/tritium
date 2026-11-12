import os

from qgis_base import *


def project_qgis():
    base_path = '/Users/huey/Dataset/topo-dataset/1107_preprocess/'
    line_width = 1.0
    line_arrow_size = 2.0
    line_arrow_interval = 3.5
    add_arrow = True
    batch_dirs = os.listdir(base_path)
    print(f"Found batches: {batch_dirs}")
    batch_clip_max_num = 1
    def construct_node(dir_path, group_infos):
        lidar_map_node = NodeInfo(file_name=os.path.join(dir_path, 'lidar_intensity.tif'),
                                layer_name='lidar_intensity',
                                node_visibility=True,
                                node_collapse=False,
                                group_infos=group_infos)

        lane_node = NodeInfo(file_name=os.path.join(dir_path, 'lanes.geojson'),
                            layer_name='lanes',
                            color=QColor(0, 255, 0),
                            width=line_width,
                            add_arrow=add_arrow,
                            arrow_color=QColor(0, 255, 0),
                            arrow_size=line_arrow_size,
                            arrow_interval=line_arrow_interval,
                            node_visibility=True,
                            node_collapse=False,
                            group_infos=group_infos)
        road_boundaries_node = NodeInfo(file_name=os.path.join(dir_path, 'road_boundaries.geojson'),
                                        layer_name='road_boundaries',
                                        color=QColor(255, 0, 0),
                                        width=line_width,
                                        add_arrow=add_arrow,
                                        arrow_color=QColor(255, 0, 0),
                                        arrow_size=line_arrow_size,
                                        arrow_interval=line_arrow_interval,
                                        node_visibility=True,
                                        node_collapse=False,
                                        group_infos=group_infos)

        lane_boundaries_node = NodeInfo(file_name=os.path.join(dir_path, 'lane_boundaries.geojson'),
                                        layer_name='lane_boundaries',
                                        color=QColor(0, 0, 255),
                                        width=line_width,
                                        add_arrow=add_arrow,
                                        arrow_color=QColor(0, 0, 255),
                                        arrow_size=line_arrow_size,
                                        arrow_interval=line_arrow_interval,
                                        node_visibility=True,
                                        node_collapse=False,
                                        group_infos=group_infos)

        trajectory_node = NodeInfo(file_name=os.path.join(dir_path, 'trajectory.geojson'),
                                layer_name='trajectory',
                                color=QColor(0, 0, 0),
                                width=line_width,
                                add_arrow=add_arrow,
                                arrow_color=QColor(0, 0, 0),
                                arrow_size=line_arrow_size,
                                arrow_interval=line_arrow_interval,
                                node_visibility=True,
                                node_collapse=False,
                                group_infos=group_infos)

        junction_node = NodeInfo(file_name=os.path.join(dir_path, 'junctions.geojson'),
                                layer_name='junctions',
                                color=QColor(255, 165, 0),
                                width=line_width,
                                add_arrow=False,
                                node_visibility=True,
                                node_collapse=False,
                                group_infos=group_infos)
        return lidar_map_node, lane_node, road_boundaries_node, lane_boundaries_node, trajectory_node, junction_node

    for batch in batch_dirs:
        batch_path = os.path.join(base_path, batch)
        print(f"Processing batch: {batch}")
        if not os.path.isdir(batch_path):
            continue
        for clip_index, clip_name in enumerate(os.listdir(batch_path)):
            print(f"Processing clip: {clip_name}")
            clip_path = os.path.join(batch_path, clip_name)
            if not os.path.isdir(clip_path):
                continue
            group_infos = [
                GroupInfo(name=batch, collapse=True, visibility=True),
                GroupInfo(name=clip_name, collapse=True, visibility=False)
            ]
            lidar_map_node, lane_node, road_boundaries_node, lane_boundaries_node, trajectory_node, junction_node = construct_node(
                clip_path, group_infos)

            load_node_info(lidar_map_node)
            load_node_info(junction_node)
            load_node_info(road_boundaries_node)
            load_node_info(lane_boundaries_node)
            load_node_info(lane_node)
            load_node_info(trajectory_node)

            if clip_index > batch_clip_max_num:
                break
