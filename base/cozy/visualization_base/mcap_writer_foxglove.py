# -*- coding: utf-8 -*-
from mcap_protobuf.writer import Writer as McapProtobufWriter

from foxglove_schemas_protobuf.Log_pb2 import Log
from foxglove_schemas_protobuf.Point3_pb2 import Point3
from foxglove_schemas_protobuf.Vector3_pb2 import Vector3
from foxglove_schemas_protobuf.Quaternion_pb2 import Quaternion
from foxglove_schemas_protobuf.Pose_pb2 import Pose
from foxglove_schemas_protobuf.LinePrimitive_pb2 import LinePrimitive
from foxglove_schemas_protobuf.CubePrimitive_pb2 import CubePrimitive
from foxglove_schemas_protobuf.TextPrimitive_pb2 import TextPrimitive
from foxglove_schemas_protobuf.ModelPrimitive_pb2 import ModelPrimitive
from foxglove_schemas_protobuf.Color_pb2 import Color
from foxglove_schemas_protobuf.KeyValuePair_pb2 import KeyValuePair
from foxglove_schemas_protobuf.SceneEntity_pb2 import SceneEntity
from foxglove_schemas_protobuf.SceneEntityDeletion_pb2 import SceneEntityDeletion
from foxglove_schemas_protobuf.SceneUpdate_pb2 import SceneUpdate
from foxglove_schemas_protobuf.PackedElementField_pb2 import PackedElementField
from foxglove_schemas_protobuf.PointCloud_pb2 import PointCloud
from foxglove_schemas_protobuf.FrameTransform_pb2 import FrameTransform
from foxglove_schemas_protobuf.CameraCalibration_pb2 import CameraCalibration
from foxglove_schemas_protobuf.CompressedImage_pb2 import CompressedImage

from google.protobuf.timestamp_pb2 import Timestamp
from google.protobuf.duration_pb2 import Duration

import os
from pathlib import Path
import open3d as o3d
import numpy as np

from cozy import lb, nb

local_glb_path = "/Users/huey/Dataset/lexus.glb"
ego_car_frame_id = "ego"
map_frame_id = "base_link"
TOPIC_MARKERS_CAR = "/markers/car"
TOPIC_SEMANTIC_MAP = "/semantic_map"
TOPIC_MARKERS_ANNOTATIONS = "/markers/annotations"
TOPIC_LIDAR_POINTS = "/lidar0"


def make_protobuf_timestamp(timestamp_ns: int) -> Timestamp:
    seconds = timestamp_ns // 1_000_000_000
    nanos = timestamp_ns % 1_000_000_000
    return Timestamp(seconds=seconds, nanos=nanos)


def fpath_to_timestamp_ns(fpath: str) -> int:
    return int(Path(fpath).stem.split(".")[0])


def protobuf_timestamp_to_timestamp_ns(timestamp: Timestamp) -> int:
    return timestamp.seconds * 1_000_000_000 + timestamp.nanos


def write_car_models(ts_ns: int, frame_id: str, writer: McapProtobufWriter, topic_name: str = TOPIC_MARKERS_CAR):
    model_url = Path(local_glb_path).absolute().as_uri()
    car_model = ModelPrimitive(pose=Pose(position=Vector3(x=0, y=0, z=0), orientation=Quaternion(x=0, y=0, z=1, w=0)),
                               scale=Vector3(x=1, y=1, z=1),
                               url=model_url)
    # build SceneEntity safely via CopyFrom / add() to avoid passing Python lists directly to repeated composite fields
    car_entity = SceneEntity(timestamp=make_protobuf_timestamp(ts_ns),
                             frame_id=frame_id,
                             id="car",
                             lifetime=Duration(seconds=0, nanos=0),
                             frame_locked=True,
                             models=[car_model])
    car_sceneupdate_msg = SceneUpdate(entities=[car_entity], deletions=[])
    writer.write_message(topic=topic_name, message=car_sceneupdate_msg, log_time=ts_ns, publish_time=ts_ns)


