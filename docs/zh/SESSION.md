# Session 传输集成

[English](../SESSION.md)

`linkerbot::transport::Session` 负责接收帧路由和发送调度。解码器与快照组装器位于 `linkerta::core`。SocketCAN 工厂打开经典 CAN，本协议不接受 CAN FD 帧。

## 依赖与安装

CMake 先查找与 0.3.4 版本兼容的已安装 `linkerbot-transport` 包。找不到时，拉取提交 `0e190032d65ed0b315337b45b3779cf4f650ed68`。运行时回归检查 Session 异常发布与终止故障处理行为。transport 公共结构有新增字段，须使用匹配的头文件和库重新编译驱动及下游应用，不承诺旧 SDK 的二进制 ABI 兼容。本次依赖更新不计为新增实机验收。使用源码回退构建时，依赖库的测试、示例和基准程序均被关闭。

离线源码构建可使用已有源码目录，传入以下 CMake 选项：

```text
-DLINKERTA_ALLOW_DEPENDENCY_DOWNLOADS=OFF
-DFETCHCONTENT_SOURCE_DIR_LINKERBOT_TRANSPORT=/path/to/linkerbot-transport-cpp
-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/path/to/googletest
```

使用已安装依赖时，将 `CMAKE_PREFIX_PATH` 设为对应安装前缀。只有 `BUILD_TESTING=ON` 时才需要 GoogleTest。禁用下载后，缺少依赖会使配置失败。安装方法及导出的 `linkerta::core` 目标见 [INSTALL.md](INSTALL.md)。

## 构造、路由与所有权

`LinkerArm(interface_name, bitrate)` 在已经配置好的 Linux SocketCAN 接口上创建带工厂的 Session。bitrate 参数必须为正，但不会重新配置接口。工厂为 Session 设置 32 个待发送批次的容量。`"master_arm"` 构造函数探测名称以 `can` 或 `vcan` 开头的已有接口；没有接口或不止一个接口返回有效版本响应时，构造失败。发现过程不会配置接口，也不能仅凭这个响应进一步识别具体物理设备。

`LinkerArm(shared_ptr<Session>, SessionOptions)` 复用调用方传入的 Session，默认值如下：

- `own_session=false`：析构时不关闭共享 Session。
- `receive_capacity=64`：该遥操臂接收队列的帧容量。
- `command_timeout=250 ms`：构造时版本请求发送及归零命令的时间预算。
- `transmit_gap=5000 us`：每条命令发送前的等待时间，包括单帧批次。
- `endpoint` 为空：生成实例独有的 endpoint 名称。

注入不会改变 Session 的发送容量。传输库自身的通用默认值为 2000 个待发送批次、每个接收队列 512 帧；上面的遥操臂默认值会覆盖接收队列容量。正在执行的批次不占用待发送队列槽位。

构造时先注册 `0x64` 到 `0x68` 五条标准帧路由，再发送版本请求。注入构造和显式接口构造只等待请求发送完成，**不等待**版本响应，因此 `getVersion()` 最初可能为空。发现过程和显式恢复另有响应检查。同一 Session 上，两个遥操臂不能同时占用这些标准 ID 路由。扩展 ID 是不同的路由键，但遥操臂协议会拒绝扩展帧响应。

默认情况下，析构会关闭该遥操臂的接收队列、取消其 endpoint 并等待接收线程结束，Session 和其他路由保持打开。设置 `own_session=true` 后，遥操臂可以关闭整个 Session 并请求恢复。关闭会影响共享该 Session 的所有客户端，所以应指定唯一的生命周期所有者，并为独立客户端分配不同的 endpoint 名称。

`Session::create()` 拥有带工厂的连接；`Session::own()` 拥有传输对象，但没有恢复工厂。`share()`、`lease()` 和 `borrow()` 围绕已有传输设施复用 Session。即使外部总线仍然打开，关闭这个 Session 也会使所有客户端失效。

## 新读取、缓存读取与同步

