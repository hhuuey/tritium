from qgis.core import QgsProject, QgsApplication
from qgis.core import QgsLayerTreeNode, QgsLayerTreeGroup, QgsLayerTree, QgsLayerTreeLayer
from qgis.core import QgsMapLayer, QgsVectorLayer, QgsRasterLayer
from qgis.core import QgsSymbol, QgsMarkerSymbol, QgsMarkerLineSymbolLayer
from qgis.core import QgsPalLayerSettings, QgsTextFormat, QgsVectorLayerSimpleLabeling
from qgis.core import QgsRasterShader, QgsColorRampShader, QgsRasterBandStats, QgsSingleBandPseudoColorRenderer
from qgis.core import QgsCategorizedSymbolRenderer, QgsRendererCategory
from qgis.core import QgsWkbTypes
from qgis.PyQt.QtGui import QColor, QFont

from dataclasses import dataclass, field

from typing import List, Optional
import enum
import os

add_arrow = True
line_width = 1.0
point_width = 3.0
line_arrow_size = 2.0
line_arrow_interval = 3.5


@dataclass
class GroupInfo:
    name: str
    collapse: bool = True
    visibility: bool = True


def create_group(group_info: GroupInfo, parent_node: QgsLayerTreeNode = None) -> QgsLayerTreeGroup:
    if parent_node is None:
        parent_node = QgsProject.instance().layerTreeRoot()
    new_group = parent_node.addGroup(group_info.name)
    if group_info.collapse:
        new_group.setExpanded(False)
    else:
        new_group.setExpanded(True)
    if group_info.visibility:
        new_group.setItemVisibilityChecked(True)
    else:
        new_group.setItemVisibilityChecked(False)
    return new_group


def create_groups(group_infos: list[GroupInfo], parent_node: QgsLayerTreeNode = None) -> QgsLayerTreeGroup:
    if parent_node is None:
        parent_node = QgsProject.instance().layerTreeRoot()
    current_group = parent_node
    for group_info in group_infos:
        found = current_group.findGroup(group_info.name)
        if found is None:
            current_group = create_group(group_info, parent_node=current_group)
        else:
            if group_info.collapse:
                found.setExpanded(False)
            else:
                found.setExpanded(True)
            if group_info.visibility:
                found.setItemVisibilityChecked(True)
            else:
                found.setItemVisibilityChecked(False)
            current_group = found
    return current_group


def set_line_symbol(color, width=1.0, add_arrow=False, arrow_color=QColor(0, 0, 0), arrow_size=0.3, arrow_interval=3.0):
    symbol = QgsSymbol.defaultSymbol(QgsWkbTypes.LineGeometry)
    symbol.setColor(color)
    symbol.setWidth(width)
    if add_arrow:
        arrow_marker = QgsMarkerSymbol.createSimple({
            'name': 'arrowhead',
            'color': f'{arrow_color.red()},{arrow_color.green()},{arrow_color.blue()}',
            'size': str(arrow_size)
        })
        marker_line = QgsMarkerLineSymbolLayer.create({'placement': 'last_vertex', 'interval': str(arrow_interval)})
        marker_line.setSubSymbol(arrow_marker)
        marker_line.setPlacement(QgsMarkerLineSymbolLayer.Interval)
        symbol.appendSymbolLayer(marker_line)

    return symbol


def _to_qcolor(color_value):
    if isinstance(color_value, QColor):
        return color_value
    if isinstance(color_value, (list, tuple)) and len(color_value) in (3, 4):
        return QColor(*color_value)
    if isinstance(color_value, str):
        return QColor(color_value)
    return None


def _resolve_category_color(category_map: dict, value, fallback_color: QColor):
    if not category_map:
        return fallback_color
    mapped = category_map.get(value)
    if mapped is None:
        mapped = category_map.get(str(value))
    return mapped if mapped is not None else fallback_color


