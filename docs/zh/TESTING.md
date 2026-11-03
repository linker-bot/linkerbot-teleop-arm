# 测试

[English](../TESTING.md)

在仓库根目录执行 `./test.sh`。需要使用自己的构建目录时，先以 `cmake -S src/linkerta -B build -DBUILD_TESTING=ON` 配置并构建，再执行 `ctest --test-dir build --output-on-failure --no-tests=error`。

默认核心测试通过内存传输运行协议、快照组装器和运行时，不需要 CAN 设备、root 权限或 ROS 环境。ROS2 和 vcan 测试需要下文的准备步骤。CI 在 Linux、原生 ARM64 Linux 和 macOS 上构建，使用独立的 Linux 任务运行 ASan/UBSan 和 TSan，并通过 colcon 运行 ROS2 包测试。

CMake 先查找已安装的 GoogleTest 包，找不到时下载固定在提交 `f8d7d77c06936315286eb55f8de22cd23c188571` 的 v1.14.0。使用 `-DLINKERTA_ALLOW_DEPENDENCY_DOWNLOADS=OFF` 禁用下载。离线构建时提供 `-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/absolute/path/to/googletest` 或 `-DGTest_DIR=/path/to/installed/cmake/GTest`。

`./test.sh --coverage` 覆盖核心库，要求配置时已有 lcov/genhtml。覆盖率目标不可用时会失败。硬件时序、bus-off、CAN ACK 和标定需要单独的台架测试。

传输依赖可通过 `-DFETCHCONTENT_SOURCE_DIR_LINKERBOT_TRANSPORT=/path/to/linkerbot-transport-cpp` 离线提供。Session 测试使用依赖库的 Session 和 FakeTransport，在解码器与快照测试之外验证路由、队列边界、取消和外部所有权，详见 [SESSION.md](SESSION.md)。

## 检查资源回收

`Lifecycle.RepeatedCreateReadCloseReleasesResources` 先预热运行时，再创建、读取并关闭 40 个由 FakeTransport 支持的 Session/LinkerArm 实例。Linux 会记录稳定的 `/proc/self/task` 基线，在循环结束后精确比较线程数和文件描述符数；允许有限时间等待内核完成线程清理，但不放宽数量差值。其他平台仍执行生命周期循环，只是不做 Linux procfs 断言。

这个循环可以发现资源未释放的问题，但不足以证明八小时稳定性。带设备信息和原始记录的硬件运行步骤见 [SOAK.md](SOAK.md)。

## 可选集成测试

在仓库根目录执行以下命令，可在 ROS2 Humble 中构建并运行真实节点和 executor。测试还会将仅用于测试的 FakeTransport fixture 作为独立进程启动：

```sh
source /opt/ros/humble/setup.bash
colcon build --packages-select linkerta --cmake-args -DUSE_ROS2=ON -DBUILD_TESTING=ON
colcon test --packages-select linkerta --return-code-on-test-failure --event-handlers console_direct+
colcon test-result --verbose
```

Linux vcan 任务会先创建隔离接口。创建接口需要管理员权限，测试本身不会配置接口。应使用测试接口，切勿使用正在工作的机器人总线：

```sh
sudo modprobe vcan
sudo ip link add dev vcan-test type vcan
sudo ip link set vcan-test up
cmake -S src/linkerta -B build-vcan -DBUILD_TESTING=ON -DLINKERTA_TEST_VCAN_INTERFACE=vcan-test
cmake --build build-vcan --parallel
ctest --test-dir build-vcan --output-on-failure --no-tests=error
sudo ip link delete vcan-test
```

启用 `LINKERTA_BUILD_BENCHMARKS=ON` 可构建测量工具及其 CSV/统计测试，详见 [BENCHMARK.md](BENCHMARK.md)。已记录的 CI 结果和仍待完成的硬件检查见 [VALIDATION.md](VALIDATION.md)。

执行 `python3 tools/check_doc_links.py` 可检查 Markdown 中指向本地文件的链接，不检查远程 URL 或标题锚点。打开接口的 API 示例需要硬件；安装消费测试会链接库并使用 FakeTransport。
