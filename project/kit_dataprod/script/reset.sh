#!/usr/bin/env bash
set -euo pipefail

if (( $# != 0 )); then
    echo "用法：$0" >&2
    echo "该命令会停止 Dagster 并删除运行记录、manifest 和算法产物。" >&2
    exit 2
fi

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
project_root=$(dirname -- "$script_dir")
if [[ -x "$project_root/.deps/uv/uv" ]]; then
    export PATH="$project_root/.deps/uv:$PATH"
fi
export DAGSTER_PIPELINE_RUNTIME_ROOT=${DAGSTER_PIPELINE_RUNTIME_ROOT:-$project_root}

runtime_exports=$(uv run --project "$project_root" \
    dagster-pipeline runtime-env)
eval "$runtime_exports"

configured_directories=(
    "$DAGSTER_HOME" \
    "$DAGSTER_PIPELINE_ARTIFACT_ROOT" \
    "$DAGSTER_PIPELINE_MANIFEST_ROOT"
)
allow_external_reset=${DAGSTER_PIPELINE_ALLOW_EXTERNAL_RESET:-0}
for directory in "${configured_directories[@]}"; do
    if [[ -z "$directory" || "$directory" == "/" || "$directory" == "$HOME" ]] ||
       [[ "$directory" == "$project_root" ]] ||
       [[ "$project_root" == "$directory/"* ]]; then
        echo "拒绝删除危险路径：$directory" >&2
        exit 1
    fi
    if [[ "$directory" != "$project_root/"* ]]; then
        if [[ "$allow_external_reset" != "1" ]] ||
           [[ ! "$directory" =~ ^/[^/]+/[^/]+/[^/]+ ]]; then
            echo "拒绝删除项目外路径：$directory" >&2
            echo "外部 runtime 根需要 DAGSTER_PIPELINE_ALLOW_EXTERNAL_RESET=1 且至少三级目录。" >&2
            exit 1
        fi
    fi
done

paths_overlap() {
    local first=$1
    local second=$2
    [[ "$first" == "$second" ]] ||
        [[ "$first" == "$second/"* ]] ||
        [[ "$second" == "$first/"* ]]
}

if paths_overlap "$DAGSTER_HOME" "$DAGSTER_PIPELINE_ARTIFACT_ROOT" ||
   paths_overlap "$DAGSTER_HOME" "$DAGSTER_PIPELINE_MANIFEST_ROOT"; then
    echo "拒绝删除与 DAGSTER_HOME 重叠的 artifact 或 manifest 路径。" >&2
    exit 1
fi
if [[ "$DAGSTER_PIPELINE_ARTIFACT_ROOT" == "$DAGSTER_PIPELINE_MANIFEST_ROOT" ]] ||
   [[ "$DAGSTER_PIPELINE_ARTIFACT_ROOT" == "$DAGSTER_PIPELINE_MANIFEST_ROOT/"* ]]; then
    echo "拒绝删除包含 artifact 根目录的 manifest 路径。" >&2
    exit 1
fi
preserved_config="$DAGSTER_PIPELINE_RUNTIME_ROOT/build/config"
if paths_overlap "$preserved_config" "$DAGSTER_HOME" ||
   paths_overlap "$preserved_config" "$DAGSTER_PIPELINE_ARTIFACT_ROOT" ||
   paths_overlap "$preserved_config" "$DAGSTER_PIPELINE_MANIFEST_ROOT"; then
    echo "拒绝删除与安装配置目录重叠的 Dagster 状态、artifact 或 manifest 路径：$preserved_config" >&2
    exit 1
fi

reset_directories=(
    "$DAGSTER_HOME"
    "$DAGSTER_PIPELINE_ARTIFACT_ROOT"
)
manifest_is_nested=0
if [[ "$DAGSTER_PIPELINE_MANIFEST_ROOT" == "$DAGSTER_PIPELINE_ARTIFACT_ROOT/"* ]]; then
    manifest_is_nested=1
else
    reset_directories+=("$DAGSTER_PIPELINE_MANIFEST_ROOT")
fi

validate_owner_file() {
    local directory=$1
    local owner_file="$directory/.dagster-pipeline-runtime-owner"
    local -a owner_lines=()

    if [[ ! -f "$owner_file" ]]; then
        echo "拒绝删除不属于当前项目和 environment 的路径：$directory" >&2
        return 1
    fi
    mapfile -t owner_lines < "$owner_file"
    if (( ${#owner_lines[@]} != 2 )) ||
       [[ "${owner_lines[0]}" != "$project_root" ]] ||
       [[ "${owner_lines[1]}" != "$DAGSTER_PIPELINE_ENVIRONMENT" ]]; then
        echo "拒绝删除不属于当前项目和 environment 或 owner marker 损坏的路径：$directory" >&2
        return 1
    fi
}

for directory in "${reset_directories[@]}"; do
    validate_owner_file "$directory"
done
if (( manifest_is_nested )); then
    manifest_owner_file="$DAGSTER_PIPELINE_MANIFEST_ROOT/.dagster-pipeline-runtime-owner"
    if [[ -e "$manifest_owner_file" || -L "$manifest_owner_file" ]]; then
        validate_owner_file "$DAGSTER_PIPELINE_MANIFEST_ROOT"
    fi
fi

"$script_dir/stop.sh"
printf '清空 Dagster 状态、manifest 和算法产物。\n'
for directory in "${reset_directories[@]}"; do
    if [[ "$directory" == "$DAGSTER_PIPELINE_ARTIFACT_ROOT" ]]; then
        while IFS= read -r -d '' entry; do
            command rm -rf -- "$entry"
        done < <(find "$directory" -mindepth 1 -maxdepth 1 \
            ! -name .dagster-pipeline-runtime-owner -print0)
    else
        command rm -rf -- "$directory"
    fi
done
