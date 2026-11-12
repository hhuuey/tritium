from .obj import NObj2D
from .bbox import BBox2D
from typing import Dict, Set
from dataclasses import dataclass, field


@dataclass
class AssociationResult:
    match_id: str = ''
    match_iou: float = 0.0

    def __hash__(self):
        # 使用 match_id 和 match_iou 的组合来生成哈希值
        return hash(self.match_id)

    def __eq__(self, other):
        # 比较两个 AssociationResult 实例是否相等
        if not isinstance(other, AssociationResult):
            return NotImplemented
        return self.match_id == other.match_id


@dataclass
class AssociationResults:
    associations: Dict[str, AssociationResult] = field(default_factory=dict)
    unmatched_ids: Set[str] = field(default_factory=set)


# 对每个bbox1, 在bbox2中找到iou最大
def undirected_iou_match(bbox1_dict: Dict[str, NObj2D], bbox2_dict: Dict[str, NObj2D], iou_threshold=0.3):
    associations = AssociationResults()
    for id1, bbox1 in bbox1_dict.items():
        max_iou = 0
        best_match_idx = -1
        for id2, bbox2 in bbox2_dict.items():
            iou = BBox2D.iou(bbox1.bbox, bbox2.bbox)
            if iou > max_iou:
                max_iou = iou
                best_match_idx = id2
        if best_match_idx != -1 and max_iou > iou_threshold:
            associations.associations[id1] = AssociationResult(match_id=best_match_idx, match_iou=max_iou)
        else:
            associations.unmatched_ids.add(id1)
    return associations


def bidirect_iou_match(bbox1_dict: Dict[str, NObj2D], bbox2_dict: Dict[str, NObj2D], iou_threshold=0.3):
    # bbox1 -> bbox2
    assoc_1to2 = undirected_iou_match(bbox1_dict, bbox2_dict, iou_threshold)
    # bbox2 -> bbox1
    assoc_2to1 = undirected_iou_match(bbox2_dict, bbox1_dict, iou_threshold)

    final_assoc_1to2 = AssociationResults()
    for id1, assoc in assoc_1to2.associations.items():
        id2 = assoc.match_id
        if id2 in assoc_2to1.associations and assoc_2to1.associations[id2].match_id == id1:
            final_assoc_1to2.associations[id1] = assoc
        else:
            final_assoc_1to2.unmatched_ids.add(id1)

    # unmatched
    for id1 in assoc_1to2.unmatched_ids:
        final_assoc_1to2.unmatched_ids.add(id1)
    for id2 in assoc_2to1.unmatched_ids:
        final_assoc_1to2.unmatched_ids.add(id2)
    return final_assoc_1to2
