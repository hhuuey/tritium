#!/usr/bin/env bash
set -euo pipefail

if (( $# != 0 )); then
    echo "用法：$0" >&2
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

pid_file="$DAGSTER_HOME/dagster-pipeline-dev.pid"
if [[ ! -f "$pid_file" ]]; then
    printf '该 runtime 没有由 run.sh 管理的 Dagster 进程。\n'
    exit 0
fi

managed_pid=$(sed -n '1p' "$pid_file")
expected_start_time=$(sed -n '2p' "$pid_file")
recorded_project_root=$(sed -n '3p' "$pid_file")
recorded_environment=$(sed -n '4p' "$pid_file")
if [[ "$recorded_project_root" != "$project_root" ]] ||
   [[ "$recorded_environment" != "$DAGSTER_PIPELINE_ENVIRONMENT" ]]; then
    echo "拒绝停止其他项目或 runtime 管理的进程。" >&2
    exit 1
fi
if [[ ! "$managed_pid" =~ ^[1-9][0-9]*$ ]] ||
   [[ ! "$expected_start_time" =~ ^[0-9]+$ ]] ||
   [[ ! -r "/proc/$managed_pid/stat" ]] ||
   [[ "$(awk '{print $22}' "/proc/$managed_pid/stat")" != "$expected_start_time" ]]; then
    command rm -f -- "$pid_file"
    printf '移除失效的 Dagster PID 记录。\n'
    exit 0
fi

process_ids=()
collect_process_tree() {
    local parent_pid=$1
    local child_pid
    while IFS= read -r child_pid; do
        if [[ "$child_pid" =~ ^[1-9][0-9]*$ ]]; then
            collect_process_tree "$child_pid"
        fi
    done < <(pgrep -P "$parent_pid" 2>/dev/null || true)
    process_ids+=("$parent_pid")
}
collect_process_tree "$managed_pid"

process_is_running() {
    local process_id=$1
    local process_state
    if [[ ! -r "/proc/$process_id/stat" ]]; then
        return 1
    fi
    process_state=$(awk '{print $3}' "/proc/$process_id/stat")
    [[ "$process_state" != "Z" ]]
}

printf '停止该 runtime 的 Dagster 进程树（PID %s）。\n' "$managed_pid"
for process_id in "${process_ids[@]}"; do
    kill -TERM "$process_id" 2>/dev/null || true
done
for (( attempt = 0; attempt < 50; attempt++ )); do
    any_running=false
    for process_id in "${process_ids[@]}"; do
        if process_is_running "$process_id"; then
            any_running=true
            break
        fi
    done
    if [[ "$any_running" == false ]]; then
        command rm -f -- "$pid_file"
        exit 0
    fi
    sleep 0.1
done

printf 'Dagster 进程未及时退出，强制终止。\n'
for process_id in "${process_ids[@]}"; do
    kill -KILL "$process_id" 2>/dev/null || true
done
command rm -f -- "$pid_file"
