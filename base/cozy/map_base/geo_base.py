import math
from .map_geom import MapPoint, MapGeometry, MapGeometryType
from .data_infra import g_geo_eps_, EARTH_RADIUS
from typing import Optional, List
import copy
from enum import Enum
import numpy as np


class PointInPolygonStatus(Enum):
    INSIDE = 1  # 点在内部
    ON_BOUNDARY = 2  # 点在边界上
    OUTSIDE = 3  # 点在外部


def unwrap_angle(current_wrapped, previous_unwrapped=0.0):
    delta = current_wrapped - previous_unwrapped
    # 调整差值到 [-pi, pi) 区间
    delta_adjusted = delta - 2 * math.pi * round(delta / (2 * math.pi))
    return previous_unwrapped + delta_adjusted


# wgs84坐标系计算两点间的距离
def pt2pt_wgs84(p1: MapPoint, p2: MapPoint) -> float:
    # 将经纬度从度转换为弧度
    lat1, lon1 = math.radians(p1.y), math.radians(p1.x)
    lat2, lon2 = math.radians(p2.y), math.radians(p2.x)
    # 计算经纬度差值
    dlat = lat2 - lat1
    dlon = lon2 - lon1
    # Haversine公式
    a = math.sin(dlat / 2)**2 + math.cos(lat1) * math.cos(lat2) * math.sin(dlon / 2)**2
    c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))
    # 计算距离
    distance = EARTH_RADIUS * c
    return distance


# 笛卡尔坐标计算两点间的距离
def pt2pt(p1: MapPoint, p2: MapPoint, use_z: bool = False) -> float:
    return (p1 - p2).magnitude(use_z)


# 计算点到线段的投影长度
def proj_pt2line(point: MapPoint, line_start_pt: MapPoint, line_end_pt: MapPoint) -> float:
    line_vec = line_end_pt - line_start_pt
    if line_vec.magnitude() < g_geo_eps_:
        return 0.0
    point_vec = point - line_start_pt
    return point_vec.dot(line_vec.normalize())


def proj_pt2line_ex(point: MapPoint, a: MapPoint, b: MapPoint, use_z: bool = False):
    vec = b - a
    seg_len = vec.magnitude(use_z=use_z)
    if seg_len < g_geo_eps_:
        return 0.0, 0.0
    ap = point - a
    proj_len = ap.dot(vec) / seg_len if not use_z else ((ap.x * vec.x + ap.y * vec.y + ap.z * vec.z) / seg_len)
    return proj_len, seg_len


def proj_pt2polyline(point_raw: MapPoint, polyline: MapGeometry, use_z: bool = False):
    """Return offset (arc-length) of point projected onto polyline."""
    if polyline.type != MapGeometryType.LINESTRING:
        raise ValueError("proj_pt2polyline only supports LINESTRING")
    point = copy.deepcopy(point_raw)
    pts = copy.deepcopy(polyline.points)
    if len(pts) < 2:
        return 0.0
    if not use_z:
        point.z = 0.0
        for pt in pts:
            pt.z = 0.0

    best_m = 0.0
    best_dist2 = float("inf")
    accumulated = 0.0

    for i in range(len(pts) - 1):
        a = pts[i]
        b = pts[i + 1]
        proj_len_raw, seg_len = proj_pt2line_ex(point, a, b, use_z=use_z)
        if seg_len < g_geo_eps_:
            continue
        proj_len_clamped = min(seg_len, max(0.0, proj_len_raw))
        t = proj_len_clamped / seg_len
        proj_pt = MapPoint(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t if use_z else a.z)

        dist2 = (point - proj_pt).magnitude(use_z=use_z)**2
        if dist2 < best_dist2 - g_geo_eps_:
            best_dist2 = dist2
            best_m = accumulated + proj_len_clamped
        accumulated += seg_len

    return best_m


# 计算点到线段的最短距离
def pt2line_min_dist(point, line_start_pt, line_end_pt):
    line_vec = line_end_pt - line_start_pt
    line_len = line_vec.magnitude()
    if line_len < g_geo_eps_:
        return pt2pt(point, line_start_pt)
    proj_len = proj_pt2line(point, line_start_pt, line_end_pt)
    if proj_len <= g_geo_eps_:
        return pt2pt(point, line_start_pt)
    if proj_len >= line_len - g_geo_eps_:
        return pt2pt(point, line_end_pt)
    return pt2pt(point, line_start_pt + line_vec.normalize() * proj_len)


# 计算点到线段的垂直距离
def perpendicular_distance(point: MapPoint, line_start_pt: MapPoint, line_end_pt: MapPoint) -> float:
    line_vec = line_end_pt - line_start_pt
    line_len = line_vec.magnitude()
    if line_len < g_geo_eps_:
        return pt2pt(point, line_start_pt)
    point_vec = point - line_start_pt
    # Calculate the magnitude of the cross product for the perpendicular distance
    return abs(point_vec.cross(line_vec.normalize()).magnitude())


def _douglas_peucker_recursive(point_list, perpendicular_threshold):
    if len(point_list) < 3:
        return point_list
    # 找到距离最大的点
    start, end = point_list[0], point_list[-1]
    # 查找距离最大的点及其索引
    dists = [perpendicular_distance(point_list[i], start, end) for i in range(1, len(point_list) - 1)]
    if dists:
        max_dist, index = max((d, idx) for idx, d in enumerate(dists, 1))
    else:
        max_dist, index = 0, 0
    # 判断是否超过阈值
    if max_dist >= perpendicular_threshold:  # Include equality to retain borderline points
        # 保留最大点，并递归处理两段
        left = _douglas_peucker_recursive(point_list[:index + 1], perpendicular_threshold)
        right = _douglas_peucker_recursive(point_list[index:], perpendicular_threshold)
        return left[:-1] + right
    else:
        # 只保留首尾点
        return [start, end]


