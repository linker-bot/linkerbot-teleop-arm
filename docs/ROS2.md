# ROS2 接口、参数与时间

[文档索引](README.md) · [部署](INSTALL.md) · [故障排查](TROUBLESHOOTING.md)

当前 CI 验证 ROS2 Humble。其他发行版没有同等验证承诺。节点名默认为 `linkerta_node`，通过单线程 executor 处理 timer、参数和服务；独立 worker 执行可能阻塞的 CAN 请求。

## 启动与配置

从仓库根目录构建、安装后：

```sh
source /opt/ros/humble/setup.bash
./build.sh ros2
./startup.sh
```

launch 加载安装目录的 `share/linkerta/config/lta.yaml`，默认允许进程退出后 3 秒重启。直接运行可以选择外部配置而无需重新编译：

```sh
source install/setup.bash
ros2 run linkerta linkerta_node --ros-args --params-file /absolute/path/lta.yaml
```

修改源码配置文件不会修改已经安装的副本；更新安装副本或重新安装，再重启节点。参数在打开硬件前校验，运行中为只读：

- `device="master_arm"`：`can_interface` 为空时进行设备发现；候选不唯一会拒绝。
- `can_interface=""`：非空时使用指定的已配置接口；不负责配置 bitrate。
- `poll_rate_hz=100.0`：有限值，1..200，调度上限而非设备吞吐保证。
- `request_timeout_ms=250`、`max_snapshot_age_ms=250`：各 1..10000 ms。
- `negation=[-1,-1,-1,-1,-1,-1,-1,-1,1,-1,1,-1,-1,-1]`：14 项，每项 -1 或 1；应用于 ROS2 输出。
- `publish_legacy_topics=true`：同时控制左右旧话题和旧故障字符串。
- `calibration=0`：只允许 0，禁止启动时标定。
- `allow_calibration=false`：显式允许服务请求，不会自行发标定。
- `calibration_joint=255`：0..13 或 255，供一次性服务使用。

## 话题与单位

- `/joint_states`，`sensor_msgs/msg/JointState`：14 关节，位置为弧度。
- `/left_arm_joint_control`、`/right_arm_joint_control`，同类型：各 7 关节，位置仍为度。名称中的 control 是旧兼容命名，这些发布是读取到的状态，不是设备控制命令。
- `/joint_error_code`，`std_msgs/msg/String`：旧兼容故障文本，未知位输出 `unknown`。
- `/diagnostics`，`diagnostic_msgs/msg/DiagnosticArray`：完整/陈旧/超时、故障有效性、快照序号/generation 及标定发送状态。

左侧名称为 `joint61..joint67`，右侧为 `joint51..joint57`。不发布推导速度或力矩。话题为源码中的绝对名称；多节点部署须显式 remap，namespace 本身不会修改绝对话题名。

只有新的完整且新鲜快照才发布位置，一份结果不会反复发布；缺帧、超时或陈旧结果只影响诊断。来自同一快照的三个位置消息用相同 stamp。stamp 以发布时 ROS now 减去 steady clock 测得的最新组成帧年龄，近似映射主机收帧时间，**不是设备采样时间**。模拟时钟暂停、变速或跳变会影响映射；负值钳零。诊断消息用其发布时刻的 ROS now。

## 一次性标定服务

`~/calibrate_once` 的类型是 `std_srvs/srv/Trigger`。默认禁用；经人工确认后，在启动参数中设置 `allow_calibration=true`，再显式调用：

```sh
ros2 service call /linkerta_node/calibrate_once std_srvs/srv/Trigger '{}'
```

一次被接受的请求就消耗本进程的机会，发送失败也不重试。`success=false` 在这里包括“已排队但无法确认设备执行”的情况，必须读取 `message` 和诊断；当前未实现或验证设备完成 ACK，不能把它当作普通成功/失败事务。节点重启会重置进程内次数限制，但不会自动标定；外部调用方不得因响应为 false 自动重试。
