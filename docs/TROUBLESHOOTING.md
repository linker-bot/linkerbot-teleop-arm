# 故障排查

先保留完整错误消息、提交版本和启动参数，再按下列路径缩小问题。
不要关闭编译警告、跳过测试或重复标定来掩盖错误。

## 配置或构建失败

检查工具链和实际选择的构建模式：

```sh
cmake --version
./build.sh core
```

需要 CMake 3.18+、C++17 和线程支持。仅 ROS2 模式需要 ament、colcon 及 ROS 消息包；
先 `source /opt/ros/humble/setup.bash`，再 `./build.sh ros2`。
当前验证的是 Humble，不要混用其他 ROS 安装前缀。

找不到 `linkerbot-comm` 时，确认固定依赖能下载，或在离线环境安装依赖并正确设置
`CMAKE_PREFIX_PATH`。找不到 GoogleTest 时检查测试依赖，不要取消测试作为修复。
使用 `find_package` 的下游要指向安装前缀，而非源码或旧 build 目录。
安装和干净目录消费说明见 [INSTALL.md](INSTALL.md)。

编译器报错或警告应保留第一处完整诊断、编译器版本和构建命令。
项目已链接 Threads；不要先修改链接参数或降低警告等级。

## 进程立即退出或无法打开接口

CLI 参数错误会在硬件打开前拒绝；运行 `linkerta --help` 核对参数。
ROS2 参数同样先验证，尤其检查 `calibration` 必须为 0、方向数组长度和值、
频率和超时范围。空 `can_interface` 会走设备发现；多个候选设备时应显式选择接口。

只读检查系统接口：

```sh
ip -details link show can0
ip -s link show can0
```

接口不存在、未启用或 bitrate 不匹配时，由管理员按设备配置在停机窗口处理，
见安装指南。业务节点不负责提权或配置接口。SocketCAN 不是 `/dev/tty*` 串口；
不要把 `dialout` 组、chmod 或长期 sudo 运行当作通用修复。
若存在权限错误，应保留 errno 并核实容器网络命名空间、设备可见性和系统安全策略。

## 节点存在但位置话题无数据

```sh
ros2 node list
ros2 node info /linkerta_node
ros2 topic echo /diagnostics
ros2 topic echo /joint_states
```

先确认发现的是预期节点和接口。`/joint_states` 是弧度；旧控制话题是度，且可能被
`publish_legacy_topics=false` 关闭。收到不完整、超时或陈旧响应时不发布位置是预期行为。
不能用旧缓存持续发布来“恢复频率”。

检查总线计数、电源、布线、bitrate 和终端条件；必要时使用已安装的 can-utils
执行 `candump can0` 只读抓帧。保存原始帧并对照 [PROTOCOL.md](PROTOCOL.md) 的 ID、
DLC 与标志位规则。六字节旧格式缺少故障位时应显示 unknown。
四帧没有已确认的设备周期号，不能从主机到达顺序断言同步采样。

## 配置变更没有生效

`ros2 launch` 通常读取安装 share 目录。用 `ros2 pkg prefix linkerta` 核对选中的包，
确认没有 source 旧工作区。修改源码 YAML 后重新安装配置，或用
`--ros-args --params-file /absolute/path/lta.yaml` 指定文件。
这些参数只在启动时读取；运行时设参被拒绝属于接口契约，不需要修改源码解除限制。

## 标定返回 false、结果不确定

先读服务 response.message 和 `/diagnostics`。默认禁用、已消耗一次机会、排队待发送和
发送结果不确定的含义不同。由于当前未实现或验证设备完成 ACK，即使命令已发出也不返回确认成功。
不要将 `calibration` 改成 1，不要用自动重启或重复服务调用当作恢复措施。
保持设备安全状态，由操作者按设备规程确认是否已执行；详见 [SAFETY.md](SAFETY.md)。

## 超时、溢出或资源增长

先记录当前请求频率、超时、CPU/内存负载和 CAN 错误计数。可通过启动参数降低请求频率，
而非修改代码 sleep 或提权改变线程优先级。增大超时前应查清缺帧和总线错误原因。

记录进程 RSS、线程和 fd 数的时间趋势，区分启动预热与持续增长。短期回归见
[TESTING.md](TESTING.md)，手动长期采样见 [SOAK.md](SOAK.md)。模拟工具见
[BENCHMARK.md](BENCHMARK.md)，其结果不能证明实机稳定性。

## 提交问题时附带什么？

提供提交 SHA、OS/内核、编译器、ROS_DISTRO、构建和运行命令、去除敏感信息的参数、
完整错误日志、适配器和固件标识、只读接口状态，以及最小可复现步骤。
若附基准 CSV，请同时附 metadata，并保留 synthetic 标志和失败样本。
不要只附平均延迟、截图或“重启后好了”的结论。
