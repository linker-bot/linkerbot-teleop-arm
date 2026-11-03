# 测试快速参考

[English](README.md)

在仓库根目录执行 `./test.sh`，即可配置、构建并运行无需硬件的核心测试。它不会启用 ROS2 或 vcan。构建选项见[测试指南](../../../docs/zh/TESTING.md)，依赖准备见[构建指南](../../../docs/zh/INSTALL.md)。测试运行的是驱动使用的解码器、组装器和基于 Session 的 arm。

- `test_protocol.cpp`：响应长度和标志、角度边界、版本字节、四块组装、重复帧、时差、接收边界、generation 和新鲜度。
- `test_runtime.cpp`：新读取与缓存读取、截止时间计算、并发、发送失败、溢出、共享、恢复及标定不重放。
- `test_session.cpp`：精确路由、有界队列、取消、共享所有权，以及依赖库异常发布顺序的回归。
- `test_canbus_io.cpp`：通过接管的 Linux 数据报套接字运行依赖库真实的 SocketCAN 编解码和 I/O 路径，不涉及物理 CAN 设备。
- `test_node_config.cpp` 与 `test_cli.cpp`：参数和命令行验证、拒绝启动标定、帮助信息及数值边界。
- `test_lifecycle.cpp`：重复创建、读取、关闭 Session/arm；Linux 还检查清理后的进程线程数与文件描述符数。
- `consumer/` 与 `install_consumer.cmake.in`：安装并移动安装目录，运行安装后的 CLI 帮助，再使用已安装公共头文件和 `linkerta::core` 构建、执行独立的 `find_package` 消费程序。
- `test_ros2_node.cpp`、`ros2_test_fixture.cpp` 与 `test_ros2_launch.py`：仅在 `USE_ROS2=ON` 时启用，通过 FakeTransport 验证真实节点、executor、话题、单位、时间戳、诊断、标定限制和退出。
- `test_vcan.cpp`：在显式提供、预先配置好的 Linux vcan 接口上运行可选的真实 SocketCAN 集成测试，测试不会自行配置接口。
- `test_benchmark.cpp` 与 `verify_benchmark.py`：仅在 `LINKERTA_BUILD_BENCHMARKS=ON` 时启用，验证基准工具参数和模拟输出。

空测试集或失败用例都必须使测试运行失败。运行 ROS2 测试前，应加载 ROS2 环境并安装声明的依赖。运行 vcan 测试前，先配置测试接口，再传入 `-DLINKERTA_TEST_VCAN_INTERFACE=<name>`。核心测试不需要 root 或 CAN 硬件。`./test.sh` 在本地平台运行，CI 负责 sanitizer 和架构矩阵。

物理确认、bus-off 恢复、固件时序和标定完成需要设备测试。模拟耗时仍是模拟数据。详见 [Session 契约](../../../docs/zh/SESSION.md)与[手动长稳测试流程](../../../docs/zh/SOAK.md)。
