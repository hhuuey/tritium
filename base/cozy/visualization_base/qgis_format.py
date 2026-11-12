from qgis.core import QgsProject, QgsLayerTreeGroup
from qgis.core import QgsMapLayer, QgsVectorLayer, QgsLayerTreeLayer, QgsCategorizedSymbolRenderer, QgsSymbol
from qgis.core import QgsRasterLayer, QgsSingleBandPseudoColorRenderer, QgsRasterShader, QgsColorRampShader, QgsRasterBandStats
from qgis.core import QgsRendererCategory, QgsMarkerLineSymbolLayer, QgsLineSymbol, QgsMarkerSymbol, QgsWkbTypes
from qgis.PyQt.QtCore import QVariant
from qgis.PyQt.QtGui import QColor

import os


def move_layer_to_group(layer, group_names: list[str]) -> bool:
    """
    将已添加到项目的 layer 移动到嵌套分组 group_names 指定的路径下（若不存在则逐级创建）。
    group_names:
      - None/[]: 将图层放到 root
      - "GroupA" 或 ["GroupA"]: 在 root 下找/创建 GroupA
      - ["Parent","Child"]: 在 root 下找/创建 Parent，再在 Parent 下找/创建 Child，然后把图层放到 Child 下
    返回 True 表示成功，False 表示失败。
    """
    project = QgsProject.instance()
    root = project.layerTreeRoot()

    # 确保 layer 已加入项目（但不一定出现在图层树中）
    node = root.findLayer(layer.id())
    if node is None:
        project.addMapLayer(layer, False)
        node = root.findLayer(layer.id())
        if node is None:
            print(f"无法在项目中找到图层 {layer.name()}！")
            return False

    print(f"移动图层 {layer.name()} 到分组 {group_names} 下")
    # 规范化 group_names 为列表
    if not group_names:
        names = []
    elif isinstance(group_names, str):
        names = [group_names]
    else:
        try:
            names = list(group_names)
        except Exception:
            names = [str(group_names)]

    # 找到或创建目标（可能为 root）
    current_group = root
    for idx, gname in enumerate(names):
        # 在 current_group 下查找子组
        if current_group is root:
            found = root.findGroup(gname)
        else:
            found = current_group.findGroup(gname)
        if found is None:
            current_group = current_group.addGroup(gname)
        else:
            current_group = found

    target_group = current_group  # 若 names 为空则为 root

    # 如果已经在目标 group 下，则无需移动
    node = root.findLayer(layer.id())
    if node is not None and node.parent() == target_group:
        print(f"图层 {layer.name()} 已在目标分组下，无需移动")
        return True
    print(f"将图层 {layer.name()} 移动到分组 {target_group.name() if target_group != root else 'root'} 下")

    # 在目标 group 下创建新的节点（安全）
    if target_group is root:
        root.addLayer(layer)
    else:
        target_group.addLayer(layer)

    # 若节点在其它父节点下，先从父节点移除（removeChildNode 会删除该 node 对象）
    if node is not None:
        parent = node.parent()
        if parent is not None:
            parent.removeChildNode(node)
    return True


def load_tif_white_to_black(tif_path: str, layer_name: str):
    layer = QgsRasterLayer(tif_path, layer_name)
    if not layer.isValid():
        raise RuntimeError(f"加载失败: {tif_path}")

    # 获取波段统计（波段索引从1开始）
    stats = layer.dataProvider().bandStatistics(1, QgsRasterBandStats.All)
    vmin = stats.minimumValue if stats is not None else None
    vmax = stats.maximumValue if stats is not None else None

    # 保护：若 min/max 不可用，使用 0/1 作为占位
    if vmin is None or vmax is None or vmin == vmax:
        vmin, vmax = 0.0, 1.0

    # 构造颜色渐变：vmin -> white, vmax -> black
    color_ramp = QgsColorRampShader()
    color_ramp.setColorRampType(QgsColorRampShader.Interpolated)
    items = [
        QgsColorRampShader.ColorRampItem(vmin, QColor("white"), "min"),
        QgsColorRampShader.ColorRampItem(vmax, QColor("black"), "max"),
    ]
    color_ramp.setColorRampItemList(items)

    shader = QgsRasterShader()
    shader.setRasterShaderFunction(color_ramp)

    renderer = QgsSingleBandPseudoColorRenderer(layer.dataProvider(), 1, shader)
    layer.setRenderer(renderer)
    layer.triggerRepaint()
    layer.setCustomProperty("collapse_group_on_add", True)
    QgsProject.instance().addMapLayer(layer)
    return layer


