from .map_geom import MapPoint, MapGeometry, MapGeometryType, MapCRS
from .map_elem import MapElemBase
from dataclasses import asdict
from typing import Type, List
import json
import os


def _geom_to_geojson(geom: MapGeometry) -> dict:
    if geom is None:
        return None
    if geom.type == MapGeometryType.POINT:
        coords = [[p.x, p.y, p.z] if p.z != 0 else [p.x, p.y] for p in geom.points]
        return {"type": "Point", "coordinates": coords[0] if coords else [0, 0]}
    if geom.type == MapGeometryType.LINESTRING:
        coords = [[p.x, p.y, p.z] if p.z != 0 else [p.x, p.y] for p in geom.points]
        return {"type": "LineString", "coordinates": coords}
    if geom.type == MapGeometryType.POLYGON:
        coords = [[p.x, p.y, p.z] if p.z != 0 else [p.x, p.y] for p in geom.points]
        return {"type": "Polygon", "coordinates": [coords]}
    raise ValueError("Unsupported geometry type")


def _geom_from_coords(coords, geom_type: MapGeometryType, crs: MapCRS = MapCRS.WGS84) -> MapGeometry:
    if coords is None:
        return None
    if geom_type == MapGeometryType.POINT:
        if isinstance(coords, (list, tuple)):
            x = float(coords[0])
            y = float(coords[1])
            z = float(coords[2]) if len(coords) > 2 else 0.0
            return MapGeometry(points=[MapPoint(x, y, z)], type=geom_type, crs=crs)
    if geom_type == MapGeometryType.LINESTRING:
        pts = [MapPoint(float(c[0]), float(c[1]), float(c[2]) if len(c) > 2 else 0.0) for c in coords]
        return MapGeometry(points=pts, type=geom_type, crs=crs)
    if geom_type == MapGeometryType.POLYGON:
        # outer ring
        outer = coords[0] if coords and isinstance(coords[0][0], (list, tuple)) else coords
        pts = [MapPoint(float(c[0]), float(c[1]), float(c[2]) if len(c) > 2 else 0.0) for c in outer]
        return MapGeometry(points=pts, type=geom_type, crs=crs)
    raise ValueError("Unsupported geometry type")


def to_geojson(elem: MapElemBase) -> dict:
    """Convert a MapElem dataclass instance to a GeoJSON Feature dict using its `to_json()`.

    The dataclass_json `to_json()` is used to get a JSON string of the element; we parse it
    and move `geometry` into the standard GeoJSON `geometry` member. `properties` is the
    remaining fields (excluding `id` and `geometry`).
    """
    if elem is None:
        return None

    # get dict from dataclass_json; use asdict if to_json not present
    try:
        js = elem.to_json()
        data = json.loads(js)
    except Exception:
        data = asdict(elem)

    geom = data.pop("geometry", None)
    gid = data.pop("id", None)

    geometry = None
    if geom is not None:
        # if geometry is already a dict-like MapGeometry (from asdict) it will be nested
        if isinstance(geom, dict) and "type" in geom and "coordinates" in geom:
            # already geojson-like
            geometry = geom
        elif isinstance(geom, dict):
            # map_geom serialization: points/list, possibly includes 'type' and 'crs'
            try:
                raw_type = geom.get("type")
                if raw_type in MapGeometryType._value2member_map_:
                    mg_type = MapGeometryType(raw_type)
                elif raw_type in MapGeometryType.__members__:
                    mg_type = MapGeometryType[raw_type]
                else:
                    mg_type = MapGeometryType.LINESTRING
            except Exception:
                mg_type = MapGeometryType.LINESTRING
            pts = geom.get("points") or geom.get("pts") or []
            # detect crs if serialized
            crs_val = geom.get("crs")
            crs = MapCRS.WGS84
            if crs_val:
                try:
                    if '::' in crs_val:
                        crs = MapCRS(crs_val)
                    else:
                        alt = crs_val.replace(':', '::')
                        crs = MapCRS(alt) if alt in MapCRS._value2member_map_ else MapCRS(crs_val)
                except Exception:
                    crs = MapCRS.WGS84
            mg = MapGeometry(points=[
                MapPoint(p["x"], p["y"], p.get("z", 0.0)) if isinstance(p, dict) else MapPoint(
                    p[0], p[1], p[2] if len(p) > 2 else 0.0) for p in pts
            ],
                             type=mg_type,
                             crs=crs)
            geometry = _geom_to_geojson(mg)
            # include crs in properties if present
            if hasattr(mg, 'crs') and mg.crs is not None:
                data['geometry_crs'] = mg.crs.value
        elif isinstance(geom, MapGeometry):
            geometry = _geom_to_geojson(geom)

    # properties: convert sets to lists for json compatibility
    props = {}
    for k, v in data.items():
        if isinstance(v, set):
            props[k] = list(v)
        else:
            props[k] = v

    feat = {"type": "Feature", "properties": props, "geometry": geometry}
    if gid is not None:
        feat["id"] = gid
    return feat


