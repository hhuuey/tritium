from typing import Dict, Set, Any, Optional
from dataclasses import dataclass, field


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
