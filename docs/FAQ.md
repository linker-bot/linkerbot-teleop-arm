# 常见问题

## 构建需要什么？

核心要求 C++17、CMake 3.18+ 和 Threads。硬件后端使用 Linux SocketCAN；
macOS 可构建核心、CLI 帮助和模拟测试。ROS2 当前 CI 验证 Humble，不能据此保证其他发行版。

```sh
./build.sh core
./test.sh
```

ROS2 构建前先加载环境，再执行 `./build.sh ros2`。完整安装、离线依赖和下游
`find_package(linkerta CONFIG REQUIRED)` 用法见 [INSTALL.md](INSTALL.md)。

## 缺少 transport 或 GoogleTest 怎么办？

默认构建会下载固定提交的 transport；GoogleTest 仅测试构建需要。
离线环境预安装依赖，将其前缀加入 `CMAKE_PREFIX_PATH`，并设置
`LINKERTA_ALLOW_DEPENDENCY_DOWNLOADS=OFF`。不要靠跳过失败测试确认构建正常。

## 怎样选择 CAN 接口和频率？

CLI 无需改代码：

```sh
linkerta --help
linkerta --interface can0 --bitrate 1000000 --rate-hz 10 --timeout-ms 250 --count 20
```

旧形式 `linkerta can0 1000000` 继续兼容。bitrate 参数不会配置系统接口。
管理员应在进程外按设备要求配置 CAN，具体命令见安装指南。SocketCAN 网络接口
不等同于串口设备；加入 `dialout` 组不是通用解决方法。

ROS2 使用 `can_interface`、`poll_rate_hz`、`request_timeout_ms` 和
`max_snapshot_age_ms` 等启动参数。空接口通过 `device` 发现设备，显式接口更便于部署。
请求频率是主机调度目标，不是设备保证吞吐量。

## 修改 YAML 必须重新编译吗？

纯参数修改不需要重新编译 C++。但 launch 读取的是安装目录里的配置，而不是自动读取
源码目录。修改源码 YAML 后应重新安装配置，或显式给节点传入所需参数文件。
启动参数只读，修改后重启节点，不支持运行时热更。

```sh
ros2 run linkerta linkerta_node --ros-args --params-file /absolute/path/lta.yaml
```

`negation` 必须恰好 14 项，每项只能是 -1 或 1。其他合法范围见
[README](../README.md#ros2)。

## 哪个话题用弧度？没有位置消息是否故障？

`/joint_states` 是 14 个关节的标准弧度输出；兼容的左右控制话题仍是度。
`publish_legacy_topics=false` 会关闭旧话题。CLI 和核心快照也使用度。

```sh
ros2 topic echo /joint_states
ros2 topic echo /diagnostics
```

首次未取得四个响应块、超时、缺帧或结果陈旧时，节点停止发布位置，避免重复旧数据。
先查看诊断和总线状态，不要把未发布当作关节零位。故障位缺失表示 unknown，不表示正常。

## 怎样标定？为什么服务返回 success=false？

启动参数 `calibration` 只允许 0，启动或重启都不会自动标定。
只有显式设置 `allow_calibration=true` 后，才允许调用 `~/calibrate_once`；
`calibration_joint` 指定 0–13 或 255（全部）。每个进程最多尝试一次，失败也不自动重试。

协议没有已确认的设备完成 ACK，所以 Trigger 返回 `success=false`，并通过 message 和
诊断说明排队或发送状态。它不自动意味着命令没发出，也不能声称设备已经完成。
不要因为 false 就循环调用、重启重试；应按设备规程确认实际状态。见
[SAFETY.md](SAFETY.md) 和 [PROTOCOL.md](PROTOCOL.md)。

## 核心 API 应如何读取状态？

`readJointState(timeout)` 请求并等待完整新快照，失败抛出异常。
`latestSnapshot()` 只返回缓存，使用前检查 `valid` 和 `isFresh()`。
同一份快照内读取位置、故障值及 `fault_valid`，避免把未知故障误当正常。
`getVersion()` 是当前实现的版本文本，不应猜测未确认的固件版本编码。

## 模拟基准能证明实机性能吗？

不能。FakeTransport 和 vcan 覆盖软件路径，不代表真实 ACK、物理 bus-off 或设备采样同步。
[基准工具](BENCHMARK.md) 标记 synthetic 数据；[长期验证](SOAK.md) 仍需真实台架记录。
更多诊断步骤见 [TROUBLESHOOTING.md](TROUBLESHOOTING.md)。
