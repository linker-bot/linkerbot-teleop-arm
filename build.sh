#!/usr/bin/env bash
set -eo pipefail
root=$(cd -- "$(dirname -- "$0")" && pwd)
cd "$root"
case "${1:-ros2}" in
  core)
    shift || true
    cmake -S src/linkerta -B build-core -DBUILD_TESTING=ON "$@"
    cmake --build build-core --parallel
    ;;
  ros2)
    if [[ $# -gt 0 ]]; then shift; fi
    if [[ -z ${ROS_DISTRO:-} ]]; then source /opt/ros/humble/setup.bash; fi
    colcon build --packages-select linkerta --cmake-args -DUSE_ROS2=ON -DBUILD_TESTING=ON "$@"
    ;;
  *) echo "Usage: $0 [core|ros2] [CMake arguments...]" >&2; exit 2 ;;
esac
