#!/usr/bin/env bash
set -eo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
load_context "${1:-}" "${2:-}"
shift 2
[[ "$project" == fast-livo2 && "$profile" == ros1-noetic ]] ||
  fail "旧 bag 转换器必须在 FAST-LIVO2 ROS 1 环境运行。"
dispatch_container convert_bag.sh "$@"
load_application
exec fast_livo_bag_converter "$@"
