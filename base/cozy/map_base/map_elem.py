from .map_geom import MapGeometry
from .data_infra import map_dataclass
from dataclasses import field
from typing import List, Set, Dict, Tuple


@map_dataclass
class MapElemBase:
    id: str = ''
    geometry: MapGeometry = None


@map_dataclass
class LaneDivider(MapElemBase):
    lane_id: str = ''
    frame_id: str = ''


@map_dataclass
class RoadDivider(MapElemBase):
    lane_divider_id: str = ''


@map_dataclass
class LaneBoundary(MapElemBase):
    line_style: str = ''  # 实线、虚线等
    color: str = ''  # 线的颜色
    direction: str = ''  # 单向、双向等
    fishbone_type: str = ''  # 鱼骨线类型
    # 拓扑关联
    section_id: str = ''
    son_ids: List[str] = field(default_factory=list)
    son_offsets: List[float] = field(default_factory=list)
    next_ids: Set[str] = field(default_factory=set)
    in_node_seq: int = -1
    out_node_seq: int = -1
    road_id: str = ''
    road_segment_id: str = ''
    group_ids: Set[str] = field(default_factory=set)
    father_id: str = ''
    start_split_line_id: str = ''
    end_split_line_id: str = ''
    start_split_line_road_id: str = ''
    end_split_line_road_id: str = ''
    start_offset: float = 0.0
    end_offset: float = 0.0
    start_seq: int = -1
    end_seq: int = -1
    prev_ids: Set[str] = field(default_factory=set)
    key_lane_change_boundary: bool = False  # 是否为关键变道边界线
    key_start_point: bool = False  # 关键点是否为车道起点
    key_end_point: bool = False  # 关键点是否为车道终点
    inside_junction_ratio: float = 0.0  # 在路口内的比例
    inside_junction: bool = False  # 是否在路口内
    group_id: str = ''  # 分组ID，用于车道边界线的分组
    successor_group_ids: Set[str] = field(default_factory=set)  # 下一个分组
    predecessor_group_ids: Set[str] = field(default_factory=set)  # 上一个分组

    def attr_equal(self, other: 'LaneBoundary') -> bool:
        return (self.line_style == other.line_style and self.color == other.color
                and self.fishbone_type == other.fishbone_type)


@map_dataclass
class RoadKeyPoint(MapElemBase):
    road_divider_id: str = ''
    in_merge_landboundary_ids: Set[str] = field(default_factory=set)
    out_merge_landboundary_ids: Set[str] = field(default_factory=set)
    road_split_merge_laneboundary_ids: Set[str] = field(default_factory=set)
    in_road_merge_laneboundary_ids: Set[str] = field(default_factory=set)
    out_road_merge_laneboundary_ids: Set[str] = field(default_factory=set)

    in_direction: float = 0.0  # 进入方向角，度
    out_direction: float = 0.0  # 离开方向角，度
    mean_direction: float = 0.0  # 平均方向角，度
    key_direction: float = 0.0  # 关键点方向角，度

    start_point: bool = False
    end_point: bool = False
    intersection_start_point: bool = False
    intersection_end_point: bool = False
    lane_merge: bool = False
    lane_split: bool = False
    road_merge: bool = False
    road_split: bool = False
    type: str = ''
    kp_group_id: str = ''
    kp_group_seq: int = -1
    in_inside_junction: bool = False
    out_inside_junction: bool = False

    def generate_type(self):
        types = []
        if self.start_point:
            types.append('start')
        if self.end_point:
            types.append('end')
        if self.lane_merge:
            types.append('lane_merge')
        if self.lane_split:
            types.append('lane_split')
        if self.road_merge:
            types.append('road_merge')
        if self.road_split:
            types.append('road_split')
        self.type = '+'.join(types)

    def get_type_set(self) -> Set[str]:
        type_set = set()
        if self.start_point:
            type_set.add('start')
        if self.end_point:
            type_set.add('end')
        if self.lane_merge:
            type_set.add('lane_merge')
        if self.lane_split:
            type_set.add('lane_split')
        if self.road_merge:
            type_set.add('road_merge')
        if self.road_split:
            type_set.add('road_split')
        return type_set

    def inside_junction(self) -> bool:
        return self.in_inside_junction or self.out_inside_junction


