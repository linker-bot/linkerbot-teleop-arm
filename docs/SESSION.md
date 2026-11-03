# Session transport integration

The driver uses `linkerbot::transport::Session` for transport ownership, exact-ID
routing, a bounded inbox and the single transmit scheduler. `ArmProtocol` and
`SnapshotAssembler` remain device-specific, portable components. Classic CAN is
selected explicitly; enabling CAN FD on an adapter is not compatible by default.

The imported SocketCAN decoder normalizes native classic frame lengths to the
eight-byte payload capacity. The driver validates the resulting device message
against the unchanged protocol contract. RX normalization is distinct from TX:
an oversized classic transmit message is rejected before it reaches the socket.

## Dependency

CMake first searches for an installed `linkerbot-comm` package (0.3 or compatible).
Otherwise it fetches the repository at commit
`c54a0fdc390f7940177d3a7533d333cc5df5a36a`. Dependency tests, examples and
benchmarks are not part of the driver's build.

For a network-free source build provide:

```
-DLINKERTA_ALLOW_DEPENDENCY_DOWNLOADS=OFF
-DFETCHCONTENT_SOURCE_DIR_LINKERBOT_TRANSPORT=/path/to/linkerbot-transport-cpp
-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/path/to/googletest
```

Alternatively install the dependency first and set `CMAKE_PREFIX_PATH` to its
installation prefix. Missing dependencies fail configuration when downloads are
disabled; they are never silently replaced with the old socket implementation.

## Contracts

Subscribe to device routes before transmitting requests. Standard and extended
IDs are distinct; the fixed tele-arm reply IDs cannot be shared by two arm
instances on the same Session. A route collision is an error, not a broadcast.
An arm that borrows a Session must release only its own inbox and endpoint;
closing the shared Session is the application's responsibility.

Submission and receipt completion describe backend acceptance, not device
execution. A receipt wait timeout does not itself cancel queued transmission;
request cancellation must be explicit. Never automatically replay calibration
or a write whose delivery is uncertain. Driver calls already in progress cannot
necessarily be interrupted by a deadline.

Inbox overflow and a changed Session generation invalidate partially assembled
state. Recovery requires new routes and device identification; old snapshots
must not be relabeled with a new generation. A local sequence and generation
cannot replace a device sampling-cycle identifier.

## Runtime options and recovery

The injected-Session constructor borrows by default (`own_session=false`).
`SessionOptions` defaults to an inbox capacity of 64, command timeout of 250 ms
and transmit pacing of 5000 microseconds. An empty endpoint requests a unique
per-instance endpoint. Never reuse an endpoint across independently owned clients.

The interface and discovery constructors create owned factory-backed Sessions.
Only an explicitly owned Session may be recovered through `arm.recover(timeout)`.
Recovery reopens the transport, registers fresh routes, resets generation and
waits for a valid version response before enabling reads. A Session created with
`Session::own()` has no factory and cannot perform physical recovery.

If an external owner closes or recovers a borrowed Session, the old client is
invalidated; reconstruct it after the owner's recovery. A borrowed client's
destruction closes its inbox and cancels its endpoint, but does not close the
shared Session or unrelated routes. See the `installed-dependency` CI job for
configuration with an installed transport and dependency downloads disabled.
