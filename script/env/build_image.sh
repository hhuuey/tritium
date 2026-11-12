#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
require_docker
load_profile "${1:-}"
[[ $# == 1 ]] || fail "用法：build_image.sh <环境>；可用 DOCKER_PLATFORM 设置构建架构。"
if [[ -n "${DOCKER_PLATFORM:-}" ]]; then
  exec docker build --platform "$DOCKER_PLATFORM" --target "$profile" -t "$image" "$script_dir"
fi
exec docker build --target "$profile" -t "$image" "$script_dir"
