# Tests

Run `./test.sh` or configure with `cmake -S src/linkerta -B build -DBUILD_TESTING=ON`, build, then `ctest --test-dir build --output-on-failure --no-tests=error`.

Tests call the production protocol, snapshot assembler and runtime through an injected in-memory transport. The default core suite requires no CAN device, root access or ROS installation. Optional ROS2 and vcan suites have the prerequisites described below. Linux, native ARM64 Linux and macOS builds run in CI. Separate Linux jobs run ASan/UBSan and TSan. The ROS2 job builds and executes the package tests with colcon.

GoogleTest is discovered from the installed CMake package first; otherwise FetchContent obtains v1.14.0 pinned to commit f8d7d77c06936315286eb55f8de22cd23c188571. Pass `-DLINKERTA_ALLOW_DEPENDENCY_DOWNLOADS=OFF` to fail rather than download missing dependencies. For offline builds pass `-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/absolute/path/to/googletest` or `-DGTest_DIR=/path/to/installed/cmake/GTest`.

`./test.sh --coverage` requires lcov/genhtml at configure time and fails if the coverage target is unavailable. Coverage includes the core library; percentages are measured, not assumed. Hardware timing, bus-off, CAN acknowledgments and device calibration require separate bench validation.

The transport dependency can be supplied offline with
`-DFETCHCONTENT_SOURCE_DIR_LINKERBOT_TRANSPORT=/path/to/linkerbot-transport-cpp`.
Session integration tests use the dependency's real Session and FakeTransport:
routing, queue bounds, cancellation and external ownership are exercised in
addition to the unchanged device codec and snapshot suite. See [SESSION.md](SESSION.md).

## Lifecycle regression and long-running verification

`Lifecycle.RepeatedCreateReadCloseReleasesResources` warms up the runtime, then
creates, reads from and closes 40 real Session/LinkerArm instances using
FakeTransport. Linux records a stable `/proc/self/task` baseline and compares
both task and fd counts exactly after the loop, allowing a bounded wait for
kernel thread teardown but no count tolerance. Other platforms still exercise
the lifecycle loop without Linux procfs assertions.

This short simulation catches retained resources; it does not demonstrate
eight-hour stability or physical CAN behavior. The pending manual procedure in
[SOAK.md](SOAK.md) requires actual device metadata, raw observations and explicit
results. No eight-hour soak result is claimed by this CI test.

## Optional integration suites

From the repository root, ROS2 Humble builds run the real node and executor,
including a test-only FakeTransport fixture launched as a separate process:

```sh
source /opt/ros/humble/setup.bash
colcon build --packages-select linkerta --cmake-args -DUSE_ROS2=ON -DBUILD_TESTING=ON
colcon test --packages-select linkerta --return-code-on-test-failure --event-handlers console_direct+
colcon test-result --verbose
```

The Linux vcan job provisions an isolated interface before testing. Provisioning
requires administrator privileges; test execution does not configure interfaces.
Use only an interface created for this test, not a live robot bus:

```sh
sudo modprobe vcan
sudo ip link add dev vcan-test type vcan
sudo ip link set vcan-test up
cmake -S src/linkerta -B build-vcan -DBUILD_TESTING=ON -DLINKERTA_TEST_VCAN_INTERFACE=vcan-test
cmake --build build-vcan --parallel
ctest --test-dir build-vcan --output-on-failure --no-tests=error
sudo ip link delete vcan-test
```

Enable `LINKERTA_BUILD_BENCHMARKS=ON` to build the optional measurement tool and its
CSV/accounting tests; see [BENCHMARK.md](BENCHMARK.md). Synthetic results and vcan
traffic are not physical-device qualification. Fixed historical CI evidence and
remaining gaps are listed in [VALIDATION.md](VALIDATION.md).

Run `python3 tools/check_doc_links.py` to check Markdown relative file links from
the repository. This is an offline file-target check; it does not validate remote
URLs or heading anchors. API snippets involving an interface require hardware;
the installed consumer test supplies FakeTransport and tests actual linking.