def set_point_layer(layer: QgsVectorLayer,
                    color: QColor,
                    width=1.0,
                    category=False,
                    category_field: str = '',
                    category_map: Optional[dict] = None):
    if layer.geometryType() != QgsWkbTypes.PointGeometry:
        return
    symbol = QgsMarkerSymbol.createSimple({
        'name': 'circle',
        'color': f'{color.red()},{color.green()},{color.blue()}',
        'size': str(width)
    })
    layer.renderer().setSymbol(symbol)

    if category and category_field != '':
        unique_values = layer.uniqueValues(layer.fields().indexFromName(category_field))
        categories = []
        for i, value in enumerate(unique_values):
            fallback_color = QColor.fromHsv(int(i * 360 / len(unique_values)), 255, 200)
            final_color = _resolve_category_color(category_map or {}, value, fallback_color)
            symbol = QgsMarkerSymbol.createSimple({'name': 'circle', 'color': final_color.name(), 'size': str(width)})
            category = QgsRendererCategory(value, symbol, str(value))
            categories.append(category)
        renderer = QgsCategorizedSymbolRenderer(category_field, categories)
        layer.setRenderer(renderer)

    layer.triggerRepaint()


# 设置线段图层对应的颜色，宽度，是否添加箭头等样式
def set_line_layer(layer: QgsVectorLayer,
                   color: QColor,
                   width=1.0,
                   add_arrow=False,
                   arrow_color=QColor(0, 0, 0),
                   arrow_size=0.3,
                   arrow_interval=3.0,
                   category=False,
                   category_field: str = '',
                   category_map: Optional[dict] = None):
    if layer is None:
        return
    if layer.geometryType() != QgsWkbTypes.LineGeometry:
        return

    if not category:
        symbol = set_line_symbol(color, width, add_arrow, arrow_color, arrow_size, arrow_interval)
        layer.renderer().setSymbol(symbol)

    if category and category_field != '':
        unique_values = layer.uniqueValues(layer.fields().indexFromName(category_field))
        categories = []
        for i, value in enumerate(unique_values):
            fallback_color = QColor.fromHsv(int(i * 360 / len(unique_values)), 255, 200)
            final_color = _resolve_category_color(category_map or {}, value, fallback_color)
            symbol = set_line_symbol(color=final_color,
                                     width=width,
                                     add_arrow=add_arrow,
                                     arrow_color=arrow_color,
                                     arrow_size=arrow_size,
                                     arrow_interval=arrow_interval)
            category = QgsRendererCategory(value, symbol, str(value))
            categories.append(category)
        renderer = QgsCategorizedSymbolRenderer(category_field, categories)
        layer.setRenderer(renderer)

    layer.triggerRepaint()


# 设置多边形图层的颜色和透明度
def set_polygon_layer(layer: QgsVectorLayer,
                      color: QColor,
                      transparency: float = 1.0,
                      category: bool = False,
                      category_field: str = '',
                      category_map: Optional[dict] = None):
    if layer.geometryType() != QgsWkbTypes.PolygonGeometry:
        return
    if not category:
        symbol = QgsSymbol.defaultSymbol(QgsWkbTypes.PolygonGeometry)
        symbol.setColor(color)
        symbol.setOpacity(transparency)
        layer.renderer().setSymbol(symbol)

    if category and category_field != '':
        unique_values = layer.uniqueValues(layer.fields().indexFromName(category_field))
        categories = []
        for i, value in enumerate(unique_values):
            symbol = QgsSymbol.defaultSymbol(QgsWkbTypes.PolygonGeometry)
            fallback_color = QColor.fromHsv(int(i * 360 / len(unique_values)), 255, 200)
            symbol.setColor(_resolve_category_color(category_map or {}, value, fallback_color))
            symbol.setOpacity(transparency)
            category = QgsRendererCategory(value, symbol, str(value))
            categories.append(category)
        renderer = QgsCategorizedSymbolRenderer(category_field, categories)
        layer.setRenderer(renderer)
    layer.triggerRepaint()


# 设置栅格图层的颜色映射
def set_raster_layer_color_map(layer: QgsRasterLayer, black_to_white: bool = True):
    if layer is None:
        return
    if not layer.isValid():
        return
    # 获取波段统计（波段索引从1开始）
    stats = layer.dataProvider().bandStatistics(1, QgsRasterBandStats.All)
    vmin = stats.minimumValue if stats is not None else None
    vmax = stats.maximumValue if stats is not None else None

    # 保护：若 min/max 不可用，使用 0/1 作为占位
    if vmin is None or vmax is None or vmin == vmax:
        vmin, vmax = 0.0, 1.0
    # 构造颜色渐变：
    # black_to_white=True:  vmin -> black, vmax -> white
    # black_to_white=False: vmin -> white, vmax -> black
    color_ramp = QgsColorRampShader()
    color_ramp.setColorRampType(QgsColorRampShader.Interpolated)
    if black_to_white:
        items = [
            QgsColorRampShader.ColorRampItem(vmin, QColor("black"), "min"),
            QgsColorRampShader.ColorRampItem(vmax, QColor("white"), "max"),
        ]
    else:
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


