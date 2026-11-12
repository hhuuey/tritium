#!/usr/bin/env bash
# 仅在独立候选 Jazzy 容器内使用；不构建核心、不修改依赖或数据集。
set -eo pipefail

root=/catkin_ws/src/fast_livo
deps=${FAST_LIVO_VALIDATION_DEPS:-/opt/tritium/eigen33-check/install}
core=${FAST_LIVO_VALIDATION_CORE:-/catkin_ws/install/eigen33/core}
io=${FAST_LIVO_VALIDATION_IO:-/catkin_ws/install/eigen33/io}
source /opt/ros/jazzy/setup.bash
export CMAKE_PREFIX_PATH="$core:$io:$deps:/opt/ros/jazzy"
export LD_LIBRARY_PATH="$deps/lib:$core/lib:$io/lib:${LD_LIBRARY_PATH:-}"
export ROS_DOMAIN_ID=73 ROS_HOME=/results/ros
export CMAKE_BUILD_PARALLEL_LEVEL=1 MAKEFLAGS="-j1 -l1"
mkdir -p /results/tmp
export TMPDIR=/results/tmp

build() {
  test -f "$core/lib/libfast_livo_core.a"
  test -f "$io/lib/libfast_livo_io.a"
  test -f "$deps/share/eigen3/cmake/Eigen3Config.cmake"
  colcon --log-base /results/colcon build --executor sequential \
    --base-paths "$root/adapters/ros2" "$root/adapters/ros2_interfaces" \
    --build-base /jbuild --install-base /jinstall \
    --cmake-args -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "-DEigen3_DIR=$deps/share/eigen3/cmake"
  colcon --log-base /results/colcon test --executor sequential \
    --base-paths "$root/adapters/ros2" "$root/adapters/ros2_interfaces" \
    --build-base /jbuild --install-base /jinstall --return-code-on-test-failure
  colcon test-result --test-result-base /jbuild --verbose
  source /jinstall/setup.bash
  export LD_LIBRARY_PATH="$deps/lib:$core/lib:$io/lib:${LD_LIBRARY_PATH:-}"
  cp /jbuild/fast_livo_ros2/compile_commands.json /results/compile_commands.json
  {
    c++ -dumpfullversion
    grep -E '^#define EIGEN_(WORLD|MAJOR|MINOR)_VERSION' "$deps/include/eigen3/Eigen/src/Core/util/Macros.h"
    pkg-config --modversion pcl_common opencv4
    grep -E '^#define (FLANN_VERSION_|BOOST_LIB_VERSION)' /usr/include/flann/config.h /usr/include/boost/version.hpp
    grep -E '^(Eigen3_DIR|PCL_DIR|OpenCV_DIR|FLANN_DIR|Boost_INCLUDE_DIR|GTSAM_DIR|fast_livo_core_DIR|fast_livo_io_DIR):' \
      /jbuild/fast_livo_ros2/CMakeCache.txt
    if grep -q '/core/src/' /results/compile_commands.json; then
      echo "禁止适配器重新编译算法源文件" >&2
      return 1
    fi
    printf 'algorithm_source_units=0\n'
    ldd /jinstall/fast_livo_ros2/lib/fast_livo_ros2/fastlivo_mapping
    ldd /jinstall/fast_livo_ros2/lib/fast_livo_ros2/fast_livo_replayer
  } > /results/candidate-linkage.txt
}

monotonic_seconds() {
  local uptime unused
  read -r uptime unused < /proc/uptime
  printf '%s\n' "${uptime%%.*}"
}

wait_owned() {
  local pid=$1 limit=$2 deadline=$(($(monotonic_seconds) + $2))
  while kill -0 "$pid" 2>/dev/null; do
    if (( $(monotonic_seconds) >= deadline )); then
      echo "等待本次 PID=$pid 超时（${limit}s）" >&2
      kill -KILL "$pid" 2>/dev/null || true
      wait "$pid" 2>/dev/null || true
      return 124
    fi
    sleep 0.2
  done
  wait "$pid"
}

node_pid= fox_pid= gate_pid= tf_pid= replay_pid=
cleanup() {
  local pid
  for pid in "$replay_pid" "$node_pid" "$fox_pid" "$gate_pid" "$tf_pid"; do
    if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
      kill -INT "$pid" 2>/dev/null || true
      if [[ "$pid" == "$node_pid" ]]; then wait_owned "$pid" "${FINISH_TIMEOUT:-300}" || true
      else wait_owned "$pid" 30 || true; fi
    fi
  done
}

