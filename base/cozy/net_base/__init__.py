from .bbox import BBox3D, BBox2D, bbox_inside_img
from .obj import NObj, NObj2D, NObj3D, ObjCategory, Attribution
from .io import load_json_line
from .associate import undirected_iou_match, bidirect_iou_match, AssociationResult, AssociationResults
from .det_base import DetCategory, DetRecord, Det2DRecord, Det3DRecord, add_attribute, Det2dManager
from .det_match import undirected_match, bidirect_match, MatchedResult, MatchedResults, OneToOneRM, Edge, OneToManyRM

from typing import Dict, TypeAlias

Det2DRecords: TypeAlias = Dict[str, Det2DRecord]
Det3DRecords: TypeAlias = Dict[str, Det3DRecord]

__all__ = [
    'BBox3D', 'BBox2D', 'NObj', 'NObj2D', 'NObj3D', 'ObjCategory', 'Attribution', 'load_json_line',
    'undirected_iou_match', 'bidirect_iou_match', 'AssociationResult', 'AssociationResults', 'DetCategory', 'DetRecord',
    'Det2DRecord', 'Det3DRecord', 'Det2DRecords', 'Det3DRecords', 'add_attribute', 'undirected_match', 'bidirect_match',
    'MatchedResult', 'MatchedResults', 'bbox_inside_img', 'Det2dManager', 'OneToOneRM', 'Edge', 'OneToManyRM'
]
