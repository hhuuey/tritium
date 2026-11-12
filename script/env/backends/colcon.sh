#!/usr/bin/env bash
set -eo pipefail
source /opt/ros/jazzy/setup.bash
jobs="${BUILD_JOBS:-2}"
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || { echo "BUILD_JOBS 必须为正整数。" >&2; exit 1; }
export CMAKE_BUILD_PARALLEL_LEVEL="$jobs"
source_dir="/catkin_ws/src/fast_livo/adapters/ros2"
colcon --log-base /results/colcon build --executor sequential \
  --base-paths "$source_dir" "$source_dir/../ros2_interfaces" \
  --build-base /catkin_ws/build/ros2 --install-base /catkin_ws/install/ros2 \
  --cmake-args -DCMAKE_BUILD_TYPE=Release
colcon --log-base /results/colcon test --base-paths "$source_dir" "$source_dir/../ros2_interfaces" \
  --build-base /catkin_ws/build/ros2 --install-base /catkin_ws/install/ros2 \
  --return-code-on-test-failure
colcon test-result --test-result-base /catkin_ws/build/ros2 --verbose
printf '%s\n' "$TRITIUM_CACHE_KEY" > /catkin_ws/devel/.tritium-ready
echo "Jazzy 完整适配器构建和测试完成。"
