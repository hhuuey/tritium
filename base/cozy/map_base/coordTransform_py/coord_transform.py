#!/usr/bin/env python3
import json
import os
import sys
from math import sin, cos, sqrt, radians


class PointENU:
    def __init__(self, x=0.0, y=0.0, z=0.0):
        self.x = float(x)
        self.y = float(y)
        self.z = float(z)

    def __repr__(self):
        return f"PointENU(x={self.x:.6f}, y={self.y:.6f}, z={self.z:.6f})"


class PointLLH:
    def __init__(self, lon=0.0, lat=0.0, height=0.0):
        self.lon = float(lon)
        self.lat = float(lat)
        self.height = float(height)

    def __repr__(self):
        return f"PointLLH(lon={self.lon:.9f}, lat={self.lat:.9f}, height={self.height:.3f})"


class CoordinateConverter:
    # Geodetic system parameters (same as C++ implementation)
    kSemimajorAxis = 6378137.0
    kSemiminorAxis = 6356752.31425
    kFirstEccentricitySquared = 6.69437999014e-3

    def __init__(self):
        self.has_origin = False
        self.origin = PointLLH()
        self.origin_ecef = PointENU()
        # 3x3 matrices as lists of lists
        self.matrix_ecef_to_enu = [[0.0]*3 for _ in range(3)]
        self.matrix_enu_to_ecef = [[0.0]*3 for _ in range(3)]

    def set_origin(self, origin: PointLLH):
        self.origin = origin
        # compute origin ECEF
        self.origin_ecef = self._lla_to_ecef(origin)

        lon = radians(origin.lon)
        lat = radians(origin.lat)
        sLon = sin(lon)
        sLat = sin(lat)
        cLon = cos(lon)
        cLat = cos(lat)

        # fill ecef->enu matrix
        m = [[0.0]*3 for _ in range(3)]
        m[0][0] = -sLon
        m[0][1] = cLon
        m[0][2] = 0.0
        m[1][0] = -sLat * cLon
        m[1][1] = -sLat * sLon
        m[1][2] = cLat
        m[2][0] = cLat * cLon
        m[2][1] = cLat * sLon
        m[2][2] = sLat
        self.matrix_ecef_to_enu = m

        # enu->ecef is transpose
        self.matrix_enu_to_ecef = [list(col) for col in zip(*m)]

        self.has_origin = True

    def get_origin(self) -> PointLLH:
        if not self.has_origin:
            raise RuntimeError("Origin is not set. Call set_origin first.")
        return self.origin

    def lla_to_enu(self, lla: PointLLH) -> PointENU:
        if not self.has_origin:
            raise RuntimeError("Origin is not set. Call set_origin first.")
        ecef = self._lla_to_ecef(lla)
        return self._ecef_to_enu(ecef)

    def _lla_to_ecef(self, lla: PointLLH) -> PointENU:
        lon_rad = radians(lla.lon)
        lat_rad = radians(lla.lat)
        altitude = lla.height
        xi = sqrt(1 - self.kFirstEccentricitySquared * sin(lat_rad)**2)
        x = (self.kSemimajorAxis / xi + altitude) * cos(lat_rad) * cos(lon_rad)
        y = (self.kSemimajorAxis / xi + altitude) * cos(lat_rad) * sin(lon_rad)
        z = (self.kSemimajorAxis / xi * (1 - self.kFirstEccentricitySquared) + altitude) * sin(lat_rad)
        return PointENU(x, y, z)

    def _ecef_to_enu(self, ecef: PointENU) -> PointENU:
        # delta from origin
        dx = ecef.x - self.origin_ecef.x
        dy = ecef.y - self.origin_ecef.y
        dz = ecef.z - self.origin_ecef.z
        vec = [dx, dy, dz]
        # multiply matrix_ecef_to_enu * vec
        m = self.matrix_ecef_to_enu
        ex = m[0][0]*vec[0] + m[0][1]*vec[1] + m[0][2]*vec[2]
        ey = m[1][0]*vec[0] + m[1][1]*vec[1] + m[1][2]*vec[2]
        ez = m[2][0]*vec[0] + m[2][1]*vec[1] + m[2][2]*vec[2]
        return PointENU(ex, ey, ez)


