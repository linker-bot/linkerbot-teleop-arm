# Linkerhand Tele Arm

通过 classic SocketCAN 读取双臂 14 个关节，提供 C++17 核心库、命令行工具和 ROS2 节点。
核心复用 linkerbot-transport-cpp Session；协议解码和四帧快照组装不依赖 ROS。
Linux 支持 SocketCAN 硬件；macOS 用于核心构建与测试。ROS2 CI 验证 Humble。

## 构建和安装

完整说明见 [安装与部署](docs/INSTALL.md)。从仓库根目录执行：

```sh
./build.sh core
./test.sh
cmake --install build-core --prefix "$PWD/install-core"
```

缺少 transport 依赖时，CMake 默认下载固定提交。离线环境先安装依赖，再传
`-DLINKERTA_ALLOW_DEPENDENCY_DOWNLOADS=OFF -DCMAKE_PREFIX_PATH=/path/to/dependencies`。
测试需要 GoogleTest。下游使用 `find_package(linkerta CONFIG REQUIRED)` 和 `linkerta::core`。

```cpp
#include <linkerta/LinkerArm.h>

LinkerArm::LinkerArm arm("can0", 1000000);
const auto snapshot = arm.readJointState();
// snapshot.positions_degrees 是完整快照，单位为度；超时会抛出异常。
```

头文件安装路径以实际导出接口为准；其他 API 与生命周期见 [Session 迁移契约](docs/SESSION.md)。
`latestSnapshot()` 只读缓存，调用方必须检查 `valid` 和 `isFresh()`；首次未就绪的零值不是测量。

## 配置 CAN 和命令行

CAN 接口应由管理员在进程外配置，命令示例见安装指南。驱动不会运行 sudo、配置 bitrate
或自动标定。CLI 的 bitrate 参数保留调用兼容，不会修改操作系统接口。

```sh
install-core/bin/linkerta --help
install-core/bin/linkerta --interface can0 --bitrate 1000000 --rate-hz 10 --timeout-ms 250 --count 20
# 旧形式继续支持：
install-core/bin/linkerta can0 1000000
```

CLI 输出完整新快照，单位为度。查询失败以非零状态退出；不会把缓存旧值冒充新读数。
请求频率是主机调度目标，不是设备吞吐量保证。

## ROS2

```sh
source /opt/ros/humble/setup.bash
./build.sh ros2
./startup.sh
```

配置源文件为 `src/linkerta/config/lta.yaml`，安装后位于包 share 目录。
也可直接 `ros2 run linkerta linkerta_node --ros-args -p can_interface:=can0`。
启动参数只读，修改后需要重启：

- `can_interface`：空字符串时通过 `device`（默认 `master_arm`）发现设备；非空时打开已配置接口。
- `poll_rate_hz`：默认 100，范围 1–200。
- `request_timeout_ms`、`max_snapshot_age_ms`：默认 250，范围 1–10000。
- `negation`：恰好 14 个方向值，每项只能为 -1 或 1。
- `publish_legacy_topics`：默认 true，可关闭旧话题。
- `calibration`：只允许 0，启动标定被禁止。
- `allow_calibration`：默认 false；显式启用后，`calibration_joint` 为 0–13 或 255（全部）。

发布接口：

- `/joint_states`：`sensor_msgs/msg/JointState`，14 个关节，弧度。
- `/left_arm_joint_control`、`/right_arm_joint_control`：兼容话题，各 7 个关节，度。
- `/joint_error_code`：兼容字符串，缺失的故障位标为 unknown。
- `/diagnostics`：`diagnostic_msgs/msg/DiagnosticArray`，报告超时、陈旧和关节故障。

每个完整新快照只发布一次，缺帧、超时或陈旧结果不发布位置。来自同一快照的消息使用同一时间戳。
时间戳为发布时 ROS 时间减去 steady clock 测得的快照年龄，是近似主机收帧时间，非设备采样时间。
模拟时钟暂停、倍率和跳变会影响映射；负值钳制为零。

`~/calibrate_once` 使用 `std_srvs/srv/Trigger`。只有显式允许时才排队一次命令，每个进程最多尝试一次，
即使发送失败也不自动重试。由于协议没有完成 ACK，响应 `success=false` 并在 message/诊断说明状态；
它不等于设备标定成功。重启不会自动发送标定。

## 协议、测试和限制

[协议与快照](docs/PROTOCOL.md) 记录 DLC、符号角度、故障未知值和版本字节格式；
[安全边界](docs/SAFETY.md)、[测试](docs/TESTING.md) 和 [故障排查](docs/TROUBLESHOOTING.md) 提供更多说明。
四帧无设备周期编号，主机一致快照不能证明同步采样；延迟旧响应也无法完全辨别。
尚未据此承诺实机最大频率、延迟或长期稳定性。模拟测试不能替代实际总线与设备验证。

## 许可证

`package.xml` 声明 Apache-2.0，但仓库当前没有顶层 LICENSE 正文，分发前需维护者补齐。
依赖 linkerbot-transport-cpp 使用 MIT；测试依赖 GoogleTest 使用 BSD-3-Clause。
分发依赖代码或二进制时应保留对应许可和声明。
