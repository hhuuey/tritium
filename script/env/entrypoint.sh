#!/usr/bin/env bash
set -eo pipefail

if [[ -n "${ROS_DISTRO:-}" ]]; then
  source "/opt/ros/$ROS_DISTRO/setup.bash"
fi
exec "$@"
