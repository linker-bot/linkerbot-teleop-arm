# 故障排查指南

本文档提供 Linkerhand-Tele-Arm 常见问题的排查和解决方案。

## 目录

- [编译问题](#编译问题)
- [运行时问题](#运行时问题)
- [通信问题](#通信问题)
- [ROS2 问题](#ros2-问题)
- [配置问题](#配置问题)
- [性能问题](#性能问题)
- [获取帮助](#获取帮助)

---

## 编译问题

### 问题：找不到 CMake 或版本过低

**错误信息**:
```
CMake 3.15 or higher is required
```

**解决方案**:
1. 更新 CMake：
   ```bash
   sudo apt-get update
   sudo apt-get install cmake
   ```

2. 或从源码安装最新版本：
   ```bash
   wget https://cmake.org/files/v3.27/cmake-3.27.0.tar.gz
   tar -xzf cmake-3.27.0.tar.gz
   cd cmake-3.27.0
   ./bootstrap && make && sudo make install
   ```

---

### 问题：找不到 pthread 库

**错误信息**:
```
undefined reference to `pthread_*`
```

**解决方案**:
1. 安装 pthread 开发库：
   ```bash
   sudo apt-get install libpthread-stubs0-dev
   ```

2. 确认 CMakeLists.txt 中已链接 Threads：
   ```cmake
   find_package(Threads REQUIRED)
   target_link_libraries(linkerta PRIVATE Threads::Threads)
   ```

---

### 问题：ROS2 模式下找不到 ament_cmake

**错误信息**:
```
Could not find ament_cmake
```

**解决方案**:
1. 确认已安装 ROS2：
   ```bash
   sudo apt-get install ros-humble-ament-cmake
   ```

2. 确认已 source ROS2 环境：
   ```bash
   source /opt/ros/humble/setup.bash
   ```

3. 使用 colcon 构建：
   ```bash
   colcon build --cmake-args -DUSE_ROS2=ON
   ```

---

### 问题：编译警告过多

**解决方案**:
1. 检查代码是否符合规范：
   ```bash
   clang-format -i src/**/*.{cpp,h}
   clang-tidy src/**/*.{cpp,h}
   ```

2. 调整 CMakeLists.txt 中的警告级别（如果需要）

---

## 运行时问题

### 问题：无法打开 CAN 设备

**错误信息**:
```
Cannot open CAN device: can0
```

**可能原因**:
1. CAN 设备未启动
2. 权限不足
3. 设备不存在

**解决方案**:
1. **启动 CAN 设备**:
   ```bash
   sudo ip link set can0 up type can bitrate 1000000
   ```

2. **检查设备状态**:
   ```bash
   ip link show can0
   ifconfig can0
   ```

3. **检查设备是否存在**:
   ```bash
   ip link show
   ```

4. **检查权限**:
   ```bash
   # 可能需要将用户添加到 dialout 组
   sudo usermod -a -G dialout $USER
   # 重新登录后生效
   ```

---

### 问题：程序启动后立即退出

**可能原因**:
1. CAN 设备未正确初始化
2. 配置文件路径错误
3. 参数配置错误

**解决方案**:
1. **检查 CAN 设备**:
   ```bash
   ip link show can0
   ```

2. **检查配置文件**:
   ```bash
   ls -la src/linkerta/config/lta.yaml
   cat src/linkerta/config/lta.yaml
   ```

3. **查看程序日志**:
   - 检查终端输出
   - 检查 ROS2 日志（如果使用 ROS2 模式）

---

### 问题：无法读取关节数据

**可能原因**:
1. CAN 通信失败
2. 设备未响应
3. 数据解析错误

**解决方案**:
1. **检查 CAN 通信**:
   ```bash
   # 使用 can-utils 工具测试
   sudo apt-get install can-utils
   candump can0
   ```

2. **检查设备连接**:
   - 确认 CAN 线缆连接正确
   - 检查设备电源

3. **检查波特率**:
   ```bash
   # 确认波特率匹配
   ip link show can0
   ```

---

## 通信问题

### 问题：CAN 总线错误

**错误信息**:
```
CAN bus error: ...
```

**可能原因**:
1. 总线负载过高
2. 硬件故障
3. 波特率不匹配

**解决方案**:
1. **检查总线负载**:
   ```bash
   ip -s link show can0
   # 查看错误统计
   ```

2. **检查硬件**:
   - 检查 CAN 线缆
   - 检查终端电阻（120Ω）
   - 检查设备连接

3. **调整波特率**:
   ```bash
   sudo ip link set can0 down
   sudo ip link set can0 up type can bitrate 1000000
   ```

---

### 问题：数据延迟或丢失

**可能原因**:
1. 系统负载过高
2. CAN 总线负载过高
3. 线程优先级问题

**解决方案**:
1. **检查系统负载**:
   ```bash
   top
   htop
   ```

2. **检查 CAN 总线统计**:
   ```bash
   ip -s link show can0
   ```

3. **优化程序**:
   - 减少不必要的日志输出
   - 调整线程优先级（需要 root 权限）

---

## ROS2 问题

### 问题：找不到 ROS2 节点

**错误信息**:
```
Package 'linkerta' not found
```

**解决方案**:
1. **确认已构建 ROS2 包**:
   ```bash
   colcon build --cmake-args -DUSE_ROS2=ON
   ```

2. **source 环境**:
   ```bash
   source install/setup.bash
   ```

3. **检查包是否存在**:
   ```bash
   ros2 pkg list | grep linkerta
   ```

---

### 问题：话题无数据

**可能原因**:
1. 节点未运行
2. 话题名称错误
3. 节点崩溃

**解决方案**:
1. **检查节点状态**:
   ```bash
   ros2 node list
   ros2 node info /linkerta_node
   ```

2. **检查话题**:
   ```bash
   ros2 topic list
   ros2 topic echo /left_arm_joint_control
   ```

3. **检查节点日志**:
   ```bash
   ros2 run linkerta linkerta_node
   ```

---

### 问题：Launch 文件无法启动

**错误信息**:
```
[ERROR] [launch]: process has died
```

**解决方案**:
1. **检查配置文件路径**:
   ```bash
   ros2 pkg prefix linkerta
   # 确认配置文件路径正确
   ```

2. **手动运行节点**:
   ```bash
   ros2 run linkerta linkerta_node
   # 查看具体错误信息
   ```

3. **检查参数**:
   ```bash
   # 验证配置文件格式
   cat src/linkerta/config/lta.yaml
   ```

---

## 配置问题

### 问题：配置更改不生效

**可能原因**:
1. 未重新编译
2. 配置文件路径错误
3. 缓存问题

**解决方案**:
1. **重新编译**:
   ```bash
   ./build.sh
   # 或
   colcon build --cmake-args -DUSE_ROS2=ON
   ```

2. **检查配置文件路径**:
   - 非 ROS2 模式：代码中硬编码路径
   - ROS2 模式：`src/linkerta/config/lta.yaml`

3. **清理构建缓存**:
   ```bash
   rm -rf build install log
   ./build.sh
   ```

---

### 问题：标定失败

**可能原因**:
1. 关节未到达刻度线
2. 标定参数错误
3. 通信问题

**解决方案**:
1. **确认关节位置**:
   - 确保所有关节都到达刻度线位置

2. **检查配置**:
   ```yaml
   calibration: 1  # 必须为 1
   ```

3. **重新标定**:
   - 重启程序
   - 等待标定完成
   - 将 `calibration` 改回 `0` 并重新编译

---

## 性能问题

### 问题：CPU 使用率过高

**可能原因**:
1. 循环频率过高
2. 日志输出过多
3. 线程阻塞

**解决方案**:
1. **调整循环频率**:
   - 检查代码中的 sleep 时间
   - 优化数据读取频率

2. **减少日志输出**:
   - 关闭调试日志
   - 使用条件编译

3. **优化线程**:
   - 检查线程同步
   - 避免死锁

---

### 问题：内存泄漏

**解决方案**:
1. **使用内存检测工具**:
   ```bash
   valgrind --leak-check=full ./linkerta
   ```

2. **检查代码**:
   - 确保所有 new 都有对应的 delete
   - 使用智能指针

---

## 获取帮助

如果以上方案无法解决您的问题，请：

1. **查看日志**:
   - 程序输出
   - ROS2 日志
   - 系统日志

2. **收集信息**:
   - 错误信息
   - 系统信息（`uname -a`）
   - ROS2 版本（`ros2 --version`）
   - 配置文件内容

3. **寻求帮助**:
   - 查看 [FAQ.md](FAQ.md)
   - 查看 [README.md](../README.md)
   - 提交 GitHub Issue
   - 联系技术支持：[https://linkerbot.cn/aboutUs](https://linkerbot.cn/aboutUs)

---

**最后更新**: 2026
