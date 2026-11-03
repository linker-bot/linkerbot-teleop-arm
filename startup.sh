#!/usr/bin/env bash
set -eo pipefail
root=$(cd -- "$(dirname -- "$0")" && pwd)
if [[ -z ${ROS_DISTRO:-} ]]; then source /opt/ros/humble/setup.bash; fi
source "$root/install/setup.bash"
exec ros2 launch linkerta run.launch.py "$@"