# 加载 geojson文件为图层
def load_vector_layer(geojson_file_path: str, layer_name: str) -> QgsVectorLayer:
    vlayer = QgsVectorLayer(geojson_file_path, layer_name, "ogr")
    if not vlayer.isValid():
        return None
    return vlayer


# 加载tif文件为图层
def load_raster_layer(tif_path: str, layer_name: str) -> QgsRasterLayer:
    layer = QgsRasterLayer(tif_path, layer_name)
    if not layer.isValid():
        return None
    return layer


class LayerType(enum.Enum):
    VECTOR = 1
    RASTER = 2


@dataclass
class NodeInfo:
    file_name: str
    type: LayerType = LayerType.VECTOR
    group_infos: List[GroupInfo] = field(default_factory=list)
    layer_name: str = ''
    color: QColor = QColor(255, 0, 0)
    width: float = line_width
    add_arrow: bool = add_arrow
    arrow_color: QColor = QColor(0, 0, 0)
    arrow_size: float = line_arrow_size
    arrow_interval: float = line_arrow_interval
    transparency: float = 1.0  # 仅针对多边形图层
    node_visibility: bool = False
    node_collapse: bool = True
    category: bool = False  # 是否按类别渲染(针对矢量图层)
    category_field: str = 'id'
    category_map: dict = field(default_factory=dict)
    raster_black_to_white: bool = True
    show_labels: bool = False
    label_field: str = ''
    label_font_size: float = 10.0
    label_font_color: QColor = QColor(0, 0, 0)
    label_buffer_enabled: bool = True


def load_node_info(node: NodeInfo):
    layer = None
    if node.type == LayerType.VECTOR:
        layer = load_vector_layer(node.file_name, node.layer_name)
        if layer is None:
            return
        if layer.geometryType() == QgsWkbTypes.LineGeometry:
            set_line_layer(layer,
                           node.color,
                           width=node.width,
                           add_arrow=node.add_arrow,
                           arrow_color=node.arrow_color,
                           arrow_size=node.arrow_size,
                           arrow_interval=node.arrow_interval,
                           category=node.category,
                           category_field=node.category_field,
                           category_map=node.category_map)
        if layer.geometryType() == QgsWkbTypes.PolygonGeometry:
            set_polygon_layer(layer,
                              node.color,
                              transparency=node.transparency,
                              category=node.category,
                              category_field=node.category_field,
                              category_map=node.category_map)
        if layer.geometryType() == QgsWkbTypes.PointGeometry:
            set_point_layer(layer, node.color, node.width, node.category, node.category_field, node.category_map)
        if node.show_labels and node.label_field:
            set_single_labels_advanced(layer,
                                       label_field=node.label_field,
                                       font_size=node.label_font_size,
                                       font_color=node.label_font_color,
                                       buffer_enabled=node.label_buffer_enabled)
    if node.type == LayerType.RASTER:
        layer = load_raster_layer(node.file_name, node.layer_name)
        if layer is None:
            return
        set_raster_layer_color_map(layer, black_to_white=node.raster_black_to_white)

    if layer is not None:
        project = QgsProject.instance()
        project.addMapLayer(layer, False)
        group = create_groups(node.group_infos)
        tree_layer: QgsLayerTreeLayer = group.insertLayer(0, layer)
        tree_layer.setItemVisibilityChecked(node.node_visibility)
        tree_layer.setExpanded((not node.node_collapse))
        return

    return


