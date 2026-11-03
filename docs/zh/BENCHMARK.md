# 测量查询延迟

[English](../BENCHMARK.md)

构建 `linkerta_benchmark` 可记录查询结果和延迟。它使用现有轮询实现，不配置 CAN、不标定关节，也不恢复传输。模拟模式通过 `FakeTransport` 运行 `LinkerArm` 和 Session，可用于检查工具本身，不能据此制定硬件延迟目标。

## 构建并检查模拟输出

```sh
cmake -S src/linkerta -B build-benchmark -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON -DLINKERTA_BUILD_BENCHMARKS=ON
cmake --build build-benchmark --parallel 2
ctest --test-dir build-benchmark --output-on-failure --no-tests=error
./build-benchmark/linkerta_benchmark --simulate --count 100 --rate-hz 20 \
  --timeout-ms 250 --output synthetic.csv
```

每次运行都应使用新的输出路径；已有 CSV 或元数据文件会被拒绝。模拟模式下，`--drop-every 10` 会每十次省略一个完整响应。每次省略都应产生失败记录，而不是重复上一份快照。

失败请求会被记录，运行继续，不进行重试。全部请求成功时退出码为 `0`，有请求失败时为 `1`，参数/I/O/初始化错误为 `2`，收到 SIGINT/SIGTERM 后为 `128 + signal`。信号使程序在下一个请求前停止；正在执行的请求可能先完成或等到截止时间。

## 在硬件上运行

按 `INSTALL.md` 中的部署步骤配置 CAN 接口。硬件运行必须提供接口名称、设备标识和固件标签：

```sh
./build-benchmark/linkerta_benchmark --interface can0 \
  --device-label arm-A-adapter-A --firmware recorded-firmware-version \
  --count 10000 --rate-hz 20 --timeout-ms 250 --output hardware-arm-A.csv
```

`--simulate` 与 `--interface` 互斥。硬件路径使用已经配置好的接口，核心构造函数接收兼容旧接口的预期 bitrate `1000000`，但不会应用这个值。结果旁应记录实际的外部 bitrate 配置、适配器、总线负载、主机、CPU 调频策略和接线。

两条路径都保留驱动每次发送前 5000 微秒的等待时间。请求频率是上限，慢请求之后不会突发补发。

## 输出格式

CSV 为每次尝试的请求记录一行，包含主机单调时钟下的累计时间、请求耗时（微秒）、成功状态、快照 sequence/generation、最老数据块年龄、帧间时差（微秒）、路由 pending/high-water/overflow、Session pending/high-water/overflow，以及带引号的错误文本。失败行的快照序号和年龄字段为空，但保留当前 Session generation。计数器是累计值的快照，不是每次请求的增量。

成功表示驱动按主机侧组帧规则得到了完整快照，各帧仍可能来自不同的硬件采样周期。

`OUTPUT.metadata.json` 记录 `synthetic`、`complete`、UTC 开始时间、构建所用的源码版本、设备/固件标签、观察到的版本文本、计数、参数及 p50/p95/p99。分位数仅对成功请求使用最近秩法计算；没有成功请求时为 JSON `null`。报告分位数时，应同时报告失败数量和成功率。

元数据初始为 `complete=false`，只有全部计划尝试完成后才改变，因此可识别中断运行。在 Git 仓库外构建时，revision 为 `unknown`，应随结果另行提供源码版本。

## 仍需完成的硬件测量

- 抓取原始帧，确认各固件的 6/7/8 字节响应布局、版本编码、命令 ACK 行为和最小请求间隔。
- 固定设备、固件、适配器、主机、总线负载和请求设置。将原始 CSV/元数据与外部抓帧一同保存，在修改影响性能的代码前重复基线运行。
- 运行约定的长期负载（原方案建议至少八小时），监测完整快照率、失败、延迟分位数、线程/文件描述符数量及内存增长。
- 在实际硬件上验证断线、bus-off 和恢复。vcan 或 FakeTransport 结果不能证明电气行为或物理 ACK。
- 根据这些测量制定硬件延迟和吞吐目标。工具及其模拟检查不包含已经完成的物理台架结果。
