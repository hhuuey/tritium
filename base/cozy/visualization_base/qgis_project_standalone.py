# ...existing code...

def export_to_qgis(clip_data, output_qgz: str):
    """
    导出 clip_data 到 QGIS 项目文件

    Args:
        clip_data: ClipData 对象
        output_qgz: 输出的 .qgz 文件路径
    """
    try:
        from qgis.core import (
            QgsApplication, QgsProject, QgsVectorLayer, QgsFeature,
            QgsGeometry, QgsPointXY, QgsField, QgsSingleSymbolRenderer,
            QgsLineSymbol, QgsMarkerSymbol
        )
        from qgis.PyQt.QtCore import QVariant
        from qgis.PyQt.QtGui import QColor
    except ImportError:
        print("❌ PyQGIS not installed. Install with: conda install -c conda-forge qgis")
        return

    # 初始化 QGIS（无 GUI 模式）
    QgsApplication.setPrefixPath("", True)
    qgs = QgsApplication([], False)
    qgs.initQgis()

    project = QgsProject.instance()

    # 1. 创建 Lanes 图层
    lane_layer = QgsVectorLayer("LineString?crs=EPSG:4326", "Lanes", "memory")
    lane_pr = lane_layer.dataProvider()
    lane_pr.addAttributes([
        QgsField("id", QVariant.String),
        QgsField("section_id", QVariant.String),
        QgsField("road_id", QVariant.String),
    ])
    lane_layer.updateFields()

    for lane in clip_data.lanes.values():
        feat = QgsFeature()
        pts = [QgsPointXY(p.x, p.y) for p in lane.geometry.points]
        feat.setGeometry(QgsGeometry.fromPolylineXY(pts))
        feat.setAttributes([
            lane.id,
            getattr(lane, 'section_id', ''),
            getattr(lane, 'road_id', ''),
        ])
        lane_pr.addFeature(feat)

    lane_layer.updateExtents()
    symbol = QgsLineSymbol.createSimple({'color': 'blue', 'width': '1.5'})
    lane_layer.setRenderer(QgsSingleSymbolRenderer(symbol))
    project.addMapLayer(lane_layer)

    # 2. 创建 Sections 图层（如果存在）
    if hasattr(clip_data, 'sections') and clip_data.sections:
        section_layer = QgsVectorLayer("LineString?crs=EPSG:4326", "Sections", "memory")
        section_pr = section_layer.dataProvider()
        section_pr.addAttributes([
            QgsField("id", QVariant.String),
            QgsField("lane_num", QVariant.Int),
            QgsField("next_ids", QVariant.String),
        ])
        section_layer.updateFields()

        for section in clip_data.sections.values():
            feat = QgsFeature()
            pts = [QgsPointXY(p.x, p.y) for p in section.geometry.points]
            feat.setGeometry(QgsGeometry.fromPolylineXY(pts))
            feat.setAttributes([
                section.id,
                section.lane_num_,
                ','.join(section.next_ids),
            ])
            section_pr.addFeature(feat)

        section_layer.updateExtents()
        symbol = QgsLineSymbol.createSimple({'color': 'red', 'width': '2.0'})
        section_layer.setRenderer(QgsSingleSymbolRenderer(symbol))
        project.addMapLayer(section_layer)

    # 3. 创建 Roads 图层（如果存在）
    if hasattr(clip_data, 'roads') and clip_data.roads:
        road_layer = QgsVectorLayer("LineString?crs=EPSG:4326", "Roads", "memory")
        road_pr = road_layer.dataProvider()
        road_pr.addAttributes([
            QgsField("id", QVariant.String),
            QgsField("section_count", QVariant.Int),
        ])
        road_layer.updateFields()

        for road in clip_data.roads.values():
            feat = QgsFeature()
            pts = [QgsPointXY(p.x, p.y) for p in road.geometry.points]
            feat.setGeometry(QgsGeometry.fromPolylineXY(pts))
            feat.setAttributes([
                road.id,
                len(road.section_ids_),
            ])
            road_pr.addFeature(feat)

        road_layer.updateExtents()
        symbol = QgsLineSymbol.createSimple({'color': 'green', 'width': '2.5'})
        road_layer.setRenderer(QgsSingleSymbolRenderer(symbol))
        project.addMapLayer(road_layer)

    # 保存项目
    project.write(output_qgz)
    print(f"✓ QGIS project saved: {output_qgz}")

    # 清理
    qgs.exitQgis()