from cozy import mb
import numpy as np


def hausdorff_distance(poly1: mb.MapGeometry, poly2: mb.MapGeometry) -> float:
    """
    计算两条 polyline 的 Hausdorff 距离

    定义：max(h(A,B), h(B,A))
    其中 h(A,B) = max_{a∈A} min_{b∈B} dist(a,b)

    优点：对整体形状敏感
    缺点：对离群点敏感
    """
    from scipy.spatial.distance import directed_hausdorff

    pts1 = np.array([[p.x, p.y] for p in poly1.points])
    pts2 = np.array([[p.x, p.y] for p in poly2.points])

    d1 = directed_hausdorff(pts1, pts2)[0]
    d2 = directed_hausdorff(pts2, pts1)[0]

    return max(d1, d2)


def frechet_distance(poly1: mb.MapGeometry, poly2: mb.MapGeometry) -> float:
    """
    Fréchet 距离（狗绳距离）

    定义：想象一人一狗沿各自曲线行走，绳子的最小最大长度

    优点：考虑曲线顺序，适合轨迹比较
    缺点：计算复杂度 O(n²)
    """
    from similaritymeasures import frechet_dist

    pts1 = np.array([[p.x, p.y] for p in poly1.points])
    pts2 = np.array([[p.x, p.y] for p in poly2.points])

    return frechet_dist(pts1, pts2)


def chamfer_distance(poly1: mb.MapGeometry, poly2: mb.MapGeometry) -> float:
    """
    Chamfer 距离（双向最近点距离的平均）

    定义：(Σ min_dist(a→B) + Σ min_dist(b→A)) / (|A| + |B|)

    优点：计算快，对局部偏差不敏感
    缺点：不考虑点的对应关系
    """
    pts1 = np.array([[p.x, p.y] for p in poly1.points])
    pts2 = np.array([[p.x, p.y] for p in poly2.points])

    # A → B
    dist_1to2 = np.mean([np.min(np.linalg.norm(pts2 - p, axis=1)) for p in pts1])

    # B → A
    dist_2to1 = np.mean([np.min(np.linalg.norm(pts1 - p, axis=1)) for p in pts2])

    return (dist_1to2 + dist_2to1) / 2


def polyline_iou(poly1: mb.MapGeometry, poly2: mb.MapGeometry, buffer_width: float = 1.0) -> float:
    """
    基于缓冲区的 IoU

    步骤：
    1. 对两条线做 buffer 生成多边形
    2. 计算多边形的交并比

    优点：直观，适合车道线评测
    缺点：依赖 buffer 宽度参数
    """
    from shapely.geometry import LineString

    line1 = LineString([(p.x, p.y) for p in poly1.points])
    line2 = LineString([(p.x, p.y) for p in poly2.points])

    # 生成缓冲区
    buffer1 = line1.buffer(buffer_width)
    buffer2 = line2.buffer(buffer_width)

    # 计算 IoU
    intersection = buffer1.intersection(buffer2).area
    union = buffer1.union(buffer2).area

    return intersection / union if union > 0 else 0.0


# 车道线评测指标（来自 nuScenes devkit）
def nuscenes_lane_metrics(gt_lane, pred_lane):
    """
    nuScenes 车道线评测标准

    指标：
    1. Chamfer Distance < 1.0m
    2. Endpoint Error < 2.0m
    3. F1-score（基于距离阈值）
    """
    # 1. Chamfer Distance
    cd = chamfer_distance(gt_lane, pred_lane)

    # 2. Endpoint Error
    gt_start = gt_lane.points[0]
    gt_end = gt_lane.points[-1]
    pred_start = pred_lane.points[0]
    pred_end = pred_lane.points[-1]

    start_error = np.linalg.norm([gt_start.x - pred_start.x, gt_start.y - pred_start.y])
    end_error = np.linalg.norm([gt_end.x - pred_end.x, gt_end.y - pred_end.y])

    # 3. F1-score（距离阈值 1.0m）
    matched = cd < 1.0 and start_error < 2.0 and end_error < 2.0

    return {'chamfer_dist': cd, 'start_error': start_error, 'end_error': end_error, 'matched': matched}


# Argoverse 车道线评测
def argoverse_lane_metrics(gt_lane, pred_lane):
    """
    Argoverse 评测标准

    指标：
    1. Average Precision（AP）at IoU > 0.5
    2. Hausdorff Distance
    3. Lateral Error（横向误差）
    """
    # 1. IoU
    iou = polyline_iou(gt_lane, pred_lane, buffer_width=0.5)

    # 2. Hausdorff Distance
    hd = hausdorff_distance(gt_lane, pred_lane)

    # 3. Lateral Error（采样点的垂直距离）
    lateral_errors = []
    for gt_pt in gt_lane.points:
        min_dist = min([
            point_to_segment_distance(gt_pt, pred_lane.points[i], pred_lane.points[i + 1])
            for i in range(len(pred_lane.points) - 1)
        ])
        lateral_errors.append(min_dist)

    return {
        'iou': iou,
        'hausdorff_dist': hd,
        'lateral_error_mean': np.mean(lateral_errors),
        'lateral_error_max': np.max(lateral_errors)
    }


