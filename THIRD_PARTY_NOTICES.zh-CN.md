# 第三方声明

[English](THIRD_PARTY_NOTICES.md) · 简体中文

Linkerhand Tele Arm 使用 [Apache-2.0](LICENSE)。依赖保留各自的许可证和版权声明。

## linkerbot-transport-cpp

核心库 `linkerta::core` 链接此运行时传输库。使用 [MIT 许可证](LICENSES/linkerbot-transport-cpp.MIT.txt)，并保留[上游 NOTICE](LICENSES/linkerbot-transport-cpp.NOTICE.txt)。源码 fallback 固定在 [e514b832e09ecd60a56d2c67e39c54285793fff8](https://github.com/soulteary/linkerbot-transport-cpp/tree/e514b832e09ecd60a56d2c67e39c54285793fff8)，上述文件逐字复制自该提交。

已安装的 transport 可能不同于 fallback 版本，分发时保留实际所用包的声明。本项目的 fallback 构建不包含 transport 的测试和可选厂商二进制库。

## GoogleTest

GoogleTest 只用于测试，不链接进已安装的驱动或核心库。其 [BSD-3-Clause 许可证](LICENSES/GoogleTest.BSD-3-Clause.txt)逐字复制自 fallback 使用的 [v1.14.0 提交 f8d7d77c06936315286eb55f8de22cd23c188571](https://github.com/google/googletest/tree/f8d7d77c06936315286eb55f8de22cd23c188571)。分发 GoogleTest 源码或包含它的测试二进制时保留该文件。

## 安装位置

CMake 将本文件、英文声明、项目 LICENSE 和 LICENSES 目录安装到 `<prefix>/share/licenses/linkerta`，或 GNUInstallDirs 配置的数据目录。分发二进制时保留这些文件。ROS2、系统库和其他单独提供的包保留各自声明；这里不是构建机器或操作系统中所有软件的清单。
