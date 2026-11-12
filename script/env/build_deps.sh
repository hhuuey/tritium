#!/usr/bin/env bash
set -eo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
load_context "${1:-}" "${2:-}"
[[ $# == 2 ]] || fail "用法：build_deps.sh <项目> <环境>"
[[ "$project" != shell ]] || fail "shell 环境不需要项目第三方编译。"
dispatch_container build_deps.sh

jobs="${BUILD_JOBS:-2}"
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || fail "BUILD_JOBS 必须为正整数。"
mkdir -p /opt/tritium/deps-build /opt/tritium/deps-install /opt/tritium/ros-deps/src
exec 9>/opt/tritium/deps-build/.lock
flock -n 9 || fail "第三方缓存正被编译或运行中的项目使用，请先停止相关任务。"
rm -f /opt/tritium/deps-install/.ready
for dependency in "${dependencies[@]}"; do
  validate_dependency "$dependency" /deps
done
if [[ "$profile" == ros1-noetic ]]; then
  source /opt/ros/noetic/setup.bash
fi
export CMAKE_PREFIX_PATH="/opt/tritium/deps-install:${CMAKE_PREFIX_PATH:-}"
export LD_LIBRARY_PATH="/opt/tritium/deps-install/lib:${LD_LIBRARY_PATH:-}"
eigen_options=()
if [[ "$project" == fast-livo2 && "$profile" != ros1-noetic ]]; then
  eigen_options=(-DEigen3_DIR=/opt/tritium/deps-install/share/eigen3/cmake
    -DEIGEN3_INCLUDE_DIR=/opt/tritium/deps-install/include/eigen3)
fi

cmake_dependency() {
  local name="$1"
  shift
  echo "配置第三方依赖：$name"
  cmake -S "/deps/$name" -B "/opt/tritium/deps-build/$name" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/tritium/deps-install \
    "${eigen_options[@]}" "$@"
}

for dependency in "${dependencies[@]}"; do
  case "$dependency" in
    eigen-3.3.7)
      cmake_dependency eigen-3.3.7 -DBUILD_TESTING=OFF
      cmake --install /opt/tritium/deps-build/eigen-3.3.7
      export Eigen3_DIR=/opt/tritium/deps-install/share/eigen3/cmake
      ;;
    Sophus)
      cmake_dependency Sophus
      cmake --build /opt/tritium/deps-build/Sophus --target Sophus -j"$jobs"
      cmake --install /opt/tritium/deps-build/Sophus
      mkdir -p /opt/tritium/deps-install/lib/cmake/Sophus
      printf '%s\n' \
        'set(Sophus_INCLUDE_DIRS "/opt/tritium/deps-install/include")' \
        'set(Sophus_INCLUDE_DIR "${Sophus_INCLUDE_DIRS}")' \
        'set(Sophus_LIBRARIES "/opt/tritium/deps-install/lib/libSophus.so")' \
        'set(Sophus_LIBRARY "${Sophus_LIBRARIES}")' \
        > /opt/tritium/deps-install/lib/cmake/Sophus/SophusConfig.cmake
      ;;
    gtsam)
      cmake_dependency gtsam -DGTSAM_BUILD_WITH_MARCH_NATIVE=OFF -DGTSAM_USE_SYSTEM_EIGEN=ON \
        -DGTSAM_BUILD_TESTS=OFF -DGTSAM_BUILD_EXAMPLES_ALWAYS=OFF \
        -DGTSAM_BUILD_UNSTABLE=OFF -DGTSAM_WITH_TBB=OFF
      cmake --build /opt/tritium/deps-build/gtsam -j"$jobs"
      cmake --install /opt/tritium/deps-build/gtsam
      ;;
    Livox-SDK)
      cmake_dependency Livox-SDK
      cmake --build /opt/tritium/deps-build/Livox-SDK -j"$jobs"
      cmake --install /opt/tritium/deps-build/Livox-SDK
      ;;
    rpg_vikit)
      cmake -S /deps/rpg_vikit/vikit_common -B /opt/tritium/deps-build/vikit_common \
        -DCMAKE_BUILD_TYPE=Release -DUSE_ROS=OFF -DCMAKE_INSTALL_PREFIX=/opt/tritium/deps-install \
        "${eigen_options[@]}"
      cmake --build /opt/tritium/deps-build/vikit_common --target vikit_common -j"$jobs"
      cmake --install /opt/tritium/deps-build/vikit_common
      ;;
    gnss_comm|livox_ros_driver) ;;
    *) fail "未定义的第三方构建步骤：$dependency" ;;
  esac
done

for dependency in "${ros_dependencies[@]}"; do
  target="/opt/tritium/ros-deps/src/$dependency"
  if [[ -e "$target" || -L "$target" ]]; then
    [[ -L "$target" && "$(readlink "$target")" == "/deps/$dependency" ]] ||
      fail "第三方工作空间已有冲突路径：$target"
  else
    ln -s "/deps/$dependency" "$target"
  fi
done
if [[ "$profile" == ros1-noetic ]]; then
  catkin_make -C /opt/tritium/ros-deps --force-cmake -j"$jobs"
fi
printf '%s\n' "$TRITIUM_CACHE_KEY" > /opt/tritium/deps-install/.ready
echo "第三方依赖编译成功，请继续运行 build.sh $project $profile"
