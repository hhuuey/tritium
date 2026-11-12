#!/usr/bin/env bash
# Header-only JSON dependencies: prefer apt, otherwise install verified releases locally.

native_json_requirements=(nlohmann-json3-dev:3.11.2 libvalijson-dev:1.0)

native_json_package_ready() {
    local package=$1 minimum=$2 version
    [[ "$(dpkg-query -W -f='${Status}' "$package" 2>/dev/null)" == 'install ok installed' ]] || return 1
    version=$(dpkg-query -W -f='${Version}' "$package")
    dpkg --compare-versions "$version" ge "$minimum"
}

native_json_collect_apt_packages() {
    local requirement package minimum candidate
    native_json_apt_packages=()
    for requirement in "${native_json_requirements[@]}"; do
        package=${requirement%%:*}
        minimum=${requirement#*:}
        native_json_package_ready "$package" "$minimum" && continue
        candidate=$(apt-cache policy "$package" 2>/dev/null | awk '$1 == "Candidate:" { print $2; exit }' || true)
        if [[ -n "$candidate" && "$candidate" != '(none)' ]] && \
            dpkg --compare-versions "$candidate" ge "$minimum"; then
            native_json_apt_packages+=("$package")
        fi
    done
}

native_json_system_config() {
    local package=$1 filename=$2 path
    while IFS= read -r path; do
        if [[ "$path" == /usr/*/"$filename" && -f "$path" ]]; then
            dirname -- "$path"
            return 0
        fi
    done < <(dpkg-query -L "$package")
    echo "$package 已安装，但缺少系统 $filename。请重新安装该开发包。" >&2
    return 1
}

native_json_install_release() {
    local name=$1 version=$2 url=$3 digest=$4 config_relative=$5
    local prefix="$project_root/.deps/json/$name-$version" archive
    local -a options=()
    case "$name" in
        nlohmann) options=(-DJSON_BuildTests=OFF -DJSON_MultipleHeaders=ON -DJSON_Install=ON) ;;
        valijson) options=(-Dvalijson_BUILD_TESTS=OFF -Dvalijson_BUILD_EXAMPLES=OFF) ;;
    esac
    archive="$prefix/source.tar.gz"
    native_json_prefix="$prefix/install"
    if [[ -f "$prefix/source.sha256" && "$(cat "$prefix/source.sha256")" == "$digest" && \
          -s "$prefix/install.sha256" && -f "$native_json_prefix/$config_relative" ]] && \
        sha256sum --check --status "$prefix/install.sha256"; then
        return 0
    fi
    if [[ "${DAGSTER_PIPELINE_SKIP_SYSTEM_DEPS:-0}" == 1 ]]; then
        echo "缺少有效的 $name $version 本地依赖；取消 DAGSTER_PIPELINE_SKIP_SYSTEM_DEPS 后重新安装。" >&2
        return 1
    fi
    mkdir -p "$prefix"
    if [[ ! -f "$archive" ]] || ! printf '%s  %s\n' "$digest" "$archive" | sha256sum --check --status; then
        echo "系统仓库不满足要求，下载 $name $version 到项目 .deps/。"
        if ! calibration_download "$url" --output "$archive.tmp"; then
            rm -f -- "$archive.tmp"
            echo "下载 $name $version 失败，请检查网络或代理后重试 ./script/install.sh。" >&2
            return 1
        fi
        if ! printf '%s  %s\n' "$digest" "$archive.tmp" | sha256sum --check; then
            rm -f -- "$archive.tmp"
            echo "$name $version 源码 SHA256 校验失败。" >&2
            return 1
        fi
        mv -- "$archive.tmp" "$archive"
    fi
    rm -rf -- "$prefix/source" "$prefix/build" "$prefix/install"
    mkdir -p "$prefix/source"
    tar -xzf "$archive" --strip-components=1 -C "$prefix/source"
    "$system_cmake" -S "$prefix/source" -B "$prefix/build" -G Ninja \
        "${native_cmake_args[@]}" \
        -DCMAKE_C_COMPILER="$system_cc" -DCMAKE_CXX_COMPILER="$system_cxx" \
        -DCMAKE_INSTALL_PREFIX="$native_json_prefix" -DCMAKE_INSTALL_LIBDIR=lib \
        "${options[@]}"
    "$system_cmake" --install "$prefix/build"
    [[ -f "$native_json_prefix/$config_relative" ]] || {
        echo "$name $version 安装未生成 $config_relative。" >&2
        return 1
    }
    find "$native_json_prefix" -type f -print0 | sort -z | xargs -0 sha256sum > "$prefix/install.sha256"
    printf '%s\n' "$digest" > "$prefix/source.sha256"
}

prepare_native_json_dependencies() {
    native_json_dependency_files=()
    if native_json_package_ready nlohmann-json3-dev 3.11.2; then
        nlohmann_json_cmake_dir=$(native_json_system_config nlohmann-json3-dev nlohmann_jsonConfig.cmake)
        native_json_nlohmann_include=/usr/include
    else
        native_json_install_release nlohmann 3.11.2 \
            https://codeload.github.com/nlohmann/json/tar.gz/refs/tags/v3.11.2 \
            d69f9deb6a75e2580465c6c4c5111b89c4dc2fa94e3a85fcd2ffcd9a143d9273 \
            share/cmake/nlohmann_json/nlohmann_jsonConfig.cmake
        nlohmann_json_cmake_dir="$native_json_prefix/share/cmake/nlohmann_json"
        native_json_nlohmann_include="$native_json_prefix/include"
        native_json_dependency_files+=("$native_json_prefix/../source.sha256" "$native_json_prefix/../install.sha256")
    fi
    if native_json_package_ready libvalijson-dev 1.0; then
        valijson_cmake_dir=$(native_json_system_config libvalijson-dev valijsonConfig.cmake)
        native_json_valijson_include=/usr/include
    else
        native_json_install_release valijson 1.0 \
            https://codeload.github.com/tristanpenman/valijson/tar.gz/refs/tags/v1.0 \
            6b9f0bc89880feb3fe09aa469cd81f6168897d2fbb4e715853da3b94afd3779a \
            lib/cmake/valijson/valijsonConfig.cmake
        valijson_cmake_dir="$native_json_prefix/lib/cmake/valijson"
        native_json_valijson_include="$native_json_prefix/include"
        native_json_dependency_files+=("$native_json_prefix/../source.sha256" "$native_json_prefix/../install.sha256")
    fi
    native_json_dependency_files+=("$nlohmann_json_cmake_dir/nlohmann_jsonConfig.cmake" "$valijson_cmake_dir/valijsonConfig.cmake")
    native_cmake_args+=("-Dnlohmann_json_DIR=$nlohmann_json_cmake_dir" "-Dvalijson_DIR=$valijson_cmake_dir")
    printf '使用 nlohmann/json：%s\n使用 Valijson：%s\n' "$nlohmann_json_cmake_dir" "$valijson_cmake_dir"
}