def _parse_point_llh(elem, default_height=0.0):
    """Robustly parse a point containing lon/lat/height from various shapes.
    Accepts dict with keys ('lon'|'lng'), 'lat', ('height'|'h'|'alt'),
    or a list/tuple [lon, lat, height?].
    """
    if elem is None:
        return None
    if isinstance(elem, (list, tuple)):
        if len(elem) >= 2:
            lon = float(elem[0])
            lat = float(elem[1])
            h = float(elem[2]) if len(elem) >= 3 else 0.0
            return PointLLH(lon, lat, h)
        return None
    if isinstance(elem, dict):
        # possible key variants
        lon = elem.get('x', None)
        lat = elem.get('y', None)
        h = default_height
        if lon is None or lat is None:
            return None
        try:
            return PointLLH(float(lon), float(lat), float(h))
        except Exception:
            return None
    return None


def _to_enu_dict(enu_pt: PointENU):
    return {'x': enu_pt.x, 'y': enu_pt.y}


def _transform_points_list(conv: CoordinateConverter, pts):
    """Transform a list of points in-place and return new list of ENU dicts."""
    if not isinstance(pts, list):
        return pts
    new_pts = []
    for p in pts:
        lla = _parse_point_llh(p)
        if lla is None:
            # keep original if cannot parse
            new_pts.append(p)
            continue
        enu = conv.lla_to_enu(lla)
        new_pts.append(_to_enu_dict(enu))
    return new_pts


def _transform_point_single(conv: CoordinateConverter, p):
    lla = _parse_point_llh(p)
    if lla is None:
        return p
    return _to_enu_dict(conv.lla_to_enu(lla))


def _read_origin(orig_path: str):
    if not orig_path:
        return None
    if not os.path.exists(orig_path):
        return None
    try:
        with open(orig_path, 'r') as f:
            text = f.read().strip()
        if not text:
            return None
        parts = [p for p in text.replace('\n', ' ').replace(',', ' ').split(' ') if p]
        if len(parts) < 2:
            return None
        lon = float(parts[0])
        lat = float(parts[1])
        height = float(parts[2]) if len(parts) >= 3 else 0.0
        return PointLLH(lon, lat, height)
    except Exception:
        return None


def coord_transform(orig_path: str, json_file: str, output_file: str) -> None:
    """Read origin from orig_path and transform relevant coordinates in json_file to ENU.

    Returns path to the new JSON file written (json_file with suffix _enu.json).
    """
    origin = _read_origin(orig_path)
    if origin is None:
        raise FileNotFoundError(f"Origin file not found or invalid: {orig_path}")

    if not os.path.exists(json_file):
        raise FileNotFoundError(f"JSON file not found: {json_file}")

    with open(json_file, 'r', encoding='utf-8') as f:
        data_raw = json.load(f)

    conv = CoordinateConverter()
    conv.set_origin(origin)
    data = data_raw['hdmap_data']
    # convert lines
    lines = data.get('lines') or []
    for line in lines:
        for seg in line.get('line_segments', []) or []:
            seg_pts = seg.get('points')
            if seg_pts is not None:
                seg['points'] = _transform_points_list(conv, seg_pts)

    # convert stop lines
    stop_lines = data.get('stop_lines') or []
    for stop_line in stop_lines:
        pts = stop_line.get('points')
        if pts is not None:
            stop_line['points'] = _transform_points_list(conv, pts)

    # convert road markers
    road_markers = data.get('road_markers') or []
    for road_marker in road_markers:
        if 'centroid' in road_marker:
            road_marker['centroid'] = _transform_point_single(conv, road_marker['centroid'])

    # convert crosswalks
    cross_walks = data.get('cross_walks') or []
    for cross_walk in cross_walks:
        b = cross_walk.get('boundary')
        if b is not None:
            cross_walk['boundary'] = _transform_points_list(conv, b)

    # convert lanes
    lanes = data.get('lanes') or []
    for lane in lanes:
        cp = lane.get('center_points')
        if cp is not None:
            lane['center_points'] = _transform_points_list(conv, cp)

    # convert lane links
    lane_links = data.get('lane_links') or []
    for lane_link in lane_links:
        rp = lane_link.get('ref_points')
        if rp is not None:
            lane_link['ref_points'] = _transform_points_list(conv, rp)

    # convert junctions
    junctions = data.get('junctions') or []
    for junction in junctions:
        b = junction.get('boundary')
        if b is not None:
            junction['boundary'] = _transform_points_list(conv, b)

    # write new file next to original with suffix
    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
    return