foxglove_ready() {
  local run=$1
  local deadline=$(($(monotonic_seconds) + 30))
  while (( $(monotonic_seconds) < deadline )); do
    curl --silent --show-error --include --no-buffer --max-time 1 \
      -H 'Connection: Upgrade' -H 'Upgrade: websocket' \
      -H 'Sec-WebSocket-Version: 13' \
      -H 'Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==' \
      -H "Sec-WebSocket-Protocol: ${FOXGLOVE_PROTOCOL:-foxglove.sdk.v1}" \
      http://127.0.0.1:8765/ > "$run/foxglove-handshake.txt" 2> "$run/foxglove-curl.log" || true
    if grep -aq '101 Switching Protocols' "$run/foxglove-handshake.txt" &&
       grep -aEq '"op"[[:space:]]*:[[:space:]]*"serverInfo"' "$run/foxglove-handshake.txt"; then
      return 0
    fi
    if ! kill -0 "$node_pid" 2>/dev/null; then cat "$run/node.log" >&2; return 1; fi
    kill -0 "$fox_pid"
    sleep 0.2
  done
  echo "Foxglove 未完成 websocket 握手或未返回 serverInfo" >&2
  return 1
}

run() {
  local window=${1:-12} run=${2:-/results/run-12s}
  local expected=${EXPECTED_TRAJECTORY:-/results/native-12s.txt}
  local expected_backend=${EXPECTED_BACKEND_TRAJECTORY:-$expected}
  local expected_odometry=${EXPECTED_ODOMETRY_TRAJECTORY:-$expected_backend}
  local gate_done=false
  local duration_args=()
  if [[ "$window" != full ]]; then duration_args=(--duration "$window"); fi
  if [[ "$window" == full && -z "${EXPECTED_TRAJECTORY:-}" ]]; then
    echo "full 验收必须显式指定对应完整 native EXPECTED_TRAJECTORY" >&2; return 1
  fi
  if [[ "$window" == full && -z "${EXPECTED_BACKEND_TRAJECTORY:-}" ]]; then
    echo "full 验收必须显式指定2737关键帧的 EXPECTED_BACKEND_TRAJECTORY" >&2; return 1
  fi
  test -s "$expected"
  test -s "$expected_backend"
  if [[ -e "$run" ]]; then echo "拒绝覆盖已有验收目录: $run" >&2; return 1; fi
  mkdir -p "$run"
  source /jinstall/setup.bash
  export LD_LIBRARY_PATH="$deps/lib:$core/lib:$io/lib:${LD_LIBRARY_PATH:-}"
  local binary=/jinstall/fast_livo_ros2/lib/fast_livo_ros2
  trap cleanup EXIT
  trap 'exit 130' INT TERM
  "$binary/fastlivo_mapping" --ros-args \
    --params-file /jinstall/fast_livo_ros2/share/fast_livo_ros2/config/HH.yaml \
    -p use_sim_time:=true -p adapter.input_reliability:=reliable -p adapter.input_statistics:=true \
    "-p" "laserMapping.outputfilepath:=$run" \
    > "$run/node.log" 2>&1 &
  node_pid=$!
  /opt/ros/jazzy/lib/foxglove_bridge/foxglove_bridge --ros-args \
    -p address:=0.0.0.0 -p port:=8765 > "$run/foxglove.log" 2>&1 &
  fox_pid=$!
  "$binary/fast_livo_receipt_gate" --ros-args \
    "-p" "expected_trajectory:=$expected" "-p" "published_statistics:=$run/published-inputs.json" \
    "-p" "expected_odometry_trajectory:=$expected_odometry" \
    "-p" "frontend_trajectory:=$run/Log/result/Grass01.txt" \
    "-p" "request_optimization:=true" \
    "-p" "output_directory:=$run" "-p" "ready_file:=$run/receipt-gate.ready" \
    "-p" "timeout:=${DRAIN_TIMEOUT:-180}.0" > "$run/receipt-gate.log" 2>&1 &
  gate_pid=$!
  timeout 120 ros2 topic echo --no-daemon --once /tf tf2_msgs/msg/TFMessage \
    > "$run/tf.yaml" 2> "$run/tf-error.log" &
  tf_pid=$!
  printf 'node=%s\nfoxglove=%s\nreceipt_gate=%s\ntf_probe=%s\n' \
    "$node_pid" "$fox_pid" "$gate_pid" "$tf_pid" > "$run/owned-pids.txt"
  foxglove_ready "$run"
  local ready_deadline=$(($(monotonic_seconds) + 30))
  until [[ -f "$run/receipt-gate.ready" ]]; do
    kill -0 "$gate_pid"
    kill -0 "$node_pid"
    if (( $(monotonic_seconds) >= ready_deadline )); then echo "完整 odometry 验收订阅未匹配" >&2; return 1; fi
    sleep 0.1
  done
  "$binary/fast_livo_replayer" --dataset /neutral --rate "${REPLAY_RATE:-1}" \
    --wait-timeout 60 --drain-time 0 --stats-file "$run/published-inputs.json" \
    "${duration_args[@]}" > "$run/replay.log" 2>&1 &
  replay_pid=$!
  printf 'replayer=%s\n' "$replay_pid" >> "$run/owned-pids.txt"
  while kill -0 "$replay_pid" 2>/dev/null; do
    if ! kill -0 "$node_pid" 2>/dev/null; then
      echo "定位节点提前退出，停止本次回放并保留现场" >&2
      return 1
    fi
    if [[ "$gate_done" == false ]] && ! kill -0 "$gate_pid" 2>/dev/null; then
      if ! wait "$gate_pid"; then echo "验收 gate 失败，停止回放并保留现场" >&2; return 1; fi
      gate_pid=
      gate_done=true
    fi
    sleep 0.2
  done
  wait "$replay_pid"
  replay_pid=
  if [[ "$gate_done" == false ]]; then wait_owned "$gate_pid" "${DRAIN_TIMEOUT:-180}"; fi
  gate_pid=
  wait_owned "$tf_pid" 5
  tf_pid=
  grep -q 'child_frame_id: aft_mapped' "$run/tf.yaml"
  kill -0 "$node_pid"
  grep -Eq 'success[:=][[:space:]]*(True|true)' "$run/optimize-service.txt"
  kill -INT "$node_pid"
  wait_owned "$node_pid" "${FINISH_TIMEOUT:-300}"
  node_pid=
  kill -INT "$fox_pid"
  wait_owned "$fox_pid" 30
  fox_pid=
  test -s "$run/Log/result/Grass01.txt"
  test -s "$run/TUM/opt_trajectory_after.txt"
  test -s "$run/global_pcd/after_optimization.pcd"
  awk 'NF && $1 !~ /^#/ {print $1}' "$expected" > "$run/native-axis.txt"
  awk 'NF && $1 !~ /^#/ {print $1}' "$run/Log/result/Grass01.txt" > "$run/frontend-axis.txt"
  awk 'NF && $1 !~ /^#/ {print $1}' "$run/TUM/opt_trajectory_after.txt" > "$run/backend-axis.txt"
  awk 'NF && $1 !~ /^#/ {print $1}' "$expected_backend" > "$run/native-backend-axis.txt"
  cmp "$run/native-axis.txt" "$run/frontend-axis.txt"
  cmp "$run/native-backend-axis.txt" "$run/backend-axis.txt"
  "$binary/fast_livo_compare" trajectory "$expected" "$run/Log/result/Grass01.txt" "$run/frontend-comparison.json"
  "$binary/fast_livo_compare" trajectory "$expected_backend" "$run/TUM/opt_trajectory_after.txt" \
    "$run/backend-comparison.json"
  "$binary/fast_livo_compare" map "$run/global_pcd/after_optimization.pcd" "$run/map-completeness.json"
  {
    printf 'node_exit=0\nreplayer_exit=0\nfoxglove_exit=0\n'
    printf 'window=%s\nrate=%s\nROS_DOMAIN_ID=%s\n' "$window" "${REPLAY_RATE:-1}" "$ROS_DOMAIN_ID"
    cat "$run/published-inputs.json" "$run/received-inputs.json" "$run/drain-result.json"
    grep -E 'EOF|输入窗口完成' "$run/replay.log"
    wc -l "$run/Log/result/Grass01.txt" "$run/TUM/opt_trajectory_after.txt"
    printf '可靠输入完整、native完整时间轴、优化服务、TF、Foxglove和安全退出均通过。\n'
  } | tee "$run/summary.txt"
}

case "${1:-}" in
  build) build ;;
  run) shift; run "$@" ;;
  *) echo "用法: bash $0 build | run [12|full] [NEW_RESULT_DIRECTORY]" >&2; exit 2 ;;
esac
