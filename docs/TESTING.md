# Tests

Run `./test.sh` or configure with `cmake -S src/linkerta -B build -DBUILD_TESTING=ON`, build, then `ctest --test-dir build --output-on-failure --no-tests=error`.

Tests call the production protocol, snapshot assembler and runtime through an injected in-memory transport. They require no CAN device, root access or ROS installation. Linux, native ARM64 Linux and macOS builds run in CI. Separate Linux jobs run ASan/UBSan and TSan. The ROS2 job builds and executes the package tests with colcon.

GoogleTest is discovered from the installed CMake package first; otherwise FetchContent obtains v1.14.0 pinned to commit f8d7d77c06936315286eb55f8de22cd23c188571. Pass `-DLINKERTA_ALLOW_DEPENDENCY_DOWNLOADS=OFF` to fail rather than download missing dependencies. For offline builds pass `-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/absolute/path/to/googletest` or `-DGTest_DIR=/path/to/installed/cmake/GTest`.

`./test.sh --coverage` requires lcov/genhtml at configure time and fails if the coverage target is unavailable. Coverage includes the core library; percentages are measured, not assumed. Hardware timing, bus-off, CAN acknowledgments and device calibration require separate bench validation.