def douglaspeuker(geom: MapGeometry, perpendicular_threshold: float) -> MapGeometry:
    if geom.type != MapGeometryType.LINESTRING:
        raise ValueError("Douglas-Peucker simplification only supports LINESTRING geometries.")

    # Step 1: Extract points from the geometry
    point_list = geom.points
    print(f"Original points: {point_list}")

    # Step 2: Perform Douglas-Peucker simplification
    qualify_list = _douglas_peucker_recursive(point_list, perpendicular_threshold)
    print(f"Points after Douglas-Peucker simplification: {qualify_list}")
    return MapGeometry(points=qualify_list, type=geom.type, crs=geom.crs)


def geom_buffer(geom: MapGeometry, buffer_dist: float) -> MapGeometry:
    if geom.type != MapGeometryType.POINT:
        raise NotImplementedError("Buffering currently only supports POINT geometries.")

    num_segments = 36  # Number of segments to approximate the circle
    angle_increment = 2 * math.pi / num_segments
    buffer_points = []

    for i in range(num_segments):
        angle = i * angle_increment
        dx = buffer_dist * math.cos(angle)
        dy = buffer_dist * math.sin(angle)
        buffer_point = MapPoint(geom.points[0].x + dx, geom.points[0].y + dy)
        buffer_points.append(buffer_point)
    return MapGeometry(points=buffer_points, type=MapGeometryType.POLYGON, crs=geom.crs)


def geom_length(geom: MapGeometry, use_z: bool = False) -> float:
    if geom.type == MapGeometryType.POINT:
        return 0.0

    if geom.type == MapGeometryType.LINESTRING or geom.type == MapGeometryType.POLYGON:
        total_length = 0.0
        for i in range(len(geom.points) - 1):
            total_length += pt2pt(geom.points[i], geom.points[i + 1], use_z=use_z)
        return total_length

    raise NotImplementedError("Length calculation only supports LINESTRING and POLYGON geometries.")


def geoms_distance(geom1: MapGeometry, geom2: MapGeometry) -> float:
    if geom1.type == MapGeometryType.POINT and geom2.type == MapGeometryType.POINT:
        return pt2pt(geom1.points[0], geom2.points[0])

    if geom1.type == MapGeometryType.POINT and geom2.type == MapGeometryType.LINESTRING:
        min_distance = float('inf')
        for i in range(len(geom2.points) - 1):
            dist = pt2line_min_dist(geom1.points[0], geom2.points[i], geom2.points[i + 1])
            min_distance = min(min_distance, dist)
        return min_distance

    if geom1.type == MapGeometryType.LINESTRING and geom2.type == MapGeometryType.POINT:
        # LineString to Point distance (reverse of above)
        return geoms_distance(geom2, geom1)

    if geom1.type == MapGeometryType.POINT and geom2.type == MapGeometryType.POLYGON:
        # 如果点在多边形内部，距离为0
        if point_in_polygon(geom1.points[0], geom2):
            return 0.0
        # 否则求点到多边形边界的最短距离
        min_distance = float('inf')
        for i in range(len(geom2.points) - 1):
            dist = pt2line_min_dist(geom1.points[0], geom2.points[i], geom2.points[i + 1])
            min_distance = min(min_distance, dist)
        return min_distance

    if geom1.type == MapGeometryType.POLYGON and geom2.type == MapGeometryType.POINT:
        # Polygon to Point distance (reverse of above)
        return geoms_distance(geom2, geom1)

    # For other geometry combinations, raise NotImplementedError
    raise NotImplementedError("Distance calculation for these geometry types is not implemented.")


# 每间隔d_distance距离插入一个点
def geom_densify(geom, d_distance, use_z: bool = False):
    """
    每间隔 d_distance 距离插入一个点。

    参数:
        geom: MapGeometry (只支持 LINESTRING)
        d_distance: 插值间距（米）
        use_z: 如果为 True，则在插值时对 z 也进行线性插值；否则新插入点的 z 使用起点的 z 值以保持连续性。
    """
    if geom.type != MapGeometryType.LINESTRING:
        raise NotImplementedError("Densification currently only supports LINESTRING geometries.")
    densified_points = []
    for pt_i in range(len(geom.points) - 1):
        start, end = geom.points[pt_i], geom.points[pt_i + 1]
        # 保留原始点（引用或副本均可，这里保留原始对象以维持属性）
        densified_points.append(start)

        # 根据 use_z 决定距离计算是否使用 3D
        seg_len = pt2pt(start, end, use_z=use_z)
        if seg_len < d_distance:
            continue

        # 插入点数量（避免插入与段端重合的点）
        n_insert = int(math.floor(seg_len / d_distance))
        for j in range(1, n_insert + 1):
            insert_dist = j * d_distance
            if seg_len - insert_dist <= g_geo_eps_:
                break
            t = insert_dist / seg_len
            ix = start.x + (end.x - start.x) * t
            iy = start.y + (end.y - start.y) * t
            if use_z:
                iz = start.z + (end.z - start.z) * t
            else:
                # 不插值时，保持与起点相同的 z（避免丢失高程信息）
                iz = start.z
            densified_points.append(MapPoint(ix, iy, iz))
    # 保证包含最后一个点（保留其原始 z）
    last_pt = geom.points[-1]
    densified_points.append(MapPoint(last_pt.x, last_pt.y, last_pt.z))
    return MapGeometry(points=densified_points, type=geom.type, crs=geom.crs)


# 顺时针旋转90度的法线向量
def geom_normal_line(seg_pt1: MapPoint, seg_pt2: MapPoint):
    vec = seg_pt2 - seg_pt1
    return MapPoint(-vec.y, vec.x).normalize()


