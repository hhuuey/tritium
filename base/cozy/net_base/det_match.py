from .bbox import BBox2D
from typing import Dict, Set, Any, Optional
from dataclasses import dataclass, field


@dataclass
class MatchedResult:
    match_id: str = ''
    match_iou: float = 0.0

    def __hash__(self):
        # 使用 match_id 和 match_iou 的组合来生成哈希值
        return hash(self.match_id)

    def __eq__(self, other):
        # 比较两个 MatchedResult 实例是否相等
        if not isinstance(other, MatchedResult):
            return NotImplemented
        return self.match_id == other.match_id


@dataclass
class MatchedResults:
    associations: Dict[str, MatchedResult] = field(default_factory=dict)
    unmatched_ids: Set[str] = field(default_factory=set)


# 对每个bbox1, 在bbox2中找到iou最大
def undirected_match(bbox1_dict: Dict[str, BBox2D], bbox2_dict: Dict[str, BBox2D], iou_threshold=0.3):
    associations = MatchedResults()
    for id1, bbox1 in bbox1_dict.items():
        max_iou = 0
        best_match_idx = -1
        for id2, bbox2 in bbox2_dict.items():
            iou = BBox2D.iou(bbox1, bbox2)
            if iou > max_iou:
                max_iou = iou
                best_match_idx = id2
        if best_match_idx != -1 and max_iou > iou_threshold:
            associations.associations[id1] = MatchedResult(match_id=best_match_idx, match_iou=max_iou)
        else:
            associations.unmatched_ids.add(id1)
    return associations


def bidirect_match(bbox1_dict: Dict[str, BBox2D], bbox2_dict: Dict[str, BBox2D], iou_threshold=0.3):
    # bbox1 -> bbox2
    assoc_1to2 = undirected_match(bbox1_dict, bbox2_dict, iou_threshold)
    # bbox2 -> bbox1
    assoc_2to1 = undirected_match(bbox2_dict, bbox1_dict, iou_threshold)

    final_assoc_1to2 = MatchedResults()
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


@dataclass
class Edge:
    edge_id: str
    node_id_1: str
    node_id_2: str
    value: Optional[Any] = None

    @staticmethod
    def from_values(node_id_1, node_id_2, value=None):
        return Edge(
            edge_id=f"{node_id_1}_{node_id_2}",
            node_id_1=node_id_1,
            node_id_2=node_id_2,
            value=value,
        )


@dataclass
class OneToOneRM:
    all_edges: Dict[str, Edge] = field(default_factory=dict)
    a_2_b: Dict[str, str] = field(default_factory=dict)
    b_2_a: Dict[str, str] = field(default_factory=dict)

    def attach(self, source_id: str, target_id: str, value: Any = None):
        edge = Edge.from_values(source_id, target_id, value)
        self.all_edges[edge.edge_id] = edge

        self.a_2_b[source_id] = edge.edge_id
        self.b_2_a[target_id] = edge.edge_id

    def get(self, source_id: str) -> Optional[str]:
        edge_id = self.a_2_b.get(source_id, None)
        if edge_id is None:
            return None
        return self.all_edges[edge_id].node_id_2

    def get_inv(self, target_id: str) -> Optional[str]:
        edge_id = self.b_2_a.get(target_id, None)
        if edge_id is None:
            return None
        return self.all_edges[edge_id].node_id_1


@dataclass
class OneToManyRM:
    all_edges: Dict[Any, Edge] = field(default_factory=dict)
    a_2_bs: Dict[Any, Set[Any]] = field(default_factory=dict)
    b_2_a: Dict[Any, Any] = field(default_factory=dict)

    def attach(self, a_id: Any, b_id: Any, value: Any = None):
        # 如果 b_id 已经关联了一个 a_id，先解除关联
        if b_id in self.b_2_a:
            old_a_id = self.b_2_a[b_id]
            self.a_2_bs[old_a_id].remove(b_id)
            if len(self.a_2_bs[old_a_id]) == 0:
                del self.a_2_bs[old_a_id]
            del self.b_2_a[b_id]

        # 创建新的边并建立关联
        edge = Edge.from_values(a_id, b_id, value)
        self.all_edges[edge.edge_id] = edge

        if a_id not in self.a_2_bs:
            self.a_2_bs[a_id] = set()
        self.a_2_bs[a_id].add(b_id)
        self.b_2_a[b_id] = a_id

    def get(self, a_id: Any) -> Set[Any]:
        return self.a_2_bs.get(a_id, set())

    def get_inv(self, b_id: Any) -> Optional[Any]:
        return self.b_2_a.get(b_id, None)
