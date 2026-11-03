[English](FAQ.md) | [简体中文](zh/FAQ.md)

# Frequently asked questions

## What do I need to build the project?

The core requires C++17, CMake 3.18+, and Threads. Hardware access uses Linux SocketCAN. macOS can build the core, CLI help, and simulation tests. ROS2 CI uses Humble; other distributions have not received equivalent validation.

```sh
./build.sh core
./test.sh
```

For ROS2, source its environment first, then run `./build.sh ros2`. See [INSTALL.md](INSTALL.md) for installation, offline dependencies, and downstream `find_package(linkerta CONFIG REQUIRED)` usage.

## What if transport or GoogleTest is missing?

The default build downloads a pinned transport commit. GoogleTest is needed only for tests. For offline builds, install dependencies first, add their prefixes to `CMAKE_PREFIX_PATH`, and set `LINKERTA_ALLOW_DEPENDENCY_DOWNLOADS=OFF`. Missing dependencies cause configuration to fail; install them before rebuilding and testing.

## How do I choose a CAN interface and polling rate?

Use CLI options without changing code:

```sh
linkerta --help
linkerta --interface can0 --bitrate 1000000 --rate-hz 10 --timeout-ms 250 --count 20
```

The legacy form `linkerta can0 1000000` remains supported. Its bitrate argument does not configure the system interface. An administrator must configure CAN outside the process to match the device; see the installation guide. SocketCAN is a network interface, not a serial device, so membership in `dialout` is not a general fix.

ROS2 exposes startup parameters including `can_interface`, `poll_rate_hz`, `request_timeout_ms`, and `max_snapshot_age_ms`. An empty interface uses `device` for discovery; an explicit interface makes deployment more predictable. The rate is a host scheduling target, not guaranteed device throughput.

## Do YAML changes require recompilation?

Parameter-only changes do not require recompiling C++. However, launch reads the installed configuration, not the source copy. After changing the source YAML, reinstall it or explicitly pass the desired parameter file. Startup parameters are read-only: restart the node to apply changes.

```sh
ros2 run linkerta linkerta_node --ros-args --params-file /absolute/path/lta.yaml
```

`negation` must contain exactly 14 entries, each -1 or 1. See [README](../README.md#ros2) for the other valid ranges.

## Which topic uses radians? Does missing position data mean a fault?

`/joint_states` publishes all 14 joints in radians. The legacy left/right control topics still use degrees. `publish_legacy_topics=false` disables the old topics. CLI and core snapshots also use degrees.

```sh
ros2 topic echo /joint_states
ros2 topic echo /diagnostics
```

The node stops publishing positions before the first complete four-block response and on timeout, missing frames, or stale results, rather than repeating old data. Check diagnostics and bus state. Missing messages do not mean a zero pose; missing fault bits are `unknown`, not healthy.

## How does calibration work, and why does the service return success=false?

The startup parameter `calibration` accepts only 0. Starting or restarting the node never calibrates automatically. The `~/calibrate_once` service is allowed only when `allow_calibration=true` is explicitly set. `calibration_joint` selects 0–13 or 255 (all). Each process can attempt calibration at most once, with no automatic retry after failure.

There is no confirmed device-completion ACK. Trigger therefore returns `success=false`, while its message and diagnostics describe queueing or sending. The command may already have been sent; completion simply cannot be confirmed. Do not loop calls or restart to retry on false. Follow the device procedure to establish its actual state. See [SAFETY.md](SAFETY.md) and [PROTOCOL.md](PROTOCOL.md).

## How should I read state through the core API?

`readJointState(timeout)` requests and waits for a complete new snapshot, throwing on failure. `latestSnapshot()` returns only cached data: check `valid` and `isFresh()` first. Read positions, faults, and `fault_valid` from the same snapshot so unknown faults are not mistaken for healthy joints. `getVersion()` provides the implementation's version text; do not infer an unverified firmware encoding.

## Can simulation benchmarks prove hardware performance?

No. FakeTransport and vcan exercise software paths, not physical ACK, real bus-off, or synchronized device sampling. The [benchmark tool](BENCHMARK.md) labels synthetic data; [long-running validation](SOAK.md) still requires physical-bench records. See [TROUBLESHOOTING.md](TROUBLESHOOTING.md) for diagnostic steps.
