# C++ API 与线程契约

[文档索引](README.md) · [安装消费](INSTALL.md) · [Session](SESSION.md)

安装后使用 `#include <linkerta/LinkerArm.h>` 和 `#include <linkerta/ArmProtocol.h>`，链接 `linkerta::core`。核心 API 不依赖 ROS2；真实 SocketCAN I/O 仅在 Linux 可用。

## 请求与缓存是不同操作

- `readJointState(timeout)` 发起一次请求，等待四块完整的新快照；默认 250 ms。超时、发送失败、断线或需要身份重验时抛异常。等待请求槽、发送和接收共用该请求的 deadline。
- `latestSnapshot()` 复制缓存并检查 Session 状态和路由溢出，不发查询。必须检查 `valid` 与 `isFresh(now, max_age)`；超时后缓存可能仍是上一份有效快照，因此不能把缓存当作新测量。
- `getJointPosition()` 是兼容包装，会发起 `readJointState()`，返回度数。
- `getJointErrorCode()` 只复制缓存故障布尔值，不带有效性；新代码应同时读取快照的 `fault_valid`，避免把未知误认成正常。位置、故障值和有效性应从同一份 `ArmSnapshot` 读取，不要组合两次读取的结果。
- `getVersion()` 返回受锁保护的已接收版本字符串，尚未收到时为空；不是同步的版本查询。版本编码限制见协议指南。
- `lastError()` 返回当前错误文本，不是完整错误历史。

`ArmSnapshot` 包含 14 个 `positions_degrees`、`faults` 与 `fault_valid`、四块的 `received_at`、最新到达时间 `captured_at`、`sequence`、`generation`、`received_mask` 和 `valid`。时间使用 steady clock；`isFresh` 检查所有组成帧，包括最老的一帧。

下例会打开真实设备；先按安装指南配置接口：

```cpp
#include <linkerta/LinkerArm.h>
#include <chrono>

int main() {
    LinkerArm::LinkerArm arm("can0", 1000000);
    const auto fresh = arm.readJointState(std::chrono::milliseconds(250));
    const auto cached = arm.latestSnapshot();
    if (!cached.valid || !cached.isFresh(LinkerArm::Protocol::Clock::now(),
                                        std::chrono::milliseconds(250)))
        return 1;
    return fresh.positions_degrees.size() == 14 ? 0 : 1;
}
```

异常应由应用捕获并转为其错误策略；这个最小示例没有实现服务级重试、日志或进程监管。

## Session 注入与所有权

可传入 `std::shared_ptr<linkerbot::transport::Session>`，用于复用连接或注入 FakeTransport。`SessionOptions` 默认：

- `receive_capacity=64`，每个 arm 的有界接收队列；溢出使快照失效。
- `endpoint` 为空时生成独立名称，用于发送取消隔离。
- `command_timeout=250 ms`，`transmit_gap=5000 us`。
- `own_session=false`；析构移除自己的接收路由并取消自己的发送，不关闭外部共享 Session。

同一 Session 的标准 ID `0x64..0x68` 精确路由不能被第二个 arm 同时占用；不同 endpoint 名称不会解除路由冲突。自建接口/发现构造函数持有其 Session 的生命周期。

所有权不是自动恢复许可：只有显式拥有 Session 的 arm 可以调用 `recover(timeout)`。恢复还要求 Session 具备可重建连接的工厂；`Session::own()` 没有该工厂，即使 `own_session=true` 也不能物理恢复。显式关闭的 Session 不能用 recovery 重开。成功恢复后重建路由、清空旧状态并等待版本回复。外部拥有者恢复共享 Session 后，旧 arm 必须重建，不能继续发布旧 generation 的数据。

## 并发与退出

内部接收线程与快照锁保护状态复制；请求操作通过请求槽串行化。应用不应在仍有其他线程访问 arm 时销毁对象。析构会等待接收线程，ROS2 节点还会等待当前 worker 请求结束。

队列 deadline 和取消不能强制打断已进入的第三方驱动/系统调用；不要据此承诺硬实时退出时限。应用的关闭顺序应为：停止新任务、停止 executor/调用方、等待在途请求、销毁 arm，最后由所有者关闭共享 Session。

## 标定

`resetZero(joint)` 接受 0..13 或 255（全部）。它是有副作用的显式操作，不应放入通用重试循环。方法返回只说明发送路径完成，不能证明设备执行完成；协议没有已验证的完成 ACK。ROS2 的每进程一次保护仅存在于节点服务，直接调用 C++ API 的应用需自行限制调用次数。
