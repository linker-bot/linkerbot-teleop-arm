[English](API.md) | [简体中文](zh/API.md)

# Using the C++ API

[Documentation](README.md) · [Installation](INSTALL.md) · [Session](SESSION.md)

After installation, include `<linkerta/LinkerArm.h>` and `<linkerta/ArmProtocol.h>` and link `linkerta::core`. The core API does not depend on ROS2. Hardware SocketCAN I/O is available only on Linux.

## Fresh reads and cached snapshots

- `readJointState(timeout)` sends a request and waits for a new, complete four-block snapshot. The default is 250 ms. It throws on timeout, send failure, disconnection, or required identity revalidation. Waiting for the request slot, sending, and receiving share one deadline.
- `latestSnapshot()` copies the cache and checks Session state and route overflow without sending a query. Check `valid` and `isFresh(now, max_age)`: after a timeout, the cache may still contain the previous valid snapshot, not a new measurement.
- `getJointPosition()` is a compatibility wrapper around `readJointState()` and returns degrees.
- `getJointErrorCode()` copies cached fault booleans without validity information. New code should also read `fault_valid`. Read positions, faults, and validity from the same `ArmSnapshot` rather than combining separate reads.
- `getVersion()` returns the received version string under a lock, or an empty string before a reply arrives. It is not a synchronous version query. See the protocol guide for encoding limits.
- `lastError()` returns the current error text, not an error history.

`ArmSnapshot` contains 14 `positions_degrees`, `faults`, and `fault_valid` entries; four `received_at` timestamps; the latest arrival time `captured_at`; and `sequence`, `generation`, `received_mask`, and `valid`. Timestamps use the steady clock. `isFresh` checks every constituent frame, including the oldest.

This example opens `can0`. Configure the interface as described in the installation guide before running it:

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

The example only demonstrates reading. Applications should catch exceptions and decide how to log errors and stop work; do not wrap it in an unlimited retry loop.

## Session injection and ownership

Pass a `std::shared_ptr<linkerbot::transport::Session>` to share a connection or inject FakeTransport. `SessionOptions` defaults are:

- `receive_capacity=64`: a bounded per-arm receive queue; overflow invalidates the snapshot.
- An empty `endpoint` generates an independent name to isolate send cancellation.
- `command_timeout=250 ms`, `transmit_gap=5000 us`.
- `own_session=false`: destruction removes this arm's routes and cancels its sends, without closing the external shared Session.

Two arms cannot claim the same exact standard-ID routes `0x64..0x68` on one Session. Different endpoint names do not resolve route conflicts. Interface and discovery constructors own their Session's lifetime.

Only an arm that explicitly owns its Session may call `recover(timeout)`, and the Session needs a factory that can recreate the connection. `Session::own()` has no such factory, so `own_session=true` alone cannot enable physical recovery. Recovery cannot reopen an explicitly closed Session. Successful recovery rebuilds routes, clears old state, and waits for a version reply. If an external owner recovers a shared Session, reconstruct the old arm rather than publishing data from its previous generation.

## Concurrency and shutdown

The receive thread and snapshot readers use the same state lock. Requests run sequentially through the request slot. Stop other threads from accessing an arm before destroying it. Destruction joins the receive thread; the ROS2 node also waits for the worker's current request.

A request deadline limits the waiting budget, and cancellation can prevent unsent work. Neither can forcibly interrupt a third-party driver or system call already in progress, so shutdown has no hard real-time guarantee. Stop new work, stop the executor or callers, wait for in-flight requests, destroy the arm, and finally have the owner close the shared Session.

## Calibration

`resetZero(joint)` accepts 0..13 or 255 (all joints). It is an explicit operation with side effects and must not be placed in a generic retry loop. Returning means the send path completed, not that the device finished calibration: no completion ACK has been verified. The once-per-process guard belongs only to the ROS2 service. Applications calling C++ directly must limit attempts themselves.
