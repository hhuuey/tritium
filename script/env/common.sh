#!/usr/bin/env bash

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd "$script_dir/../.." && pwd)"
deps_root="$repo_dir/.deps"
dataset_root="$repo_dir/.dataset"
result_root="$repo_dir/.result"
image="tritium-ros1:noetic"

fail() {
  echo "$*" >&2
  exit 1
}

load_project() {
  project="${1:-}"
  case "$project" in
    fast-livo2|fast-calib2|shell) source "$script_dir/projects/$project.sh" ;;
    *) fail "用法：$(basename "$0") <fast-livo2|fast-calib2|shell> <环境> [参数]" ;;
  esac
  if [[ -n "${TRITIUM_PROJECT:-}" && "$TRITIUM_PROJECT" != "$project" ]]; then
    fail "当前容器属于 ${TRITIUM_PROJECT}，不能操作 ${project}。"
  fi
}

load_profile() {
  profile="${1:-}"
  case "$profile" in
    ros1-noetic|ros2-jazzy|cpp-cpu|python-cpu|python-cuda) ;;
    *) fail "环境必须为 ros1-noetic、ros2-jazzy、cpp-cpu、python-cpu 或 python-cuda。" ;;
  esac
  source "$script_dir/profiles/$profile.sh"
  if [[ -n "${TRITIUM_PROFILE:-}" && "$TRITIUM_PROFILE" != "$profile" ]]; then
    fail "当前容器属于 ${TRITIUM_PROFILE}，不能操作 ${profile}。"
  fi
}

load_context() {
  load_project "${1:-}"
  load_profile "${2:-}"
  local supported=false candidate
  for candidate in "${supported_profiles[@]}"; do
    if [[ "$candidate" == "$profile" ]]; then supported=true; fi
  done
  [[ "$supported" == true ]] || fail "${project} 尚未适配 ${profile}，不会用其他环境代替运行。"
  if [[ "$project" == fast-livo2 && "$profile" == cpp-cpu ]]; then
    dependencies=(eigen-3.3.7 Sophus gtsam rpg_vikit)
    ros_dependencies=()
  fi
  if [[ "$project" == fast-livo2 && "$profile" == ros2-jazzy ]]; then
    dependencies=(eigen-3.3.7 Sophus gtsam rpg_vikit)
    ros_dependencies=()
  fi
  if [[ "$profile" != ros1-noetic ]]; then
    container="$container-$profile"
  fi
  container="${TRITIUM_CONTAINER:-$container}"
}

require_docker() {
  command -v docker >/dev/null 2>&1 || fail "未找到 Docker，请先安装 Docker Desktop。"
  docker info >/dev/null 2>&1 || fail "Docker 服务不可用，请先启动 Docker Desktop。"
}

require_container() {
  require_docker
  local identity
  identity="$(docker inspect -f '{{index .Config.Labels "tritium.project"}}' "$container")" ||
    fail "无法检查容器 ${container}，请先运行 script/env/start_docker.sh $project $profile"
  [[ "$identity" == "$project" ]] || fail "容器不是新的 $project 开发环境，请先停止旧容器。"
  [[ "$(docker inspect -f '{{.State.Running}}' "$container")" == true ]] ||
    fail "容器未运行，请先运行 script/env/start_docker.sh $project $profile"
  [[ "$(docker inspect -f '{{index .Config.Labels "tritium.profile"}}' "$container")" == "$profile" ]] ||
    fail "容器环境与 $profile 不匹配，请明确停止旧容器后重新创建。"
}

dispatch_container() {
  local command="$1"
  shift
  if [[ -n "${TRITIUM_PROJECT:-}" ]]; then
    return
  fi
  require_container
  local options=(-e "BUILD_JOBS=${BUILD_JOBS:-2}")
  if [[ -t 0 && -t 1 ]]; then
    options+=(-it)
  fi
  exec docker exec "${options[@]}" "$container" "/opt/tritium/scripts/$command" "$project" "$profile" "$@"
}

hash_file() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | cut -d ' ' -f 1
    return
  fi
  shasum -a 256 "$1" | cut -d ' ' -f 1
}

