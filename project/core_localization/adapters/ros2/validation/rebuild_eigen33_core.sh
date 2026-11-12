#!/usr/bin/env bash
# 仅重建原受控 Eigen33 core；禁止混入正式 cpp 安装库。
set -euo pipefail
root=/catkin_ws/src/fast_livo
deps=/opt/tritium/eigen33-check/install
build=/catkin_ws/build/eigen33-core
install=/catkin_ws/install/eigen33/core
test -f "$build/CMakeCache.txt"
test -f "$deps/lib/libSophus.so"
test -f "$root/core/src/validation.cpp"
source /opt/tritium/eigen33-check/environment.sh
mkdir -p "$build/validation-tmp"
export TMPDIR="$build/validation-tmp"
export CMAKE_BUILD_PARALLEL_LEVEL=1 MAKEFLAGS="-j1 -l1"
cmake -S "$root/core" -B "$build" -DCMAKE_BUILD_TYPE=Release \
  "-DCMAKE_INSTALL_PREFIX=$install" -DBUILD_TESTING=ON -DFAST_LIVO_FP_MODEL=legacy \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_EXPORT_NO_PACKAGE_REGISTRY=ON -DCMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY=ON \
  "-DEigen3_DIR=$deps/share/eigen3/cmake" "-DEIGEN3_INCLUDE_DIR=$deps/include/eigen3" \
  "-DSophus_DIR=$deps/lib/cmake/Sophus" "-DGTSAM_DIR=$deps/lib/cmake/GTSAM" \
  "-Dvikit_common_DIR=$deps/lib/cmake/vikit_common"
cmake --build "$build" --parallel 1
ctest --test-dir "$build" --output-on-failure --parallel 1
cmake --install "$build"
grep '"command".*validation.cpp' "$build/compile_commands.json" |
  tee "$build/validation-compile-command.txt"
grep -q -- '-fno-fast-math' "$build/validation-compile-command.txt"
grep -q -- '-std=c++17' "$build/validation-compile-command.txt"
"$install/bin/fast_livo_numerics"
sha256sum "$install/lib/libfast_livo_core.a"
