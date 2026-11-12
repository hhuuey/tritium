#!/usr/bin/env bash
# Sourced by install.sh; all Python packages remain in the root uv environment.

calibration_nvidia_proxy=${DAGSTER_PIPELINE_NVIDIA_PROXY:-http://proxy-qomolo.wellspiking.ai:32389}

calibration_privileged() {
    if (( $(id -u) == 0 )); then
        "$@"
    elif command -v sudo >/dev/null 2>&1; then
        sudo "$@"
    else
        echo "安装标定系统依赖需要 root 权限或 sudo：$*" >&2
        return 1
    fi
}

calibration_apt_get() {
    # Command options override old host proxy settings, including apt.conf.d.
    # Scope the proxy to NVIDIA so Ubuntu/internal repositories keep their own route.
    calibration_privileged apt-get "$@" \
        -o "Acquire::http::Proxy::developer.download.nvidia.com=$calibration_nvidia_proxy" \
        -o "Acquire::https::Proxy::developer.download.nvidia.com=$calibration_nvidia_proxy" \
        -o Acquire::Retries=3
}

calibration_download() {
    # Public source downloads use the terminal's route first, then the known cloud proxy.
    if curl --fail --location --connect-timeout 15 --retry 2 "$@"; then
        return 0
    fi
    [[ "$calibration_nvidia_proxy" != DIRECT ]] || return 1
    curl --proxy "$calibration_nvidia_proxy" --noproxy "" \
        --fail --location --connect-timeout 15 --retry 3 "$@"
}

calibration_nvidia_repository() {
    local distro_id distro_version repository_arch keyring
    local -a proxy_options=(--proxy "$calibration_nvidia_proxy" --noproxy "")
    if [[ "$calibration_nvidia_proxy" == DIRECT ]]; then
        proxy_options=(--proxy "" --noproxy "*")
    fi
    distro_id=$(. /etc/os-release; printf '%s' "$ID")
    distro_version=$(. /etc/os-release; printf '%s' "$VERSION_ID")
    case "$distro_id" in
        ubuntu) distro_version=${distro_version//./} ;;
        debian) distro_version=${distro_version%%.*} ;;
        *) echo "CUDA/TensorRT 自动安装仅支持 Ubuntu/Debian。" >&2; return 1 ;;
    esac
    case "$(dpkg --print-architecture)" in
        amd64) repository_arch=x86_64 ;;
        arm64) repository_arch=sbsa ;;
        *) echo "CUDA/TensorRT 自动安装不支持当前架构。" >&2; return 1 ;;
    esac
    keyring="$project_root/.deps/nvidia/cuda-keyring_1.1-1_all.deb"
    mkdir -p "$(dirname -- "$keyring")"
    curl "${proxy_options[@]}" --fail --location --retry 3 \
        "https://developer.download.nvidia.com/compute/cuda/repos/${distro_id}${distro_version}/${repository_arch}/cuda-keyring_1.1-1_all.deb" \
        --output "$keyring"
    calibration_privileged dpkg -i "$keyring"
    calibration_apt_get update
}

calibration_tensorrt_available() {
    local prefix=$1 multiarch=$2 header library plugin
    header="$prefix/include/NvInfer.h"
    [[ -f "$header" ]] || header="$prefix/include/$multiarch/NvInfer.h"
    [[ -f "$header" ]] || return 1
    for library in "$prefix/lib/libnvinfer.so" "$prefix/lib64/libnvinfer.so" \
        "$prefix/lib/$multiarch/libnvinfer.so" "$prefix/targets/$multiarch/lib/libnvinfer.so"; do
        plugin="$(dirname -- "$library")/libnvinfer_plugin.so"
        [[ -f "$library" && -f "$plugin" ]] && return 0
    done
    return 1
}

