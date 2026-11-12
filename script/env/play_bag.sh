#!/usr/bin/env bash
set -eo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
load_context "${1:-}" "${2:-}"
shift 2
[[ "$project" == fast-livo2 && "$profile" != cpp-cpu ]] ||
  fail "该项目/环境没有实时播包入口。"
dispatch_container play_bag.sh "$@"
load_application
if [[ "$profile" == ros2-jazzy ]]; then
  if [[ "${1:-}" == --neutral ]]; then
    shift
    [[ $# -gt 0 ]] || fail "用法：play_bag.sh fast-livo2 ros2-jazzy --neutral <中立数据目录> [回放参数]"
    dataset="$1"
    shift
    if [[ "$dataset" != /* ]]; then dataset="/data/$dataset"; fi
    [[ -d "$dataset" && -r "$dataset/manifest.yaml" ]] || fail "中立数据目录无效：$dataset"
    echo "正在回放中立数据：$dataset；完整性验证应使用 reliable 输入并等待前端排空。"
    exec ros2 run fast_livo_ros2 fast_livo_replayer --dataset "$dataset" "$@"
  fi
  [[ $# -gt 0 ]] || fail "ROS 2 播包需要指定原生 ROS2 bag 目录或文件，不能直接使用旧 .bag。"
  bag="$(resolve_bag "$1")"
  [[ "$bag" != *.bag ]] || fail "ROS 1 .bag 请先转换为中立数据，再使用 --neutral 回放。"
  shift
  exec ros2 bag play "$bag" --clock "$@"
fi
bag_name="HH-LVGO-01_compressed.bag"
if [[ $# -gt 0 && "$1" != -* ]]; then
  bag_name="$1"
  shift
fi
bag="$(resolve_bag "$bag_name")"
rosnode ping -c 1 /laserMapping >/dev/null 2>&1 || fail "算法尚未就绪，请先运行 run.sh fast-livo2。"
echo "正在播放：$bag"
echo "播包结束后，在 run.sh 终端按 Enter 触发离线优化。"
exec rosbag play "$bag" "$@"