validate_dependency() {
  local name="$1" root="$2" commit patch submodules
  commit="$(jq -er --arg name "$name" '.[$name].commit' "$script_dir/deps.lock.json")"
  [[ -d "$root/$name/.git" || -f "$root/$name/.git" ]] ||
    fail "缺少依赖 ${name}，请先运行 prepare_deps.sh $project $profile"
  [[ "$(git -c safe.directory="$root/$name" -C "$root/$name" rev-parse HEAD)" == "$commit" ]] ||
    fail "$name 版本与锁定清单不一致。"
  submodules="$(git -c safe.directory="$root/$name" -C "$root/$name" submodule status --recursive)"
  if grep -q '^[-+U]' <<< "$submodules"; then
    fail "$name 子模块尚未初始化或版本不一致，请检查源码。"
  fi
  patch="$(jq -r --arg name "$name" '.[$name].patch // empty' "$script_dir/deps.lock.json")"
  if [[ -n "$patch" ]]; then
    [[ -f "$script_dir/patches/$patch" ]] || fail "缺少依赖补丁：$patch"
    git -c safe.directory="$root/$name" -C "$root/$name" apply --reverse --check "$script_dir/patches/$patch" ||
      fail "$name 缺少所需补丁，请先运行 prepare_deps.sh $project $profile"
  fi
  if [[ -n "$(git -c safe.directory="$root/$name" -C "$root/$name" status --porcelain)" ]]; then
    echo "依赖 $name 有本地修改（含已应用补丁），保留并使用当前工作树。" >&2
  fi
}

load_dependencies() {
  exec 8>/opt/tritium/deps-build/.lock
  flock -sn 8 || fail "第三方依赖正在编译，请等待完成后再编译或运行项目。"
  [[ -f /opt/tritium/deps-install/.ready ]] ||
    fail "第三方依赖尚未编译，请运行 build_deps.sh $project $profile"
  [[ "$(cat /opt/tritium/deps-install/.ready)" == "$TRITIUM_CACHE_KEY" ]] ||
    fail "第三方缓存身份不匹配，请重新编译依赖。"
  if [[ "$profile" == ros1-noetic ]]; then
    source /opt/ros/noetic/setup.bash
  fi
  if [[ "$profile" == ros2-jazzy ]]; then
    source /opt/ros/jazzy/setup.bash
  fi
  export CMAKE_PREFIX_PATH="/opt/tritium/deps-install:${CMAKE_PREFIX_PATH:-}"
  export LD_LIBRARY_PATH="/opt/tritium/deps-install/lib:${LD_LIBRARY_PATH:-}"
  export CMAKE_PREFIX_PATH="/catkin_ws/install/core:${CMAKE_PREFIX_PATH:-}"
  export CMAKE_PREFIX_PATH="/catkin_ws/install/io:$CMAKE_PREFIX_PATH"
  export LD_LIBRARY_PATH="/catkin_ws/install/core/lib:${LD_LIBRARY_PATH:-}"
  export PATH="/catkin_ws/install/apps/bin:$PATH"
  if [[ "$profile" == ros1-noetic ]]; then
    source /opt/tritium/ros-deps/devel/setup.bash
  fi
}

load_application() {
  load_dependencies
  [[ -f /catkin_ws/devel/.tritium-ready ]] ||
    fail "项目尚未编译，请运行 build.sh $project $profile"
  [[ "$(cat /catkin_ws/devel/.tritium-ready)" == "$TRITIUM_CACHE_KEY" ]] ||
    fail "项目缓存身份不匹配，请重新编译项目。"
  [[ ! /opt/tritium/deps-install/.ready -nt /catkin_ws/devel/.tritium-ready ]] ||
    fail "第三方依赖已更新，请先运行 build.sh $project $profile"
  case "$profile" in
    ros1-noetic) source /catkin_ws/devel/setup.bash ;;
    ros2-jazzy) source /catkin_ws/install/ros2/setup.bash ;;
  esac
}

resolve_bag() {
  local bag="${1:-HH-LVGO-01_compressed.bag}"
  if [[ "$bag" != /* ]]; then
    bag="/data/$bag"
  fi
  [[ -r "$bag" && ( -f "$bag" || ( "$profile" == ros2-jazzy && -d "$bag" ) ) ]] ||
    fail "找不到或无法读取数据包：$bag"
  printf '%s\n' "$bag"
}

run_roslaunch() {
  local launch_pid="" launch_exit=0 status=0
  interrupt_launch() {
    launch_exit="$1"
    if [[ -n "$launch_pid" ]] && kill -0 "$launch_pid" 2>/dev/null; then
      kill -TERM "$launch_pid"
    fi
  }
  trap 'interrupt_launch 130' INT
  trap 'interrupt_launch 143' TERM
  # 保留终端输入，并让父进程在 roslaunch 初始化期间也能转发中断。
  roslaunch "$@" <&0 &
  launch_pid=$!
  if [[ "$launch_exit" != 0 ]]; then
    interrupt_launch "$launch_exit"
  fi
  while true; do
    if wait "$launch_pid"; then
      status=0
      break
    else
      status=$?
    fi
    if ! kill -0 "$launch_pid" 2>/dev/null; then
      break
    fi
  done
  trap - INT TERM
  if [[ "$launch_exit" != 0 ]]; then
    return "$launch_exit"
  fi
  return "$status"
}