# 单次播发语义地图
def write_semantic_map_once(map, writer: McapProtobufWriter, ts_ns: int, topic_name: str = TOPIC_SEMANTIC_MAP):
    lane_segments = map.vector_lane_segments.items()

    map_entities: list[SceneEntity] = [None] * len(lane_segments)

    for id, (_, segment) in enumerate(lane_segments):
        pts = [Point3(x=x, y=y, z=z) for x, y, z in segment.polygon_boundary]
        line = LinePrimitive(
            type=0,  # O代表polygon
            pose=Pose(position=Vector3(x=0, y=0, z=0), orientation=Quaternion(x=0, y=0, z=0, w=1)),
            thickness=0.1,
            scale_invariant=False,
            points=pts,
            color=Color(r=0.2, g=0.627, b=0.173, a=1))
        map_entities[id] = SceneEntity(
            timestamp=make_protobuf_timestamp(ts_ns),
            frame_id=map_frame_id,
            id=str(id),
            lifetime=Duration(seconds=0, nanos=0),
            frame_locked=True,
            lines=[line],
        )

    map_sceneupdate_msg = SceneUpdate(entities=map_entities, deletions=[])
    writer.write_message(topic=topic_name, message=map_sceneupdate_msg, log_time=ts_ns, publish_time=ts_ns)


# color: (r, g, b, a)
def write_annotation_during(annotation,
                            start_ts_ns: int,
                            end_ts_ns: int,
                            writer: McapProtobufWriter,
                            topic_name: str = TOPIC_MARKERS_ANNOTATIONS,
                            color: tuple = (0.0, 0.0, 0.0, 0.6)):
    write_annotation_once(annotation, ts_ns=start_ts_ns, writer=writer, topic_name=topic_name, color=color)
    write_annotation_deletion_once(annotation, end_ts_ns=end_ts_ns, writer=writer, topic_name=topic_name)
    pass


def write_annotation_deletion_once(annotation,
                                   end_ts_ns: int,
                                   writer: McapProtobufWriter,
                                   topic_name: str = TOPIC_MARKERS_ANNOTATIONS):
    # 框删除
    all_entity_deletions = []
    sed = SceneEntityDeletion(id=annotation.id, timestamp=make_protobuf_timestamp(end_ts_ns), type=0)
    all_entity_deletions.append(sed)
    scenedelete_msg = SceneUpdate(entities=[], deletions=all_entity_deletions)
    writer.write_message(topic=topic_name, message=scenedelete_msg, log_time=end_ts_ns, publish_time=end_ts_ns)
    # 标签删除
    all_entity_label_deletions = []
    sed_label = SceneEntityDeletion(id=f"{annotation.id}_label", timestamp=make_protobuf_timestamp(end_ts_ns), type=0)
    all_entity_label_deletions.append(sed_label)
    scenedelete_label_msg = SceneUpdate(entities=[], deletions=all_entity_label_deletions)
    writer.write_message(topic=f"{topic_name}_labels",
                         message=scenedelete_label_msg,
                         log_time=end_ts_ns,
                         publish_time=end_ts_ns)


