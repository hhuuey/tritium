#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
load_context "${1:-}" "${2:-}"
[[ $# == 2 ]] || fail "用法：start_docker.sh <项目> <环境>"
[[ -z "${TRITIUM_PROJECT:-}" ]] || fail "请在宿主机启动容器。"
require_docker
endpoint="$(docker context inspect --format '{{.Endpoints.docker.Host}}')"
[[ "$endpoint" == unix://* ]] ||
  fail "请在 Docker 主机本地或 Remote SSH 终端运行；本机目录不能自动挂载到远端 Docker。"
if [[ "$profile" == python-cuda && "$(uname -s)" != Linux ]]; then
  fail "CUDA 环境必须在 Linux NVIDIA 主机执行，Mac 不支持 CUDA 透传。"
fi
docker image inspect "$image" >/dev/null || fail "请先运行 script/env/build_image.sh $profile"
[[ -d "$data_dir" ]] || fail "数据目录不存在：${data_dir}，请准备数据或设置对应 DATA_DIR 环境变量。"
command -v jq >/dev/null 2>&1 || fail "缺少 jq，请先安装 jq。"
if [[ "$project" != shell ]]; then
  for dependency in "${dependencies[@]}"; do
    validate_dependency "$dependency" "$deps_root"
  done
fi
hardware_options=(--label "tritium.profile=$profile")
if [[ "$profile" == ros2-jazzy ]]; then
  hardware_options+=(--shm-size 512m)
fi
if [[ "$profile" == python-cuda ]]; then
  [[ "$(uname -s)" == Linux ]] || fail "CUDA 环境必须在 Linux NVIDIA 主机执行，Mac 不支持 CUDA 透传。"
  [[ "$(docker info -f '{{.OSType}}')" == linux ]] || fail "CUDA 需要 Linux 容器。"
  hardware_options+=(--gpus all)
fi

code_cli="$(command -v code || true)"
if [[ -z "$code_cli" ]]; then
  code_cli="/Applications/Visual Studio Code.app/Contents/Resources/app/bin/code"
fi
[[ -x "$code_cli" ]] || fail "未找到 VS Code，请安装 VS Code 并将 code 命令加入 PATH。"
extensions="$("$code_cli" --list-extensions)"
grep -Fxiq 'ms-vscode-remote.remote-containers' <<< "$extensions" ||
  fail "请先安装 VS Code Dev Containers 扩展。"

image_id="$(docker image inspect -f '{{.Id}}' "$image")"
architecture="$(docker image inspect -f '{{.Architecture}}' "$image")"
lock_hash="$({
  cat "$script_dir/deps.lock.json" "$script_dir/projects/$project.sh" "$script_dir/profiles/$profile.sh"
  for patch in "$script_dir"/patches/*.patch; do
    [[ ! -f "$patch" ]] || cat "$patch"
  done
} | hash_file /dev/stdin)"
cache_key="$project-$profile-$architecture-${image_id:7:12}-${lock_hash:0:12}"
mkdir -p "$result_dir/Log/PCD" "$result_dir/Log/result" "$result_dir/Log/Colmap/images" \
  "$result_dir/Log/Colmap/sparse/0" "$result_dir/ros" "$result_dir/output"

docker run --rm --detach --init --name "$container" \
  --label "tritium.project=$project" \
  "${hardware_options[@]}" \
  -p "127.0.0.1:$foxglove_port:8765" \
  -e ROS_HOME=/results/ros -e "TRITIUM_PROJECT=$project" -e "TRITIUM_CACHE_KEY=$cache_key" \
  -e "TRITIUM_FOXGLOVE_PORT=$foxglove_port" \
  -e "TRITIUM_PROFILE=$profile" \
  -v "$script_dir:/opt/tritium/scripts:ro" \
  -v "$deps_root:/deps" \
  -v "$repo_dir/$project_path:/catkin_ws/src/$package" \
  -v "$data_dir:/data:ro" -v "$result_dir:/results" \
  -v "$result_dir/Log:/catkin_ws/src/$package/Log" \
  -v "$result_dir/output:/catkin_ws/src/$package/output" \
  -v "$cache_key-deps-build:/opt/tritium/deps-build" \
  -v "$cache_key-deps-install:/opt/tritium/deps-install" \
  -v "$cache_key-ros-deps:/opt/tritium/ros-deps" \
  -v "$cache_key-build:/catkin_ws/build" -v "$cache_key-devel:/catkin_ws/devel" \
  -v "$cache_key-install:/catkin_ws/install" \
  -v "$vscode_volume:/root/.vscode-server" \
  "$image"

docker exec "$container" true
echo "容器已待命，未启动算法。第三方源码：$deps_root"
if [[ "$profile" == python-cuda ]]; then
  if ! docker exec "$container" nvidia-smi; then
    docker stop --timeout 5 "$container"
    fail "GPU 不可用，验证失败的容器已停止，不会切换为 CPU。"
  fi
fi
echo "当前环境：${profile}；手动编译入口：$script_dir/build.sh $project $profile"
authority="$(printf '{"containerName":"/%s"}' "$container" | od -An -tx1 | tr -d ' \n')"
if ! "$code_cli" --new-window --folder-uri "vscode-remote://attached-container+$authority/catkin_ws/src/$package"; then
  fail "容器已启动，但 VS Code 自动连接失败，请手动附加到 ${container}。"
fi