def _load_hdmap(json_path: str):
    if not os.path.exists(json_path):
        raise FileNotFoundError(json_path)
    with open(json_path, 'r', encoding='utf-8') as f:
        j = json.load(f)
    # If the file wraps hdmap_data, extract it, otherwise assume file itself is hdmap_data
    if isinstance(j, dict) and 'hdmap_data' in j:
        return j['hdmap_data']
    return j


def _as_map(items):
    """Normalize items (list or dict) to a mapping id->entry when possible.
    If items is a list and entries contain 'id', use that as key; otherwise use index strings."""
    if items is None:
        return {}
    if isinstance(items, dict):
        return items
    if isinstance(items, list):
        out = {}
        for i, it in enumerate(items):
            key = None
            if isinstance(it, dict):
                key = it.get('id')
            if key is None:
                key = str(i)
            out[str(key)] = it
        return out
    return {}


def compare_maps(gt_file: str, pred_file: str, tol: float = 1e-3):
    """Compare map elements between ground-truth JSON and ENU JSON.

    Returns (all_match: bool, report: dict)
    report contains per-element mismatch lists and summary counts.
    """
    gt = _load_hdmap(gt_file)
    pred = _load_hdmap(pred_file)

    all_ok = True

    gt_lines = _as_map(gt.get('lines') if isinstance(gt, dict) else None)
    pred_lines = _as_map(pred.get('lines') if isinstance(pred, dict) else None)

    d_x_max = 0.0
    d_y_max = 0.0
    for id, gt_line in gt_lines.items():
        pred_line = pred_lines.get(id)
        for seg_idx in range(len(gt_line.get('line_segments', []) or [])):
            gt_pts = gt_line['line_segments'][seg_idx]['points']
            pred_pts = pred_line['line_segments'][seg_idx]['points']
            for i in range(len(gt_pts)):
                gt_pt = gt_pts[i]
                pred_pt = pred_pts[i]
                dx = abs(gt_pt['x'] - pred_pt['x'])
                dy = abs(gt_pt['y'] - pred_pt['y'])
                if dx > d_x_max:
                    d_x_max = dx
                if dy > d_y_max:
                    d_y_max = dy
    print(f'Max line differences: d_x={d_x_max:.9f}, d_y={d_y_max:.9f}')

    return all_ok


if __name__ == '__main__':
    orig = '/mnt/cfs-baidu/public/huaqiang.dai/pilot_sclip_hdmap_loc_output_final/17H971250_20250325T20422317429066030001742906663000/lml_results/enu_origin.txt'
    jfile = '/mnt/cfs-baidu/public/huaqiang.dai/pilot_sclip_hdmap_loc_input/existed_map/matched-17H971250_20250325T20422317429066030001742906663000.json'

    gt_file = '/mnt/cfs-baidu/public/huaqiang.dai/pilot_sclip_hdmap_loc_output_final/17H971250_20250325T20422317429066030001742906663000/lml_results/hdmap.json'
    pred_file = '/mnt/cfs-baidu/public/huaqiang.dai/pilot_sclip_hdmap_loc_output_final/17H971250_20250325T20422317429066030001742906663000/lml_results/hdmap_enu.json'

    # out = coord_transform(orig, jfile)
    compare_maps(gt_file, pred_file)
    print('Wrote ENU map')