def set_single_labels(layer: QgsVectorLayer,
                      label_field: str,
                      font_size: float = 10.0,
                      font_color: QColor = QColor(0, 0, 0),
                      font_family: str = 'Arial',
                      enable: bool = True):
    if layer is None or not layer.isValid():
        return

    # 检查字段是否存在
    if layer.fields().indexFromName(label_field) == -1:
        print(f"Warning: Field '{label_field}' not found in layer '{layer.name()}'")
        return

    # 创建标注设置
    label_settings = QgsPalLayerSettings()

    # 设置标注字段
    label_settings.fieldName = label_field

    # 设置文本格式
    text_format = QgsTextFormat()

    # 设置字体
    font = QFont(font_family)
    font.setPointSizeF(font_size)
    text_format.setFont(font)

    # 设置字体颜色
    text_format.setColor(font_color)

    # 可选：设置背景（提高可读性）
    # buffer_settings = text_format.buffer()
    # buffer_settings.setEnabled(True)
    # buffer_settings.setSize(1.0)
    # buffer_settings.setColor(QColor(255, 255, 255))
    # text_format.setBuffer(buffer_settings)

    label_settings.setFormat(text_format)

    # 设置标注位置（针对不同几何类型）
    if layer.geometryType() == QgsWkbTypes.PointGeometry:
        label_settings.placement = QgsPalLayerSettings.OverPoint
    elif layer.geometryType() == QgsWkbTypes.LineGeometry:
        label_settings.placement = QgsPalLayerSettings.Line
    elif layer.geometryType() == QgsWkbTypes.PolygonGeometry:
        label_settings.placement = QgsPalLayerSettings.OverPoint

    # 启用标注
    label_settings.enabled = enable

    # 应用标注设置
    labeling = QgsVectorLayerSimpleLabeling(label_settings)
    layer.setLabeling(labeling)
    layer.setLabelsEnabled(enable)

    # 刷新图层
    layer.triggerRepaint()


def set_single_labels_advanced(layer: QgsVectorLayer,
                               label_field: str,
                               font_size: float = 10.0,
                               font_color: QColor = QColor(0, 0, 0),
                               font_family: str = 'Arial',
                               enable: bool = True,
                               buffer_enabled: bool = False,
                               buffer_size: float = 1.0,
                               buffer_color: QColor = QColor(255, 255, 255)):
    if layer is None or not layer.isValid():
        return

    if layer.fields().indexFromName(label_field) == -1:
        print(f"Warning: Field '{label_field}' not found in layer '{layer.name()}'")
        return

    # 创建标注设置
    label_settings = QgsPalLayerSettings()
    label_settings.fieldName = label_field

    # 设置文本格式
    text_format = QgsTextFormat()

    # 字体设置
    font = QFont(font_family)
    font.setPointSizeF(font_size)
    text_format.setFont(font)
    text_format.setColor(font_color)

    # 缓冲区设置（白色描边）
    if buffer_enabled:
        buffer_settings = text_format.buffer()
        buffer_settings.setEnabled(True)
        buffer_settings.setSize(buffer_size)
        buffer_settings.setColor(buffer_color)
        text_format.setBuffer(buffer_settings)

    label_settings.setFormat(text_format)

    # # 点要素
    # QgsPalLayerSettings.Placement.OverPoint          # 点上方
    # QgsPalLayerSettings.Placement.AroundPoint        # 点周围

    # # 线要素
    # QgsPalLayerSettings.Placement.Line               # 沿线平行
    # QgsPalLayerSettings.Placement.Curved             # 沿线弯曲
    # QgsPalLayerSettings.Placement.Horizontal         # 水平放置

    # # 面要素
    # QgsPalLayerSettings.Placement.OverPoint          # 多边形中心
    # QgsPalLayerSettings.Placement.Free               # 自由放置
    # QgsPalLayerSettings.Placement.OrderedPositionsAroundPoint  # 按顺序围绕点

    # 设置标注位置
    if layer.geometryType() == QgsWkbTypes.PointGeometry:
        label_settings.placement = QgsPalLayerSettings.Placement.AroundPoint
    elif layer.geometryType() == QgsWkbTypes.LineGeometry:
        label_settings.placement = QgsPalLayerSettings.Placement.Line
    elif layer.geometryType() == QgsWkbTypes.PolygonGeometry:
        label_settings.placement = QgsPalLayerSettings.Placement.OverPoint

    label_settings.enabled = enable

    # 应用标注
    labeling = QgsVectorLayerSimpleLabeling(label_settings)
    layer.setLabeling(labeling)
    layer.setLabelsEnabled(enable)

    layer.triggerRepaint()


def init_qgis():
    # 初始化 QGIS（无 GUI 模式）
    QgsApplication.setPrefixPath("", True)
    qgs = QgsApplication([], False)
    qgs.initQgis()
    return qgs


def save_qgis(qgs, output_qgz):
    project = QgsProject.instance()
    # 保存项目
    project.write(output_qgz)
    print(f"✓ QGIS project saved: {output_qgz}")
    # 清理
    qgs.exitQgis()