@map_dataclass
class KeyPointLink(MapElemBase):
    distance: float = 0.0
    angle_diff: float = 0.0
    perpendicular_dist: float = 0.0
    two_polyline_contains_other_polylines: bool = False
    rb_id: str = ''
    rb_ratio: float = 0.0
    rb_length: float = 0.0
    intersect: bool = False
    intersect_other_line_id: str = ''
    intersect_other_line_start_pt_offset: float = 0.0
    intersect_other_line_end_pt_offset: float = 0.0


@map_dataclass
class KeyPointSplitMergeInfo(MapElemBase):
    detect_road_boundary_ids: Set[str] = field(default_factory=set)


@map_dataclass
class LaneBoundarySplitLine(MapElemBase):
    lane_boundary_id: str = ''
    core_point_ids: Set[str] = field(default_factory=set)
    key_point_ids: Set[str] = field(default_factory=set)
    type: str = ''
    directions: float = 0.0  # 分割线主方向角，度(每一个keypoint上的方向角的平均值)


@map_dataclass
class RoadBoundary(MapElemBase):
    type: str = ''  # 护栏、路缘石等
    # 拓扑关联
    section_id: str = ''
    side: str = ''  # left/right, 只针对虚拟的道路边界线
    father_id: str = ''
    z_list: List[float] = field(default_factory=list)  # 路缘石高度列表
    z_valid: bool = False  # z_list是否有效，只有当z_list长度和geometry.points长度一致时才有效


@map_dataclass
class Lane(MapElemBase):
    lane_num: int = 0  # 总车道数
    lane_seq: int = 0  # 从左到右，1, 2, 3...
    lane_width: float = 0.0
    length: float = 0.0
    section_id: str = ''
    road_segment_id: str = ''
    road_id: str = ''
    prev_ids: Set[str] = field(default_factory=set)
    next_ids: Set[str] = field(default_factory=set)
    left_boundary_id: str = ''
    right_boundary_id: str = ''
    match_id: str = ''  # 用于和预测结果匹配

    start_point_error: float = 0.0
    end_point_error: float = 0.0
    mean_error: float = 0.0
    max_error: float = 0.0
    median_error: float = 0.0

    start_offset: float = 0.0
    end_offset: float = 0.0
    start_split_line_id: str = ''
    end_split_line_id: str = ''
    front_link_type: str = ''
    sub_lane_ids: List[str] = field(default_factory=list)  # 合并的lane的id集合
    rightest: bool = False  # 最右侧车道
    leftest: bool = False  # 最左侧车道

    lane_info_ids: List[str] = field(default_factory=list)  # 车道信息标签集合
    lane_info_types: List[str] = field(default_factory=list)  # 车道信息标签集合
    lane_info_offsets: List[float] = field(default_factory=list)  # 车道信息标签集合

    val_matched: bool = True  # 验证时是否被匹配上
    boundary_split: bool = False  # 车道边界线是否被分割
    boundary_merge: bool = False  # 车道边界线是否被合并

    key_start_point: bool = False  # 关键点是否为车道起点
    key_end_point: bool = False  # 关键点是否为车道终点

    # 新版数据结构
    lane_type: str = ''  # 车道类型
    lane_class: str = ''  # 车道等级
    turn_type: Set[str] = field(default_factory=set)  # 转向类型
    section_id: str = ''
    left_lane_id: str = ''
    right_lane_id: str = ''
    left_boundary_ids: List[str] = field(default_factory=list)  # 左侧边界ID
    right_boundary_ids: List[str] = field(default_factory=list)  # 右侧边界ID
    left_reverse_ids: List[str] = field(default_factory=list)  # 左侧反向车道ID
    right_reverse_ids: List[str] = field(default_factory=list)  # 右侧反向车道ID
    # 推理出的属性
    obj_ids_: Set[str] = field(default_factory=set)
    with_traffic_light_: bool = False
    junction_id_: str = ''
    in_traffic_light_junctions_: bool = False
    enter_junction_id_: str = ''
    exit_junction_id_: str = ''
    merged_section_id_: str = ''
    merged_lane_id_: str = ''

    inside_junction_ratio: float = 0.0  # 在路口内的比例
    inside_junction: bool = False  # 是否在路口内

    def generate_type_type(self):
        self.turn_type = set(self.lane_info_types)


