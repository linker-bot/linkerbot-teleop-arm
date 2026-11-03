# 测试快速参考

从项目根目录执行 `./test.sh`。测试调用实际协议解码器、快照组装器和 Session；内存传输使用依赖提供的 FakeTransport。

- `test_protocol.cpp`：协议长度、角度与完整快照；保留 PR3 的回归用例。
- `test_runtime.cpp`：实际 Session 上的读取、超时、恢复和生命周期。
- `test_session.cpp`：路由、队列、取消与共享会话所有权。
- `test_canbus_io.cpp`：实际 SocketCAN 后端的 Linux datagram socket 测试。
- `test_node_config.cpp`：参数与启动标定保护。

测试不要求真实 CAN 硬件；Linux 专属 socket 用例在其他平台不注册。物理 ACK、bus-off、固件及标定成功仍需台架验证。

详细说明见 [测试文档](../../../docs/TESTING.md) 与 [Session 契约](../../../docs/SESSION.md)。