# color: (r, g, b, a)
def write_annotation_once(annotation,
                          ts_ns: int,
                          writer: McapProtobufWriter,
                          topic_name: str = TOPIC_MARKERS_ANNOTATIONS,
                          color=(0.0, 0.0, 0.0, 0.6)):
    entity: nb.BBox3D = annotation.bbox
    timestamp_ns = ts_ns
    track_uuid = annotation.id
    l = entity.l
    w = entity.w
    h = entity.h
    q = lb.rpy_to_q([entity.roll, entity.pitch, entity.yaw])
    qx = q[0]
    qy = q[1]
    qz = q[2]
    qw = q[3]
    x = entity.x
    y = entity.y
    z = entity.z
    cube = CubePrimitive(
        pose=Pose(position=Vector3(x=x, y=y, z=z), orientation=Quaternion(x=qx, y=qy, z=qz, w=qw)),
        size=Vector3(x=l, y=w, z=h),
        color=Color(r=color[0], g=color[1], b=color[2], a=color[3]),
    )
    se = SceneEntity(
        timestamp=make_protobuf_timestamp(timestamp_ns),
        frame_id=map_frame_id,
        id=track_uuid,
        lifetime=Duration(seconds=0, nanos=0),
        frame_locked=True,
        cubes=[cube],
        metadata=[KeyValuePair(key="category", value=annotation.category.type.value)],
    )
    sceneupdate_msg = SceneUpdate(entities=[se], deletions=[])
    writer.write_message(topic=topic_name, message=sceneupdate_msg, log_time=ts_ns, publish_time=ts_ns)

    txt = TextPrimitive(
        pose=Pose(position=Vector3(x=x, y=y, z=z + h), orientation=Quaternion(x=qx, y=qy, z=qz, w=qw)),
        billboard=True,
        text=f"{annotation.id}",
        font_size=0.5,
        color=Color(r=1.00, g=1.00, b=1.00, a=1.6),
    )
    se_label = SceneEntity(
        timestamp=make_protobuf_timestamp(timestamp_ns),
        frame_id=map_frame_id,
        id=f"{track_uuid}_label",
        lifetime=Duration(seconds=0, nanos=0),
        frame_locked=True,
        texts=[txt],
    )
    sceneupdate_label_msg = SceneUpdate(entities=[se_label], deletions=[])
    writer.write_message(topic=f"{topic_name}_labels",
                         message=sceneupdate_label_msg,
                         log_time=ts_ns,
                         publish_time=ts_ns)


# 单次播发人工标注框
# color: (r, g, b, a)
def write_annotations_once(annotations,
                           ts_ns: int,
                           writer: McapProtobufWriter,
                           topic_name: str = TOPIC_MARKERS_ANNOTATIONS,
                           color=(0.0, 0.0, 0.0, 0.6)):
    for annotation in annotations:
        write_annotation_once(annotation, ts_ns, writer, topic_name, color=color)


def write_lidar_data_once(pcd_file,
                          ts_ns: int,
                          writer: McapProtobufWriter,
                          topic_name: str = TOPIC_LIDAR_POINTS,
                          frame_id: str = map_frame_id):
    if not os.path.exists(pcd_file):
        raise FileNotFoundError(f"PCD not found: {pcd_file}")

    # 读取 pcd
    pcd = o3d.t.io.read_point_cloud(pcd_file)
    pts = None
    intensity = None
    if "positions" in pcd.point:
        pts = pcd.point["positions"].numpy().astype(np.float32)
    if "intensity" in pcd.point:
        intensity = pcd.point["intensity"].numpy()
    # 构建字段列表与数据列（必有 x,y,z）
    if pts is None or pts.size == 0:
        return
    fields = [
        PackedElementField(name="x", offset=0, type=7),
        PackedElementField(name="y", offset=4, type=7),
        PackedElementField(name="z", offset=8, type=7),
    ]
    data_cols = [
        pts[:, 0].astype(np.float32),
        pts[:, 1].astype(np.float32),
        pts[:, 2].astype(np.float32),
    ]

    if intensity is not None:
        # 拉平并与 pts 长度对齐
        inten = np.asarray(intensity)
        if inten.ndim > 1 and inten.shape[1] == 1:
            inten = inten.reshape(-1)
        if inten.shape[0] != pts.shape[0]:
            # 长度不匹配则忽略 intensity
            inten = None
        else:
            # 判断类型并添加
            if np.issubdtype(inten.dtype, np.floating):
                fields.append(PackedElementField(name="intensity", offset=12, type=7))
                data_cols.append(inten.astype(np.float32))
            else:
                # 整数类型统一转为 uint32 存储
                fields.append(PackedElementField(name="intensity", offset=12, type=5))
                data_cols.append(inten.astype(np.uint32).astype(np.float32))  # 转为 bytes 时以 4 字节为单位
    # 合并成 (N, M) 并写入 foxglove PointCloud
    pdata = np.stack(data_cols, axis=1)
    point_stride = pdata.shape[1] * 4  # 每个字段 4 字节
    timestamp = make_protobuf_timestamp(ts_ns)
    pointcloud_msg = PointCloud(timestamp=timestamp,
                                frame_id=frame_id,
                                pose=Pose(position=Vector3(x=0, y=0, z=0), orientation=Quaternion(x=0, y=0, z=0, w=0)),
                                point_stride=point_stride,
                                fields=fields,
                                data=pdata.tobytes())
    writer.write_message(topic=topic_name, message=pointcloud_msg, log_time=ts_ns, publish_time=ts_ns)


