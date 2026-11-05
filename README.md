# LinkerBot Teleoperation Arm

English · [简体中文](README.zh-CN.md)

[![C++ Report Card](https://github.com/linker-bot/linkerbot-teleop-arm/actions/workflows/cppreportcard.yml/badge.svg)](https://github.com/linker-bot/linkerbot-teleop-arm/actions/workflows/cppreportcard.yml)

Read 14 joints from a dual-arm device over classic SocketCAN. The project provides
a C++17 core library, a CLI and a ROS2 node. Communication uses
linkerbot-transport-cpp Session; decoding and snapshot assembly do not depend on ROS.
Linux provides hardware I/O. macOS runs core builds and tests; ROS2 CI tests Humble.

Start with the commands below. The [documentation index](docs/README.md) covers
API usage, ROS2, troubleshooting and validation.

## Project naming and compatibility

The repository is named `linkerbot-teleop-arm`. Existing integration names remain
`linkerta`: the CMake and ROS2 packages, `linkerta::core`, CLI and node names,
header and installation paths, and `LINKERTA_*` options are unchanged. Existing
launch commands, ROS topics, services and configuration keys remain supported.

## Build and install

Run from the repository root:

```sh
./build.sh core
./test.sh
cmake --install build-core --prefix "$PWD/install-core"
```

CMake fetches a pinned transport revision if it cannot find an installed package.
For an offline build, install the dependencies first and pass
`-DLINKERTA_ALLOW_DEPENDENCY_DOWNLOADS=OFF -DCMAKE_PREFIX_PATH=/path/to/dependencies`.
Tests also require GoogleTest. See [installation](docs/INSTALL.md).
C++ consumers use `find_package(linkerta CONFIG REQUIRED)` and `linkerta::core`.

```cpp
#include <linkerta/LinkerArm.h>

LinkerArm::LinkerArm arm("can0", 1000000);
const auto snapshot = arm.readJointState();
// snapshot.positions_degrees is a complete snapshot in degrees; timeout throws.
```

Headers install under `<prefix>/include/linkerta`. The [C++ API guide](docs/API.md)
covers calls; the [Session guide](docs/SESSION.md) covers sharing, shutdown and recovery.
`latestSnapshot()` only reads the cache. Check `valid` and `isFresh()` before using it;
zero-initialized storage before the first reply is not a measured pose.

## Configure CAN and run the CLI

An administrator configures CAN outside the process; commands are in the installation
guide. The driver does not invoke sudo, configure bitrate or calibrate at startup.
The CLI bitrate argument is retained for compatibility and does not change the OS interface.

```sh
install-core/bin/linkerta --help
install-core/bin/linkerta --interface can0 --bitrate 1000000 --rate-hz 10 --timeout-ms 250 --count 20
# The positional form remains supported:
install-core/bin/linkerta can0 1000000
```

Each output line is a new snapshot in degrees. A query failure exits with a nonzero
status. `--rate-hz` limits requests; device response time and bus load also limit throughput.

## ROS2

```sh
source /opt/ros/humble/setup.bash
./build.sh ros2
./startup.sh
```

The source configuration is `src/linkerta/config/lta.yaml`; installation copies it
into the package share directory. You can also run
`ros2 run linkerta linkerta_node --ros-args -p can_interface:=can0`.
Driver parameters are read-only after startup:

- `can_interface`: empty selects discovery by `device` (default `master_arm`); otherwise opens that configured interface.
- `poll_rate_hz`: default 100, range 1–200.
- `request_timeout_ms`, `max_snapshot_age_ms`: default 250, range 1–10000.
- `negation`: exactly 14 values, each -1 or 1.
- `publish_legacy_topics`: default true; can disable legacy topics.
- `calibration`: only 0 is accepted; startup calibration is forbidden.
- `allow_calibration`: default false. When enabled, `calibration_joint` selects 0–13 or 255 (all).

Published interfaces:

- `/joint_states`: `sensor_msgs/msg/JointState`, 14 joints in radians.
- `/left_arm_joint_control`, `/right_arm_joint_control`: legacy topics, seven joints each in degrees.
- `/joint_error_code`: legacy string; missing fault bits are `unknown`.
- `/diagnostics`: `diagnostic_msgs/msg/DiagnosticArray`, including timeout, stale data and joint faults.

The node publishes each new complete snapshot once. Missing, expired or incomplete
results stop position publication and update diagnostics. Messages from one snapshot
share a timestamp. The stamp approximates host reception by subtracting steady-clock
snapshot age from ROS time; it is not device sampling time. Paused, scaled or jumping
simulation time affects that mapping; negative values are clamped to zero.

`~/calibrate_once` uses `std_srvs/srv/Trigger`. It queues at most one attempt per
process when explicitly enabled, with no retry after send failure. Device completion
ACKs are not implemented or verified, so the response uses `success=false` and explains
the state in its message and diagnostics. Restart does not send calibration automatically.

## Protocol and tests

The [protocol guide](docs/PROTOCOL.md) describes DLC, signed angles, unknown faults
and version bytes. See [operating limits](docs/SAFETY.md), [tests](docs/TESTING.md)
and [troubleshooting](docs/TROUBLESHOOTING.md).
The four replies have no device cycle ID, so assembly cannot prove simultaneous
sampling or reject every late reply. [Validation records](docs/VALIDATION.md) separate
completed tests from pending hardware work.

## License

The project uses [Apache-2.0](LICENSE). The runtime transport uses MIT and the
GoogleTest test dependency uses BSD-3-Clause. Original notices are in [LICENSES](LICENSES).
Installation includes them under `<prefix>/share/licenses/linkerta`; keep that directory
with binary distributions. See [third-party notices](THIRD_PARTY_NOTICES.md).
