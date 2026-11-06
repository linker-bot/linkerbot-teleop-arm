[English](../INSTALL.md) | [简体中文](INSTALL.md)

# 构建、安装和部署

核心库要求 C++17、CMake 3.18+、Threads 和 linkerbot-transport 0.3.4。
默认缺失依赖会从固定提交下载；离线环境应预安装 linkerbot-transport 并设置
`CMAKE_PREFIX_PATH`、`LINKERTA_ALLOW_DEPENDENCY_DOWNLOADS=OFF`。GoogleTest 只在测试构建需要。

```sh
cmake -S src/linkerta -B build-core -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DCMAKE_INSTALL_PREFIX="$PWD/install-core"
cmake --build build-core --parallel
ctest --test-dir build-core --output-on-failure --no-tests=error
cmake --install build-core
```

下游项目使用已安装的依赖及本库：

```cmake
find_package(linkerta CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE linkerta::core)
```

ROS2 已在 Humble 验证。先 source `/opt/ros/humble/setup.bash`，再执行
`./build.sh ros2`；`./build.sh core` 不要求 ROS2 或 colcon。
`./startup.sh` 加载工作区环境并用 `exec` 启动 `ros2 launch`。脚本不拦截 launch 的退出码；节点信号、退出和默认 respawn 由 launch 管理，不保证节点退出码直接成为脚本退出码。
脚本不配置网卡、不修改权限、不自动标定。

## CAN 显式部署

以下是管理员在停机窗口执行的示例；接口名、bitrate 必须匹配实际设备。
不要在驱动进程中运行这些命令：

```sh
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 1000000
sudo ip link set can0 up
ip -details link show can0
```

普通业务进程仅打开已配置的 classic SocketCAN 接口。Linux 提供硬件后端；
macOS 可构建核心库、协议测试及 CLI 参数帮助，但不提供 SocketCAN 硬件。

```sh
linkerta --help
linkerta --interface can0 --bitrate 1000000 --rate-hz 10 --timeout-ms 250 --count 20
```

旧调用 `linkerta can0 1000000` 保留兼容。bitrate 参数不会配置操作系统接口。
位置输出以度为单位，必须取得完整新快照；失败通过非零退出码报告。

## 数据单位与许可证

标准 ROS `/joint_states` 使用弧度；旧左右控制话题继续使用度，并可禁用。
核心库位置使用度。协议长度、未知故障位和版本编码限制见 [PROTOCOL.md](PROTOCOL.md)。
标定当前未实现或验证设备完成 ACK，不自动重放，不能把后端接受发送等同于设备完成。

项目的 [Apache-2.0 许可证](../../LICENSE)和[第三方声明](../../THIRD_PARTY_NOTICES.md)会随安装保存在 `<prefix>/share/licenses/linkerta`。分发二进制时请保留这个目录。transport 的 MIT 和 GoogleTest 的 BSD-3-Clause 原文位于 [LICENSES](../../LICENSES)；GoogleTest 只用于测试，不链接进驱动。
