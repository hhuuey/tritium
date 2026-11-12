from rtree import index
from typing import List, Callable, Dict
from dataclasses import field, dataclass
from .map_elem import MapGeometry, MapElemBase


def geom_bbox_from_geom(geom: MapGeometry, buffer_size: float = 1.0) -> tuple:
    """Return bbox (minx, miny, maxx, maxy) for a MapGeometry LineString"""
    pts = geom.points
    xs = [p.x for p in pts]
    ys = [p.y for p in pts]
    return (min(xs) - buffer_size / 2.0, min(ys) - buffer_size / 2.0, max(xs) + buffer_size / 2.0,
            max(ys) + buffer_size / 2.0)


@dataclass
class ElemsToIndex:
    elems: List = field(default_factory=list)
    get_bbox_func: Callable = None  # Function to get bounding box from an element


class RTreeWrapper:

    def __init__(self, elems_raw: Dict[str, MapElemBase], buffer_size: float = 1.0):
        elems_to_index = ElemsToIndex(
            elems=list(elems_raw.values()),
            get_bbox_func=lambda lb: geom_bbox_from_geom(lb.geometry, buffer_size=2 * buffer_size))
        self.idx = index.Index()
        # internal id counter for rtree entries
        self._next_id = 0
        # maps rtree id -> element
        self._rtree_id_to_elem = {}
        # maps python id(element) -> rtree id
        self._elem_id_to_rtree_id = {}
        # maps rtree id -> bbox used when inserting (needed for delete)
        self._rtree_id_to_bbox = {}
        # function to compute bbox for an element
        self._get_bbox = elems_to_index.get_bbox_func

        # insert initial elements
        for elem in elems_to_index.elems:
            self.insert(elem)

    def _assign_id(self):
        rid = self._next_id
        self._next_id += 1
        return rid

    def insert(self, elem):
        """Insert an element into the R-tree. If the element already exists, it
        will be removed first and re-inserted (so the bbox is updated).
        Returns the assigned rtree id.
        """
        bbox = self._get_bbox(elem)
        existing = self._elem_id_to_rtree_id.get(id(elem))
        if existing is not None:
            # remove old entry first so we don't have duplicates
            self.remove_by_id(existing)

        rid = self._assign_id()
        self.idx.insert(rid, bbox, obj=elem)
        self._rtree_id_to_elem[rid] = elem
        self._elem_id_to_rtree_id[id(elem)] = rid
        self._rtree_id_to_bbox[rid] = bbox
        return rid

    def remove_by_obj(self, elem):
        """Remove an element from the R-tree by object reference. Returns True if removed."""
        rid = self._elem_id_to_rtree_id.get(id(elem))
        if rid is None:
            return False
        return self.remove_by_id(rid)

    def remove_by_id(self, rid):
        """Remove an element from the R-tree by its rtree id. Returns True if removed."""
        bbox = self._rtree_id_to_bbox.get(rid)
        if bbox is None:
            return False
        try:
            # rtree requires the same bbox used at insert to delete
            self.idx.delete(rid, bbox)
        except Exception:
            # if deletion fails for any reason, continue to clean internal maps
            pass

        elem = self._rtree_id_to_elem.pop(rid, None)
        if elem is not None:
            self._elem_id_to_rtree_id.pop(id(elem), None)
        self._rtree_id_to_bbox.pop(rid, None)
        return True

    def query_idx(self, bbox):
        return list(self.idx.intersection(bbox))

    def query_objects(self, bbox):
        return [obj.object for obj in self.idx.intersection(bbox, objects=True)]

    def query_obj(self, geom: MapGeometry, buffer_size: float = 1.0):
        """Query R-tree for elements whose bounding boxes intersect with the given geometry."""
        bbox = geom_bbox_from_geom(geom, buffer_size=buffer_size)
        return self.query_objects(bbox)


if __name__ == "__main__":
    print("Testing RTreeWrapper...")