# 针对前后时序上相同的点进行删除
def geom_deduplicated(geom: MapGeometry):
    if geom.type != MapGeometryType.LINESTRING:
        raise NotImplementedError("Deduplicated currently only supports LINESTRING geometries.")
    new_pts = []
    for pt in geom.points:
        if len(new_pts) == 0 or pt != new_pts[-1]:
            new_pts.append(pt)
    return MapGeometry(points=new_pts, type=geom.type, crs=geom.crs)


# 去除所有重复点（包括不连续的）
def geom_remove_duplicates(geom: MapGeometry, eps: float = g_geo_eps_) -> MapGeometry:
    if geom.type != MapGeometryType.LINESTRING:
        raise NotImplementedError("geom_remove_duplicates only supports LINESTRING geometries.")

    if len(geom.points) < 2:
        return MapGeometry(points=geom.points, type=geom.type, crs=geom.crs)

    new_pts = [geom.points[0]]
    for pt in geom.points[1:]:
        # 检查是否与已有点重复
        is_duplicate = any(pt2pt(pt, existing_pt) < eps for existing_pt in new_pts)
        if not is_duplicate:
            new_pts.append(pt)

    return MapGeometry(points=new_pts, type=geom.type, crs=geom.crs)


# 判断点集是否共线
def geom_collinear(map_pts: List[MapPoint]) -> bool:
    if len(map_pts) < 3:
        return True  # Less than 3 points are always collinear
    # Check if all points are collinear
    for i in range(2, len(map_pts)):
        val = (map_pts[1] - map_pts[0]).cross(map_pts[i] - map_pts[0]).z
        if abs(val) > g_geo_eps_:
            return False
    return True


# 判断点在线段的方向
def orientation(a: MapPoint, b: MapPoint, c: MapPoint) -> int:
    # val > 0: 逆时针(CCW), val < 0: 顺时针(CW), |val|<=eps: 共线
    val = (b - a).cross(c - a).z
    if abs(val) <= g_geo_eps_:
        return 0
    return 2 if val > 0 else 1  # 与原返回约定保持一致: 0共线,1顺时针,2逆时针


# line1 = [p1, q1]
# line2 = [p2, q2]
def do_intersect(p1: MapPoint, q1: MapPoint, p2: MapPoint, q2: MapPoint) -> bool:

    def on_segment(a: MapPoint, b: MapPoint, c: MapPoint) -> bool:
        # 假设 a,b,c 共线，判断 b 是否在 ac 的包围盒内（含端点，带容差）
        return (min(a.x, c.x) - g_geo_eps_ <= b.x <= max(a.x, c.x) + g_geo_eps_
                and min(a.y, c.y) - g_geo_eps_ <= b.y <= max(a.y, c.y) + g_geo_eps_)

    # 包围盒快速排除
    if (max(p1.x, q1.x) + g_geo_eps_ < min(p2.x, q2.x) or max(p2.x, q2.x) + g_geo_eps_ < min(p1.x, q1.x)
            or max(p1.y, q1.y) + g_geo_eps_ < min(p2.y, q2.y) or max(p2.y, q2.y) + g_geo_eps_ < min(p1.y, q1.y)):
        return False

    o1 = orientation(p1, q1, p2)
    o2 = orientation(p1, q1, q2)
    o3 = orientation(p2, q2, p1)
    o4 = orientation(p2, q2, q1)

    # 一般相交
    if o1 != o2 and o3 != o4:
        return True

    # 共线相交（端点在线段上）
    if o1 == 0 and on_segment(p1, p2, q1):
        return True
    if o2 == 0 and on_segment(p1, q2, q1):
        return True
    if o3 == 0 and on_segment(p2, p1, q2):
        return True
    if o4 == 0 and on_segment(p2, q1, q2):
        return True

    return False


def segment_intersection_point(geom1: MapGeometry, geom2: MapGeometry) -> Optional[MapPoint]:
    if geom1.type != MapGeometryType.LINESTRING or geom2.type != MapGeometryType.LINESTRING:
        raise NotImplementedError("segment_intersection_point 仅支持 LINESTRING")
    if len(geom1.points) != 2 or len(geom2.points) != 2:
        raise ValueError("segment_intersection_point 仅支持两点的 LINESTRING")
    a, b = geom1.points
    c, d = geom2.points
    if not do_intersect(a, b, c, d):
        return None
    line1 = b - a
    line2 = d - c
    rxs = line1.cross(line2).z
    q = c - a
    if abs(rxs) <= g_geo_eps_:

        def on_seg(p: MapPoint, u: MapPoint, v: MapPoint) -> bool:
            return (min(u.x, v.x) - g_geo_eps_ <= p.x <= max(u.x, v.x) + g_geo_eps_
                    and min(u.y, v.y) - g_geo_eps_ <= p.y <= max(u.y, v.y) + g_geo_eps_)

        for p in (a, b, c, d):
            if on_seg(p, a, b) and on_seg(p, c, d):
                return MapPoint(p.x, p.y, p.z)
        return None
    t = q.cross(line2).z / rxs
    ix = a.x + t * line1.x
    iy = a.y + t * line1.y
    return MapPoint(ix, iy, 0.0)


def segments_intersect(geom1: MapGeometry, geom2: MapGeometry) -> bool:
    return segment_intersection_point(geom1, geom2) is not None


