# Runtime and calibration behavior

## Interface configuration

Configure bitrate and bring the CAN interface up before starting the process.
The library opens that interface without running `sudo`, `ip` or `chmod`, or
modifying other interfaces. Its bitrate argument must be positive but is not
applied to the operating system. The CLI and ROS node validate their parameters
before opening hardware.

The factory opens classic SocketCAN on Linux. Other platforms can run protocol
and Session code with injected transports, but have no native SocketCAN backend.

## Frames and cached state

The driver rejects classic transmit payloads over eight bytes and invalid
standard IDs. On reception, SocketCAN filters RTR/error frames and caps native
classic lengths at eight bytes. The codec checks the resulting ID, flags and
reply length; it cannot detect a raw DLC already changed by normalization.
Accepted six-, seven- and eight-byte replies are listed in
[PROTOCOL.md](PROTOCOL.md).

Only a complete four-block response becomes a snapshot. A failed new read
throws, while a cached read can still return an older valid snapshot after a
response timeout. Check its age and fault validity as well as `valid`; absent
fault bits mean unknown. Session close/fault, generation changes and observed
inbox overflow invalidate committed data. Standard ROS topics use radians;
compatibility topics retain degrees.

## Explicit calibration only

Startup `calibration` must be zero. The CLI and constructors do not issue reset
commands. `resetZero(joint)` is an explicit library operation: indices `0..13`
send `0xC0` with one index byte, while `255` sends an empty `0xC1`. It invalidates
the cached snapshot before attempting transmission. Library callers can invoke
this operation repeatedly; the core API does not impose a one-time guard.

The ROS `~/calibrate_once` service is disabled unless startup configuration sets
`allow_calibration=true`. Its first permitted request consumes a per-node guard
before queuing the command. The worker attempts `calibration_joint` once; failure,
uncertain delivery and recovery do not trigger another attempt. Restarting resets
the guard, but a new explicit service request is still required. The guard is
stored only in memory, not as a persistent acknowledgment or idempotency key.

Calibration completion has no implemented reply. Even an accepted ROS service
request returns `success=false` because queuing or dispatch cannot confirm device
completion. Read the message and diagnostics for the attempt's status, then use
an operator-approved device check to resolve uncertain delivery before resending.

## Lifetime and hardware limits

Cached state and version access are synchronized, and printing uses a copied
snapshot outside the state lock. Application code must finish its member calls
before destroying a core arm object. A borrower closes its own inbox and cancels
its endpoint; only the agreed owner closes or recovers a shared Session. Driver
calls already entered can delay cancellation, recovery and shutdown beyond a
requested deadline. See [SESSION.md](SESSION.md) for the exact ownership contract.

FakeTransport, adopted datagram sockets and Linux vcan test software behavior
without moving hardware. Physical CAN ACK, termination, bus-off, firmware timing
and calibration completion need device tests. Use [SOAK.md](SOAK.md) and
[BENCHMARK.md](BENCHMARK.md) to collect those results; the short lifecycle test
does not establish eight-hour stability.
