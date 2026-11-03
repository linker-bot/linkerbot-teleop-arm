# Runtime and calibration safety boundaries

## Interface configuration

The process opens preconfigured CAN interfaces. Configure bitrate and bring the
chosen interface up outside the process. The library does not run `sudo`, `ip`,
`chmod` or modify other interfaces. The core bitrate argument is checked for a
positive value but is not applied to the operating system. CLI and ROS parameter
validation happen before their hardware construction paths.

The production factory opens classic SocketCAN on Linux. Protocol and Session
code can be exercised on other platforms with injected transports; that does not
provide a native non-Linux SocketCAN backend.

## Frames and cached state

Classic transmit payloads larger than eight bytes and invalid standard IDs are
rejected. The production SocketCAN backend filters RTR/error frames and normalizes
native classic receive length to its eight-byte capacity. The device codec checks
the normalized ID, flags and required reply lengths. A malformed raw DLC is not
necessarily observable after backend normalization. See [PROTOCOL.md](PROTOCOL.md)
for the accepted six-, seven- and eight-byte reply variants.

Only a complete four-block response becomes a snapshot. New reads throw on
failure; they do not return the previous pose as a successful response. Explicit
cached reads can still expose an older valid snapshot after a response timeout.
Consumers must check age and fault validity as well as `valid`. Missing fault
bits mean unknown, not healthy. Session close/fault, generation changes and
observed inbox overflow invalidate committed data. Standard ROS publications use
radians; the compatibility topics deliberately retain degrees.

## Explicit calibration only

Startup `calibration` must be zero. The CLI and constructors do not issue reset
commands. `resetZero(joint)` is an explicit library operation: indices `0..13`
send `0xC0` with one index byte, while `255` sends an empty `0xC1`. It invalidates
the cached snapshot before attempting transmission. Library callers can invoke
this operation repeatedly; the core API does not impose a one-time guard.

The ROS `~/calibrate_once` service adds a per-node guard. It is disabled unless
`allow_calibration=true` was set at startup. The first permitted request consumes
the guard before its command is queued; failure or uncertain delivery does not
restore it. The worker attempts the configured `calibration_joint` once, and
there is no automatic retry or replay during recovery. A restarted process has a
new guard, but does not calibrate unless a new explicit service request arrives.
This guard is in memory; it is not a persistent acknowledgment or idempotency key.

No implemented protocol reply confirms calibration completion. A dispatched
command means local backend acceptance only, so even a queued ROS service request
returns `success=false` with an explanation that device completion is unconfirmed.
Its message and diagnostics distinguish queuing/attempt outcomes; neither proves
the device's final pose. Resolve uncertain delivery by an operator-approved device
check rather than blindly resending.

## Lifetime and hardware limits

Cached state and version access are synchronized, and printing uses a copied
snapshot outside the state lock. Application code must finish its member calls
before destroying a core arm object. A borrower closes its own inbox and cancels
its endpoint; only the agreed owner closes or recovers a shared Session. Driver
calls already entered can delay cancellation, recovery and shutdown beyond a
requested deadline. See [SESSION.md](SESSION.md) for the exact ownership contract.

FakeTransport, adopted datagram sockets and Linux vcan tests validate software
behavior without moving hardware. They cannot establish physical CAN ACK,
termination, bus-off behavior, firmware response timing or successful calibration.
The short lifecycle regression is not evidence of an eight-hour soak. Follow
[SOAK.md](SOAK.md) and [BENCHMARK.md](BENCHMARK.md) for the separate device
validation procedure and retain actual results before making performance claims.
