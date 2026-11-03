# Run an eight-hour hardware soak

Use a dedicated bench. Record the adapter, firmware, bitrate, termination, power
supply, host load, Git commits, kernel and compiler. Configure CAN outside the
process and start at a request rate already validated on that device. The
configured rate is a scheduling limit, not a guarantee of device throughput.

For at least eight hours, record fresh complete snapshots, timeouts, invalid
frames, overflow/recovery events, host timestamps, process RSS, thread count and
open fd count. Keep the raw observations. Separate startup warm-up from the
measurement period, then compare resource trends.

Inject disconnect/reconnect, missing replies and load under controlled bench
conditions. Check that invalid or stale data is not published and calibration
commands are not replayed.

Accept the run only after investigating every recovery failure, with no
unexplained sustained resource growth, stale snapshot publication or sanitizer
findings. Report p50/p95/p99 from the recorded samples, including units and sample
count.

CI does not execute this soak. FakeTransport and vcan cannot establish hardware
ACK, physical bus-off behavior or device throughput; no completed eight-hour
hardware result accompanies this procedure.