def construct_nodes_for_data_file(
    raw_config,
    data_file_path,
    group_infos,
):
    file_name = os.path.basename(data_file_path)
    lower_file_name = file_name.lower()
    is_raster_file = lower_file_name.endswith('.tif') or lower_file_name.endswith('.tiff')
    config = dict(raw_config)
    config.pop('file', None)

    if 'color' in config:
        color_tuple = config['color']
        config['color'] = QColor(*color_tuple)

    if 'category_map' in config and config['category_map']:
        parsed_category_map = {}
        raw_category_map = config['category_map']

        if isinstance(raw_category_map, dict):
            category_items = raw_category_map.items()
        else:
            category_items = raw_category_map

        for item in category_items:
            if isinstance(raw_category_map, dict):
                key, raw_color = item
            else:
                if not isinstance(item, (list, tuple)) or len(item) != 2:
                    continue
                key, raw_color = item

            parsed_color = _to_qcolor(raw_color)
            if parsed_color is not None and parsed_color.isValid():
                parsed_category_map[key] = parsed_color

        config['category_map'] = parsed_category_map

    if 'color' in config and 'arrow_color' not in config:
        config['arrow_color'] = config['color']

    layer_name = os.path.splitext(file_name)[0]
    if 'category' in config and config['category']:
        layer_name += f'_cat_by_({config["category_field"]})'

    # 默认值补充
    defaults = {
        'file_name': data_file_path,
        'layer_name': layer_name,
        'type': LayerType.RASTER if is_raster_file else LayerType.VECTOR,
        'node_collapse': True,
        'group_infos': group_infos,
    }

    final_config = {**defaults, **config}
    return NodeInfo(**final_config)


def is_supported_data_file(file_name: str) -> bool:
    lower_name = file_name.lower()
    return lower_name.endswith('.geojson') or lower_name.endswith('.tif') or lower_name.endswith('.tiff')


def is_same_config_file(config_file_name: str, disk_file_name: str) -> bool:
    cfg_lower = config_file_name.lower()
    disk_lower = disk_file_name.lower()
    if cfg_lower == disk_lower:
        return True

    cfg_stem, cfg_ext = os.path.splitext(cfg_lower)
    disk_stem, disk_ext = os.path.splitext(disk_lower)
    raster_exts = ('.tif', '.tiff')
    if cfg_stem == disk_stem and cfg_ext in raster_exts and disk_ext in raster_exts:
        return True

    return False


def find_matched_file_path_in_dir(dir_path: str, target_file_name: str):
    for file_name in os.listdir(dir_path):
        if not is_supported_data_file(file_name):
            continue
        if is_same_config_file(target_file_name, file_name):
            return os.path.join(dir_path, file_name)
    return None


def build_group_infos_from_base(data_base_path: str, file_dir_path: str):
    """
    从 base 目录开始，为路径中的每一级目录构建 GroupInfo。
    所有层 collapse=True；仅最后一层 visibility=False，其余为 True。
    """
    base_name = os.path.basename(os.path.normpath(data_base_path))
    rel_dir = os.path.relpath(file_dir_path, data_base_path)
    dir_parts = [] if rel_dir == '.' else rel_dir.split(os.sep)
    group_names = [base_name] + dir_parts

    group_infos = []
    for idx, name in enumerate(group_names):
        group_infos.append(GroupInfo(name=name, collapse=True, visibility=(idx != len(group_names) - 1)))

    return group_infos


def project_qgis(
    data_base_path,
    config,
    output_qgz_project_file,
    batch_clip_max_num=None,
):
    if not os.path.isdir(data_base_path):
        print(f"Invalid base dir: {data_base_path}")
        return

    qgs = init_qgis()

    loaded_file_count = 0
    for root, _, files in os.walk(data_base_path):
        supported_files = [f for f in files if is_supported_data_file(f)]
        if not supported_files:
            continue

        group_infos = build_group_infos_from_base(data_base_path, root)
        loaded_in_current_dir = False
        for cfg in config:
            cfg_file_name = cfg.get('file')
            if not cfg_file_name:
                continue

            matched_path = find_matched_file_path_in_dir(root, cfg_file_name)
            if matched_path is None:
                continue

            node = construct_nodes_for_data_file(
                cfg,
                matched_path,
                group_infos,
            )
            load_node_info(node)
            loaded_in_current_dir = True
            print(f"Loaded from file: {matched_path}, config file key: {cfg_file_name}")

        if loaded_in_current_dir:
            loaded_file_count += 1

        if batch_clip_max_num is not None and loaded_file_count >= batch_clip_max_num:
            print(f"Reached max load limit: {batch_clip_max_num}")
            return

    save_qgis(qgs, output_qgz_project_file)