def geom_intersect_points(geom1: MapGeometry, geom2: MapGeometry, dedup_eps: float = 1e-5) -> List[MapPoint]:
    """
    计算两条折线的所有交点（空间索引去重）

    优化：使用字典按网格分组，加速去重检查
    """
    if geom1.type != MapGeometryType.LINESTRING or geom2.type != MapGeometryType.LINESTRING:
        raise NotImplementedError("geom_intersect_points 仅支持 LINESTRING")

    pts1, pts2 = geom1.points, geom2.points

    # 网格大小 = dedup_eps * 2
    grid_size = dedup_eps * 2
    grid_dict = {}  # {(grid_x, grid_y): [MapPoint, ...]}

    def add_to_grid(pt: MapPoint):
        gx = int(pt.x / grid_size)
        gy = int(pt.y / grid_size)

        # 检查当前网格及周围 8 个网格
        for dx in [-1, 0, 1]:
            for dy in [-1, 0, 1]:
                neighbor_key = (gx + dx, gy + dy)
                if neighbor_key in grid_dict:
                    for existing_pt in grid_dict[neighbor_key]:
                        if pt2pt(pt, existing_pt) < dedup_eps:
                            return False  # 已存在相近点

        # 添加到当前网格
        if (gx, gy) not in grid_dict:
            grid_dict[(gx, gy)] = []
        grid_dict[(gx, gy)].append(pt)
        return True

    # 计算所有交点
    for i in range(len(pts1) - 1):
        seg1 = MapGeometry(points=[pts1[i], pts1[i + 1]], type=MapGeometryType.LINESTRING, crs=geom1.crs)
        for j in range(len(pts2) - 1):
            seg2 = MapGeometry(points=[pts2[j], pts2[j + 1]], type=MapGeometryType.LINESTRING, crs=geom2.crs)
            ipt = segment_intersection_point(seg1, seg2)

            if ipt is not None:
                add_to_grid(ipt)

    # 收集所有点
    intersect_pts = []
    for grid_pts in grid_dict.values():
        intersect_pts.extend(grid_pts)

    return intersect_pts


def geom_substring(geom_raw: MapGeometry,
                   start_offset: float,
                   end_offset: Optional[float] = None,
                   use_z: bool = False) -> MapGeometry:
    geom = copy.deepcopy(geom_raw)
    if geom.type != MapGeometryType.LINESTRING:
        raise NotImplementedError("geom_substring only supports LINESTRING geometries.")

    if len(geom.points) < 2:
        raise ValueError("LINESTRING must have at least 2 points.")
    if not use_z:
        for pt in geom.points:
            pt.z = 0.0
    total_length = geom_length(geom, use_z=use_z)

    # 处理默认值和边界条件
    if end_offset is None:
        end_offset = total_length

    # 限制范围
    start_offset = max(0, min(start_offset, total_length))
    end_offset = max(0, min(end_offset, total_length))

    if start_offset > end_offset:
        raise ValueError(f"start_offset ({start_offset}) must be less than end_offset ({end_offset})")

    # 辅助函数：根据距离找到对应的点
    def interpolate_point(target_distance: float) -> MapPoint:
        """根据目标距离在 polyline 上插值得到点"""
        accumulated_dist = 0.0

        for i in range(len(geom.points) - 1):
            p1, p2 = geom.points[i], geom.points[i + 1]
            seg_len = pt2pt(p1, p2, use_z=use_z)

            if accumulated_dist + seg_len >= target_distance - g_geo_eps_:
                # 目标点在当前线段上
                remaining_dist = target_distance - accumulated_dist

                if seg_len < g_geo_eps_:
                    return MapPoint(p1.x, p1.y, p1.z)

                t = remaining_dist / seg_len
                return MapPoint(p1.x + (p2.x - p1.x) * t, p1.y + (p2.y - p1.y) * t,
                                p1.z + (p2.z - p1.z) * t if p1.z is not None and p2.z is not None else 0.0)

            accumulated_dist += seg_len

        # 如果到达终点
        return MapPoint(geom.points[-1].x, geom.points[-1].y, geom.points[-1].z)

    # 处理起点和终点相同的情况（退化线段）
    if abs(start_offset - end_offset) < g_geo_eps_:
        point = interpolate_point(start_offset)
        return MapGeometry(points=[point, MapPoint(point.x, point.y, point.z)],
                           type=MapGeometryType.LINESTRING,
                           crs=geom.crs)

    # 收集截取线段的所有点
    result_points = []
    accumulated_dist = 0.0
    started = False

    for i in range(len(geom.points) - 1):
        p1, p2 = geom.points[i], geom.points[i + 1]
        seg_len = pt2pt(p1, p2, use_z=use_z)
        next_accumulated = accumulated_dist + seg_len

        # 判断起点是否在当前线段
        if not started and accumulated_dist <= start_offset < next_accumulated:
            start_point = interpolate_point(start_offset)
            result_points.append(start_point)
            started = True

        # 如果已经开始，添加线段端点
        if started:
            # 判断终点是否在当前线段
            if accumulated_dist < end_offset <= next_accumulated:
                end_point = interpolate_point(end_offset)
                result_points.append(end_point)
                break
            else:
                # 添加当前线段的终点
                result_points.append(MapPoint(p2.x, p2.y, p2.z))

        accumulated_dist = next_accumulated

    # 边界情况：起点或终点恰好在顶点上
    if not started and abs(start_offset) < g_geo_eps_:
        result_points.append(MapPoint(geom.points[0].x, geom.points[0].y, geom.points[0].z))

    if len(result_points) < 2:
        # 如果没有收集到足够的点，可能是精度问题，返回起终点
        start_point = interpolate_point(start_offset)
        end_point = interpolate_point(end_offset)
        result_points = [start_point, end_point]

    return MapGeometry(points=result_points, type=MapGeometryType.LINESTRING, crs=geom.crs)


# 便捷函数：从起点截取指定长度
def geom_substring_from_start(geom: MapGeometry, offset: float) -> MapGeometry:
    return geom_substring(geom, 0, offset)


# 便捷函数：从终点向前截取指定长度
def geom_substring_from_end(geom: MapGeometry, offset: float) -> MapGeometry:
    total_length = geom_length(geom)
    return geom_substring(geom, max(0, total_length - offset), total_length)