@map_dataclass
class Section(MapElemBase):
    lane_ids: Set[str] = field(default_factory=set)
    prev_ids: Set[str] = field(default_factory=set)
    next_ids: Set[str] = field(default_factory=set)
    left_ids: Set[str] = field(default_factory=set)
    right_ids: Set[str] = field(default_factory=set)
    left_reverse_ids: Set[str] = field(default_factory=set)
    right_reverse_ids: Set[str] = field(default_factory=set)
    main_path: bool = False
    stub_path: bool = False
    left_opposite_section_ids: Set[str] = field(default_factory=set)
    right_opposite_section_ids: Set[str] = field(default_factory=set)
    # 以下属性从车道信息推断
    obj_ids_: Set[str] = field(default_factory=set)
    with_traffic_light_: bool = False
    junction_id_: str = ''
    in_traffic_light_junctions_: bool = False
    enter_junction_id_: str = ''
    exit_junction_id_: str = ''
    lane_num_: int = 0
    # 以下属性从道路信息推断
    road_id_: str = ''
    non_stub_btw_flj_: bool = False  # 是否是连接红绿灯路口间的非分叉道路
    lane_change_: bool = False  # 道路内是否存在车道变更
    section_ids_: List[str] = field(default_factory=list)
    lane_change_st_section_ids_: List[str] = field(default_factory=list)
    # 从merge_section推断
    merged_section_id_: str = ''


@map_dataclass
class MergedSection(MapElemBase):
    section_ids_: List[str] = field(default_factory=list)
    obj_ids_: Set[str] = field(default_factory=set)
    with_traffic_light_: bool = False
    junction_id_: str = ''
    in_traffic_light_junctions_: bool = False
    enter_junction_id_: str = ''
    exit_junction_id_: str = ''
    lane_num_: int = 0
    road_id_: str = ''
    non_stub_btw_flj_: bool = False  # 是否是连接红绿灯路口间的非分叉道路
    lane_change_: bool = False  # 道路内是否存在车道变更
    section_ids_: List[str] = field(default_factory=list)
    prev_ids_: Set[str] = field(default_factory=set)  # 下一个merge_section的id
    next_ids_: Set[str] = field(default_factory=set)  # 上一个merge_section的id


@map_dataclass
class MergedLane(MapElemBase):
    next_ids_: Set[str] = field(default_factory=set)
    prev_ids_: Set[str] = field(default_factory=set)
    lane_num: int = 0  # 总车道数
    lane_seq: int = 0  # 从左到右，1, 2, 3...
    lane_ids_: List[str] = field(default_factory=list)  # debug
    section_ids_: List[str] = field(default_factory=list)  # debug
    merged_section_id: str = ''
    road_id: str = ''


@map_dataclass
class Trajectory(MapElemBase):
    index: int = 1
    road_id: str = ''


