# Test quick reference

[简体中文](README.zh-CN.md)

From the repository root, `./test.sh` configures, builds and runs the hardware-free
core tests. It does not enable ROS2 or vcan. Use [the testing guide](../../../docs/TESTING.md)
for build options and [the build guide](../../../docs/INSTALL.md) for dependencies.
The tests exercise the codec, assembler and Session-backed arm used by the driver.

- `test_protocol.cpp`: reply lengths and flags, angle boundaries, version bytes,
  four-block assembly, duplicates, skew, cutoffs, generation and freshness.
- `test_runtime.cpp`: fresh versus cached reads, deadline accounting, concurrency,
  send failures, overflow, sharing, recovery and no calibration replay.
- `test_session.cpp`: exact routes, bounded queues, cancellation, shared ownership
  and the dependency's exception-publication ordering regression.
- `test_canbus_io.cpp`: the dependency's real SocketCAN codec/I/O paths over adopted
  Linux datagram sockets. These are not physical CAN devices.
- `test_node_config.cpp` and `test_cli.cpp`: parameter and argument validation,
  startup-calibration rejection, help and numeric boundaries.
- `test_lifecycle.cpp`: repeated Session/arm create-read-close cycles; Linux also
  checks process thread and file-descriptor counts after teardown.
- `consumer/` and `install_consumer.cmake.in`: install and relocate the package,
  run installed CLI help, then build and execute an independent `find_package`
  consumer using the installed public headers and `linkerta::core`.
- `test_ros2_node.cpp`, `ros2_test_fixture.cpp` and `test_ros2_launch.py`: enabled
  only with `USE_ROS2=ON`; exercise the actual node, executor, topics, units,
  timestamps, diagnostics, calibration guard and shutdown with FakeTransport.
- `test_vcan.cpp`: opt-in real SocketCAN integration on an explicitly supplied,
  preconfigured Linux vcan interface. The test never configures one itself.
- `test_benchmark.cpp` and `verify_benchmark.py`: enabled only with
  `LINKERTA_BUILD_BENCHMARKS=ON`; validate benchmark options and synthetic output.

An empty suite or a failed test must fail the run. For ROS2 tests, source the
ROS2 environment and install the declared dependencies.
For vcan, configure a test interface first, then pass
`-DLINKERTA_TEST_VCAN_INTERFACE=<name>`. Core tests need neither root nor CAN
hardware. `./test.sh` runs on the local platform; CI runs the sanitizer and
architecture matrix.

Physical acknowledgment, bus-off recovery, firmware timing and calibration
completion require device tests. Synthetic timings are simulation data. See
[the Session contract](../../../docs/SESSION.md) and
[the manual soak procedure](../../../docs/SOAK.md).
