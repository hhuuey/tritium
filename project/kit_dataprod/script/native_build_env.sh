#!/usr/bin/env bash
# Keep apt headers, libraries and build tools together, even inside an active Conda shell.

native_prepare_environment() {
    install_uv_command=$(type -P uv || true)
    local entry prefix variable sanitized_path=
    local -a conda_prefixes=()
    for variable in ${!CONDA_PREFIX@}; do
        [[ -n "${!variable}" ]] && conda_prefixes+=("${!variable}")
    done
    if [[ -n "${CONDA_EXE:-}" ]]; then
        conda_prefixes+=("$(dirname -- "$(dirname -- "$CONDA_EXE")")")
    fi
    local -a path_entries=()
    IFS=: read -r -a path_entries <<< "$PATH"
    for entry in "${path_entries[@]}"; do
        [[ -n "$entry" ]] || continue
        case "$entry" in
            */miniconda*/*|*/anaconda*/*|*/conda/*) continue ;;
        esac
        local from_conda=0
        for prefix in "${conda_prefixes[@]}"; do
            if [[ "$entry" == "$prefix" || "$entry" == "$prefix/"* ]]; then
                from_conda=1
                break
            fi
        done
        (( from_conda == 0 )) || continue
        sanitized_path+="${sanitized_path:+:}$entry"
    done
    export PATH=${sanitized_path:-/usr/bin:/bin}
    # ROS 等外部 Python 路径不能进入项目虚拟环境；同时隔离用户级 site-packages。
    unset PYTHONPATH PYTHONHOME
    export PYTHONNOUSERSITE=1
    unset CC CXX CFLAGS CXXFLAGS CPPFLAGS LDFLAGS
    unset CUDACXX CUDAHOSTCXX CUDAFLAGS CUDAToolkit_ROOT CUDA_PATH CUDA_HOME
    unset CPATH C_INCLUDE_PATH CPLUS_INCLUDE_PATH OBJC_INCLUDE_PATH
    unset LIBRARY_PATH LD_LIBRARY_PATH LD_PRELOAD
    unset COMPILER_PATH GCC_EXEC_PREFIX
    unset PKG_CONFIG_PATH PKG_CONFIG_LIBDIR PKG_CONFIG_SYSROOT_DIR
    unset CMAKE_PREFIX_PATH CMAKE_INCLUDE_PATH CMAKE_LIBRARY_PATH CMAKE_PROGRAM_PATH
    unset CMAKE_FRAMEWORK_PATH CMAKE_APPBUNDLE_PATH CMAKE_FIND_ROOT_PATH CMAKE_TOOLCHAIN_FILE
    unset CMAKE_GENERATOR CMAKE_GENERATOR_PLATFORM CMAKE_GENERATOR_TOOLSET
    unset Eigen3_ROOT Eigen3_DIR fmt_ROOT fmt_DIR OpenCV_ROOT OpenCV_DIR PCL_ROOT PCL_DIR
    unset Sophus_ROOT Sophus_DIR nlohmann_json_ROOT nlohmann_json_DIR valijson_ROOT valijson_DIR
    export CMAKE_PREFIX_PATH=/usr
    local tools_dir=${DAGSTER_PIPELINE_BUILD_TOOLS_DIR:-/usr/bin}
    system_cmake="$tools_dir/cmake"
    system_ctest="$tools_dir/ctest"
    system_ninja="$tools_dir/ninja"
    system_pkg_config="$tools_dir/pkg-config"
    native_cmake_args=(
        -DCMAKE_PREFIX_PATH=/usr
        -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF
        -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF
        -DCMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH=OFF
        -DCMAKE_FIND_USE_PACKAGE_ROOT_PATH=OFF
        '-DCMAKE_IGNORE_PATH=/usr/local;/usr/local/include;/usr/local/lib;/usr/local/lib64;/usr/local/share/cmake;/usr/local/lib/cmake'
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        "-DCMAKE_MAKE_PROGRAM=$system_ninja"
        "-DPKG_CONFIG_EXECUTABLE=$system_pkg_config"
    )
    native_prepare_pkg_config
}

native_prepare_pkg_config() {
    local multiarch
    multiarch=$(/usr/bin/dpkg-architecture -qDEB_HOST_MULTIARCH 2>/dev/null || true)
    export PKG_CONFIG_LIBDIR="${multiarch:+/usr/lib/$multiarch/pkgconfig:}/usr/lib/pkgconfig:/usr/share/pkgconfig"
}

native_prepare_system_headers() {
    # GCC searches /usr/local/include before /usr/include even when CMake selects apt libraries.
    # Prepend only these public header directories; moving all /usr/include breaks include_next.
    local headers="$project_root/.deps/native-headers" package
    native_prepare_pkg_config
    mkdir -p "$headers"
    rm -f -- "$headers/json-c"
    for package in yaml-cpp fmt nlohmann valijson; do
        rm -f -- "$headers/$package"
        case "$package" in
            yaml-cpp) [[ -z "${DAGSTER_PIPELINE_YAML_CPP_DIR:-}" ]] || continue ;;
            fmt) [[ -z "${DAGSTER_PIPELINE_FMT_DIR:-}" ]] || continue ;;
        esac
        case "$package" in
            nlohmann) ln -s "${native_json_nlohmann_include:-/usr/include}/$package" "$headers/$package" ;;
            valijson) ln -s "${native_json_valijson_include:-/usr/include}/$package" "$headers/$package" ;;
            *) ln -s "/usr/include/$package" "$headers/$package" ;;
        esac
    done
    native_cmake_args+=("-DCMAKE_CXX_FLAGS=-I\"$headers\"")
}

native_cache_signature() {
    local dependency
    local -a signature_packages=(
        cmake ninja-build build-essential libeigen3-dev libfmt-dev libyaml-cpp-dev
        libopencv-dev libpcl-dev nlohmann-json3-dev libvalijson-dev libffi-dev libnvinfer-dev cuda-toolkit-12-8
    )
    if declare -p native_signature_packages >/dev/null 2>&1; then
        signature_packages=("${native_signature_packages[@]}")
    fi
    {
        printf '%s\0' "$project_root" "${system_cc:-/usr/bin/cc}" "${system_cxx:-/usr/bin/c++}" \
            "${system_cmake:-/usr/bin/cmake}" "${native_cmake_args[@]}" "$@"
        sha256sum "$project_root/script/native_build_env.sh" "$project_root/script/native_calibration_deps.sh"
        if [[ -f "$project_root/script/native_json_deps.sh" ]]; then
            sha256sum "$project_root/script/native_json_deps.sh"
        fi
        if [[ -f "$project_root/script/install.sh" ]]; then
            sha256sum "$project_root/script/install.sh"
        fi
        for dependency in "$@"; do
            if [[ -f "$dependency" ]]; then
                sha256sum "$dependency"
            elif [[ -d "$dependency" ]]; then
                local -a configs=("$dependency"/*.cmake)
                for dependency in "${configs[@]}"; do
                    [[ ! -f "$dependency" ]] || sha256sum "$dependency"
                done
            fi
        done
        dpkg-query -W -f='${Package}=${Version}\n' "${signature_packages[@]}" 2>/dev/null || true
    } | sha256sum | cut -d ' ' -f 1
}

native_prepare_build_cache() {
    local build_dir=$1 signature=$2 previous=
    if [[ -f "$build_dir/.native-dependencies" ]]; then
        previous=$(cat "$build_dir/.native-dependencies")
    fi
    if [[ -d "$build_dir" && "$previous" != "$signature" ]]; then
        printf '构建依赖或环境变化，重建缓存：%s\n' "$build_dir"
        rm -rf -- "$build_dir"
    fi
}

native_record_build_cache() {
    mkdir -p -- "$1"
    printf '%s\n' "$2" > "$1/.native-dependencies"
}