@map_dataclass
class Road(MapElemBase):
    section_ids_: List[str] = field(default_factory=list)
    inside_tlj_: bool = False  # 是否在红绿灯路口内
    non_stub_btw_flj_: bool = False  # 是否是连接红绿灯路口间的非分叉道路
    lane_change_: bool = False  # 道路内是否存在车道变更
    lane_change_st_section_ids_: List[List[str]] = field(default_factory=list)  # 道路内的合并车道组
    merged_section_ids_: List[str] = field(default_factory=list)  # 合并段ID
    merged_lane_ids_: Set[str] = field(default_factory=set)  # 合并车道ID
    lane_ids: List[str] = field(default_factory=list)  # 道路包含的车道ID
    next_ids: Set[str] = field(default_factory=set)
    prev_ids: Set[str] = field(default_factory=set)

    road_width: float = 0.0  # 道路宽度（车道宽度之和）
    junction_ids: List[str] = field(default_factory=list)
    enter_junctions: List[bool] = field(default_factory=list)  # 是否为进入路口
    motor_lane: bool = False  # 是否为机动车道

    no_right_turn: bool = False  # 是否为禁止右转道路
    no_left_turn: bool = False  # 是否为禁止左转道路
    no_straight: bool = False  # 是否为禁止直行道路
    no_u_turn: bool = False  # 是否为禁止掉头道路
    turn_types: Set[str] = field(default_factory=set)  # 道路转向类型集合

    start_split_line_id: str = ''
    end_split_line_id: str = ''
    lane_num: int = 0  # 车道数
    inside_junction: bool = False  # 是否在路口内
    road_id: str = ''
    link_type: str = ''  # 直行，左转，右转，掉头等


@map_dataclass
class RoadObject(MapElemBase):
    obj_type: str = ''
    lane_ids: Set[str] = field(default_factory=set)


@map_dataclass
class Junction(MapElemBase):
    lane_ids: Set[str] = field(default_factory=set)
    # 推理出的属性
    enter_lane_ids_: Set[str] = field(default_factory=set)  # 进入路口车道
    exit_lane_ids_: Set[str] = field(default_factory=set)  # 离开路口车道
    with_traffic_light_: bool = False


@map_dataclass
class JunctionVCenterline(MapElemBase):
    junction_id: str = ''
    enter_lane_id: str = ''
    exit_lane_id: str = ''
    enter_section_id: str = ''
    exit_section_id: str = ''
    control_pts_: List[Tuple[float, float]] = field(default_factory=list)
    alpha_: float = 0.0
    beta_: float = 0.0


@map_dataclass
class MiddlewareMap:
    lane_dividers: Dict[str, LaneDivider] = field(default_factory=dict)
    road_dividers: Dict[str, RoadDivider] = field(default_factory=dict)
    junctions: Dict[str, Junction] = field(default_factory=dict)
    road_objects: Dict[str, RoadObject] = field(default_factory=dict)
    lanes: Dict[str, Lane] = field(default_factory=dict)
    sections: Dict[str, Section] = field(default_factory=dict)
    roads: Dict[str, Road] = field(default_factory=dict)  # 以道路分叉打断，推理出的道路
    merge_sections: Dict[str, MergedSection] = field(default_factory=dict)  # 以车道数变化，推理出的合并车道组
    merge_lanes: Dict[str, MergedLane] = field(default_factory=dict)  # 以车道数变化，推理出的合并车道
    trajectories: Dict[str, Trajectory] = field(default_factory=dict)  # 轨迹
    junction_vcls: Dict[str, JunctionVCenterline] = field(default_factory=dict)  # 路口连接线


@map_dataclass
class StopLine(MapElemBase):
    stop_line_type: str = ''  # 停止线类型: # 停止线，停车让行线，减速让行线, 待转停止线


@map_dataclass
class RoadMarker(MapElemBase):
    marker_type: str = ''  # 路面标识类型: 箭头，减速带，斑马线等


@map_dataclass
class CrossWalk(MapElemBase):
    crosswalk_type: str = ''  # 人行横道类型: 普通斑马线，带交通灯斑马线等


@map_dataclass
class IgnoreArea(MapElemBase):
    ignore_type: str = ''  # 忽略区域类型: 施工区，禁行区等


@map_dataclass
class SceneType(MapElemBase):
    scene_type: str = ''  # 场景类型


if __name__ == "__main__":
    print("MapElemBase test")
