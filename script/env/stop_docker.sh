#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
load_context "${1:-}" "${2:-}"
[[ $# == 2 ]] || fail "用法：stop_docker.sh <项目> <环境>"
[[ -z "${TRITIUM_PROJECT:-}" ]] || fail "请在宿主机停止容器。"
require_docker
if ! docker container inspect "$container" >/dev/null 2>&1; then
  echo "容器 $container 不存在，无需停止。"
  exit 0
fi
docker stop --timeout 30 "$container"
echo "容器已停止，源码、结果和命名卷保留。"
