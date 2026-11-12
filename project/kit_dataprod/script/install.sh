#!/usr/bin/env bash
set -euo pipefail

# 定位项目根目录，保证脚本可以从任意工作目录执行。
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
project_root=$(dirname -- "$script_dir")
source_config_root=${DAGSTER_PIPELINE_CONFIG_ROOT:-$project_root/config}
export DAGSTER_PIPELINE_RUNTIME_ROOT=${DAGSTER_PIPELINE_RUNTIME_ROOT:-$project_root}

source "$script_dir/native_build_env.sh"
native_prepare_environment

# 首次 apt update 也可能读取已有 NVIDIA 源，提前接入云桌面可用的下载代理。
source "$script_dir/native_calibration_deps.sh"
source "$script_dir/native_json_deps.sh"

# 安装 Python、rsync 和全部 native 工程所需的 Ubuntu/Debian 系统软件包。
system_packages=(
    python3
    python3-venv
    build-essential
    cmake
    ninja-build
    pkg-config
    libeigen3-dev
    libopencv-dev
    libpcl-dev
    libyaml-cpp-dev
    libpng-dev
    libssl-dev
    libfmt-dev
    libffi-dev
    ca-certificates
    curl
    gnupg
    rsync
)
missing_packages=()
if [[ "${DAGSTER_PIPELINE_SKIP_SYSTEM_DEPS:-0}" != "1" ]]; then
    if ! command -v apt-get >/dev/null 2>&1 || ! command -v dpkg-query >/dev/null 2>&1; then
        echo "自动安装系统依赖需要 apt-get 和 dpkg-query。" >&2
        echo "请手动安装：${system_packages[*]}" >&2
        exit 1
    fi

    for package_name in "${system_packages[@]}"; do
        if [[ "$(dpkg-query -W -f='${Status}' "$package_name" 2>/dev/null)" != \
            "install ok installed" ]]; then
            missing_packages+=("$package_name")
        fi
    done

    json_packages_ready=1
    for requirement in "${native_json_requirements[@]}"; do
        native_json_package_ready "${requirement%%:*}" "${requirement#*:}" || json_packages_ready=0
    done
    if (( ${#missing_packages[@]} > 0 || json_packages_ready == 0 )); then
        calibration_apt_get update
        native_json_collect_apt_packages
        missing_packages+=("${native_json_apt_packages[@]}")
    fi
    if (( ${#missing_packages[@]} > 0 )); then
        printf '安装系统依赖：%s\n' "${missing_packages[*]}"
        calibration_apt_get install -y --no-install-recommends "${missing_packages[@]}"
    fi
fi

# 保留原来的 uv 可执行文件（包括 Conda PATH 中的 uv）；缺失时自动安装到项目工具目录。
if [[ -z "$install_uv_command" ]]; then
    install_uv_command="$project_root/.deps/uv/uv"
    if [[ ! -x "$install_uv_command" ]]; then
        mkdir -p "$project_root/.deps/uv"
        calibration_download https://astral.sh/uv/install.sh --output "$project_root/.deps/uv/install.sh"
        if ! UV_INSTALL_DIR="$project_root/.deps/uv" UV_NO_MODIFY_PATH=1 \
            sh "$project_root/.deps/uv/install.sh"; then
            [[ "$calibration_nvidia_proxy" != DIRECT ]] || exit 1
            HTTPS_PROXY="$calibration_nvidia_proxy" HTTP_PROXY="$calibration_nvidia_proxy" \
                https_proxy="$calibration_nvidia_proxy" http_proxy="$calibration_nvidia_proxy" \
                NO_PROXY= no_proxy= UV_INSTALL_DIR="$project_root/.deps/uv" UV_NO_MODIFY_PATH=1 \
                sh "$project_root/.deps/uv/install.sh"
        fi
    fi
fi
uv() { "$install_uv_command" "$@"; }
# 跨文件系统的云桌面使用 copy；Python 及所有算法共用根项目 .venv。
export UV_LINK_MODE=${UV_LINK_MODE:-copy}
if uv sync --project "$project_root" --python 3.12 --locked; then
    :
else
    uv_sync_status=$?
    [[ "$calibration_nvidia_proxy" != DIRECT ]] || exit "$uv_sync_status"
    HTTPS_PROXY="$calibration_nvidia_proxy" HTTP_PROXY="$calibration_nvidia_proxy" \
        https_proxy="$calibration_nvidia_proxy" http_proxy="$calibration_nvidia_proxy" \
        NO_PROXY= no_proxy= uv sync --project "$project_root" --python 3.12 --locked
fi

# 优先使用本脚本安装的系统 OpenCV，避免 Conda 或 /usr/local 中的 CUDA 版本覆盖它。
opencv_cmake_dir=${DAGSTER_PIPELINE_OPENCV_DIR:-}
if [[ -z "$opencv_cmake_dir" ]]; then
    opencv_config=$(
        dpkg-query -L libopencv-dev 2>/dev/null \
            | command grep '/OpenCVConfig.cmake$' \
            | command head -n 1 \
            || true
    )
    opencv_cmake_dir=$(dirname -- "$opencv_config")
fi
if [[ ! -f "$opencv_cmake_dir/OpenCVConfig.cmake" ]]; then
    echo "找不到系统 OpenCVConfig.cmake：$opencv_cmake_dir" >&2
    exit 1
fi

# 同样固定 apt 安装的 PCL；Conda 编译器的 multiarch 路径不会自动搜索该目录。
pcl_cmake_dir=${DAGSTER_PIPELINE_PCL_DIR:-}
if [[ -z "$pcl_cmake_dir" ]]; then
    pcl_config=$(
        dpkg-query -L libpcl-dev 2>/dev/null \
            | command grep '/PCLConfig.cmake$' \
            | command head -n 1 \
            || true
    )
    pcl_cmake_dir=$(dirname -- "$pcl_config")
fi
if [[ ! -f "$pcl_cmake_dir/PCLConfig.cmake" ]]; then
    echo "找不到系统 PCLConfig.cmake：$pcl_cmake_dir" >&2
    exit 1
fi

# 固定 apt 的 yaml-cpp，避免链接到需要更新 libstdc++ 的 Conda 版本。
yaml_cpp_cmake_dir=${DAGSTER_PIPELINE_YAML_CPP_DIR:-}
if [[ -z "$yaml_cpp_cmake_dir" ]]; then
    yaml_cpp_config=$(
        dpkg-query -L libyaml-cpp-dev 2>/dev/null \
            | command grep -E '/(yaml-cpp-config|yaml-cppConfig)\.cmake$' \
            | command head -n 1 \
            || true
    )
    yaml_cpp_cmake_dir=$(dirname -- "$yaml_cpp_config")
fi
if [[ ! -f "$yaml_cpp_cmake_dir/yaml-cpp-config.cmake" && \
      ! -f "$yaml_cpp_cmake_dir/yaml-cppConfig.cmake" ]]; then
    echo "找不到系统 yaml-cpp CMake 配置：$yaml_cpp_cmake_dir，请安装 libyaml-cpp-dev。" >&2
    exit 1
fi
printf '使用 yaml-cpp：%s\n' "$yaml_cpp_cmake_dir"

# Sophus 必须与系统 fmt 一起构建，不能复用导出 Conda fmt 版本的配置。
fmt_cmake_dir=${DAGSTER_PIPELINE_FMT_DIR:-}
if [[ -z "$fmt_cmake_dir" ]]; then
    fmt_config=$(
        dpkg-query -L libfmt-dev 2>/dev/null \
            | command grep -E '/(fmt-config|fmtConfig)\.cmake$' \
            | command head -n 1 \
            || true
    )
    fmt_cmake_dir=$(dirname -- "$fmt_config")
fi
if [[ ! -f "$fmt_cmake_dir/fmt-config.cmake" && ! -f "$fmt_cmake_dir/fmtConfig.cmake" ]]; then
    echo "找不到系统 fmt CMake 配置：$fmt_cmake_dir，请安装 libfmt-dev。" >&2
    exit 1
fi
printf '使用 fmt：%s\n' "$fmt_cmake_dir"
for build_tool in "$system_cmake" "$system_ctest" "$system_ninja" "$system_pkg_config"; do
    if [[ ! -x "$build_tool" ]]; then
        echo "找不到系统构建工具：$build_tool" >&2
        exit 1
    fi
done

# 激活 Conda 时 CC/CXX 可能指向其私有 sysroot；native 依赖均来自 apt，统一用系统编译器。
system_cc=${DAGSTER_PIPELINE_CC:-/usr/bin/cc}
system_cxx=${DAGSTER_PIPELINE_CXX:-/usr/bin/c++}
if [[ ! -x "$system_cc" || ! -x "$system_cxx" ]]; then
    echo "找不到系统 C/C++ 编译器：$system_cc、$system_cxx" >&2
    exit 1
fi
prepare_native_json_dependencies
native_prepare_system_headers

# 标定与其他算法共用根 uv 环境；完整相机版是默认构建。
case "${DAGSTER_PIPELINE_CALIBRATE_WITH_CAMERA:-ON}" in
    ON|on|1|TRUE|true|YES|yes) calibrate_with_camera=ON ;;
    OFF|off|0|FALSE|false|NO|no) calibrate_with_camera=OFF ;;
    *) echo "DAGSTER_PIPELINE_CALIBRATE_WITH_CAMERA 必须是 ON 或 OFF。" >&2; exit 1 ;;
esac
prepare_calibration_dependencies

# 路径、包版本和依赖配置内容共同决定缓存；第一次自动清理旧脚本留下的缓存。
native_signature=$(native_cache_signature "$opencv_cmake_dir" "$pcl_cmake_dir" \
    "$yaml_cpp_cmake_dir" "$fmt_cmake_dir" "$sophus_cmake_dir" \
    "$cuda_root" "$tensorrt_root" "$calibrate_with_camera" "${native_json_dependency_files[@]}")

# 配置并增量编译原生 Localization 可执行程序和测试程序。
localization_source="$project_root/src/algorithm/calib_localization/localization"
localization_build="$project_root/build/localization"
native_prepare_build_cache "$localization_build" "$native_signature"
"$system_cmake" -S "$localization_source" \
    -B "$localization_build" \
    -G Ninja \
    "${native_cmake_args[@]}" \
    -Dfmt_DIR="$fmt_cmake_dir" \
    -DCMAKE_C_COMPILER="$system_cc" \
    -DCMAKE_CXX_COMPILER="$system_cxx" \
    -DOpenCV_DIR="$opencv_cmake_dir" \
    -DPCL_DIR="$pcl_cmake_dir" \
    -Dyaml-cpp_DIR="$yaml_cpp_cmake_dir" \
    -DBUILD_TESTING=ON
"$system_cmake" --build "$localization_build" --parallel 2

# 运行原生测试，避免把不可用的 Localization 二进制交给生产流水线。
(cd -- "$localization_build" && "$system_ctest" --output-on-failure)
localization_binary="$localization_build/localization"
if [[ ! -x "$localization_binary" ]]; then
    echo "Localization 构建未生成可执行文件：$localization_binary" >&2
    exit 1
fi

native_record_build_cache "$localization_build" "$native_signature"

# 编译地图合成 native 可执行程序。地图资产通过同一 request/result 协议
# 调用该程序，避免生产运行时回退到 Python 投影与 PCD 解析路径。
case_raster_map_source="$project_root/src/algorithm/raster_map/case_raster_map"
case_raster_map_build="$project_root/build/case_raster_map"
if [[ ! -f "$case_raster_map_source/CMakeLists.txt" ]]; then
    echo "找不到地图合成 CMake 工程：$case_raster_map_source" >&2
    exit 1
fi
native_prepare_build_cache "$case_raster_map_build" "$native_signature"
"$system_cmake" -S "$case_raster_map_source" \
    -B "$case_raster_map_build" \
    -G Ninja \
    "${native_cmake_args[@]}" \
    -Dfmt_DIR="$fmt_cmake_dir" \
    -DCMAKE_C_COMPILER="$system_cc" \
    -DCMAKE_CXX_COMPILER="$system_cxx" \
    -DOpenCV_DIR="$opencv_cmake_dir" \
    -Dyaml-cpp_DIR="$yaml_cpp_cmake_dir" \
    -DBUILD_TESTING=ON
"$system_cmake" --build "$case_raster_map_build" --parallel 2
(cd -- "$case_raster_map_build" && "$system_ctest" --output-on-failure)
case_raster_map_binary="$case_raster_map_build/case_raster_map"
if [[ ! -x "$case_raster_map_binary" ]]; then
    echo "地图合成构建未生成可执行文件：$case_raster_map_binary" >&2
    exit 1
fi

native_record_build_cache "$case_raster_map_build" "$native_signature"

calibrate_source="$project_root/src/algorithm/calib_localization/sensor_calibration"
calibrate_build="$project_root/build/sensor_calibration"
native_prepare_build_cache "$calibrate_build" "$native_signature"
"$system_cmake" -S "$calibrate_source" -B "$calibrate_build" -G Ninja \
    "${native_cmake_args[@]}" \
    -Dfmt_DIR="$fmt_cmake_dir" \
    -DCMAKE_C_COMPILER="$system_cc" -DCMAKE_CXX_COMPILER="$system_cxx" \
    -DOpenCV_DIR="$opencv_cmake_dir" -DPCL_DIR="$pcl_cmake_dir" \
    -Dyaml-cpp_DIR="$yaml_cpp_cmake_dir" -DSophus_DIR="$sophus_cmake_dir" \
    -DCUDAToolkit_ROOT="$cuda_root" -DTENSORRT_ROOT="$tensorrt_root" \
    -DCALIBRATE_WITH_CAMERA="$calibrate_with_camera" -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=ON
"$system_cmake" --build "$calibrate_build" --parallel 2
(cd -- "$calibrate_build" && "$system_ctest" --output-on-failure)
calibrate_binary="$calibrate_build/sensor_calibration"
calibrate_visualize_binary="$calibrate_build/sensor_calibration_visualize"
if [[ ! -x "$calibrate_binary" || ! -x "$calibrate_visualize_binary" ]]; then
    echo "标定构建未生成求解器或可视化导出器：$calibrate_build" >&2
    exit 1
fi

native_record_build_cache "$calibrate_build" "$native_signature"

# 强制执行 Python→C++ request 与 C++→Python result/artifact 往返测试。
native_python_tests=(
    "$project_root/tests/test_localization_native.py"
    "$project_root/tests/test_localization_streaming.py"
    "$project_root/tests/test_localization_resources.py"
    "$project_root/tests/test_case_raster_map_native.py"
    "$project_root/tests/test_real_calibrate_stage.py"
    "$project_root/tests/test_real_sensor_calibration_stage.py"
    "$project_root/tests/test_calibrate_visualization.py"
)
for optional_test in test_useful_height_native.py test_useful_height.py; do
    [[ -f "$project_root/tests/$optional_test" ]] && native_python_tests+=("$project_root/tests/$optional_test")
done
PYTEST_DISABLE_PLUGIN_AUTOLOAD=1 PYTEST_PLUGINS= PYTEST_ADDOPTS= \
    DAGSTER_PIPELINE_LOCALIZATION_BIN="$localization_binary" \
    DAGSTER_PIPELINE_CASE_RASTER_MAP_BIN="$case_raster_map_binary" \
    DAGSTER_PIPELINE_SENSOR_CALIBRATION_BIN="$calibrate_binary" \
    DAGSTER_PIPELINE_SENSOR_CALIBRATION_VISUALIZE_BIN="$calibrate_visualize_binary" \
    uv run --project "$project_root" pytest -q "${native_python_tests[@]}"
if [[ "$calibrate_with_camera" == ON ]]; then
    PYTEST_DISABLE_PLUGIN_AUTOLOAD=1 PYTEST_PLUGINS= PYTEST_ADDOPTS= \
        DAGSTER_PIPELINE_SENSOR_CALIBRATION_BIN="$calibrate_binary" \
        uv run --project "$project_root" pytest -q "$project_root/tests/test_calibrate_camera_gpu.py"
fi

# 根据 runtime 的安装路径，将 Hydra 源配置渲染为算法直接读取的 YAML。
runtime_exports=$(uv run --project "$project_root" \
    dagster-pipeline runtime-env \
    --config-root "$source_config_root")
eval "$runtime_exports"
uv run --project "$project_root" dagster-pipeline render-configs \
    --config-root "$source_config_root" \
    --output-directory "$DAGSTER_PIPELINE_RUNTIME_ROOT/build/config"

# 打印安装结果，后续直接运行 script/run.sh 即可。
printf '安装完成。\n'
printf 'Python 环境：%s\n' "${UV_PROJECT_ENVIRONMENT:-$project_root/.venv}"
printf 'Localization：%s\n' "$localization_binary"
printf '地图合成：%s\n' "$case_raster_map_binary"
printf '标定：%s\n' "$calibrate_binary"
printf '标定可视化：%s\n' "$calibrate_visualize_binary"
printf '运行配置：%s\n' "$DAGSTER_PIPELINE_RUNTIME_ROOT/build/config"