`readJointState(timeout=250 ms)` 将新查询串行化，发送空载荷的 `0x04` 请求，并等待同一 generation 下本地序号更新的完整快照。它使用同一个截止时间，覆盖等待前一个遥操臂操作、发送调度和响应组装。当前公开 API 在这个调用中合并了请求与等待，没有单独的异步请求/等待接口。`getJointPosition()` 调用它并返回度数位置。

`latestSnapshot()` 检查 Session 状态和接收队列溢出后，加锁复制最后提交的快照。它不发查询，也不等待采样。首次完整响应之前以及失效之后，`valid` 都为 false。使用数值前应检查 `valid`、`fault_valid` 和 `isFresh(now, max_age)`。新读取失败后，这个缓存中仍可能保留较早的有效快照。

`getJointErrorCode()` 是旧的缓存便利接口，会丢弃快照有效性和每个关节的故障有效性；单个 false 位不能证明关节正常。需要区分故障数据缺失时，应使用完整快照。`getVersion()` 读取缓存版本，只把第一个字节显示为十进制数字。解码器保留全部版本字节，但该显示方式尚未经过固件规范确认。`printJointInfo()` 在状态锁外格式化本地快照副本，不保证其新鲜度。

Session 处理底层接收和调度，每个遥操臂再为自己的接收队列增加一个读取线程。请求锁将查询、归零和恢复串行化，互斥锁保护状态复制。销毁遥操臂前，应结束应用对它的调用。`TeleArmNode` 在 executor 之外执行 CAN 查询，并在退出时等待该 worker 结束。

## 截止时间、回执与接收时间

命令使用 FIFO 提交、显式 endpoint 和每条命令独立的取消令牌。等待 Session 回执时，失败、取消、过期或等待超时都可能抛出异常。等待超时本身不会取消发送；遥操臂会在发送路径失败或超过截止时间时，显式取消该命令的令牌，不取消其他设备的操作。取消无法撤回已经进入后端的写入，驱动调用也可能在截止时间之后才返回。

回执为 `dispatched` 表示后端接受了数据，即使最后一次写入在 Session 截止时间之后才结束，也可能得到这个状态。它不确认 CAN ACK 或设备执行。遥操臂会在发送完成后检查自身截止时间，拒绝已过期的新读取。不要自动重放标定或其他发送结果不确定的命令。

`RoutedMessage.received_at`、generation 和 sequence 都是主机元数据。接收线程使用 Session 的单调接收时间戳，拒绝不同 generation 的帧，并且只在查询待完成期间接受不晚于截止时间的响应。组装器还会拒绝早于查询起始边界的时间戳。该边界早于发送调度，不是固件周期标识。延迟响应限制和组装时差规则见 [PROTOCOL.md](PROTOCOL.md)。

## 溢出、关闭与恢复

接收队列满时拒绝新到帧并增加溢出计数，不替换旧帧。检测到溢出后，待组装数据和**已提交快照**都会失效，当前查询终止。关闭、传输故障或 generation 改变还会清除缓存状态及版本数据。这些连接变化需要重新验证身份；单纯溢出不要求下次查询前重新进行版本握手。

只有 `own_session=true` 时才能调用 `arm.recover(timeout=250 ms)`。它停止接收线程，取消遥操臂的 endpoint，调用 Session 恢复，注册新路由，重置快照状态并等待有效版本响应。旧查询和标定命令不会重放。超时预算覆盖请求锁等待和响应等待，但不是同步后端关闭/打开调用的硬时限。通过 `Session::own()` 构建的 Session 没有工厂，无法物理恢复。已经显式关闭的 Session 不能通过恢复重新打开。内部路由或发送线程故障属于终止故障，需关闭并重建 Session 及其遥操臂客户端，不能尝试恢复。

外部所有者恢复借用的 Session 后，应重建遥操臂。旧路由保持关闭，缓存数据不会带入新的 generation。本地 sequence 和 generation 标识主机状态变化，不能说明是哪台物理设备或哪个固件采样周期产生了该帧。