def sort_polylines_by_angle(geom_dict: dict,
                            reference_direction: Optional[MapPoint] = None,
                            use_end_point: bool = False,
                            reverse: bool = False) -> List[str]:
    if not geom_dict:
        return []

    if len(geom_dict) == 1:
        return list(geom_dict.keys())

    # 1. 默认参考方向为正X轴（极角排序），强制 Z = 0
    if reference_direction is None:
        reference_direction = MapPoint(1, 0, 0)
    else:
        # ✅ 强制将 Z 值设为 0（仅考虑 2D 平面）
        reference_direction = MapPoint(reference_direction.x, reference_direction.y, 0)

    reference_direction = reference_direction.normalize()

    # 2. 计算每条线的方向向量（忽略 Z 值）
    def get_direction_vector(geom: MapGeometry) -> MapPoint:
        """获取 polyline 的方向向量（仅 2D，Z 值设为 0）"""
        if len(geom.points) < 2:
            return MapPoint(0, 0, 0)

        if use_end_point:
            # 使用终点：倒数第二个点到终点（进入方向）
            p1, p2 = geom.points[-2], geom.points[-1]
        else:
            # 使用起点：起点到第二个点（离开方向）
            p1, p2 = geom.points[0], geom.points[1]

        # ✅ 强制 Z = 0，仅考虑 XY 平面
        direction = MapPoint(p2.x - p1.x, p2.y - p1.y, 0)
        return direction

    # 3. 计算叉积排序键值
    def cross_product_key(item):
        geom_id, geom = item
        direction = get_direction_vector(geom).normalize()

        # 使用 vector_angle 计算有向夹角
        angle = vector_angle(reference_direction, direction)

        # 计算点积（用于区分前后,共线时使用）
        dot = reference_direction.dot(direction)

        # 排序键:
        # 1. 先按角度（逆时针为正）: -angle 使得左侧排前
        # 2. 再按点积（前后）: -dot 使得同向排前
        return (-angle, -dot)

    # 4. 排序并返回 id 列表
    sorted_items = sorted(geom_dict.items(), key=cross_product_key, reverse=reverse)
    return [item[0] for item in sorted_items]


# 便捷函数：极角排序（正X轴为参考）
def sort_polylines_polar(geom_dict: dict, use_end_point: bool = False, reverse: bool = False) -> List[str]:
    return sort_polylines_by_angle(
        geom_dict,
        reference_direction=None,  # 默认使用正X轴
        use_end_point=use_end_point,
        reverse=reverse)


# 便捷函数：基于主路方向排序
def sort_polylines_by_main_road(geom_dict: dict,
                                main_road_geom: MapGeometry,
                                merge: bool = False,
                                reverse: bool = False) -> List[str]:
    if len(main_road_geom.points) < 2:
        raise ValueError("main_road_geom must have at least 2 points")

    # 提取主路方向向量
    if merge:
        reference_direction = main_road_geom.points[1] - main_road_geom.points[0]
    else:
        reference_direction = main_road_geom.points[-1] - main_road_geom.points[-2]

    return sort_polylines_by_angle(geom_dict,
                                   reference_direction=reference_direction,
                                   use_end_point=merge,
                                   reverse=reverse if not merge else not reverse)


def point_in_polygon(point: MapPoint, polygon: MapGeometry) -> bool:
    if polygon.type != MapGeometryType.POLYGON:
        raise ValueError("polygon must be POLYGON geometry")

    if len(polygon.points) < 4:  # 至少需要 3 个顶点 + 1 个封闭点
        raise ValueError("Polygon must have at least 3 vertices")

    x, y = point.x, point.y
    n = len(polygon.points) - 1  # 去掉封闭点
    inside = False

    p1x, p1y = polygon.points[0].x, polygon.points[0].y

    for i in range(1, n + 1):
        p2x, p2y = polygon.points[i % n].x, polygon.points[i % n].y

        # 检查点是否在边界上（边的包围盒内且共线）
        if (min(p1y, p2y) - g_geo_eps_ <= y <= max(p1y, p2y) + g_geo_eps_
                and min(p1x, p2x) - g_geo_eps_ <= x <= max(p1x, p2x) + g_geo_eps_):
            # 计算点到线段的距离
            if pt2line_min_dist(point, MapPoint(p1x, p1y), MapPoint(p2x, p2y)) < g_geo_eps_:
                return True  # 点在边界上，视为内部

        # 射线法核心逻辑
        if y > min(p1y, p2y):
            if y <= max(p1y, p2y):
                if x <= max(p1x, p2x):
                    if p1y != p2y:
                        xinters = (y - p1y) * (p2x - p1x) / (p2y - p1y) + p1x
                    if p1x == p2x or x <= xinters:
                        inside = not inside

        p1x, p1y = p2x, p2y

    return inside


def point_in_polygon_detailed(point: MapPoint, polygon: MapGeometry) -> PointInPolygonStatus:
    if polygon.type != MapGeometryType.POLYGON:
        raise ValueError("polygon must be POLYGON geometry")

    if len(polygon.points) < 4:
        raise ValueError("Polygon must have at least 3 vertices")

    x, y = point.x, point.y
    n = len(polygon.points) - 1
    inside = False
    p1x, p1y = polygon.points[0].x, polygon.points[0].y

    for i in range(1, n + 1):
        p2x, p2y = polygon.points[i % n].x, polygon.points[i % n].y

        # ✅ 检查点是否在边界上
        if (min(p1y, p2y) - g_geo_eps_ <= y <= max(p1y, p2y) + g_geo_eps_
                and min(p1x, p2x) - g_geo_eps_ <= x <= max(p1x, p2x) + g_geo_eps_):
            if pt2line_min_dist(point, MapPoint(p1x, p1y), MapPoint(p2x, p2y)) < g_geo_eps_:
                return PointInPolygonStatus.ON_BOUNDARY  # 返回边界

        # 射线法
        if y > min(p1y, p2y):
            if y <= max(p1y, p2y):
                if x <= max(p1x, p2x):
                    if p1y != p2y:
                        xinters = (y - p1y) * (p2x - p1x) / (p2y - p1y) + p1x
                    if p1x == p2x or x <= xinters:
                        inside = not inside

        p1x, p1y = p2x, p2y

    return PointInPolygonStatus.INSIDE if inside else PointInPolygonStatus.OUTSIDE


