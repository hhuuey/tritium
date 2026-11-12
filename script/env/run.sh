#!/usr/bin/env bash
set -eo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
load_context "${1:-}" "${2:-}"
shift 2
if [[ "$profile" != cpp-cpu ]]; then
  [[ -t 0 && -t 1 ]] || fail "请在交互式终端运行算法。"
fi
dispatch_container run.sh "$@"
if [[ "$project" == shell ]]; then
  exec bash
fi
load_application
if [[ "$profile" == cpp-cpu ]]; then
  exec fast_livo_offline "$@"
fi
if [[ "$profile" == ros2-jazzy ]]; then
  exec ros2 launch fast_livo_ros2 HH.launch.xml outputfilepath:=/results "$@"
fi

if [[ "$project" == fast-calib2 ]]; then
  mode="${1:-single}"
  if [[ $# -gt 0 ]]; then
    shift
  fi
  case "$mode" in
    single)
      bag_file="/data/avia/mid.bag"
      image_file="/data/avia/mid.bmp"
      for argument in "$@"; do
        case "$argument" in
          bag_path:=*) bag_file="${argument#bag_path:=}" ;;
          image_path:=*) image_file="${argument#image_path:=}" ;;
        esac
      done
      [[ -f "$bag_file" && -r "$bag_file" ]] || fail "标定数据包不存在或不可读：$bag_file"
      [[ -f "$image_file" && -r "$image_file" ]] || fail "标定图像不存在或不可读：$image_file"
      run_roslaunch fast_calib calib.launch rviz:=false \
        bag_path:="$bag_file" image_path:="$image_file" output_path:=/results "$@"
      exit
      ;;
    multi)
      run_roslaunch fast_calib multi_calib.launch rviz:=false output_path:=/results "$@"
      exit
      ;;
    *) fail "用法：run.sh fast-calib2 [single|multi] [launch参数:=值]" ;;
  esac
fi

if rosnode ping -c 1 /laserMapping >/dev/null 2>&1; then
  fail "算法已经运行，请先在原 run.sh 终端按 Ctrl+C。"
fi
if [[ $# -gt 0 ]]; then
  bag="$(resolve_bag "$1")"
  shift
  play_when_ready() {
    readiness_pid=""
    readiness_signal=0
    # timeout 使用独立进程组，退出等待时需要显式回收它。
    cleanup_readiness() {
      if [[ -z "$readiness_pid" ]]; then
        return
      fi
      if kill -0 "$readiness_pid" 2>/dev/null; then
        kill -TERM "$readiness_pid"
      fi
      if wait "$readiness_pid"; then
        return
      else
        echo "节点就绪等待已结束，退出码：$?" >&2
      fi
    }
    trap cleanup_readiness EXIT
    trap 'readiness_signal=130' INT
    trap 'readiness_signal=143' TERM
    timeout 60 bash -c '
      until rosnode ping -c 1 /laserMapping >/dev/null 2>&1 &&
            rosnode ping -c 1 /foxglove_bridge >/dev/null 2>&1 &&
            rosnode ping -c 1 /republish >/dev/null 2>&1; do
        sleep 0.5
      done
    ' &
    readiness_pid=$!
    if [[ "$readiness_signal" != 0 ]]; then
      exit "$readiness_signal"
    fi
    readiness_status=0
    wait "$readiness_pid" || readiness_status=$?
    if [[ "$readiness_signal" != 0 ]]; then
      exit "$readiness_signal"
    fi
    if [[ "$readiness_status" != 0 ]]; then
      echo "算法或 Foxglove 在 60 秒内未就绪，已取消自动播包，请检查日志。" >&2
      return 1
    fi
    trap - EXIT INT TERM
    exec "$script_dir/play_bag.sh" "$project" "$profile" "$bag" "$@"
  }
  play_when_ready "$@" &
  player_pid=$!
  cleanup_player() {
    if kill -0 "$player_pid" 2>/dev/null; then
      kill "$player_pid"
    fi
    if wait "$player_pid"; then
      return
    else
      echo "播包进程已中断或失败，退出码：$?" >&2
    fi
  }
  trap cleanup_player EXIT
fi
echo "Foxglove：ws://localhost:$TRITIUM_FOXGLOVE_PORT"
echo "播包结束后按 Enter 触发优化；Ctrl+C 仅停止算法，保留容器。"
run_roslaunch --sigint-timeout 600 --sigterm-timeout 30 fast_livo HH.launch foxglove:=true outputfilepath:=/results
