# 引入必要的模块
from shutil import move
from qgis.core import QgsProject, QgsVectorLayer, QgsCategorizedSymbolRenderer, QgsSymbol, QgsRendererCategory
from qgis.PyQt.QtCore import QVariant
from qgis.core import QgsProject
from qgis.PyQt.QtGui import QColor

import os


def layer_repaint(layer, field_name, categories_info_map):
    # 检查字段是否存在
    if field_name not in [field.name() for field in layer.fields()]:
        print(f"Field '{field_name}' not found in layer")
        return

    # 创建分类列表
    categories = []
    for value, color in categories_info_map.items():
        symbol = QgsSymbol.defaultSymbol(layer.geometryType())
        symbol.setColor(color)
        category = QgsRendererCategory(QVariant(value), symbol, str(value))
        categories.append(category)

    # 创建并设置分类渲染器
    renderer = QgsCategorizedSymbolRenderer(field_name, categories)
    layer.setRenderer(renderer)

    # 触发图层重绘以显示更改
    layer.triggerRepaint()


# geojson_file_path: 可视化文件路径
# group_name：是否需要添加到group中
# category: 是否要根据某个field_name进行分类
# field_name: 分类字段
# categories_info_map: 分类字段对应的颜色
def load_qgis_core(geojson_file_path, group_name=None, category=False, field_name=None, categories_info_map=None):
    print("处理文件：", geojson_file_path)

    # 创建一个新的图层分组
    if group_name:
        root = QgsProject.instance().layerTreeRoot()
        existing_group = root.findGroup(group_name)
        if not existing_group:
            group = root.addGroup(group_name)

    layer_name = geojson_file_path.split('/')[-1].split('.')[0]

    # 加载GeoJSON文件为一个新的图层
    vlayer = QgsVectorLayer(geojson_file_path, layer_name, "ogr")

    if not vlayer.isValid():
        print("图层加载失败！")
    else:
        # 添加图层到QGIS项目中
        if group_name:
            QgsProject.instance().addMapLayer(vlayer, False)
            group.addLayer(vlayer)
            print("图层加载到group中!")
        else:
            QgsProject.instance().addMapLayer(vlayer, True)
            print("图层加载到root中!")
        if category:
            layer_repaint(vlayer, field_name, categories_info_map)


if __name__ == '__main__':
    base_path = '/home/horizon/code/ndm_horizon/tool/jira_lbs/tmp/toll_data'
    # 定义需要根据什么属性区分颜色
    field_name = 'type_readable'
    # 定义你想要渲染的属性值及其颜色的映射关系
    categories_info_map = {
        "导流带车道": QColor(255, 0, 0),  # 红色
        "非机动车道": QColor(0, 255, 0),  # 绿色
        "加速车道": QColor(0, 0, 255),  # 蓝色
        "减速车道": QColor(255, 255, 0),  # 黄色
        "普通车道": QColor(128, 0, 128),  # 紫色
        "收费站车道": QColor(0, 255, 255),  # 青色
        "未知": QColor(255, 192, 203),  # 粉红色
        "匝道": QColor(255, 165, 0),  # 橙色
    }

    dirs = sorted(os.listdir(base_path), key=lambda s: int(s.split('_')[0]))

    for idx, dir in enumerate(dirs):
        dir_path = os.path.join(base_path, dir)
        if os.path.isdir(dir_path):
            Layer_Lane_file = os.path.join(dir_path, 'Layer_Lane.txt')
            load_qgis_core(log_file, dir, False)
            log_file = os.path.join(dir_path, 'loc.geojson')
            load_qgis_core(Layer_Lane_file, dir, True, field_name, categories_info_map)
