from .data_infra import map_dataclass, g_geo_eps_
from .map_coord import MapCRS, coord_transform

import math
from enum import Enum
from dataclasses import field
from typing import List

import numpy as np

import math


class MapGeometryType(Enum):
    POINT = "Point"
    LINESTRING = "LineString"
    POLYGON = "Polygon"


@map_dataclass
class MapPoint:
    x: float
    y: float
    z: float = 0.0

    def __repr__(self):
        return f"Point(x={self.x}, y={self.y}, z={self.z})"

    # 新增：按容差比较相等
    def __eq__(self, other: object, use_z: bool = False) -> bool:
        if not isinstance(other, MapPoint):
            return False
        if use_z:
            return (abs(self.x - other.x) <= g_geo_eps_ and abs(self.y - other.y) <= g_geo_eps_
                    and abs(self.z - other.z) <= g_geo_eps_)
        else:
            return (abs(self.x - other.x) <= g_geo_eps_ and abs(self.y - other.y) <= g_geo_eps_)

    # 向量加法
    def __add__(self, other):
        if not isinstance(other, MapPoint):
            raise TypeError("Operands must be of type MapPoint")
        return MapPoint(self.x + other.x, self.y + other.y, self.z + other.z)

    # 向量减法
    def __sub__(self, other):
        if not isinstance(other, MapPoint):
            raise TypeError("Operands must be of type MapPoint")
        return MapPoint(self.x - other.x, self.y - other.y, self.z - other.z)

    # 向量点积
    def dot(self, other):
        if not isinstance(other, MapPoint):
            raise TypeError("Operands must be of type MapPoint")
        return self.x * other.x + self.y * other.y + self.z * other.z

    # 向量叉乘
    def cross(self, other):
        if not isinstance(other, MapPoint):
            raise TypeError("Operands must be of type MapPoint")
        return MapPoint(
            self.y * other.z - self.z * other.y,
            self.z * other.x - self.x * other.z,
            self.x * other.y - self.y * other.x,
        )

    # 向量模长
    def magnitude(self, use_z: bool = True):
        if not use_z:
            return math.sqrt(self.x**2 + self.y**2)
        return math.sqrt(self.x**2 + self.y**2 + self.z**2)

    # 标量乘法
    def __mul__(self, scalar):
        if not isinstance(scalar, (int, float)):
            raise TypeError("Scalar must be a number")
        return MapPoint(self.x * scalar, self.y * scalar, self.z * scalar)

    # 标量除法
    def __truediv__(self, scalar):
        if not isinstance(scalar, (int, float)):
            raise TypeError("Scalar must be a number")
        if scalar == g_geo_eps_:
            raise ValueError("Division by zero is not allowed")
        return MapPoint(self.x / scalar, self.y / scalar, self.z / scalar)

    def normalize(self):
        mag = self.magnitude()
        if mag < g_geo_eps_:
            return self
        return self / mag


