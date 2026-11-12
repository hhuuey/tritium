#!/usr/bin/env bash
set -euo pipefail

if (( $# != 0 )); then
    echo "用法：$0" >&2
    exit 2
fi
if ! command -v flock >/dev/null 2>&1; then
    echo "缺少 flock，无法保证 runtime 启动互斥。" >&2
    exit 1
fi

# 定位项目，避免依赖调用脚本时的工作目录。
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
project_root=$(dirname -- "$script_dir")
if [[ -x "$project_root/.deps/uv/uv" ]]; then
    export PATH="$project_root/.deps/uv:$PATH"
fi
export DAGSTER_PIPELINE_RUNTIME_ROOT=${DAGSTER_PIPELINE_RUNTIME_ROOT:-$project_root}

# 从平台 runtime 加载实例路径、算法可执行程序和 Dagster 服务参数。
runtime_exports=$(uv run --project "$project_root" \
    dagster-pipeline runtime-env)
eval "$runtime_exports"
rendered_config_root="$DAGSTER_PIPELINE_RUNTIME_ROOT/build/config"
case_config=${DAGSTER_PIPELINE_CASE_CONFIG:-$rendered_config_root/dagster/cases.yaml}

paths_overlap() {
    local first=$1
    local second=$2
    [[ "$first" == "$second" ]] ||
        [[ "$first" == "$second/"* ]] ||
        [[ "$second" == "$first/"* ]]
}

if paths_overlap "$DAGSTER_HOME" "$DAGSTER_PIPELINE_ARTIFACT_ROOT" ||
   paths_overlap "$DAGSTER_HOME" "$DAGSTER_PIPELINE_MANIFEST_ROOT"; then
    echo "DAGSTER_HOME 不能与 artifact 或 manifest 路径重叠。" >&2
    exit 1
fi
if [[ "$DAGSTER_PIPELINE_ARTIFACT_ROOT" == "$DAGSTER_PIPELINE_MANIFEST_ROOT" ]] ||
   [[ "$DAGSTER_PIPELINE_ARTIFACT_ROOT" == "$DAGSTER_PIPELINE_MANIFEST_ROOT/"* ]]; then
    echo "manifest 路径必须位于 artifact 根目录内，或与其互不重叠。" >&2
    exit 1
fi
if paths_overlap "$rendered_config_root" "$DAGSTER_HOME" ||
   paths_overlap "$rendered_config_root" "$DAGSTER_PIPELINE_ARTIFACT_ROOT" ||
   paths_overlap "$rendered_config_root" "$DAGSTER_PIPELINE_MANIFEST_ROOT"; then
    echo "Dagster 状态、artifact 或 manifest 路径不能与安装配置目录重叠：$rendered_config_root" >&2
    exit 1
fi

# 启动仅消费安装时渲染的配置，不覆盖已有运行配置。
if [[ ! -f "$rendered_config_root/dagster/assets.yaml" ]]; then
    echo "缺少运行配置：$rendered_config_root/dagster/assets.yaml" >&2
    echo "请先运行 ./script/install.sh，或显式执行 dagster-pipeline render-configs --config-root <源config目录> --output-directory \"$rendered_config_root\"。" >&2
    exit 1
fi

# 启动前检查安装产物和 Case 登记配置。
if [[ ! -f "$case_config" ]]; then
    echo "case 注册配置不存在：$case_config" >&2
    exit 1
fi
uv run --project "$project_root" python -c \
    'import os, sys
from pathlib import Path
from dagster_pipeline.configuration import load_case_registration_config
from dagster_pipeline.orchestration.asset_registry import load_asset_execution_specs
for spec in load_asset_execution_specs(Path(sys.argv[2])).values():
    if spec.executable is not None and (not spec.executable.is_file() or not os.access(spec.executable, os.X_OK)):
        raise SystemExit(f"{spec.asset_name}: 可执行文件不可用：{spec.executable}；请检查 assets.yaml 或运行 ./script/install.sh")
load_case_registration_config(Path(sys.argv[1]))' \
    "$case_config" "$rendered_config_root/dagster/assets.yaml"

# 创建运行目录和基础实例配置，关闭首次启动提示及遥测。
command mkdir -p \
    "$DAGSTER_HOME" \
    "$DAGSTER_PIPELINE_ARTIFACT_ROOT" \
    "$DAGSTER_PIPELINE_MANIFEST_ROOT"

# 持有整个服务生命周期的非阻塞锁，避免并发启动在同一 instance 上交错写入。
runtime_lock_file="$DAGSTER_HOME/dagster-pipeline-dev.lock"
if [[ -L "$runtime_lock_file" ]] ||
   [[ -e "$runtime_lock_file" && ! -f "$runtime_lock_file" ]]; then
    echo "runtime lock 必须是普通文件：$runtime_lock_file" >&2
    exit 1
fi
exec {runtime_lock_fd}<>"$runtime_lock_file"
if ! flock -n "$runtime_lock_fd"; then
    echo "该 runtime 正在启动或运行；请先运行 ./script/stop.sh。" >&2
    exit 1
fi

validate_owner_file() {
    local directory=$1
    local owner_file="$directory/.dagster-pipeline-runtime-owner"
    local -a owner_lines=()

    if [[ ! -f "$owner_file" ]]; then
        echo "runtime owner marker 损坏：$directory" >&2
        return 1
    fi
    mapfile -t owner_lines < "$owner_file"
    if (( ${#owner_lines[@]} != 2 )) ||
       [[ "${owner_lines[0]}" != "$project_root" ]] ||
       [[ "${owner_lines[1]}" != "$DAGSTER_PIPELINE_ENVIRONMENT" ]]; then
        echo "runtime 路径已属于其他项目或 environment，或 owner marker 损坏：$directory" >&2
        return 1
    fi
}

runtime_owner_roots=(
    "$DAGSTER_HOME"
    "$DAGSTER_PIPELINE_ARTIFACT_ROOT"
)
manifest_is_nested=0
if [[ "$DAGSTER_PIPELINE_MANIFEST_ROOT" == "$DAGSTER_PIPELINE_ARTIFACT_ROOT/"* ]]; then
    manifest_is_nested=1
else
    runtime_owner_roots+=("$DAGSTER_PIPELINE_MANIFEST_ROOT")
fi

for directory in "${runtime_owner_roots[@]}"; do
    owner_file="$directory/.dagster-pipeline-runtime-owner"
    if [[ -e "$owner_file" || -L "$owner_file" ]]; then
        validate_owner_file "$directory"
    fi
done
if (( manifest_is_nested )); then
    manifest_owner_file="$DAGSTER_PIPELINE_MANIFEST_ROOT/.dagster-pipeline-runtime-owner"
    if [[ -e "$manifest_owner_file" || -L "$manifest_owner_file" ]]; then
        validate_owner_file "$DAGSTER_PIPELINE_MANIFEST_ROOT"
    fi
fi

# 在任何配置、pool 或 case mutation 前拒绝已运行的 launcher。
pid_file="$DAGSTER_HOME/dagster-pipeline-dev.pid"
if [[ -f "$pid_file" ]]; then
    existing_pid=$(sed -n '1p' "$pid_file")
    existing_start_time=$(sed -n '2p' "$pid_file")
    if [[ "$existing_pid" =~ ^[1-9][0-9]*$ ]] &&
       [[ "$existing_start_time" =~ ^[0-9]+$ ]] &&
       [[ -r "/proc/$existing_pid/stat" ]] &&
       [[ "$(awk '{print $22}' "/proc/$existing_pid/stat")" == "$existing_start_time" ]]; then
        echo "该 runtime 已由 PID $existing_pid 启动；请先运行 ./script/stop.sh。" >&2
        exit 1
    fi
    command rm -f -- "$pid_file"
fi

launcher_start_time=$(awk '{print $22}' "/proc/$$/stat")
temporary_pid_file="$pid_file.tmp.$$"
printf '%s\n%s\n%s\n%s\n' \
    "$$" "$launcher_start_time" "$project_root" "$DAGSTER_PIPELINE_ENVIRONMENT" \
    > "$temporary_pid_file"
command mv -- "$temporary_pid_file" "$pid_file"
cleanup_pid_file() {
    if [[ -f "$pid_file" ]] &&
       [[ "$(sed -n '1p' "$pid_file")" == "$$" ]] &&
       [[ "$(sed -n '3p' "$pid_file")" == "$project_root" ]]; then
        command rm -f -- "$pid_file"
    fi
}
trap cleanup_pid_file EXIT

for directory in "${runtime_owner_roots[@]}"; do
    owner_file="$directory/.dagster-pipeline-runtime-owner"
    if [[ ! -e "$owner_file" && ! -L "$owner_file" ]]; then
        temporary_owner_file="$owner_file.tmp.$$"
        printf '%s\n%s\n' "$project_root" "$DAGSTER_PIPELINE_ENVIRONMENT" \
            > "$temporary_owner_file"
        command mv -- "$temporary_owner_file" "$owner_file"
    fi
done
if [[ ! -f "$DAGSTER_HOME/dagster.yaml" ]]; then
    printf 'telemetry:\n  enabled: false\nnux:\n  enabled: false\n' \
        > "$DAGSTER_HOME/dagster.yaml"
fi

# 幂等应用全部 pool 的 limit；null 会移除对应限制。
uv run --project "$project_root" \
    dagster-pipeline apply-runtime

# 从同一配置的 qfile_uris 注册 partition 及各 Case manifest。
uv run --project "$project_root" dagster-pipeline register-case \
    --config "$case_config"

# 在容器内监听全部网卡，以便通过映射的 3000 端口访问 Dagster UI。
cd "$project_root"
uv run --project "$project_root" python -m dagster dev \
    -h "$DAGSTER_PIPELINE_DAGSTER_HOST" \
    -p "$DAGSTER_PIPELINE_DAGSTER_PORT" \
    -m dagster_pipeline.definitions \
    -a defs \
    --log-level debug \
    --code-server-log-level debug