def from_geojson(feature: dict, cls: Type[MapElemBase]) -> MapElemBase:
    """Create an instance of `cls` from a GeoJSON Feature dict using dataclass_json `schema()` or `from_json`.

    We take `properties` as the dataclass fields, insert `id` if present, and convert `geometry`
    into the dataclass `geometry` value (MapGeometry). Auto-converts list to set based on cls type hints.
    """
    if feature is None:
        return None
    if feature.get("type") != "Feature":
        raise ValueError("feature must be a GeoJSON Feature")

    props = feature.get("properties", {}) or {}
    fid = feature.get("id", None)
    geom = feature.get("geometry", None)

    # convert geometry
    mg = None
    if geom is not None:
        gtype = MapGeometryType(geom["type"]) if "type" in geom else MapGeometryType.LINESTRING
        coords = geom.get("coordinates")
        # check for crs in properties
        crs_val = props.pop('geometry_crs', None)
        crs = MapCRS.WGS84
        if crs_val:
            try:
                # accept both 'EPSG::4326' and 'EPSG:4326'
                if '::' in crs_val:
                    crs = MapCRS(crs_val)
                else:
                    # try replace ':' with '::' for matching enum
                    alt = crs_val.replace(':', '::')
                    crs = MapCRS(alt) if alt in MapCRS._value2member_map_ else MapCRS(crs_val)
            except Exception:
                crs = MapCRS.WGS84
        mg = _geom_from_coords(coords, gtype, crs=crs)

    # assemble dict for dataclass
    data = dict(props)
    if fid is not None:
        data["id"] = fid
    if mg is not None:
        data["geometry"] = mg

    # auto-convert list to set based on cls type hints
    try:
        from typing import get_type_hints, get_origin

        type_hints = get_type_hints(cls)
        for field_name, field_type in type_hints.items():
            if field_name not in data:
                continue

            # Check if field type is Set[T]
            origin = get_origin(field_type)
            if origin is set:
                if isinstance(data[field_name], list):
                    data[field_name] = set(data[field_name])
    except Exception:
        # If type hint inspection fails, just continue
        pass

    # try to use dataclass_json's from_json if available on cls
    try:
        # cls is a dataclass with dataclass_json; use schema/load when possible
        if hasattr(cls, "schema"):
            # schema().load expects dict
            return cls.schema().load(data)
        if hasattr(cls, "schema"):
            return cls.schema().load(data)
    except Exception:
        pass

    # fallback: construct directly
    return cls(**data)


def write_elem_to_geojson(elems: List[MapElemBase], output_path: str):
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    features = [to_geojson(feat) for feat in elems]
    crs = elems[0].geometry.crs.value if elems and elems[0].geometry and elems[0].geometry.crs else ""  # wgs84 default
    with open(output_path, "w") as f:
        json.dump(
            {
                "type": "FeatureCollection",
                "features": features,
                "crs": {
                    "type": "name",
                    "properties": {
                        "name": crs
                    }
                }
            },
            f,
            indent=2)


def read_elem_from_geojson(input_path: str, cls: Type[MapElemBase]) -> List[MapElemBase]:
    with open(input_path, "r") as f:
        gj = json.load(f)
    if gj.get("type") != "FeatureCollection" or "features" not in gj:
        raise ValueError("Invalid GeoJSON FeatureCollection")
    features = gj["features"]
    elems = [from_geojson(feat, cls) for feat in features]
    return elems


def png_to_geotiff(png_path, geotiff_path, x0=0, y0=0, pixel_size=1.0, is_y_down=True, xy_change=True, crs=None):
    # 读取 PNG
    import cv2
    import numpy as np
    import rasterio
    from rasterio.transform import from_origin, Affine

    img = cv2.imread(png_path, cv2.IMREAD_UNCHANGED)
    if img.ndim == 2:
        img = img[np.newaxis, ...]  # 单通道
    else:
        img = img.transpose(2, 0, 1)  # 多通道

    if xy_change:
        img = img.transpose(0, 2, 1)  # 交换 x,y 轴

    if is_y_down:
        img = img[:, ::-1, :]  # 上下翻转
        x0, y0 = x0, y0 + img.shape[1] * pixel_size  # 调整左上角坐标

    # 构造仿射变换（左上角为 (x0, y0)，像素大小为 pixel_size）
    transform = from_origin(x0, y0, pixel_size, pixel_size)
    # transform = Affine(pixel_size, 0, x0, 0, pixel_size, y0)
    # 写入 GeoTIFF
    with rasterio.open(
            geotiff_path,
            "w",
            driver="GTiff",
            height=img.shape[1],
            width=img.shape[2],
            count=img.shape[0],
            dtype=img.dtype,
            transform=transform,
            crs=crs  # 可根据实际坐标系修改
    ) as dst:
        dst.write(img)

    # print(f"GeoTIFF saved to {geotiff_path}")
