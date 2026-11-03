# 文档

[English](../README.md) · 简体中文

首次使用请先完成构建和测试，再按安装指南配置 CAN 接口。

## 使用指南

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

- [C++ 开发与文档](CPP_GUIDE.md)。
- [C++ Report Card](QUALITY.md)。

## 修改文档

参数或命令有变化时，请同时更新对应页面和示例。参数定义在 `NodeConfig.h` 和 `TeleArmNode.cpp`，CLI 可用 `linkerta --help` 查看，协议实现在 `ArmProtocol.cpp`。新增文档后运行 `python3 tools/check_doc_links.py` 检查本地链接。

英文是默认版本，中文按同名文件放在 `docs/zh/`。参数、行为或命令有改动时两种语言一起更新。

各页按主题维护，重复说明尽量用链接代替。文档中的实机命令不会由普通单元测试执行；测试范围见[验证记录](VALIDATION.md)。