def load_vlayer_simple(geojson_file_path,
                       layer_name,
                       color,
                       width=0.5,
                       add_arrow=False,
                       arrow_color=QColor(0, 0, 0),
                       arrow_size=0.3,
                       arrow_interval=3.0):
    # 加载GeoJSON文件为一个新的图层
    vlayer = QgsVectorLayer(geojson_file_path, layer_name, "ogr")

    if not vlayer.isValid():
        print("图层加载失败！")
    else:
        # 设置图层样式
        symbol = QgsSymbol.defaultSymbol(vlayer.geometryType())
        if symbol is None:
            print(f"无法为图层 {layer_name} 创建默认符号！")
            return None
        symbol.setColor(color)
        if vlayer.geometryType() == QgsWkbTypes.LineGeometry:
            symbol.setWidth(width)

        if add_arrow:
            # 创建一个箭头 marker 符号（simple marker name = 'arrow'）
            arrow_marker = QgsMarkerSymbol.createSimple({
                'name': 'arrowhead',
                'color': f'{arrow_color.red()},{arrow_color.green()},{arrow_color.blue()}',
                'size': str(arrow_size)
            })
            marker_line = None
            # 多策略尝试兼容不同 QGIS / PyQGIS 版本的构造方式
            # 1) 有的版本接受 QgsMarkerSymbol 作为参数
            try:
                marker_line = QgsMarkerLineSymbolLayer(arrow_marker)
            except Exception:
                marker_line = None

            # 2) 尝试使用底层的 symbolLayer（QgsSimpleMarkerSymbolLayer 等）
            if marker_line is None:
                try:
                    base_layer = arrow_marker.symbolLayer(0)
                    marker_line = QgsMarkerLineSymbolLayer(base_layer)
                except Exception:
                    marker_line = None

            # 3) 回退：通过 create() 工厂创建后设置子符号（若 API 支持 setSubSymbol / setMarkerSymbol）
            if marker_line is None:
                try:
                    marker_line = QgsMarkerLineSymbolLayer.create({
                        'placement': 'last_vertex',
                        'interval': str(arrow_interval)
                    })
                    if hasattr(marker_line, 'setSubSymbol'):
                        # setSubSymbol 通常接收 QgsMarkerSymbol
                        marker_line.setSubSymbol(arrow_marker)
                    elif hasattr(marker_line, 'setMarkerSymbol'):
                        marker_line.setMarkerSymbol(arrow_marker)
                except Exception:
                    marker_line = None

            # 如果成功创建，则设置放置方式并追加到主符号
            if marker_line is not None:
                try:
                    marker_line.setPlacement(QgsMarkerLineSymbolLayer.Interval)
                except Exception:
                    try:
                        # 某些版本用整数常量
                        marker_line.setPlacement(2)
                    except Exception:
                        pass
                symbol.appendSymbolLayer(marker_line)
        # 添加图层到QGIS项目中
        vlayer.renderer().setSymbol(symbol)
        QgsProject.instance().addMapLayer(vlayer, True)
        print("图层加载到root中!")
        return vlayer


def has_child_group(group: QgsLayerTreeGroup) -> bool:
    has_group = False
    has_layer = False
    for child in group.children():
        if isinstance(child, QgsLayerTreeGroup):
            has_group = True
        else:
            # 不是 group 的节点可视为图层节点
            has_layer = True
        # 早退出
        if has_group:
            return True
    # if consider_layers: empty means no group and no layer
    return has_group


def collapse_groups(predicate=lambda name: True, root=None):
    """
    折叠满足 predicate(name) 的图层组（递归）。
    predicate: 接收组名返回 True/False。默认折叠所有组。
    root: 起始节点，默认为项目根。
    """
    project = QgsProject.instance()
    root = root or project.layerTreeRoot()

    def _recurse(node):
        for child in list(node.children()):
            if isinstance(child, QgsLayerTreeGroup):
                try:
                    if predicate(child.name()):
                        child.setExpanded(False)
                        if has_child_group(child):
                            child.setItemVisibilityChecked(True)
                        else:
                            child.setItemVisibilityChecked(False)
                except Exception:
                    # 忽略因 API 版本差异可能抛的错误
                    pass
                _recurse(child)

    _recurse(root)


def _index_of_child(parent, node):
    """
    返回 node 在 parent.children() 中的索引，找不到返回 -1。
    兼容不同 PyQGIS 版本没有 indexOfChild 的情况。
    """
    try:
        children = list(parent.children())
    except Exception:
        return -1
    for i, c in enumerate(children):
        if c is node:
            return i
    return -1


def move_layer_up(layer):
    """将图层在当前组中移动到最顶层（index 0）。"""
    root = QgsProject.instance().layerTreeRoot()
    node = root.findLayer(layer.id())
    if node is None:
        return False
    parent = node.parent()
    if parent is None:
        return False
    idx = _index_of_child(parent, node)
    if idx <= 0:
        return True
    # 在顶端插入新节点，然后删除旧节点（避免复用已删除的 node）
    new_node = QgsLayerTreeLayer(layer)
    parent.insertChildNode(0, new_node)
    try:
        parent.removeChildNode(node)
    except Exception:
        pass
    return True


base_path = '/Users/huey/Dataset/topo-dataset/1107_preprocess/'
line_width = 1.0
line_arrow_size = 2.0
line_arrow_interval = 3.5
add_arrow = True
batch_dirs = os.listdir(base_path)
print(f"Found batches: {batch_dirs}")
batch_clip_max_num = 1