# OpenLane 评测标准
def openlane_lane_metrics(gt_lanes, pred_lanes):
    """
    OpenLane 评测指标

    指标：
    1. F1-score（基于 IoU > 0.5）
    2. Precision / Recall
    3. FP / FN 统计
    """
    tp = 0  # True Positive
    fp = 0  # False Positive
    fn = 0  # False Negative

    matched_gt = set()
    matched_pred = set()

    # 匹配 GT 和预测
    for gt_id, gt_lane in gt_lanes.items():
        best_iou = 0.0
        best_pred_id = None

        for pred_id, pred_lane in pred_lanes.items():
            if pred_id in matched_pred:
                continue

            iou = polyline_iou(gt_lane.geometry, pred_lane.geometry)
            if iou > best_iou:
                best_iou = iou
                best_pred_id = pred_id

        if best_iou > 0.5:
            tp += 1
            matched_gt.add(gt_id)
            matched_pred.add(best_pred_id)

    fn = len(gt_lanes) - len(matched_gt)
    fp = len(pred_lanes) - len(matched_pred)

    precision = tp / (tp + fp) if (tp + fp) > 0 else 0
    recall = tp / (tp + fn) if (tp + fn) > 0 else 0
    f1 = 2 * precision * recall / (precision + recall) if (precision + recall) > 0 else 0

    return {'precision': precision, 'recall': recall, 'f1': f1, 'tp': tp, 'fp': fp, 'fn': fn}


def evaluate_lane_geometry(gt_lane: mb.Lane, pred_lane: mb.Lane) -> Dict[str, float]:
    """
    综合评测车道线几何相似度

    Returns:
        dict: {
            'chamfer_dist': 倒角距离,
            'hausdorff_dist': 豪斯多夫距离,
            'frechet_dist': 弗雷歇距离,
            'iou': 交并比,
            'endpoint_error': 端点误差
        }
    """
    gt_pts = np.array([[p.x, p.y] for p in gt_lane.geometry.points])
    pred_pts = np.array([[p.x, p.y] for p in pred_lane.geometry.points])

    # 1. Chamfer Distance
    cd_1to2 = np.mean([np.min(np.linalg.norm(pred_pts - p, axis=1)) for p in gt_pts])
    cd_2to1 = np.mean([np.min(np.linalg.norm(gt_pts - p, axis=1)) for p in pred_pts])
    chamfer_dist = (cd_1to2 + cd_2to1) / 2

    # 2. Hausdorff Distance
    hd_1 = directed_hausdorff(gt_pts, pred_pts)[0]
    hd_2 = directed_hausdorff(pred_pts, gt_pts)[0]
    hausdorff_dist = max(hd_1, hd_2)

    # 3. IoU (buffer 0.5m)
    line1 = LineString(gt_pts)
    line2 = LineString(pred_pts)
    buffer1 = line1.buffer(0.5)
    buffer2 = line2.buffer(0.5)
    intersection = buffer1.intersection(buffer2).area
    union = buffer1.union(buffer2).area
    iou = intersection / union if union > 0 else 0.0

    # 4. Endpoint Error
    start_error = np.linalg.norm(gt_pts[0] - pred_pts[0])
    end_error = np.linalg.norm(gt_pts[-1] - pred_pts[-1])
    endpoint_error = (start_error + end_error) / 2

    return {
        'chamfer_dist': chamfer_dist,
        'hausdorff_dist': hausdorff_dist,
        'iou': iou,
        'endpoint_error': endpoint_error
    }


def val_lanes(gt_lanes: Dict[str, mb.Lane], pred_lanes: Dict[str, mb.Lane]):
    """
    批量评测车道线

    评测流程：
    1. 匹配 GT 和预测（基于 Chamfer Distance）
    2. 计算几何相似度指标
    3. 统计 Precision / Recall / F1
    """
    results = []
    matched_pred = set()

    # 匹配阶段
    for gt_id, gt_lane in gt_lanes.items():
        best_dist = float('inf')
        best_pred_id = None

        for pred_id, pred_lane in pred_lanes.items():
            if pred_id in matched_pred:
                continue

            # 使用 Chamfer Distance 匹配
            gt_pts = np.array([[p.x, p.y] for p in gt_lane.geometry.points])
            pred_pts = np.array([[p.x, p.y] for p in pred_lane.geometry.points])

            cd_1to2 = np.mean([np.min(np.linalg.norm(pred_pts - p, axis=1)) for p in gt_pts])
            cd_2to1 = np.mean([np.min(np.linalg.norm(gt_pts - p, axis=1)) for p in pred_pts])
            cd = (cd_1to2 + cd_2to1) / 2

            if cd < best_dist:
                best_dist = cd
                best_pred_id = pred_id

        # 如果匹配成功（距离 < 2.0m）
        if best_dist < 2.0 and best_pred_id:
            matched_pred.add(best_pred_id)
            metrics = evaluate_lane_geometry(gt_lane, pred_lanes[best_pred_id])
            metrics['gt_id'] = gt_id
            metrics['pred_id'] = best_pred_id
            results.append(metrics)

    # 统计
    tp = len(results)
    fp = len(pred_lanes) - len(matched_pred)
    fn = len(gt_lanes) - tp

    precision = tp / (tp + fp) if (tp + fp) > 0 else 0
    recall = tp / (tp + fn) if (tp + fn) > 0 else 0
    f1 = 2 * precision * recall / (precision + recall) if (precision + recall) > 0 else 0

    print(f"=== 车道线评测结果 ===")
    print(f"Precision: {precision:.3f}")
    print(f"Recall: {recall:.3f}")
    print(f"F1-score: {f1:.3f}")
    print(f"TP: {tp}, FP: {fp}, FN: {fn}")

    if results:
        avg_metrics = {
            'chamfer_dist': np.mean([r['chamfer_dist'] for r in results]),
            'hausdorff_dist': np.mean([r['hausdorff_dist'] for r in results]),
            'iou': np.mean([r['iou'] for r in results]),
            'endpoint_error': np.mean([r['endpoint_error'] for r in results])
        }
        print(f"\n平均几何误差:")
        for k, v in avg_metrics.items():
            print(f"  {k}: {v:.3f}")

    return results


if __name__ == '__main__':
    pass