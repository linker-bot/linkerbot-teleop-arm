# Tests

[简体中文](zh/TESTING.md)

Run `./test.sh` from the repository root. To use your own build directory,
configure with `cmake -S src/linkerta -B build -DBUILD_TESTING=ON`, build, then run
`ctest --test-dir build --output-on-failure --no-tests=error`.

The default core suite runs the protocol, snapshot assembler and runtime through
an in-memory transport. It needs no CAN device, root access or ROS installation.
ROS2 and vcan tests require the setup below. CI builds on Linux, native ARM64
Linux and macOS, runs ASan/UBSan and TSan in separate Linux jobs, and uses colcon
for ROS2 package tests.

CMake looks for an installed GoogleTest package first, then downloads v1.14.0 at
commit `f8d7d77c06936315286eb55f8de22cd23c188571`. Disable downloads with
`-DLINKERTA_ALLOW_DEPENDENCY_DOWNLOADS=OFF`. For an offline build, supply
`-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/absolute/path/to/googletest` or
`-DGTest_DIR=/path/to/installed/cmake/GTest`.

`./test.sh --coverage` includes the core library and requires lcov/genhtml at
configure time. It fails if the coverage target is unavailable. Hardware timing,
bus-off, CAN acknowledgments and calibration need separate bench tests.

The transport dependency can be supplied offline with
`-DFETCHCONTENT_SOURCE_DIR_LINKERBOT_TRANSPORT=/path/to/linkerbot-transport-cpp`.
Session tests use the dependency's Session and FakeTransport to exercise routing,
queue bounds, cancellation and external ownership alongside the codec and snapshot
tests. See [SESSION.md](SESSION.md).

## Check resource cleanup

`Lifecycle.RepeatedCreateReadCloseReleasesResources` warms up the runtime, then
creates, reads from and closes 40 Session/LinkerArm instances backed by
FakeTransport. Linux records a stable `/proc/self/task` baseline and compares
both task and fd counts exactly after the loop, allowing a bounded wait for
kernel thread teardown but no count tolerance. Other platforms still exercise
the lifecycle loop without Linux procfs assertions.

The loop can catch retained resources, but is too short to show eight-hour
stability. Use [SOAK.md](SOAK.md) for a hardware run with device metadata and raw
observations.

## Optional integration suites

From the repository root, ROS2 Humble builds run the real node and executor,
including a test-only FakeTransport fixture launched as a separate process:

```sh
source /opt/ros/humble/setup.bash
colcon build --packages-select linkerta --cmake-args -DUSE_ROS2=ON -DBUILD_TESTING=ON
colcon test --packages-select linkerta --return-code-on-test-failure --event-handlers console_direct+
colcon test-result --verbose
```

The Linux vcan job creates an isolated interface before testing. Creating it
requires administrator privileges; the tests themselves do not configure
interfaces. Use a test interface, never a live robot bus:

```sh
sudo modprobe vcan
sudo ip link add dev vcan-test type vcan
sudo ip link set vcan-test up
cmake -S src/linkerta -B build-vcan -DBUILD_TESTING=ON -DLINKERTA_TEST_VCAN_INTERFACE=vcan-test
cmake --build build-vcan --parallel
ctest --test-dir build-vcan --output-on-failure --no-tests=error
sudo ip link delete vcan-test
```

Enable `LINKERTA_BUILD_BENCHMARKS=ON` for the measurement tool and its
CSV/accounting tests; see [BENCHMARK.md](BENCHMARK.md). For recorded CI results and
remaining hardware checks, see [VALIDATION.md](VALIDATION.md).

Run `python3 tools/check_doc_links.py` to check Markdown links to local files. It
does not check remote URLs or heading anchors. API snippets that open an
interface need hardware; the installed consumer links the library and uses
FakeTransport.
