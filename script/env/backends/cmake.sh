#!/usr/bin/env bash
set -eo pipefail

[[ -f "${TRITIUM_CMAKE_SOURCE:-/nonexistent}/CMakeLists.txt" ]] || {
  echo "未配置独立 CMake 核心源码目录，不能编译尚未迁移的算法。" >&2
  exit 1
}
core_options=(-DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/catkin_ws/install/core)
if [[ "$TRITIUM_PROFILE" != ros1-noetic ]]; then
  core_options+=(-DEigen3_DIR=/opt/tritium/deps-install/share/eigen3/cmake)
fi
cmake -S "$TRITIUM_CMAKE_SOURCE" -B /catkin_ws/build/core "${core_options[@]}"
cmake --build /catkin_ws/build/core --parallel "${BUILD_JOBS:-2}" "$@"
(cd /catkin_ws/build/core && ctest --output-on-failure)
cmake --install /catkin_ws/build/core
manifest="$(/catkin_ws/install/core/bin/fast_livo_numerics)"
if [[ "$TRITIUM_PROFILE" != ros1-noetic &&
      "$(jq -er '.eigen' <<< "$manifest")" != 3.3.7 ]]; then
  echo "当前默认兼容配置需要 Eigen 3.3.7，实际版本不符；不标记构建就绪。" >&2
  exit 1
fi
export CMAKE_PREFIX_PATH="/catkin_ws/install/core:${CMAKE_PREFIX_PATH:-}"
project_dir="$(dirname "$TRITIUM_CMAKE_SOURCE")"
cmake -S "$project_dir/io" -B /catkin_ws/build/io \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/catkin_ws/install/io
cmake --build /catkin_ws/build/io --parallel "${BUILD_JOBS:-2}"
(cd /catkin_ws/build/io && ctest --output-on-failure)
cmake --install /catkin_ws/build/io
export CMAKE_PREFIX_PATH="/catkin_ws/install/io:$CMAKE_PREFIX_PATH"
cmake -S "$project_dir/apps/offline" -B /catkin_ws/build/offline \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/catkin_ws/install/apps
cmake --build /catkin_ws/build/offline --parallel "${BUILD_JOBS:-2}"
cmake --install /catkin_ws/build/offline
if [[ "$TRITIUM_PROFILE" == ros1-noetic ]]; then
  cmake -S "$project_dir/apps/bag_converter" -B /catkin_ws/build/converter \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/catkin_ws/install/apps
  cmake --build /catkin_ws/build/converter --parallel "${BUILD_JOBS:-2}"
  (cd /catkin_ws/build/converter && ctest --output-on-failure)
  cmake --install /catkin_ws/build/converter
fi
if [[ "$TRITIUM_PROFILE" == cpp-cpu ]]; then
  printf '%s\n' "$TRITIUM_CACHE_KEY" > /catkin_ws/devel/.tritium-ready
fi
echo "完整算法库与纯安装前缀消费者构建、测试完成。"