def get_file_name(file_names, func):
    for file_name in file_names:
        if func(file_name):
            return file_name
    return None


for batch in batch_dirs:
    batch_path = os.path.join(base_path, batch)
    batch_clip_count = 0
    print(f"Processing batch: {batch}")
    if not os.path.isdir(batch_path):
        continue
    for clip_index, clip_name in enumerate(os.listdir(batch_path)):
        print(f"  Processing clip: {clip_name}")
        clip_path = os.path.join(batch_path, clip_name)
        if not os.path.isdir(clip_path):
            continue
        vlayer = None

        file_names = os.listdir(clip_path)
        lidar_intensity_file_name = get_file_name(file_names, lambda x: x.endswith('_lidar_intensity.tif'))
        if lidar_intensity_file_name:
            tif_path = os.path.join(clip_path, lidar_intensity_file_name)
            layer_name = f"lidar_intensity"
            vlayer = load_tif_white_to_black(tif_path=tif_path, layer_name=layer_name)
            if vlayer is not None:
                move_layer_to_group(vlayer, ["TopoDataset", batch, clip_name])
                move_layer_up(vlayer)

        junctions_file_name = get_file_name(file_names, lambda x: x.endswith('_junctions.geojson'))
        if junctions_file_name:
            geojson_file_path = os.path.join(clip_path, junctions_file_name)
            layer_name = f"junctions"
            vlayer = load_vlayer_simple(geojson_file_path=geojson_file_path,
                                        layer_name=layer_name,
                                        color=QColor(255, 165, 0),
                                        width=line_width,
                                        add_arrow=False)
            if vlayer is not None:
                move_layer_to_group(vlayer, ["TopoDataset", batch, clip_name])
                move_layer_up(vlayer)

        lanes_file_name = get_file_name(file_names, lambda x: x.endswith('_lanes.geojson'))
        if lanes_file_name:
            geojson_file_path = os.path.join(clip_path, lanes_file_name)
            layer_name = f"lanes"
            vlayer = load_vlayer_simple(geojson_file_path=geojson_file_path,
                                        layer_name=layer_name,
                                        color=QColor(0, 255, 0),
                                        width=line_width,
                                        add_arrow=add_arrow,
                                        arrow_color=QColor(0, 255, 0),
                                        arrow_size=line_arrow_size,
                                        arrow_interval=line_arrow_interval)
            if vlayer is not None:
                move_layer_to_group(vlayer, ["TopoDataset", batch, clip_name])
                move_layer_up(vlayer)

        road_boundaries_file_name = get_file_name(
            file_names, lambda x: x.endswith('_road_boundaries.geojson') and 'merged' not in x)
        if road_boundaries_file_name:
            geojson_file_path = os.path.join(clip_path, road_boundaries_file_name)
            layer_name = f"road_boundaries"
            vlayer = load_vlayer_simple(geojson_file_path=geojson_file_path,
                                        layer_name=layer_name,
                                        color=QColor(255, 0, 0),
                                        width=line_width,
                                        add_arrow=add_arrow,
                                        arrow_color=QColor(255, 0, 0),
                                        arrow_size=line_arrow_size,
                                        arrow_interval=line_arrow_interval)
            if vlayer is not None:
                move_layer_to_group(vlayer, ["TopoDataset", batch, clip_name])
                move_layer_up(vlayer)

        lane_boundaries_file_name = get_file_name(
            file_names, lambda x: x.endswith('_lane_boundaries.geojson') and 'merged' not in x)
        if lane_boundaries_file_name:
            geojson_file_path = os.path.join(clip_path, lane_boundaries_file_name)
            layer_name = f"lane_boundaries"
            vlayer = load_vlayer_simple(geojson_file_path=geojson_file_path,
                                        layer_name=layer_name,
                                        color=QColor(0, 0, 255),
                                        width=line_width,
                                        add_arrow=add_arrow,
                                        arrow_color=QColor(0, 0, 255),
                                        arrow_size=line_arrow_size,
                                        arrow_interval=line_arrow_interval)
            if vlayer is not None:
                move_layer_to_group(vlayer, ["TopoDataset", batch, clip_name])
                move_layer_up(vlayer)

        trajectory_file_name = get_file_name(file_names, lambda x: x.endswith('_trajectory.geojson'))
        if trajectory_file_name:
            geojson_file_path = os.path.join(clip_path, trajectory_file_name)
            layer_name = f"trajectory"
            vlayer = load_vlayer_simple(geojson_file_path=geojson_file_path,
                                        layer_name=layer_name,
                                        color=QColor(0, 0, 0),
                                        width=line_width,
                                        add_arrow=add_arrow,
                                        arrow_color=QColor(0, 0, 0),
                                        arrow_size=line_arrow_size,
                                        arrow_interval=line_arrow_interval)
            if vlayer is not None:
                move_layer_to_group(vlayer, ["TopoDataset", batch, clip_name])
                move_layer_up(vlayer)

        if clip_index > batch_clip_max_num:
            break
    collapse_groups(lambda name: True)
