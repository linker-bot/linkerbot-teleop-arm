# 文档索引与维护约定

这些指南描述当前仓库实现，不以历史设备演示替代当前验证。首次使用先完成无设备构建和测试，再配置真实 CAN 接口。

## 按任务阅读

- [安装与部署](INSTALL.md)：依赖、离线构建、安装消费及管理员配置 CAN 的边界。
- [C++ API](API.md)：请求与缓存、单位、默认值、线程和资源所有权。
- [ROS2 接口](ROS2.md)：参数、话题、时间戳、诊断和显式标定。
- [协议与快照](PROTOCOL.md)：帧长度、角度、故障有效性及设备协议未知项。
- [Session 集成](SESSION.md)：路由、回执、超时、取消及恢复。
- [故障排查](TROUBLESHOOTING.md)与[常见问题](FAQ.md)：从可观察结果定位原因。
- [测试](TESTING.md)与[验证证据](VALIDATION.md)：自动化范围、固定提交和未验证事项。
- [性能采集](BENCHMARK.md)与[长期运行](SOAK.md)：模拟结果和真实台架的区别。
- [升级与问题报告](OPERATIONS.md)：版本固定、回滚与问题复现材料。
- [安全与运行边界](SAFETY.md)：权限、标定及部署限制。

## 维护约定

修改参数、默认值、CLI、公共 API、协议或构建选项时，同步修改相关指南与可执行示例。参数以 `NodeConfig.h` 和 `TeleArmNode.cpp` 为准；CLI 以 `linkerta --help` 为准；线协议以 `ArmProtocol.cpp` 和其回归测试为准。

本项目当前指南主要使用中文，部分测试与性能指南使用英文；接口标识保持源码拼写。不要把旧指令复制为另一份入口；用相对链接指向负责该主题的指南。文档中的示例要标明是否打开硬件，避免把“无设备测试通过”写成“实机验证完成”。

组织方式参考 [linkerbot-transport-cpp 文档](https://github.com/soulteary/linkerbot-transport-cpp/tree/aa07e61ca10c2fa5b7a41de1293f87729ef761bd/docs)：按使用任务分层，明确所有权、可观察失败、证据和限制。本项目只集成 classic SocketCAN，不因此继承参考库其他后端的平台支持承诺。
