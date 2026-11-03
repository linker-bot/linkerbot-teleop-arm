# Test gate

`./test.sh` configures BUILD_TESTING=ON, builds the test executable and runs CTest. Build or test failure returns nonzero. An empty test set is an error. Linux x86_64/ARM64 and ROS2 jobs execute tests, with separate ASan+UBSan and TSan jobs.

This first gate executes the existing test suite; many legacy tests only exercise duplicated algorithms. The following safety and protocol changes replace those with tests against production code. Coverage claims in older documentation were targets, not measured results.

For offline builds configure `-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/path/to/googletest` before running `./test.sh`; the CMake cache retains it. GoogleTest is pinned to the v1.14.0 commit. Hardware behavior is outside these unit tests.
