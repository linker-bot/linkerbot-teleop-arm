# 常见问题解答 (FAQ)

本文档回答 Linkerhand-Tele-Arm 的常见问题。

## 目录

- [安装相关问题](#安装相关问题)
- [使用相关问题](#使用相关问题)
- [配置相关问题](#配置相关问题)
- [ROS2 相关问题](#ros2-相关问题)
- [通信问题](#通信问题)
- [其他问题](#其他问题)

---

## 安装相关问题

### Q1: 如何安装项目？

**A**: 有两种安装方式：

**方法一：使用构建脚本（推荐）**
```bash
./build.sh
```

**方法二：手动构建（非 ROS2）**
```bash
mkdir build && cd build
cmake ../src/linkerta
make
```

**方法三：手动构建（ROS2）**
```bash
colcon build --cmake-args -DUSE_ROS2=ON
```

详细说明请参考 [README.md](../README.md#安装)。

---

### Q2: 编译时找不到依赖怎么办？

**A**: 检查以下几点：

1. **确认已安装基础依赖**:
   ```bash
   sudo apt-get update
   sudo apt-get install build-essential cmake libpthread-stubs0-dev
   ```

2. **对于 ROS2 模式，确认 ROS2 环境**:
   ```bash
   source /opt/ros/humble/setup.bash  # 或您的 ROS2 版本
   ```

3. **检查 CMake 版本**:
   ```bash
   cmake --version  # 需要 3.15+
   ```

---

### Q3: 如何选择构建模式？

**A**: 项目支持两种构建模式：

- **非 ROS2 模式**（默认）: 直接使用 CMake 构建，生成独立可执行文件
- **ROS2 模式**: 使用 colcon 构建，生成 ROS2 节点

使用 ROS2 模式时，需要设置 `USE_ROS2=ON` 选项。

---

## 使用相关问题

### Q4: 如何启动 CAN 设备？

**A**: 使用以下命令启动 CAN 设备：

```bash
sudo ip link set can0 up type can bitrate 1000000
```

检查设备状态：
```bash
ip link show can0
ifconfig can0
```

---

### Q5: 如何配置参数？

**A**: 编辑配置文件 `src/linkerta/config/lta.yaml`：

```yaml
linkerta_node:
  ros__parameters:
    calibration: 0  # 0关闭标定 1开启标定
    negation: [-1, -1, -1, -1, -1, -1, -1, -1, 1, -1, 1, -1, -1, -1]
```

**注意**: 每次更改配置后都需要重新编译和重启程序！

---

### Q6: 如何查看实时数据？

**A**: 使用 ROS2 命令查看话题数据：

```bash
# 查看左臂关节位置
ros2 topic echo /left_arm_joint_control

# 查看右臂关节位置
ros2 topic echo /right_arm_joint_control

# 查看关节错误状态
ros2 topic echo /joint_error_code

# 查看话题发布频率
ros2 topic hz /left_arm_joint_control
```

---

## 配置相关问题

### Q7: 如何更改 CAN 设备名称？

**A**: 修改代码中的设备名称，或通过构造函数参数传入。默认使用 `can0`。

在非 ROS2 模式下，可以在创建 `LinkerArm` 实例时指定：
```cpp
LinkerArm::LinkerArm arm("can1", 1000000);  // 使用 can1
```

---

### Q8: 标定后需要做什么？

**A**: 标定完成后，必须执行以下步骤：

1. 将配置文件中的 `calibration` 参数改回 `0`
2. 重新编译项目：`./build.sh`
3. 重启程序

否则每次启动都会重新标定，可能导致问题。

---

### Q9: 如何调整关节方向？

**A**: 修改配置文件中的 `negation` 数组：

- 数组共 14 个元素
- 1-7: 左臂关节（7个）
- 8-14: 右臂关节（7个）
- 值 `-1` 或 `1` 用于反转关节方向

示例：
```yaml
negation: [-1, -1, -1, -1, -1, -1, -1,  # 左臂
           -1, -1, -1, -1, -1, -1, -1]   # 右臂
```

---

## ROS2 相关问题

### Q10: 支持哪些 ROS2 版本？

**A**: 项目支持以下 ROS2 版本：

- ROS2 Foxy
- ROS2 Humble

其他版本可能需要调整代码。

---

### Q11: 如何启动 ROS2 节点？

**A**: 使用启动脚本或 launch 文件：

```bash
# 方法一：使用启动脚本
./startup.sh

# 方法二：使用 launch 文件
ros2 launch linkerta run.launch.py
```

确保已 source ROS2 环境：
```bash
source /opt/ros/humble/setup.bash
source install/setup.bash
```

---

### Q12: 无法找到 ROS2 节点怎么办？

**A**: 检查以下几点：

1. **确认已 source 环境**:
   ```bash
   source install/setup.bash
   ```

2. **检查节点是否运行**:
   ```bash
   ros2 node list
   ```

3. **检查话题是否存在**:
   ```bash
   ros2 topic list
   ```

4. **查看节点信息**:
   ```bash
   ros2 node info /linkerta_node
   ```

---

## 通信问题

### Q13: 无法连接到 CAN 设备？

**A**: 按以下步骤排查：

1. **检查设备是否存在**:
   ```bash
   ip link show can0
   ```

2. **检查设备状态**:
   ```bash
   ifconfig can0
   ```

3. **检查权限**:
   ```bash
   sudo ip link set can0 up type can bitrate 1000000
   ```

4. **检查设备是否在线**:
   ```bash
   ip -s link show can0
   ```

---

### Q14: 通信延迟或数据丢失？

**A**: 可能的原因和解决方案：

1. **CAN 总线负载过高**: 检查是否有其他设备占用总线
2. **波特率不匹配**: 确认设备波特率设置为 1000000
3. **硬件问题**: 检查 CAN 线缆和连接
4. **系统负载**: 检查系统 CPU 和内存使用情况

---

## 其他问题

### Q15: 如何获取版本信息？

**A**: 在代码中调用：
```cpp
std::string version = arm.getVersion();
std::cout << "Version: " << version << std::endl;
```

---

### Q16: 如何重置关节零点？

**A**: 使用 `resetZero()` 方法：

```cpp
arm.resetZero();  // 重置所有关节
arm.resetZero(1); // 重置指定关节（关节编号从 0 开始）
```

---

### Q17: 如何获取关节错误状态？

**A**: 使用 `getJointErrorCode()` 方法：

```cpp
auto error_codes = arm.getJointErrorCode();
for (size_t i = 0; i < error_codes.size(); ++i) {
    if (error_codes[i]) {
        std::cout << "Joint " << i << " has error!" << std::endl;
    }
}
```

在 ROS2 模式下，也可以通过话题 `/joint_error_code` 获取。

---

### Q18: 项目支持哪些架构？

**A**: 项目支持以下架构：

- x86_64 (64位 Intel/AMD)
- aarch64 (64位 ARM)

CMake 会自动检测系统架构并选择相应的配置。

---

## 获取帮助

如果本文档无法解决您的问题，请：

1. 查看 [故障排查指南](TROUBLESHOOTING.md)
2. 查看 [README.md](../README.md)
3. 提交 GitHub Issue
4. 联系技术支持：[https://linkerbot.cn/aboutUs](https://linkerbot.cn/aboutUs)

---

**最后更新**: 2026