def write_frame_transform_once(ts_ns: int,
                               parent_frame: str,
                               child_frame: str,
                               translation: lb.Vector,
                               quaternion: lb.Quaternion,
                               writer: McapProtobufWriter,
                               topic_name: str = "tf"):
    ft = FrameTransform(timestamp=make_protobuf_timestamp(ts_ns),
                        parent_frame_id=parent_frame,
                        child_frame_id=child_frame,
                        translation=Vector3(x=translation.x, y=translation.y, z=translation.z),
                        rotation=Quaternion(x=quaternion.x, y=quaternion.y, z=quaternion.z, w=quaternion.w))
    writer.write_message(topic=topic_name, message=ft, log_time=ts_ns, publish_time=ts_ns)


def write_img_info_once(writer: McapProtobufWriter, first_timestamp_ns, calibration_info, frame_id: str):
    # 相机内参
    fx_px = calibration_info["intrinsic"][0][0]
    fy_px = calibration_info["intrinsic"][1][1]
    cx_px = calibration_info["intrinsic"][0][2]
    cy_px = calibration_info["intrinsic"][1][2]
    # used as timestamp for camera calibration
    timestamp = make_protobuf_timestamp(first_timestamp_ns)
    calib_msg = CameraCalibration(
        timestamp=timestamp,
        frame_id=frame_id,
        width=calibration_info["width"],
        height=calibration_info["height"],
        distortion_model="plumb_bob",
        D=[0.0, 0.0, 0.0, 0.0, 0.0],  # k1, k2, p1, p2, k3
        K=[fx_px, 0, cx_px, 0, fy_px, cy_px, 0, 0, 1],
        R=[1, 0, 0, 0, 1, 0, 0, 0, 1],
        P=[fx_px, 0, cx_px, 0, 0, fy_px, cy_px, 0, 0, 0, 1, 0],
    )
    writer.write_message(topic=f"/camera/{frame_id}/camera_info",
                         message=calib_msg,
                         log_time=first_timestamp_ns,
                         publish_time=first_timestamp_ns)

    # 相机外参
    cam2ego = np.array(calibration_info['cam2ego'])
    x = cam2ego[0][3]
    y = cam2ego[1][3]
    z = cam2ego[2][3]
    qx, qy, qz, qw = lb.r_to_q(cam2ego[0:3, 0:3])
    tf_msg = FrameTransform(timestamp=timestamp,
                            parent_frame_id=ego_car_frame_id,
                            child_frame_id=frame_id,
                            translation=Vector3(x=x, y=y, z=z),
                            rotation=Quaternion(x=qx, y=qy, z=qz, w=qw))
    writer.write_message(topic="tf", message=tf_msg, log_time=first_timestamp_ns, publish_time=first_timestamp_ns)
    return


def write_img_once(img_path, timestamp_ns, writer: McapProtobufWriter, frame_id: str, format="jpeg"):
    timestamp = make_protobuf_timestamp(timestamp_ns)
    img = open(img_path, "rb")
    data = bytes(img.read())
    img_msg = CompressedImage(timestamp=timestamp, frame_id=frame_id, data=data, format=format)
    writer.write_message(topic=f"/camera/{frame_id}/compressed_image",
                         message=img_msg,
                         log_time=timestamp_ns,
                         publish_time=timestamp_ns)


def write_log_once(writer: McapProtobufWriter,
                   start_time_ns: int,
                   topic_name: str = "/log",
                   log_name: str = "/log",
                   log_message: str = "log start"):
    log_msg = Log(timestamp=make_protobuf_timestamp(start_time_ns), level=2, name=log_name, message=log_message)
    writer.write_message(topic=topic_name, message=log_msg, log_time=start_time_ns, publish_time=start_time_ns)


def get_foxglove_writer(file_path: str) -> McapProtobufWriter:
    stream = open(file_path, "wb")
    writer = McapProtobufWriter(stream)
    return writer


if __name__ == "__main__":
    print("hello, world!")
