# Linkerhand-Tele-Arm

[![License: Apache-2.0](https://img.shields.io/badge/License-Apache--2.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![Platform](https://img.shields.io/badge/Platform-Linux-blue.svg)](https://www.linux.org/)
[![Architecture](https://img.shields.io/badge/Arch-x86__64%20%7C%20aarch64-lightgrey.svg)]()
[![ROS2](https://img.shields.io/badge/ROS2-Humble%20%7C%20Foxy-green.svg)](https://www.ros.org/)

> 用于第二版遥操臂的驱动程序

Linkerhand-Tele-Arm (LinkerTA) 是由灵心巧手（北京）科技有限公司开发的遥操臂驱动程序，支持通过 CAN 总线控制遥操臂设备，并提供 ROS2 接口。

## 📋 目录

- [特性](#-特性)
- [系统要求](#-系统要求)
- [快速开始](#-快速开始)
- [安装](#-安装)
- [配置](#-配置)
- [使用示例](#-使用示例)
- [ROS2 接口](#-ros2-接口)
- [项目结构](#-项目结构)
- [构建项目](#-构建项目)
- [故障排查](#-故障排查)
- [常见问题](#-常见问题)
- [贡献](#-贡献)
- [许可证](#-许可证)
- [联系我们](#-联系我们)

## ✨ 特性

- 🎯 **遥操臂控制** - 支持左右双臂的关节位置控制
- 🔌 **CAN 通信** - 基于 CAN 总线的实时通信
- 📊 **状态监控** - 实时获取关节位置和错误状态
- 🎮 **精确控制** - 支持关节位置精确控制
- 🔄 **实时反馈** - 获取关节状态和错误码
- 🛠️ **ROS2 支持** - 完整的 ROS2 节点和话题接口
- 📚 **易于配置** - 通过 YAML 配置文件进行参数设置

## 💻 系统要求

- **操作系统**: Ubuntu 20.04+ (推荐 Ubuntu 22.04)
- **架构**: x86_64 或 aarch64
- **ROS2**: ROS2 Humble 或 Foxy
- **编译器**: GCC 7.0+ 或 Clang 5.0+
- **CMake**: 3.15+
- **依赖**: 
  - pthread
  - CAN 设备驱动（SocketCAN）
  - ROS2 相关包（如果使用 ROS2 模式）

## 🚀 快速开始

### 1. 克隆仓库

```bash
git clone <repository-url>
cd linkerhand-tele-arm
```

### 2. 启动 CAN 设备

在开始使用之前，需要配置并启动 CAN 设备：

```bash
sudo ip link set can0 up type can bitrate 1000000
```

### 3. 配置参数

编辑配置文件 `src/linkerta/config/lta.yaml`：

```yaml
linkerta_node:
  ros__parameters:
    calibration: 0  # 0关闭标定 1开启标定
    negation: [-1, -1, -1, -1, -1, -1, -1, -1, 1, -1, 1, -1, -1, -1]  # 关节方向调整
```

### 4. 编译项目

```bash
./build.sh
```

### 5. 运行程序

```bash
./startup.sh
```

## 📦 安装

### 方法一：使用构建脚本（推荐）

```bash
./build.sh
```

### 方法二：手动构建

#### 非 ROS2 模式

```bash
mkdir build && cd build
cmake ../src/linkerta
make
```

#### ROS2 模式

```bash
colcon build --cmake-args -DUSE_ROS2=ON
```

## ⚙️ 配置

### 配置文件位置

配置文件位于：`src/linkerta/config/lta.yaml`

### 配置参数说明

| 参数 | 类型 | 说明 | 默认值 |
| :--- | :--- | :--- | :--- |
| `calibration` | int | 标定模式：0=关闭标定，1=开启标定 | 0 |
| `negation` | array | 关节方向调整数组，共14个数值<br/>1-7: 左臂关节，8-14: 右臂关节 | `[-1, ...]` |

### 遥操臂标定流程

1. 将每个遥操臂关节转动到刻度线位置
2. 将配置文件中的 `calibration` 参数改为 `1`
3. 重新编译项目：`./build.sh`
4. 运行程序：`./startup.sh` 完成标定
5. 将 `calibration` 参数改回 `0`
6. 重新编译项目：`./build.sh`

**注意**: 每次更改配置参数后都需要重新编译和重启程序！

## 💡 使用示例

### 基本使用（非 ROS2 模式）

```cpp
#include "LinkerArm.h"
#include <iostream>

int main() {
    // 创建遥操臂实例
    LinkerArm::LinkerArm arm("can0", 1000000);
    
    // 获取关节位置
    auto positions = arm.getJointPosition();
    for (size_t i = 0; i < positions.size(); ++i) {
        std::cout << "Joint " << i << ": " << positions[i] << std::endl;
    }
    
    // 获取关节错误状态
    auto error_codes = arm.getJointErrorCode();
    for (size_t i = 0; i < error_codes.size(); ++i) {
        std::cout << "Joint " << i << " error: " << error_codes[i] << std::endl;
    }
    
    return 0;
}
```

### ROS2 模式使用

启动 ROS2 节点：

```bash
ros2 launch linkerta run.launch.py
```

## 🔌 ROS2 接口

### 话题

#### 发布的话题

| 话题名称 | 类型 | 说明 |
| :--- | :--- | :--- |
| `/left_arm_joint_control` | `std_msgs/Float32MultiArray` | 左臂关节位置数据（7个关节） |
| `/right_arm_joint_control` | `std_msgs/Float32MultiArray` | 右臂关节位置数据（7个关节） |
| `/joint_error_code` | `std_msgs/BoolMultiArray` | 所有关节状态（1=故障，0=正常） |

### 查看话题数据

```bash
# 查看左臂关节位置
ros2 topic echo /left_arm_joint_control

# 查看右臂关节位置
ros2 topic echo /right_arm_joint_control

# 查看关节错误状态
ros2 topic echo /joint_error_code
```

### 节点信息

- **节点名称**: `linkerta_node`
- **命名空间**: `/`
- **自动重启**: 启用（延迟 3 秒）

## 📁 项目结构

```
linkerhand-tele-arm/
├── src/
│   └── linkerta/
│       ├── CMakeLists.txt       # CMake 构建配置
│       ├── package.xml           # ROS2 包配置
│       ├── config/
│       │   └── lta.yaml          # 配置文件
│       ├── launch/
│       │   └── run.launch.py     # ROS2 启动文件
│       ├── src/
│       │   ├── CanBus.h          # CAN 总线接口
│       │   ├── CanBus.cpp        # CAN 总线实现
│       │   ├── LinkerArm.h       # 遥操臂接口
│       │   ├── LinkerArm.cpp     # 遥操臂实现
│       │   ├── main.cpp          # 非 ROS2 主程序
│       │   └── main_ros2.cpp     # ROS2 主程序
│       └── test/                 # 测试目录
│           ├── CMakeLists.txt    # 测试构建配置
│           ├── test_canbus.cpp   # CanBus 单元测试
│           ├── test_linkerarm.cpp # LinkerArm 单元测试
│           └── mock_canbus.h     # Mock 对象定义
├── docs/
│   ├── FAQ.md                    # 常见问题
│   ├── TROUBLESHOOTING.md        # 故障排查
│   └── TESTING.md                # 测试文档
├── build.sh                      # 构建脚本
├── test.sh                       # 测试脚本
├── startup.sh                    # 启动脚本
├── .clang-format                 # 代码格式配置
├── .clang-tidy                   # 代码质量配置
├── .gitignore                    # Git 忽略规则
├── .gitattributes                # Git 属性配置
└── README.md                     # 本文档
```

## 🔨 构建项目

### 使用构建脚本

```bash
./build.sh
```

### 手动构建（非 ROS2）

```bash
mkdir build && cd build
cmake ../src/linkerta
make
```

### 手动构建（ROS2）

```bash
colcon build --cmake-args -DUSE_ROS2=ON
```

### 构建选项

- `USE_ROS2`: 启用 ROS2 支持（默认: OFF）
- `BUILD_TESTING`: 启用测试（默认: OFF）
- `CMAKE_BUILD_TYPE`: 构建类型（Release/Debug，默认: Release）

## 🧪 测试

项目包含完整的单元测试框架，使用 Google Test 进行测试。

### 运行测试

```bash
# 运行所有测试
./test.sh

# 生成代码覆盖率报告
./test.sh --coverage

# 显示详细输出
./test.sh --verbose
```

### 测试覆盖率

项目目标测试覆盖率 > 80%，核心功能覆盖率 > 90%。

查看详细测试文档：[测试文档](docs/TESTING.md)

## 🔧 故障排查

### CAN 设备问题

**问题**: 无法打开 CAN 设备

**解决方案**:
1. 检查 CAN 设备是否已启动：
   ```bash
   ip link show can0
   ```
2. 检查设备权限：
   ```bash
   sudo ip link set can0 up type can bitrate 1000000
   ```
3. 检查设备是否在线：
   ```bash
   ifconfig can0
   ```

### 编译问题

**问题**: CMake 找不到依赖

**解决方案**:
1. 确保已安装所有依赖：
   ```bash
   sudo apt-get update
   sudo apt-get install build-essential cmake libpthread-stubs0-dev
   ```
2. 对于 ROS2 模式，确保 ROS2 环境已正确设置：
   ```bash
   source /opt/ros/humble/setup.bash  # 或您的 ROS2 版本
   ```

### 运行时问题

**问题**: 程序无法连接到设备

**解决方案**:
1. 检查 CAN 设备状态
2. 检查配置文件路径是否正确
3. 查看程序日志输出

### ROS2 相关问题

**问题**: 无法找到 ROS2 节点

**解决方案**:
1. 确保已 source ROS2 环境：
   ```bash
   source install/setup.bash
   ```
2. 检查节点是否运行：
   ```bash
   ros2 node list
   ```

## ❓ 常见问题

### Q: 如何更改 CAN 设备名称？

A: 修改代码中的设备名称，或通过构造函数参数传入。默认使用 `can0`。

### Q: 标定后需要做什么？

A: 标定完成后，必须将 `calibration` 参数改回 `0` 并重新编译，否则每次启动都会重新标定。

### Q: 如何调整关节方向？

A: 修改配置文件中的 `negation` 数组，对应关节的值改为 `-1` 或 `1` 来反转方向。

### Q: 支持哪些 ROS2 版本？

A: 支持 ROS2 Foxy 和 Humble，其他版本可能需要调整。

### Q: 如何查看实时数据？

A: 使用 `ros2 topic echo` 命令查看相应话题，或使用 `ros2 topic hz` 查看发布频率。

## 🤝 贡献

我们欢迎社区贡献！请参考以下步骤：

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

## 📄 许可证

本项目采用 [Apache-2.0 许可证](LICENSE)。

Copyright (c) 2026 灵心巧手（北京）科技有限公司

## 📞 联系我们

- **官方网站**: [https://linkerbot.cn](https://linkerbot.cn)
- **关于我们**: [https://linkerbot.cn/aboutUs](https://linkerbot.cn/aboutUs)
- **GitHub**: [项目仓库地址]

---

**注意**: 使用前请确保设备已正确连接并配置好 CAN 接口。
