# Session transport integration

The driver uses `linkerbot::transport::Session` for exact receive routing and one
transmit scheduler per Session. The device-specific codec and snapshot assembler
remain in `linkerta::core`. The SocketCAN factory selects classic CAN explicitly;
CAN FD is not an interchangeable mode for this protocol.

## Dependency and installation

CMake first searches for an installed `linkerbot-comm` package compatible with
version 0.3. If none is found, it fetches commit
`e514b832e09ecd60a56d2c67e39c54285793fff8`. An installed package must contain the
Session exception-publication fix from that revision; the runtime regression
checks this behavior. The version constraint alone does not identify the fix.
Dependency tests, examples and benchmarks are disabled in the fallback build.

For a network-free source build, pass these CMake options with existing sources:

```text
-DLINKERTA_ALLOW_DEPENDENCY_DOWNLOADS=OFF
-DFETCHCONTENT_SOURCE_DIR_LINKERBOT_TRANSPORT=/path/to/linkerbot-transport-cpp
-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/path/to/googletest
```

Alternatively, install the dependency and GoogleTest and set `CMAKE_PREFIX_PATH`
to their prefixes. GoogleTest is needed only with `BUILD_TESTING=ON`. Missing
requirements fail configuration when downloads are disabled; the driver does not
fall back to its removed CanBus implementation. See [INSTALL.md](INSTALL.md)
for installation and the exported `linkerta::core` target.

## Construction, routes and ownership

`LinkerArm(interface_name, bitrate)` creates a factory-backed Session on an
already-configured Linux SocketCAN interface. The bitrate argument must be
positive, but does not reconfigure the interface. The factory gives the Session
a transmit capacity of 32 waiting batches. The `"master_arm"` constructor probes
existing interfaces whose names start with `can` or `vcan`; it fails if none or
more than one produces a valid version reply. Discovery does not configure
interfaces or identify a particular physical arm beyond that reply.

`LinkerArm(shared_ptr<Session>, SessionOptions)` reuses a caller-supplied Session.
Its defaults are:

- `own_session=false`: destruction does not close the shared Session.
- `receive_capacity=64`: frames in this arm's inbox.
- `command_timeout=250 ms`: constructor version-send and reset command budget.
- `transmit_gap=5000 us`: delay before each command, including single-frame batches.
- Empty `endpoint`: generate a unique per-instance endpoint.

The supplied Session's transmit capacity is unchanged. Do not confuse these
values with the transport library's general defaults of 2000 waiting batches and
512 frames per inbox. Both the Session queue and the arm inbox are bounded; an
executing batch does not consume a waiting-batch slot.

Construction registers the five standard routes `0x64` through `0x68` before
sending the version request. The injected and explicit-interface constructors
wait for that request's dispatch, **not** a version response; `getVersion()` may
initially be empty. Discovery and explicit recovery have separate reply checks.
Two arms cannot hold these same standard-ID routes on one Session. Extended IDs
are different keys, but the tele-arm protocol rejects extended replies.

By default, arm destruction closes its inbox, cancels its endpoint, joins its
reader and preserves the Session and unrelated routes. Setting `own_session=true`
allows the arm to close the entire Session and to request recovery. This flag is
an explicit lifecycle agreement, not proof that no other code shares the object:
closing a shared Session invalidates every user's routes. Assign one lifecycle
owner and do not reuse endpoint names across independent clients.

The dependency's `Session::create()` owns a factory-backed connection;
`Session::own()` owns a transport without a recovery factory. `share()`, `lease()`
and `borrow()` reuse a Session around existing transport infrastructure. They do
not create independently closable logical channels. In particular, closing one
shared Session invalidates its other clients even when its external bus remains
open.

## New reads, cached reads and synchronization

`readJointState(timeout=250 ms)` serializes a new query, sends an empty `0x04`
request and waits for a complete snapshot with a newer local sequence in the same
generation. Its one deadline includes waiting behind another arm operation,
transmit scheduling and response assembly. The current public API combines
request and wait in this call; it has no separate asynchronous request/wait API.
`getJointPosition()` calls it and returns the positions in degrees.

`latestSnapshot()` sends no query and returns a locked copy of the last committed
snapshot after checking Session state and inbox overflow. It does not wait for a
new sample or make old data fresh. Before the first complete response, or after
invalidation, `valid` is false. Callers must check `valid`, `fault_valid` and
`isFresh(now, max_age)` as appropriate. A response timeout throws from the new
read but can leave an older valid cache available for explicit inspection.

`getJointErrorCode()` is a legacy cached convenience accessor. It discards both
snapshot validity and per-joint fault validity; a false bit alone cannot prove a
healthy joint. Use the full snapshot when missing fault data matters.
`getVersion()` reads cached version data; its legacy display formats only the
first byte as decimal digits. The codec preserves all version bytes, but that
display is not a validated firmware version specification. `printJointInfo()`
formats a local snapshot copy outside the state lock and does not assert freshness.

The Session owns transport receive/scheduling work, and each arm adds a reader
thread for its routed inbox. The arm serializes query, reset and recovery operations
with a request lease and protects state copies with a mutex. Application code must
still stop its calls before destroying the arm; concurrent destruction and member
calls are not a supported lifetime contract. `TeleArmNode` separately joins its
CAN worker during teardown so its executor is not blocked by ordinary queries.

## Deadlines, receipts and receive time

Commands use FIFO submission, an explicit endpoint and a per-command cancellation
token. Session receipt waits can throw on failure, cancellation, expiry or wait
timeout. A wait timeout alone does not cancel transmission; the arm explicitly
cancels that command's token when its send path fails or exceeds the deadline.
It does not cancel other devices' work. Cancellation cannot undo a backend write
already entered, and a driver call may return after the requested deadline.

A dispatched receipt means backend acceptance, not CAN acknowledgment, device
execution or a reply. This remains true when the final write completes after its
Session deadline. The arm also checks its own deadline after dispatch and rejects
an expired new read. Do not automatically replay a calibration or a command whose
delivery is uncertain.

`RoutedMessage.received_at`, generation and sequence are host metadata. The reader
uses the Session's monotonic receive timestamp, rejects a different generation,
and accepts query replies only while a query is pending and no later than its
deadline. The assembler additionally rejects timestamps before the query's start
boundary. This boundary precedes transmit scheduling; it is not a firmware cycle
identifier. See [PROTOCOL.md](PROTOCOL.md) for late-response limits and skew rules.

## Overflow, close and recovery

A full route inbox rejects newly arriving frames and increments its overflow
counter; it does not replace old entries. Observed overflow resets pending **and
committed** snapshot validity and aborts the current query. Closing, a faulted
transport or a changed generation also clears cached state and version data.
Those connection changes require identity revalidation; overflow alone does not
require a new version handshake before the next query.

`arm.recover(timeout=250 ms)` is allowed only with `own_session=true`. It stops the
reader, cancels the arm endpoint, invokes Session recovery, registers fresh routes,
resets snapshot state and waits for a valid version response. It does not replay
old queries or calibration commands. The timeout covers lease waiting and the
reply budget, but is not a hard bound on synchronous backend close/open calls.
A Session built with `Session::own()` has no factory and cannot physically recover.
An explicitly closed Session cannot be reopened by recovery.

If an external owner closes or recovers a borrowed Session, reconstruct the arm
after that owner's recovery. The old client's closed routes and cached data are
not relabeled or automatically reattached to the new generation. A local sequence
and generation can detect host-side replacement; they cannot establish which
physical device sampled a frame or which firmware sample cycle it belongs to.
