# Tele-arm protocol and snapshot contract

[简体中文](zh/PROTOCOL.md)

The driver accepts the frames and assembles snapshots as described below.
Before a hardware release, confirm the version format, firmware variants,
calibration acknowledgement and maximum polling rate with device captures.

## Wire frames

`ArmProtocol.h` uses C++17 without Linux or ROS headers. The decoder accepts
classic standard data frames with up to eight payload bytes.
`Protocol::Frame::can_id` holds the arbitration ID only; IDs above `0x7ff` and the
flags `is_extended`, `is_remote`, `is_error` and `is_fd` are rejected. Raw
SocketCAN IDs with embedded flag bits are rejected too.

SocketCAN filters RTR/error frames before they reach Session. The arm copies the
remaining extended/FD flags from each routed message. The backend caps native
classic receive lengths at eight bytes. The standalone codec rejects DLC values
above eight, but a malformed native length is no longer visible after this
normalization.

## Commands

The arm sends these standard classic data frames:

- `0x64`, empty payload: request version bytes.
- `0x04`, empty payload: request all four position blocks.
- `0xC0`, one byte `0..13`: explicitly reset that joint's zero.
- `0xC1`, empty payload: explicitly reset all joints (`resetZero(255)`).

`readJointState()` requests all blocks with `0x04`. Constants `0x02` and `0x03`
remain in the legacy header. Calibration completion ACKs are not implemented.

## Replies

Accepted replies:

- `0x64`: one to eight version bytes, preserved exactly with their length.
  Empty replies are invalid. `getVersion()` displays byte zero as decimal
  digits; the codec preserves the bytes without assigning a version format.
  The display format still needs firmware confirmation.
- `0x65`: exactly eight bytes, left joints 0–3.
- `0x66`: exactly six bytes for left joints 4–6, optionally followed by one fault
  byte (DLC seven or eight) covering all seven left joints.
- `0x67`: exactly eight bytes, right joints 7–10.
- `0x68`: exactly six bytes for right joints 11–13, optionally followed by one
  fault byte (DLC seven or eight) covering all seven right joints.

Each angle is a signed big-endian 16-bit value in tenths of a degree. Bits 0–6
of the fault byte describe that arm's seven joints. A six-byte reply has no fault
byte, so `fault_valid` is false: those joints' fault states are unknown.

Eight-byte `0x66`/`0x68` replies preserve compatibility with the previous driver's
`DLC >= 7` behavior. The eighth byte is ignored; its meaning needs firmware
documentation. Unknown IDs and other normalized lengths leave the snapshot
unchanged and produce no publication.

`decodeFrame()` checks lengths before accessing payload data and returns a
`DecodeError` with an optional decoded frame. Invalid input produces no frame.
Callers can also construct `DecodedFrame` directly, so `SnapshotAssembler`
checks block indexes, counts and finite wrapped angles again.

## Angle representation

`decodeAngle()` uses signed arithmetic rather than an out-of-range
unsigned-to-signed cast, then wraps the result to `[-180, 180)` degrees. Both
wire representations of 180 degrees become -180 degrees. A transition from
179 to -179 is accepted; the old rejection at this boundary has been removed.

`shortestAngularDelta(previous, current)` returns the circular delta in the same
interval: 179 → -179 gives +2. It does not unwrap positions, apply joint limits,
estimate velocity or reject motion. Those decisions need a physical joint model.

The core returns degrees. The ROS adapter applies direction multipliers and
converts to radians on `/joint_states`. The legacy `/left_arm_joint_control` and
`/right_arm_joint_control` topics use the same directions but retain degrees;
set `publish_legacy_topics=false` to disable them.

## Complete snapshots

`SnapshotAssembler` has no threads, locks or hardware access. A runtime sharing
it between threads must lock both mutations and copies.

Four distinct position blocks form a snapshot, carrying their positions and
monotonic receive timestamps. The assembler commits only complete sets;
`latest()` returns a copy. Until the first commit, `valid` is false and the
zero-filled positions are not measurements. Partial updates leave the previous
committed snapshot unchanged.

The default first-to-last arrival limit is 100 ms. It can be changed in the
assembler constructor; the arm uses the default and has no `SessionOptions`
skew setting. Negative limits are rejected, and an arrival exactly at the limit
is accepted.

A duplicate block or an arrival beyond the limit discards the partial candidate
and starts a new one with that block. Backwards timestamps and malformed decoded
frames leave all state unchanged, including the stored receive clock. After a
complete set, the next block starts a new candidate.

`beginCycle()` discards only pending data when the runtime sends a new request;
the previous complete snapshot remains available for explicit cached reads.
`beginCycle(not_before)` additionally rejects a frame with a receive timestamp
earlier than the request boundary. `reset(generation)` clears pending and cached
validity, including fault validity, for a disconnected or replaced transport.
It records the new generation and preserves the monotonically increasing local
snapshot sequence counter. Generation and sequence are host metadata, not
firmware sample identifiers. The arm's new-read API commits only while a query is
pending; cached access alone does not request or assemble a new response. See
[SESSION.md](SESSION.md) for the combined request-and-wait API and cache behavior.

`received_mask == 0x0f` identifies a complete committed set. `received_at[0..3]`
retains individual arrival times. `captured_at` is the last constituent arrival,
not the device's physical acquisition time. `isFresh(now, max_age)` requires a
complete valid set and checks the age of **all four** parts, including the oldest;
future timestamps and a negative maximum age fail the check. An incomplete next
set does not refresh the old snapshot's timestamps.

## Limits of request correlation

The four reply formats have no known hardware cycle identifier. The host allows
one in-flight request and checks arrival cutoffs, assembly time and repeated
blocks. A delayed response from an earlier request can still pass those checks.
Proving that all angles belong to one hardware sample requires a firmware
sequence field or a documented transport/protocol guarantee.

The arm uses the Session's monotonic `received_at`, not the time its reader thread
later processes the queued frame. A query has both a start cutoff and a deadline;
queueing or scheduling can occur between that cutoff and actual transmission.
A complete response timeout does not make an earlier cached snapshot fresh.

A successful write confirms local backend acceptance. Calibration completion
needs a documented reply or readback; sending `0xC0`/`0xC1` is not that reply.
Do not replay a command with an uncertain result after a timeout, disconnect or
process restart.

## Regression coverage

`test_protocol.cpp` calls the production codec and assembler directly. It covers
all reply lengths from zero through nine, standard/extended/RTR/error/FD flags,
positive and negative angles, the ±180 boundary, exact version bytes, missing
fault validity, ordered and out-of-order complete sets, duplicate/expired/partial
sets, clock reversal, request cutoffs, reset generations, freshness of the oldest
part, malformed decoded fields and lack of state changes after rejected input.