def polyline_in_polygon(polyline: MapGeometry, polygon: MapGeometry, strict: bool = False) -> bool:
    if polyline.type != MapGeometryType.LINESTRING:
        raise ValueError("polyline must be LINESTRING geometry")

    if polygon.type != MapGeometryType.POLYGON:
        raise ValueError("polygon must be POLYGON geometry")

    if len(polyline.points) < 2:
        raise ValueError("polyline must have at least 2 points")

    # 检查所有点
    points_inside = [point_in_polygon_detailed(pt, polygon) for pt in polyline.points]

    if strict:
        # 严格模式：所有点都在内部
        return all(status == PointInPolygonStatus.INSIDE for status in points_inside)
    else:
        # 宽松模式：至少一个点在内部
        return any(status == PointInPolygonStatus.INSIDE for status in points_inside)


# angle_range: -pi ~ pi
def geom_direction(geom: MapGeometry, offset: float) -> float:
    if geom.type != MapGeometryType.LINESTRING:
        raise NotImplementedError("geom_direction only supports LINESTRING geometries.")

    if len(geom.points) < 2:
        return 0.0
        # raise ValueError("LINESTRING must have at least 2 points.")

    total_length = geom_length(geom)
    if offset < -0.1:
        offset = total_length

    # 限制范围
    offset = max(0, min(offset, total_length))

    accumulated_dist = 0.0
    for i in range(len(geom.points) - 1):
        p1, p2 = geom.points[i], geom.points[i + 1]
        seg_len = pt2pt(p1, p2)

        next_accumulated = accumulated_dist + seg_len

        if accumulated_dist <= offset <= next_accumulated:
            # 目标点在当前线段上
            direction_vector = p2 - p1
            angle = math.atan2(direction_vector.y, direction_vector.x)
            return angle

        accumulated_dist = next_accumulated
    # 如果到达终点
    p1, p2 = geom.points[-2], geom.points[-1]
    direction_vector = p2 - p1
    angle = math.atan2(direction_vector.y, direction_vector.x)
    return angle


from shapely.geometry import LineString, Polygon


def geom_center_point(geom: MapGeometry) -> MapPoint:
    """
    获取多边形的中心点（代表点）

    使用 Shapely 的 representative_point() 方法，保证返回的点一定在多边形内部。
    该方法通过极点法（Pole of Inaccessibility）找到离边界最远的点。

    参数:
        geom: MapGeometry 对象，类型必须为 POLYGON

    返回:
        MapPoint 对象，表示多边形的中心点

    注意：
        - 对于凹多边形和环形多边形都能正确处理
        - 返回的点一定在多边形内部
        - 如果输入不是多边形，会抛出异常
    """
    if geom.type != MapGeometryType.POLYGON:
        raise ValueError("geom_center_point only supports POLYGON geometries")

    if len(geom.points) < 4:  # 至少需要 3 个顶点 + 1 个封闭点
        raise ValueError("Polygon must have at least 3 vertices")

    try:
        # 转换为 Shapely 格式（只用 x,y）
        poly_coords = [(p.x, p.y) if hasattr(p, 'x') else (p[0], p[1]) for p in geom.points]
        polygon = Polygon(poly_coords)

        # 检查几何对象是否有效，如果无效则尝试修复
        if not polygon.is_valid:
            polygon = polygon.buffer(0)

        # 获取代表点（离边界最远的点，一定在多边形内部）
        repr_pt = polygon.representative_point()

        # 转换回 MapPoint
        return MapPoint(repr_pt.x, repr_pt.y, 0.0)

    except Exception as e:
        print(f"Warning: geom_center_point failed with error: {e}")
        # 降级方案：返回外包矩形中心
        minx = min(p.x for p in geom.points)
        maxx = max(p.x for p in geom.points)
        miny = min(p.y for p in geom.points)
        maxy = max(p.y for p in geom.points)
        return MapPoint((minx + maxx) / 2.0, (miny + maxy) / 2.0, 0.0)


def geom_intersection_ratio(geom_points, container_polygon_points):
    """
    计算几何体在容纳多边形内部的占比

    返回值范围: [0.0, 1.0]
    - 0.0: 几何体完全在多边形外部
    - 1.0: 几何体完全在多边形内部

    支持两种几何体：
    - Polygon: 计算面积占比（inner_polygon_area / outer_polygon_area）
    - LineString: 计算长度占比（line_length_inside / total_line_length）

    注意：如果几何对象无效（如自相交），会尝试修复，修复失败则返回 0.0
    """
    try:
        # 转换为 Shapely 格式（只用 x,y）
        geom_coords = [(p.x, p.y) if hasattr(p, 'x') else (p[0], p[1]) for p in geom_points]
        container_coords = [(p.x, p.y) if hasattr(p, 'x') else (p[0], p[1]) for p in container_polygon_points]

        container_polygon = Polygon(container_coords)

        # 检查容器几何对象是否有效
        if not container_polygon.is_valid:
            container_polygon = container_polygon.buffer(0)

        # 判断输入几何体的类型（根据点数判断）
        is_polygon = len(geom_coords) > 2 and geom_coords[0] == geom_coords[-1]  # 闭合多边形

        if is_polygon:
            # 处理多边形类型
            inner_polygon = Polygon(geom_coords)
            if not inner_polygon.is_valid:
                inner_polygon = inner_polygon.buffer(0)

            inner_area = inner_polygon.area
            if inner_area == 0:
                return 0.0

            # 获取两个多边形的交集
            intersection = inner_polygon.intersection(container_polygon)

            # 计算交集的面积
            intersect_area = 0
            if hasattr(intersection, 'area'):
                intersect_area = intersection.area
            elif hasattr(intersection, 'geoms'):  # 如果是多个几何体
                intersect_area = sum(geom.area for geom in intersection.geoms if hasattr(geom, 'area'))

            return intersect_area / inner_area
        else:
            # 处理线条类型
            line = LineString(geom_coords)
            if not line.is_valid:
                line = line.buffer(0)  # buffer(0) 可以修复大部分自相交问题

            total_length = line.length
            if total_length == 0:
                return 0.0

            # 获取线与多边形的交集
            intersection = line.intersection(container_polygon)

            # 计算交集的长度
            inside_length = 0
            if hasattr(intersection, 'length'):
                inside_length = intersection.length
            elif hasattr(intersection, 'geoms'):  # 如果是多个几何体
                inside_length = sum(geom.length for geom in intersection.geoms if hasattr(geom, 'length'))

            return 1.0 * inside_length / total_length

    except Exception as e:
        # 如果仍然出错，打印警告并返回 0.0
        print(f"Warning: geom_intersection_ratio failed with error: {e}")
        return 0.0


