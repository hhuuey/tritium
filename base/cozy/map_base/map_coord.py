from enum import Enum
import numpy as np
from pyproj import Transformer


class MapCRS(Enum):
    WGS84 = "EPSG::4326"
    WEBMERCATOR = "EPSG::3857"
    UTM_46N = "EPSG::32646"
    UTM_47N = "EPSG::32647"
    UTM_48N = "EPSG::32648"
    UTM_49N = "EPSG::32649"
    UTM_50N = "EPSG::32650"
    UTM_51N = "EPSG::32651"
    UTM_52N = "EPSG::32652"
    UTM_53N = "EPSG::32653"
    UTM_54N = "EPSG::32654"
    UTM_55N = "EPSG::32655"
    UTM_56N = "EPSG::32656"


def coord_transform(points: np.ndarray, source_crs: MapCRS, target_crs: MapCRS) -> np.ndarray:
    """Transform points using numpy arrays.

    - `points`: numpy array of shape (N,2) or (2,) for single point.
    - returns: numpy array of shape (N,2) (float64).
    """
    pts = np.asarray(points, dtype=float)
    if source_crs == target_crs:
        # ensure 2D shape
        if pts.ndim == 1:
            return pts.reshape(1, 2)
        return pts.copy()

    # normalize EPSG strings (handle `EPSG::4326`)
    src_epsg = source_crs.value.replace('::', ':') if '::' in source_crs.value else source_crs.value
    tgt_epsg = target_crs.value.replace('::', ':') if '::' in target_crs.value else target_crs.value

    transformer = Transformer.from_crs(src_epsg, tgt_epsg, always_xy=True)

    if pts.size == 0:
        return np.empty((0, 2), dtype=float)

    if pts.ndim == 1 and pts.shape[0] == 2:
        x, y = transformer.transform(float(pts[0]), float(pts[1]))
        return np.array([[float(x), float(y)]], dtype=float)

    xs, ys = transformer.transform(pts[:, 0], pts[:, 1])
    out = np.vstack((np.array(xs, dtype=float), np.array(ys, dtype=float))).T
    return out
