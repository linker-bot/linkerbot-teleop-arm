# C++ Report Card

[English](../QUALITY.md) · 简体中文 · [文档](README.md)

独立的 CI 工作流先生成编译数据库，再通过固定版本的 C++ Report Card action 运行 clang-format、clang-tidy 和 Cppcheck。报告作为 CI 附件上传，不自动提交评分或徽章。引用结果时，应给出具体提交和已完成的运行。

分析构建使用 Ubuntu 24.04 的 ROS2 Jazzy，以配合静态分析工具链。运行测试仍使用 Humble；分析构建不扩展已声明的硬件或 ROS 部署支持范围。

分析范围包括本项目核心库、CLI、ROS2 适配器、测量工具和测试。下载的依赖与生成的构建文件不属于本项目的分析范围，但分析源码时仍保留所需头文件路径。如果使用测试框架模型，需要有回归检查，证明模型没有掩盖真实缺陷。

门禁要求完整的 full 模式报告，所有配置的工具均零诊断。工具失败、分析不完整或产生诊断都会使任务失败。先修复或解释问题，不通过降低阈值、排除源码来换取评分。

静态分析补充[运行测试](TESTING.md)，不等于覆盖率，也不替代[实机验证](VALIDATION.md)。工具设置和构建输入见 `.cppreportcard.json` 与 `.github/workflows/cppreportcard.yml`。
