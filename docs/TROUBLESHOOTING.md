[English](TROUBLESHOOTING.md) | [简体中文](zh/TROUBLESHOOTING.md)

# Troubleshooting

Save the complete error message, commit, and startup parameters before investigating. The sections below cover builds, interfaces, topics, and configuration.

## Configuration or build failure

Check the toolchain and selected build mode:

```sh
cmake --version
./build.sh core
```

The project requires CMake 3.18+, C++17, and thread support. Only ROS2 mode requires ament, colcon, and ROS message packages. Source `/opt/ros/humble/setup.bash`, then run `./build.sh ros2`. Validation uses Humble; if several ROS distributions are installed, check that the environment does not mix their prefixes.

If `linkerbot-comm` is missing, check that the pinned dependency can be downloaded, or install it offline and set `CMAKE_PREFIX_PATH` correctly. If GoogleTest is missing, check test dependencies rather than disabling tests. Downstream `find_package` calls must point to installation prefixes, not source or old build directories. See [INSTALL.md](INSTALL.md) for installation and clean consumer builds.

Keep the first complete compiler diagnostic, compiler version, and build command. The project already links Threads. Identify the failing target and fix the cause; reducing warning levels hides information.

## Immediate exit or failure to open an interface

Invalid CLI arguments are rejected before hardware is opened. Run `linkerta --help` to check options. ROS2 also validates parameters first: verify that `calibration` is 0, the direction array has the right size and values, and rates and timeouts are in range. Empty `can_interface` uses discovery; select an explicit interface when multiple devices are candidates.

Inspect the system interface without modifying it:

```sh
ip -details link show can0
ip -s link show can0
```

If the interface is missing, down, or configured with the wrong bitrate, have an administrator correct it during a maintenance window as described in the installation guide. The node does not elevate privileges or configure interfaces. SocketCAN is not a `/dev/tty*` serial device; adding `dialout`, using chmod, or permanently running under sudo is not a general solution. For permission errors, retain errno and inspect container network namespaces, interface visibility, and system security policy.

## The node exists but position topics are silent

```sh
ros2 node list
ros2 node info /linkerta_node
ros2 topic echo /diagnostics
ros2 topic echo /joint_states
```

Confirm that this is the intended node and interface. `/joint_states` uses radians; legacy control topics use degrees and may be disabled by `publish_legacy_topics=false`. Incomplete, timed-out, or stale replies intentionally produce no position messages. Republishing cached values may increase apparent frequency while delivering stale positions to consumers.

Check bus counters, power, wiring, bitrate, and termination. If needed, use an installed can-utils `candump can0` for a read-only capture. Retain raw frames and compare their IDs, DLCs, and flags against [PROTOCOL.md](PROTOCOL.md). The six-byte legacy format has no fault bits and should report unknown. There is no confirmed device cycle number in the four frames, so arrival order alone cannot establish synchronized sampling.

## Configuration changes have no effect

`ros2 launch` normally reads the installed share directory. Use `ros2 pkg prefix linkerta` to check which package is selected and whether an old workspace has been sourced. Reinstall changed source YAML or select a file with `--ros-args --params-file /absolute/path/lta.yaml`. Parameters are read only at startup, so runtime updates are rejected. Restart with the correct configuration; no source change is needed.

## Calibration returns false or has an uncertain result

Read the service response message and `/diagnostics` first. Disabled service, consumed attempt, queued command, and uncertain sending are different states. No device-completion ACK has been implemented and verified, so sending a command does not produce confirmed success. Do not set `calibration` to 1 or use automatic restart or repeated calls as recovery. Keep the device in a safe state and follow its procedure to determine whether calibration ran; see [SAFETY.md](SAFETY.md).

## Timeouts, overflow, or resource growth

Record request rate, timeout, CPU/memory load, and CAN error counters first. Reduce the request rate through startup parameters rather than changing sleep calls or elevating privileges to alter thread priorities. Investigate missing frames and bus errors before increasing timeouts.

Track RSS, threads, and file descriptors over time to distinguish startup warm-up from continued growth. See [TESTING.md](TESTING.md) for short regressions and [SOAK.md](SOAK.md) for manual long-running collection. The [benchmark tool](BENCHMARK.md) also supports simulation; those results do not establish hardware stability.

## What should an issue report include?

Provide the commit SHA, OS/kernel, compiler, ROS_DISTRO, build and run commands, sanitized parameters, complete error logs, adapter and firmware identifiers, read-only interface state, and minimal reproduction steps. Include metadata with benchmark CSV, retaining the synthetic flag and failure samples. Average latency or screenshots alone rarely locate a problem. If restarting helped, retain the logs from before the restart too.