# 向后兼容的别名
def polygon_inside_polygon_ratio(inner_polygon_points, outer_polygon_points):
    """已弃用：请使用 geom_intersection_ratio"""
    return geom_intersection_ratio(inner_polygon_points, outer_polygon_points)


def polyline_inside_polygon_ratio(polyline_points, polygon_points):
    """已弃用：请使用 geom_intersection_ratio"""
    return geom_intersection_ratio(polyline_points, polygon_points)


def angle_difference(angle1: float, angle2: float, degree: bool = False) -> float:
    """
    计算从angle1到angle2的有向夹角
    返回值范围: [-π, π]，正值=逆时针，负值=顺时针

    示例:
        angle_difference(0, π/2) = π/2  (逆时针90度)
        angle_difference(π/2, 0) = -π/2 (顺时针90度)
    """
    diff = angle2 - angle1
    diff_angle = math.atan2(math.sin(diff), math.cos(diff))
    if degree:
        diff_angle = math.degrees(diff_angle)
    return diff_angle


def vector_angle(v1: MapPoint, v2: MapPoint) -> float:
    """
    计算从向量v1到向量v2的有向夹角
    返回值范围: [-π, π]，正值=逆时针，负值=顺时针

    示例:
        v1 = MapPoint(1, 0), v2 = MapPoint(0, 1)
        vector_angle(v1, v2) = π/2  (逆时针90度)
    """
    cross_z = v1.cross(v2).z
    dot = v1.dot(v2)
    return math.atan2(cross_z, dot)


def angle_mean(angles: list[float]) -> float:
    if not angles:
        return 0.0
    # 将角度转换到 [-pi, pi) 区间
    angles = [np.mod(angle + np.pi, 2 * np.pi) - np.pi for angle in angles]
    # 转为单位向量
    x_mean = np.mean(np.cos(angles))
    y_mean = np.mean(np.sin(angles))
    # 还原角度
    return math.atan2(y_mean, x_mean)


def perpendicular_distance_along_direction(pt1: MapPoint, pt2: MapPoint, direction_angle: float) -> float:
    """
    计算两个点在给定方向上的纵向距离（垂直于方向的距离）

    参数:
        pt1: 第一个点
        pt2: 第二个点
        direction_angle: 方向角度（弧度），范围 [-π, π]

    返回:
        垂直于方向的距离（纵向距离）

    示例:
        pt1 = MapPoint(0, 0), pt2 = MapPoint(2, 1)
        direction_angle = 0 (正东) -> 垂直距离 = 1 (y方向差)
        direction_angle = π/2 (正北) -> 垂直距离 = 2 (x方向差)
    """
    # 两点之间的向量
    vec = pt2 - pt1
    vec.z = 0  # 只考虑2D平面

    # 垂直于给定方向的单位向量（逆时针旋转90度）
    # 如果方向是 (cos(θ), sin(θ))，则垂直方向是 (-sin(θ), cos(θ))
    perpendicular_vec = MapPoint(math.cos(direction_angle), math.sin(direction_angle), 0)

    # 计算两点连线在垂直方向上的投影长度（点积）
    perpendicular_dist = abs(vec.dot(perpendicular_vec))

    return perpendicular_dist