@map_dataclass
class MapGeometry:
    points: List[MapPoint] = field(default_factory=list)
    type: MapGeometryType = MapGeometryType.LINESTRING
    crs: MapCRS = None

    def __repr__(self):
        return f"MapGeometry type={self.type}, points={self.points}, crs={self.crs})"

    def _points_key(self):
        return tuple((float(p.x), float(p.y), float(p.z)) for p in self.points)

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, MapGeometry):
            return False
        return self.type == other.type and self._points_key() == other._points_key()

    def __hash__(self) -> int:
        return hash(self._points_key())

    def length(self, use_z: bool = False) -> float:
        length = 0.0
        for i in range(1, len(self.points)):
            length += (self.points[i - 1] - self.points[i]).magnitude(use_z=use_z)
        return length

    def crc_transform(self, target_crs: MapCRS) -> "MapGeometry":
        if self.crs == target_crs:
            return self
        arr = self.to_numpy()  # shape (N,3)
        if arr.size == 0:
            return MapGeometry(points=[], type=self.type, crs=target_crs)

        # extract xy for transform
        xy = arr[:, :2]
        transformed = coord_transform(xy, self.crs, target_crs)  # returns ndarray (N,2)

        # rebuild points: use transformed x,y and original z
        new_pts = []
        for i in range(transformed.shape[0]):
            x, y = float(transformed[i, 0]), float(transformed[i, 1])
            z = float(arr[i, 2]) if arr.shape[1] > 2 else 0.0
            new_pts.append(MapPoint(x, y, z))

        # construct via MapGeometry.from_numpy to keep consistent
        np_pts = np.array([[p.x, p.y, p.z] for p in new_pts], dtype=float)
        return MapGeometry.from_numpy(np_pts[:, :3], self.type, crs=target_crs)

    def get_bbox2d(self):
        """Get the bounding box of the geometry as (minx, miny, maxx, maxy)"""
        if not self.points:
            return None
        xs = [p.x for p in self.points]
        ys = [p.y for p in self.points]
        return (min(xs), min(ys), max(xs), max(ys))

    def coord_transform(self, transforme_matrix: np.ndarray) -> "MapGeometry":
        """Apply a 4x4 transformation matrix to the points.

        The points are treated as (x, y, z, 1) homogeneous coordinates.
        """
        if transforme_matrix.shape != (4, 4):
            raise ValueError("transforme_matrix must be a 4x4 numpy array")
        arr = self.to_numpy()  # shape (N,3)
        if arr.size == 0:
            return MapGeometry(points=[], type=self.type, crs=self.crs)
        # convert to homogeneous coordinates
        ones = np.ones((arr.shape[0], 1), dtype=float)
        arr_homogeneous = np.hstack((arr, ones))
        transformed = transforme_matrix.dot(arr_homogeneous.T).T
        return MapGeometry.from_numpy(transformed[:, :3], self.type, crs=self.crs)

    def coord_offset(self,
                     x_offset: float = 0.0,
                     y_offset: float = 0.0,
                     z_offset: float = 0.0,
                     ratio: float = 1.0) -> "MapGeometry":
        arr = self.to_numpy()  # shape (N,3)
        if arr.size == 0:
            return MapGeometry(points=[], type=self.type, crs=self.crs)
        arr[:, 0] = (arr[:, 0] - x_offset) / ratio
        arr[:, 1] = (arr[:, 1] - y_offset) / ratio
        arr[:, 2] = arr[:, 2] - z_offset
        self.points = MapGeometry.from_numpy(arr, self.type, crs=self.crs).points
        return self

    def coord_offset_reverse(self,
                             x_offset: float = 0.0,
                             y_offset: float = 0.0,
                             z_offset: float = 0.0,
                             ratio: float = 1.0) -> "MapGeometry":
        arr = self.to_numpy()  # shape (N,3)
        if arr.size == 0:
            return MapGeometry(points=[], type=self.type, crs=self.crs)
        arr[:, 0] = (arr[:, 0] * ratio) + x_offset
        arr[:, 1] = (arr[:, 1] * ratio) + y_offset
        arr[:, 2] = arr[:, 2] + z_offset
        self.points = MapGeometry.from_numpy(arr, self.type, crs=self.crs).points
        return self

    def to_numpy(self) -> np.ndarray:
        """Convert the points to a numpy array of shape (N, 3)"""
        if not self.points:
            return np.empty((0, 3), dtype=float)
        return np.array([[p.x, p.y, p.z] for p in self.points], dtype=float)

    @staticmethod
    def from_numpy(pts: np.ndarray,
                   geom_type: MapGeometryType = MapGeometryType.LINESTRING,
                   crs: MapCRS = None) -> "MapGeometry":
        """Create a MapGeometry from a numpy array of shape (N, 2) or (N, 3)"""
        if pts.ndim != 2 or pts.shape[1] not in (2, 3):
            raise ValueError("pts must be a numpy array of shape (N, 2) or (N, 3)")
        points = []
        for i in range(pts.shape[0]):
            if pts.shape[1] == 2:
                points.append(MapPoint(float(pts[i, 0]), float(pts[i, 1]), 0.0))
            else:
                points.append(MapPoint(float(pts[i, 0]), float(pts[i, 1]), float(pts[i, 2])))

        if geom_type == MapGeometryType.POLYGON and points[0] != points[-1]:
            # close the polygon if not closed
            points.append(points[0])
        return MapGeometry(points=points, type=geom_type, crs=crs)

    @staticmethod
    def from_shapely(shapely_geom, geom_type: 'MapGeometryType' = None, crs: MapCRS = None) -> "MapGeometry":
        """Construct MapGeometry from a Shapely geometry (Point/LineString/Polygon/Multi*).

        If `geom_type` is provided it will be used as the target MapGeometryType, otherwise
        the shapely geometry type will be used to infer a type.
        """
        if shapely_geom is None:
            return MapGeometry(points=[], type=(geom_type or MapGeometryType.LINESTRING), crs=crs)
        if hasattr(shapely_geom, 'is_empty') and shapely_geom.is_empty:
            return MapGeometry(points=[], type=(geom_type or MapGeometryType.LINESTRING), crs=crs)

        gname = getattr(shapely_geom, 'geom_type', None)
        # infer type if not provided
        if geom_type is None:
            if gname in ("Point", "MultiPoint"):
                geom_type = MapGeometryType.POINT
            elif gname in ("Polygon", "MultiPolygon"):
                geom_type = MapGeometryType.POLYGON
            else:
                geom_type = MapGeometryType.LINESTRING

        pts: list[MapPoint] = []

        def _append_coords(seq):
            for c in seq:
                x = float(c[0])
                y = float(c[1])
                z = float(c[2]) if len(c) > 2 else 0.0
                pts.append(MapPoint(x, y, z))

        if geom_type == MapGeometryType.POINT:
            if gname == 'Point':
                coords = list(shapely_geom.coords)
                if coords:
                    c = coords[0]
                    pts.append(MapPoint(float(c[0]), float(c[1]), float(c[2]) if len(c) > 2 else 0.0))
            elif gname == 'MultiPoint':
                first = next(iter(shapely_geom.geoms), None)
                if first is not None:
                    c = list(first.coords)[0]
                    pts.append(MapPoint(float(c[0]), float(c[1]), float(c[2]) if len(c) > 2 else 0.0))
            else:
                coords = list(getattr(shapely_geom, 'coords', []))
                if coords:
                    c = coords[0]
                    pts.append(MapPoint(float(c[0]), float(c[1]), float(c[2]) if len(c) > 2 else 0.0))
        elif geom_type == MapGeometryType.LINESTRING:
            if gname == 'LineString':
                _append_coords(list(shapely_geom.coords))
            elif gname == 'MultiLineString':
                for part in shapely_geom.geoms:
                    _append_coords(list(part.coords))
            elif gname == 'Polygon':
                _append_coords(list(shapely_geom.exterior.coords))
            elif gname == 'MultiPolygon':
                first = next(iter(shapely_geom.geoms), None)
                if first is not None:
                    _append_coords(list(first.exterior.coords))
            else:
                _append_coords(list(getattr(shapely_geom, 'coords', [])))
        elif geom_type == MapGeometryType.POLYGON:
            if gname == 'Polygon':
                _append_coords(list(shapely_geom.exterior.coords))
            elif gname == 'MultiPolygon':
                first = next(iter(shapely_geom.geoms), None)
                if first is not None:
                    _append_coords(list(first.exterior.coords))
            elif gname in ('LineString', 'MultiLineString'):
                if gname == 'MultiLineString':
                    for part in shapely_geom.geoms:
                        _append_coords(list(part.coords))
                else:
                    _append_coords(list(shapely_geom.coords))
            else:
                _append_coords(list(getattr(shapely_geom, 'coords', [])))
        else:
            _append_coords(list(getattr(shapely_geom, 'coords', [])))

        return MapGeometry(points=pts, type=geom_type, crs=crs)

    def to_shapely(self):
        """Convert MapGeometry to a Shapely geometry (Point/LineString/Polygon/MultiPoint).

        - POINT -> Point (single) or MultiPoint (multiple)
        - LINESTRING -> LineString
        - POLYGON -> Polygon using points as exterior ring
        """
        from shapely.geometry import Point as ShPoint, LineString as ShLine, Polygon as ShPoly, MultiPoint as ShMultiPoint

        coords = [(p.x, p.y) if p.z == 0.0 else (p.x, p.y, p.z) for p in self.points]

        if self.type == MapGeometryType.POINT:
            if not coords:
                return None
            if len(coords) == 1:
                return ShPoint(coords[0])
            return ShMultiPoint(coords)
        if self.type == MapGeometryType.LINESTRING:
            return ShLine(coords)
        if self.type == MapGeometryType.POLYGON:
            return ShPoly(coords)
        return None

    @staticmethod
    def from_list(lst: List[List[float]],
                  geom_type: MapGeometryType = MapGeometryType.LINESTRING,
                  crs: MapCRS = None) -> "MapGeometry":
        """
        从 list[list[x, y, z]] 或 list[list[x, y]] 构造 MapGeometry
        """
        return MapGeometry.from_numpy(np.array(lst), geom_type=geom_type, crs=crs)

    def to_list(self, use_z=True) -> List[List[float]]:
        """
        转为 list[list[x, y, z]]，如果 z==0 也会输出三维
        """
        if use_z:
            return [[p.x, p.y, p.z] for p in self.points]
        return [[p.x, p.y] for p in self.points]

    def to_2d(self):
        for p in self.points:
            p.z = 0.0
        return self

    def start_point(self) -> MapPoint:
        if not self.points:
            return None
        return self.points[0]

    def end_point(self) -> MapPoint:
        if not self.points:
            return None
        return self.points[-1]

    def start_point_geometry(self) -> "MapGeometry":
        if not self.points:
            return MapGeometry(points=[], type=MapGeometryType.POINT, crs=self.crs)
        return MapGeometry(points=[self.points[0]], type=MapGeometryType.POINT, crs=self.crs)

    def end_point_geometry(self) -> "MapGeometry":
        if not self.points:
            return MapGeometry(points=[], type=MapGeometryType.POINT, crs=self.crs)
        return MapGeometry(points=[self.points[-1]], type=MapGeometryType.POINT, crs=self.crs)

    def cut_by_offsets(self, offsets: List[float], use_z: bool = False) -> List["MapGeometry"]:
        if self.type != MapGeometryType.LINESTRING:
            return [self]
        if not self.points or len(self.points) < 2:
            return []

        total_length = self.length(use_z=use_z)
        if total_length <= g_geo_eps_:
            return [self]

        valid_offsets = sorted({float(o) for o in offsets if g_geo_eps_ < o < total_length - g_geo_eps_})
        if not valid_offsets:
            return [self]

        segments: List[List[MapPoint]] = []
        current_points: List[MapPoint] = [self.points[0]]
        offset_index = 0
        accumulated = 0.0

        for i in range(1, len(self.points)):
            p0 = self.points[i - 1]
            p1 = self.points[i]
            seg_len = (p1 - p0).magnitude(use_z=use_z)
            if seg_len <= g_geo_eps_:
                current_points.append(p1)
                continue

            while offset_index < len(
                    valid_offsets) and valid_offsets[offset_index] <= accumulated + seg_len + g_geo_eps_:
                target_offset = valid_offsets[offset_index]
                if target_offset < accumulated - g_geo_eps_:
                    offset_index += 1
                    continue
                ratio = (target_offset - accumulated) / seg_len
                ratio = max(0.0, min(1.0, ratio))
                split_point = p0 + (p1 - p0) * ratio
                current_points.append(split_point)
                if len(current_points) >= 2:
                    segments.append(current_points)
                current_points = [split_point]
                offset_index += 1

            accumulated += seg_len
            current_points.append(p1)

        if len(current_points) >= 2:
            segments.append(current_points)

        return [MapGeometry(points=seg, type=self.type, crs=self.crs) for seg in segments]


if __name__ == "__main__":
    point = MapGeometry([MapPoint(1.3, 2.6, 3)], MapGeometryType.POINT)
    line = MapGeometry([MapPoint(1.34, 2.141, 3.1341), MapPoint(4.141, 5.14214, 6.1241)], MapGeometryType.LINESTRING)
    polygon = MapGeometry(
        [MapPoint(1.34, 2.141, 3.1341),
         MapPoint(4.141, 5.14214, 6.1241),
         MapPoint(7.141, 8.14214, 9.1241)],
        MapGeometryType.POLYGON,
    )
    pj = point.to_json()
    pts = MapGeometry.from_json(pj)
    print(point)
    print(line)
    print(polygon)
