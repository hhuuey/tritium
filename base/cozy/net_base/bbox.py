# -*- coding: utf-8 -*-
from cozy import lb

import numpy as np
from dataclasses import dataclass


@dataclass
class BBox2D:
    x: float = 0.0  # 左上角x
    y: float = 0.0  # 左上角y
    w: float = 0.0  # 宽度
    h: float = 0.0  # 高度

    def to_xyxy(self):
        return [self.x, self.y, self.x + self.w, self.y + self.h]

    def to_xywh(self):
        return [self.x, self.y, self.w, self.h]

    def to_cxcywh(self):
        return [self.x + self.w / 2, self.y + self.h / 2, self.w, self.h]

    def to_xysr(self):  # [x,y,s,r] where x,y is the centre of the box and s is the scale/area and r is the aspect ratio
        w = self.w
        h = self.h
        x = self.x + w / 2.
        y = self.y + h / 2.
        s = w * h  # scale is just area
        r = w / float(h)
        return [x, y, s, r]

    @staticmethod
    def from_xyxy(xyxy):
        return BBox2D(x=xyxy[0], y=xyxy[1], w=xyxy[2] - xyxy[0], h=xyxy[3] - xyxy[1])

    @staticmethod
    def from_xyxy_dim2(xyxy):
        return BBox2D.from_xyxy([xyxy[0][0], xyxy[0][1], xyxy[1][0], xyxy[1][1]])

    @staticmethod
    def inter_area(bbox1, bbox2) -> float:
        f_bbox1 = bbox1.to_xyxy()
        f_bbox2 = bbox2.to_xyxy()
        x1 = max(f_bbox1[0], f_bbox2[0])
        y1 = max(f_bbox1[1], f_bbox2[1])
        x2 = min(f_bbox1[2], f_bbox2[2])
        y2 = min(f_bbox1[3], f_bbox2[3])
        inter_area = max(0, x2 - x1) * max(0, y2 - y1)
        return inter_area

    @staticmethod
    def union_area(bbox1, bbox2) -> float:
        inter_area = BBox2D.inter_area(bbox1, bbox2)
        f_bbox1 = bbox1.to_xyxy()
        f_bbox2 = bbox2.to_xyxy()
        bbox1_area = (f_bbox1[2] - f_bbox1[0]) * (f_bbox1[3] - f_bbox1[1])
        bbox2_area = (f_bbox2[2] - f_bbox2[0]) * (f_bbox2[3] - f_bbox2[1])
        union_area = bbox1_area + bbox2_area - inter_area
        return union_area

    @staticmethod
    def iou(bbox1, bbox2) -> float:
        inter_area = BBox2D.inter_area(bbox1, bbox2)
        union_area = BBox2D.union_area(bbox1, bbox2)
        if union_area == 0:
            return 0.0
        iou = inter_area / union_area
        return iou

    @staticmethod
    def c_xyxy_to_xysr(bbox_list: list) -> list:
        w = bbox_list[2] - bbox_list[0]
        h = bbox_list[3] - bbox_list[1]
        x = bbox_list[0] + w / 2.
        y = bbox_list[1] + h / 2.
        s = w * h  #scale is just area
        r = w / float(h)
        return [x, y, s, r]

    @staticmethod
    def c_xysr_to_xyxy(xysr_list: list) -> list:
        w = np.sqrt(xysr_list[2] * xysr_list[3])
        h = xysr_list[2] / w
        return [xysr_list[0] - w / 2., xysr_list[1] - h / 2., xysr_list[0] + w / 2., xysr_list[1] + h / 2.]


@dataclass
class BBox3D:
    l: float = 0.0
    w: float = 0.0
    h: float = 0.0
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0
    roll: float = 0.0
    pitch: float = 0.0
    yaw: float = 0.0

    #bbox_shape: # [x, y, z, l, w, h, roll, pitch, yaw]
    @staticmethod
    def from_list(bbox_shape: list):
        return BBox3D(x=bbox_shape[0],
                      y=bbox_shape[1],
                      z=bbox_shape[2],
                      l=bbox_shape[3],
                      w=bbox_shape[4],
                      h=bbox_shape[5],
                      roll=bbox_shape[6],
                      pitch=bbox_shape[7],
                      yaw=bbox_shape[8])

    def resize(self, scale: float):
        self.l *= scale
        self.w *= scale
        self.h *= scale

    def to_img_pixels(self, cam2img_matrix: np.ndarray):
        uvs = []
        vertices = self.to_vertex()
        for v in vertices:
            v_hom = np.array([v[0], v[1], v[2], 1.0])
            uv_hom = cam2img_matrix @ v_hom
            if uv_hom[2] <= 0:
                return []
            uv = uv_hom[:2] / uv_hom[2]
            uvs.append((int(uv[0]), int(uv[1])))
        return uvs

    def to_img_pixels_2d(self, cam2img_matrix: np.ndarray):
        uvs = self.to_img_pixels(cam2img_matrix)
        if not uvs:
            return []
        uvs = np.array(uvs)
        x_min, y_min = np.min(uvs, axis=0)
        x_max, y_max = np.max(uvs, axis=0)
        uvs = [(int(x_min), int(y_min)), (int(x_max), int(y_max))]
        return uvs

    def to_edges(self):
        lines = [
            (0, 1),
            (1, 2),
            (2, 3),
            (3, 0),  # 上面四条边
            (4, 5),
            (5, 6),
            (6, 7),
            (7, 4),  # 下面四条边
            (0, 4),
            (1, 5),
            (2, 6),
            (3, 7)  # 四条竖直边
        ]
        vertices = self.to_vertex()
        line_points = []
        for start, end in lines:
            line_points.append((vertices[start], vertices[end]))
        return line_points

    def to_vertex(self):
        R = lb.rpy_to_r([self.roll, self.pitch, self.yaw])
        x_corners = [self.l / 2, self.l / 2, -self.l / 2, -self.l / 2, self.l / 2, self.l / 2, -self.l / 2, -self.l / 2]
        y_corners = [self.w / 2, -self.w / 2, -self.w / 2, self.w / 2, self.w / 2, -self.w / 2, -self.w / 2, self.w / 2]
        z_corners = [self.h / 2, self.h / 2, self.h / 2, self.h / 2, -self.h / 2, -self.h / 2, -self.h / 2, -self.h / 2]
        corners_3d = np.dot(R, np.vstack([x_corners, y_corners, z_corners]))
        corners_3d[0, :] += self.x
        corners_3d[1, :] += self.y
        corners_3d[2, :] += self.z
        return corners_3d.T.tolist()


# bbox: [x1,y1,x2,y2]
# 只有与图像有相交的，就认为是有效的
def bbox_inside_img(bbox: BBox2D, img_width, img_height):
    if BBox2D.iou(bbox, BBox2D.from_xyxy([0, 0, img_width, img_height])) > 0:
        return True
    return False