def construct_polygon_from_two_polylines(geom1: MapGeometry, geom2: MapGeometry, merge: bool = False) -> MapGeometry:
    """
    从两条polyline构造一个无自交的多边形

    算法：
    1. 根据merge参数确定连接方式：
       - merge=False (分叉): geom1起点 -> geom2起点, geom1终点 -> geom2终点
       - merge=True (合并): geom1终点 -> geom2终点, geom1起点 -> geom2起点
    2. 检测连接线是否与两条polyline相交
    3. 如果有交点，裁剪polyline到最近的交点，确保形成无自交多边形

    参数:
        geom1: 第一条polyline
        geom2: 第二条polyline
        merge: False表示分叉场景，True表示合并场景

    返回:
        无自交的多边形geometry
    """
    if geom1.type != MapGeometryType.LINESTRING or geom2.type != MapGeometryType.LINESTRING:
        raise ValueError("construct_polygon_from_two_polylines only supports LINESTRING geometries")

    if len(geom1.points) < 2 or len(geom2.points) < 2:
        raise ValueError("Both polylines must have at least 2 points")

    # 深拷贝避免修改原始数据
    line1 = copy.deepcopy(geom1)
    line2 = copy.deepcopy(geom2)

    # 根据merge参数确定连接方式
    if not merge:
        # 分叉场景: 起点相连，终点相连
        start_connection = MapGeometry(points=[line1.points[0], line2.points[0]],
                                       type=MapGeometryType.LINESTRING,
                                       crs=geom1.crs)
        end_connection = MapGeometry(points=[line1.points[-1], line2.points[-1]],
                                     type=MapGeometryType.LINESTRING,
                                     crs=geom1.crs)
    else:
        # 合并场景: 终点相连，起点相连
        start_connection = MapGeometry(points=[line1.points[-1], line2.points[-1]],
                                       type=MapGeometryType.LINESTRING,
                                       crs=geom1.crs)
        end_connection = MapGeometry(points=[line1.points[0], line2.points[0]],
                                     type=MapGeometryType.LINESTRING,
                                     crs=geom1.crs)

    # 检测起点连接线与两条polyline的交点
    start_intersects_1 = geom_intersect_points(start_connection, line1)
    start_intersects_2 = geom_intersect_points(start_connection, line2)

    # 检测终点连接线与两条polyline的交点
    end_intersects_1 = geom_intersect_points(end_connection, line1)
    end_intersects_2 = geom_intersect_points(end_connection, line2)

    # 过滤掉端点处的交点（容差范围内）
    def filter_endpoint_intersections(intersect_points: List[MapPoint], endpoint1: MapPoint,
                                      endpoint2: MapPoint) -> List[MapPoint]:
        filtered = []
        for pt in intersect_points:
            dist1 = pt2pt(pt, endpoint1)
            dist2 = pt2pt(pt, endpoint2)
            if dist1 > g_geo_eps_ and dist2 > g_geo_eps_:
                filtered.append(pt)
        return filtered

    if not merge:
        # 分叉场景
        start_conn_pt1, start_conn_pt2 = line1.points[0], line2.points[0]
        end_conn_pt1, end_conn_pt2 = line1.points[-1], line2.points[-1]
    else:
        # 合并场景
        start_conn_pt1, start_conn_pt2 = line1.points[-1], line2.points[-1]
        end_conn_pt1, end_conn_pt2 = line1.points[0], line2.points[0]

    start_intersects_1 = filter_endpoint_intersections(start_intersects_1, start_conn_pt1, start_conn_pt2)
    start_intersects_2 = filter_endpoint_intersections(start_intersects_2, start_conn_pt1, start_conn_pt2)
    end_intersects_1 = filter_endpoint_intersections(end_intersects_1, end_conn_pt1, end_conn_pt2)
    end_intersects_2 = filter_endpoint_intersections(end_intersects_2, end_conn_pt1, end_conn_pt2)

    # 如果有交点，需要裁剪polyline
    if start_intersects_1 or start_intersects_2 or end_intersects_1 or end_intersects_2:
        # 找到最近的交点并裁剪
        if not merge:
            # 分叉场景：从起点开始裁剪
            cut_offset_1: Optional[float] = None
            cut_offset_2: Optional[float] = None

            # 检查起点连接线的交点
            for pt in start_intersects_1:
                offset = proj_pt2polyline(pt, line1, use_z=False)
                if cut_offset_1 is None or offset < cut_offset_1:
                    cut_offset_1 = offset

            for pt in start_intersects_2:
                offset = proj_pt2polyline(pt, line2, use_z=False)
                if cut_offset_2 is None or offset < cut_offset_2:
                    cut_offset_2 = offset

            # 检查终点连接线的交点（从终点往回算）
            line1_len = geom_length(line1)
            line2_len = geom_length(line2)

            for pt in end_intersects_1:
                offset = proj_pt2polyline(pt, line1, use_z=False)
                if cut_offset_1 is None or offset > cut_offset_1:
                    cut_offset_1 = offset

            for pt in end_intersects_2:
                offset = proj_pt2polyline(pt, line2, use_z=False)
                if cut_offset_2 is None or offset > cut_offset_2:
                    cut_offset_2 = offset

            # 应用裁剪
            if cut_offset_1 is not None:
                line1 = geom_substring(line1, 0, cut_offset_1)
            if cut_offset_2 is not None:
                line2 = geom_substring(line2, 0, cut_offset_2)
        else:
            # 合并场景：从终点开始裁剪
            cut_offset_1: Optional[float] = None
            cut_offset_2: Optional[float] = None

            line1_len = geom_length(line1)
            line2_len = geom_length(line2)

            # 检查起点连接线的交点（在合并场景中，这是终点连接）
            for pt in start_intersects_1:
                offset = proj_pt2polyline(pt, line1, use_z=False)
                if cut_offset_1 is None or offset > cut_offset_1:
                    cut_offset_1 = offset

            for pt in start_intersects_2:
                offset = proj_pt2polyline(pt, line2, use_z=False)
                if cut_offset_2 is None or offset > cut_offset_2:
                    cut_offset_2 = offset

            # 检查终点连接线的交点（在合并场景中，这是起点连接）
            for pt in end_intersects_1:
                offset = proj_pt2polyline(pt, line1, use_z=False)
                if cut_offset_1 is None or offset < cut_offset_1:
                    cut_offset_1 = offset

            for pt in end_intersects_2:
                offset = proj_pt2polyline(pt, line2, use_z=False)
                if cut_offset_2 is None or offset < cut_offset_2:
                    cut_offset_2 = offset

            # 应用裁剪
            if cut_offset_1 is not None:
                line1 = geom_substring(line1, cut_offset_1, line1_len)
            if cut_offset_2 is not None:
                line2 = geom_substring(line2, cut_offset_2, line2_len)

    # 构造多边形
    polygon_points = []
    polygon_points.extend(line1.points)
    polygon_points.extend(line2.points[::-1])
    polygon_points.append(polygon_points[0])  # 闭合多边形

    return MapGeometry(points=polygon_points, type=MapGeometryType.POLYGON, crs=geom1.crs)
