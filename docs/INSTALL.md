[English](INSTALL.md) | [简体中文](zh/INSTALL.md)

# Building, installing, and deploying

The core requires C++17, CMake 3.18+, Threads, and linkerbot-transport 0.3.3. Missing dependencies are downloaded from fixed commits by default. For offline builds, preinstall linkerbot-transport, set `CMAKE_PREFIX_PATH`, and use `LINKERTA_ALLOW_DEPENDENCY_DOWNLOADS=OFF`. GoogleTest is needed only for test builds.

```sh
cmake -S src/linkerta -B build-core -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DCMAKE_INSTALL_PREFIX="$PWD/install-core"
cmake --build build-core --parallel
ctest --test-dir build-core --output-on-failure --no-tests=error
cmake --install build-core
```

Downstream projects use the installed dependency and library:

```cmake
find_package(linkerta CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE linkerta::core)
```

ROS2 has been validated on Humble. Source `/opt/ros/humble/setup.bash`, then run `./build.sh ros2`. `./build.sh core` needs neither ROS2 nor colcon. `./startup.sh` loads the workspace and uses `exec` to run `ros2 launch`. It does not intercept launch's exit code. Launch manages node signals, exits, and default respawn; a node's exit code is not guaranteed to become the script's exit code. These scripts do not configure interfaces, change permissions, or calibrate automatically.

## Configure CAN explicitly

The following is an administrator's example for a maintenance window. Match the interface and bitrate to the device; do not run these commands from the driver process:

```sh
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 1000000
sudo ip link set can0 up
ip -details link show can0
```

The application only opens a preconfigured classic SocketCAN interface. Linux provides the hardware backend. macOS can build the core, protocol tests, and CLI help but has no SocketCAN hardware backend.

```sh
linkerta --help
linkerta --interface can0 --bitrate 1000000 --rate-hz 10 --timeout-ms 250 --count 20
```

The legacy invocation `linkerta can0 1000000` remains supported. The bitrate argument does not configure the operating-system interface. Position output is in degrees and requires a complete new snapshot; failures return a nonzero exit code.

## Units and licenses

The standard ROS `/joint_states` topic uses radians. Legacy arm-control topics remain in degrees and can be disabled. Core positions use degrees. See [PROTOCOL.md](PROTOCOL.md) for payload lengths, unknown fault states, and version-encoding limits. A device-completion ACK for calibration has not been implemented or verified. Calibration is not replayed automatically, and backend send acceptance does not mean device completion.

The project's [Apache-2.0 license](../LICENSE) and [third-party notices](../THIRD_PARTY_NOTICES.md) are installed under `<prefix>/share/licenses/linkerta`. Retain that directory when distributing binaries. The original transport MIT and GoogleTest BSD-3-Clause licenses are in [LICENSES](../LICENSES). GoogleTest is used only for testing and is not linked into the driver.
