#!/usr/bin/env bash
set -eo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
load_context "${1:-}" "${2:-}"
shift 2
[[ "$project" != shell ]] || fail "shell 仅提供开发环境，没有项目构建目标。"
dispatch_container build.sh "$@"
load_dependencies
exec 9>/catkin_ws/build/.lock
flock -n 9 || fail "当前项目已有构建任务。"
rm -f /catkin_ws/devel/.tritium-ready
if [[ "$project" == fast-livo2 ]]; then
  export TRITIUM_CMAKE_SOURCE="/catkin_ws/src/$package/core"
  "$script_dir/backends/cmake.sh"
  export CMAKE_PREFIX_PATH="/catkin_ws/install/core:${CMAKE_PREFIX_PATH:-}"
  export LD_LIBRARY_PATH="/catkin_ws/install/core/lib:${LD_LIBRARY_PATH:-}"
  if [[ "$profile" == cpp-cpu ]]; then
    [[ $# == 0 ]] || fail "无ROS构建不接受catkin参数。"
    exit 0
  fi
fi
if [[ "$profile" == ros2-jazzy ]]; then
  [[ $# == 0 ]] || fail "Jazzy 构建不接受 catkin 参数。"
  exec "$script_dir/backends/colcon.sh"
fi

jobs="${BUILD_JOBS:-2}"
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || fail "BUILD_JOBS 必须为正整数。"
catkin_make -C /catkin_ws --force-cmake -j"$jobs" "$@"
printf '%s\n' "$TRITIUM_CACHE_KEY" > /catkin_ws/devel/.tritium-ready
echo "项目编译成功，重启算法即可使用新程序，无需重启容器。"
