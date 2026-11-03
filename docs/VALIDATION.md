# 已有验证和待测项目

[文档索引](README.md) · [测试运行](TESTING.md) · [性能采集](BENCHMARK.md)

## 自动化测试覆盖

当前 CI 覆盖 Linux x86-64/ARM64、macOS 核心构建、ASan/UBSan、TSan、安装重定位消费、ROS2 Humble 与 Linux vcan。FakeTransport 替换底层传输，测试仍调用生产代码中的 Session、协议解码器和快照组装器。

PR8 合并前的固定证据提交为 `3fa95b4e90e76b27a2d556950f64b0a05ba359ca`：

- [PR CI](https://github.com/soulteary/linkerhand-tele-arm/actions/runs/41648503034)：Linux/TSan/ASan 各 58 项，vcan 59 项，macOS 核心 53 项。
- ROS2 colcon 汇总 73 项、零错误/失败/跳过；另对四项生命周期与退出测试各重复三次。
- 基线工具测试启用时，Linux 62 项、macOS 57 项；覆盖缺回复统计、非整数成功率精度、输出防覆盖及 SIGTERM 部分结果完整性。

这些数字对应上述固定提交。后续用例可能增减，查看新版本时应核对该提交在 [Actions](https://github.com/soulteary/linkerhand-tele-arm/actions) 中的结果。

## 仍需实机验证

- vcan 不验证接线、终端电阻、物理 ACK、适配器驱动或真实 bus-off。
- 模拟基线明确标记 `synthetic=true`，不能用于实机吞吐/延迟 SLO。
- 没有设备周期编号时，主机组帧无法严格证明同步采样或完全排除晚到的旧响应。
- 6/7/8 字节回复在不同固件上的实际分布、版本编码、标定完成 ACK，需要设备资料和抓帧。
- 本轮没有八小时真实设备 soak；步骤见 [SOAK.md](SOAK.md)。

进行发布验证时，需要记录设备、固件、适配器、接口、bitrate、负载、主机和源码提交，并保留原始 CAN 抓帧、CSV 和 metadata。实机结论应对应这些原始记录；上面的自动化结果不能替代尚未完成的测试。
