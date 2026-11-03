# Manual eight-hour soak (not executed by CI)

Use a dedicated bench with documented adapter, firmware, bitrate, termination,
power supply and host load. Record the exact git commits, kernel and compiler.
Configure CAN outside the process. Start with a conservative validated request
rate; do not assume the configured rate is a device guarantee.

For at least eight hours record completed fresh snapshots, timeouts, invalid
frames, overflow/recovery events, host timestamp and process RSS, thread count,
and open fd count. Preserve raw observations rather than only averages. Include
startup warm-up as a separate phase and compare resource trends after warm-up.
On a controlled bench inject disconnect/reconnect, missing replies and load;
verify invalid/stale data is not published and no calibration is replayed.

Acceptance requires no unexplained sustained resource growth, no stale snapshot
publication, no sanitizer findings, and explicit investigation of every recovery
failure. Report p50/p95/p99 only from recorded samples with units and sample count.
A fake/vcan run cannot establish hardware ACK, physical bus-off behavior or
real-device throughput. This procedure is a pending manual check, not a claim
that an eight-hour test has passed.