prepare_calibration_dependencies() {
    local prefix candidate multiarch toolkit_package tensorrt_version package sophus_signature archive
    local -a gpu_packages=()
    sophus_cmake_dir=${DAGSTER_PIPELINE_SOPHUS_DIR:-}
    if [[ -z "$sophus_cmake_dir" ]]; then
        prefix="$project_root/.deps/sophus-1.22.10"
        sophus_cmake_dir="$prefix/install/share/sophus/cmake"
        sophus_signature=$(
            native_signature_packages=(cmake ninja-build build-essential libeigen3-dev libfmt-dev)
            native_cache_signature "$prefix" "$fmt_cmake_dir"
        )
        if [[ ! -f "$sophus_cmake_dir/SophusConfig.cmake" && \
              "${DAGSTER_PIPELINE_SKIP_SYSTEM_DEPS:-0}" == 1 ]]; then
            echo "缺少 Sophus；取消 SKIP_SYSTEM_DEPS 或设置 DAGSTER_PIPELINE_SOPHUS_DIR。" >&2
            return 1
        fi
        native_prepare_build_cache "$prefix/build" "$sophus_signature"
        if [[ ! -f "$prefix/build/.native-dependencies" || \
              ! -f "$sophus_cmake_dir/SophusConfig.cmake" ]]; then
            rm -rf -- "$prefix/install"
            mkdir -p "$prefix"
            archive="$prefix/source.tar.gz"
            if [[ ! -f "$archive" ]] || ! printf '%s  %s\n' \
                eb1da440e6250c5efc7637a0611a5b8888875ce6ac22bf7ff6b6769bbc958082 "$archive" \
                | sha256sum --check --status; then
                calibration_download https://github.com/strasdat/Sophus/archive/refs/tags/1.22.10.tar.gz \
                    --output "$archive"
            fi
            printf '%s  %s\n' eb1da440e6250c5efc7637a0611a5b8888875ce6ac22bf7ff6b6769bbc958082 "$archive" | sha256sum --check
            tar -xzf "$archive" -C "$prefix"
            "$system_cmake" -S "$prefix/Sophus-1.22.10" -B "$prefix/build" -G Ninja \
                "${native_cmake_args[@]}" \
                -DCMAKE_C_COMPILER="$system_cc" -DCMAKE_CXX_COMPILER="$system_cxx" \
                -Dfmt_DIR="$fmt_cmake_dir" -DCMAKE_INSTALL_PREFIX="$prefix/install" \
                -DBUILD_SOPHUS_TESTS=OFF -DBUILD_SOPHUS_EXAMPLES=OFF
            "$system_cmake" --build "$prefix/build" --parallel 2
            "$system_cmake" --install "$prefix/build"
            native_record_build_cache "$prefix/build" "$sophus_signature"
        fi
    fi
    if [[ ! -f "$sophus_cmake_dir/SophusConfig.cmake" ]]; then
        echo "找不到 SophusConfig.cmake：$sophus_cmake_dir" >&2
        return 1
    fi

    cuda_root=${DAGSTER_PIPELINE_CUDA_ROOT:-}
    tensorrt_root=${DAGSTER_PIPELINE_TENSORRT_ROOT:-}
    [[ "$calibrate_with_camera" == ON ]] || return 0
    multiarch=$(dpkg-architecture -qDEB_HOST_MULTIARCH)
    if [[ -z "$cuda_root" ]]; then
        for candidate in /usr/local/cuda /usr/local/cuda-12.8 /usr; do
            if [[ -x "$candidate/bin/nvcc" ]]; then cuda_root=$candidate; break; fi
        done
    fi
    if [[ -n "$cuda_root" && ! -x "$cuda_root/bin/nvcc" ]]; then
        echo "CUDA 路径缺少 nvcc：$cuda_root" >&2
        return 1
    fi
    if [[ -n "$tensorrt_root" ]]; then
        calibration_tensorrt_available "$tensorrt_root" "$multiarch" || {
            echo "TensorRT 路径缺少头文件、nvinfer 或插件库：$tensorrt_root" >&2
            return 1
        }
    elif calibration_tensorrt_available /usr "$multiarch"; then
        tensorrt_root=/usr
    fi
    toolkit_package=cuda-toolkit-12-8
    tensorrt_version=10.8.0.43-1+cuda12.8
    # Automatic installation selects one tested CUDA/TensorRT pair. Explicit
    # prefixes let deployments keep their own compatible development packages.
    if [[ ( -z "$cuda_root" || -z "$tensorrt_root" ) && \
        -z "${DAGSTER_PIPELINE_CUDA_ROOT:-}" && -z "${DAGSTER_PIPELINE_TENSORRT_ROOT:-}" ]]; then
        cuda_root=
        tensorrt_root=
    fi
    [[ -n "$cuda_root" ]] || gpu_packages+=("$toolkit_package")
    # apt keeps newer candidates for dependencies unless they are also explicit.
    # Pin the full TensorRT runtime/header dependency set in one transaction.
    [[ -n "$tensorrt_root" ]] || gpu_packages+=(
        "libnvinfer-dev=$tensorrt_version" "libnvinfer-plugin-dev=$tensorrt_version"
        "libnvinfer-headers-dev=$tensorrt_version" "libnvinfer-headers-plugin-dev=$tensorrt_version"
        "libnvinfer10=$tensorrt_version" "libnvinfer-plugin10=$tensorrt_version")
    if (( ${#gpu_packages[@]} )); then
        if [[ "${DAGSTER_PIPELINE_SKIP_SYSTEM_DEPS:-0}" == 1 ]]; then
            echo "缺少 CUDA/TensorRT；取消 SKIP_SYSTEM_DEPS 或设置对应依赖路径。" >&2
            return 1
        fi
        for package in "${gpu_packages[@]}"; do
            if ! apt-cache show "$package" >/dev/null 2>&1; then
                calibration_nvidia_repository
                break
            fi
        done
        # Toolkit packages install development/runtime libraries without replacing the GPU driver.
        calibration_apt_get install -y --no-install-recommends "${gpu_packages[@]}"
        [[ -n "$cuda_root" ]] || cuda_root=/usr/local/cuda-12.8
        [[ -n "$tensorrt_root" ]] || tensorrt_root=/usr
    fi
    if [[ ! -x "$cuda_root/bin/nvcc" ]]; then
        echo "CUDA 安装后缺少 nvcc：$cuda_root" >&2
        return 1
    fi
    calibration_tensorrt_available "$tensorrt_root" "$multiarch" || {
        echo "TensorRT 安装后缺少头文件、nvinfer 或插件库：$tensorrt_root" >&2
        return 1
    }
}
