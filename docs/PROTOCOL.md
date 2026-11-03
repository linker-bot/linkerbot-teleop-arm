# Tele-arm protocol and snapshot contract

This document describes the currently implemented host contract, based on the
existing tele-arm driver. It is not a substitute for a firmware protocol
specification. Hardware captures must confirm the version format, firmware
variants, calibration acknowledgement and maximum polling rate before release.

## Wire frames

`ArmProtocol.h` is portable C++17 and does not include Linux or ROS headers.
`Protocol::Frame::can_id` contains only the arbitration ID. Its explicit flags are
`is_extended`, `is_remote`, `is_error` and `is_fd`; the decoder rejects any of
them and any ID above the standard 11-bit range. A raw SocketCAN ID containing
flag bits is also rejected, rather than masked into a seemingly valid data frame. Only classic standard data frames are
accepted; the payload capacity is eight bytes. In the live path, SocketCAN filters
RTR/error frames before they enter Session, and the arm copies the remaining
extended/FD flags from the routed message. The backend normalizes native classic
receive lengths to at most eight bytes; the standalone codec rejects DLC values
above eight, but cannot recover a malformed native length already normalized by
the backend.

## Commands

The current arm runtime sends these standard classic data frames:

- `0x64`, empty payload: request version bytes.
- `0x04`, empty payload: request all four position blocks.
- `0xC0`, one byte `0..13`: explicitly reset that joint's zero.
- `0xC1`, empty payload: explicitly reset all joints (`resetZero(255)`).

There is no implemented calibration completion ACK. Constants `0x02` and `0x03`
remain in the legacy header, but `readJointState()` uses the full `0x04` request.

## Replies

Accepted replies:

- `0x64`: one to eight version bytes, preserved exactly with their length.
  The codec does not infer a multibyte or semantic version format. Empty replies
  are invalid. The runtime's legacy `getVersion()` display interprets only byte
  zero as decimal digits; that presentation is not a firmware-validated version
  specification.
- `0x65`: exactly eight bytes, left joints 0–3.
- `0x66`: exactly six bytes for left joints 4–6, optionally followed by one fault
  byte (DLC seven or eight) covering all seven left joints.
- `0x67`: exactly eight bytes, right joints 7–10.
- `0x68`: exactly six bytes for right joints 11–13, optionally followed by one
  fault byte (DLC seven or eight) covering all seven right joints.

Each angle is a signed big-endian 16-bit value in tenths of a degree. A fault byte
uses bits 0–6 for the corresponding arm's seven joints. The absent fault byte in
a six-byte reply means **unknown**, not healthy. `fault_valid` records this
distinction. Eight-byte `0x66`/`0x68` replies remain accepted for compatibility
with the previous driver's `DLC >= 7` behavior. The eighth byte is ignored and
has no assigned semantics; firmware documentation is still needed to interpret
it. Unknown IDs and all other normalized lengths are rejected without publishing
or changing a snapshot.

`decodeFrame()` returns both a classified `DecodeError` and an optional decoded
frame. Length validation occurs before any data access. Invalid inputs have no
decoded frame. `SnapshotAssembler` additionally checks decoded block indexes,
counts and finite wrapped angle ranges, since it is a public API and callers
could construct a `DecodedFrame` without using the decoder.

## Angle representation

`decodeAngle()` converts unsigned bytes using an explicitly signed arithmetic
operation, without implementation-defined out-of-range unsigned-to-signed casts.
The result is wrapped to `[-180, 180)` degrees, preserving the legacy
representation while removing the old sample rejection at the branch cut.
Both wire representations of 180 degrees become -180 degrees. A transition
179 → -179 is accepted; it no longer freezes the last sample.

`shortestAngularDelta(previous, current)` reports the circular delta in the same
interval (179 → -179 produces +2). It does not automatically unwrap positions,
apply physical joint limits, estimate velocity, or reject motion. A continuous
joint model and a finite-range joint model require different policies. Neither
should be invented from these bytes alone. The core values are degrees;
the ROS adapter converts them to radians on `/joint_states`. Legacy
`/left_arm_joint_control` and `/right_arm_joint_control` retain degrees and can be
disabled with `publish_legacy_topics=false`. Direction multipliers are applied
consistently to both representations.

## Complete snapshots

`SnapshotAssembler` is a single-owner component. A shared runtime must hold its
state lock around mutations and copies; the assembler deliberately does not
manage its own thread or hardware access.

Four distinct position blocks form a snapshot. Each accepted block contributes
its position values and monotonic receive timestamp. Only a complete set is
committed atomically to `latest()`, which returns a copy. Before the first complete
set, `valid` is false and the zero-initialized storage is not a measured zero
pose. Pending partial updates never change the previously committed snapshot.

The assembler's default maximum first-to-last arrival skew is 100 ms and can be
changed when constructing an assembler. The arm currently uses that default;
`SessionOptions` does not expose a separate skew setting.
Negative skew is invalid; exactly the configured boundary is accepted. A duplicate
block discards the partial candidate and starts a new candidate with that block.
A block received beyond the skew window also discards the incomplete candidate
and starts over. Backwards timestamps and malformed decoded frames do not mutate
state, including the stored receive clock. After a complete set, the following
block starts a new candidate.

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

These four frame formats contain no known hardware cycle identifier. A host can
enforce one in-flight request, an arrival cutoff, a bounded assembly window and
no repeated block within a candidate. Those rules do not prove that the four
angles were sampled simultaneously. A delayed previous response that arrives
after the cutoff and within the window cannot reliably be distinguished from
the current response. Claims of strict hardware-cycle correlation require a
firmware sequence field or a documented transport/protocol guarantee.

The arm uses the Session's monotonic `received_at`, not the time its reader thread
later processes the queued frame. A query has both a start cutoff and a deadline;
queueing or scheduling can occur between that cutoff and actual transmission.
A complete response timeout does not make an earlier cached snapshot fresh.

Likewise, a successful socket write means the local backend accepted the request.
It does not mean the device replied or executed a calibration. The decoder only
observes known reply data; it does not treat a sent `0xC0`/`0xC1` command as an
acknowledgement. Calibration success requires a separately documented reply or
readback contract. Timeout, disconnection or process restart must not silently
replay a command with an uncertain result.

## Regression coverage

`test_protocol.cpp` calls the production codec and assembler directly. It covers
all reply lengths from zero through nine, standard/extended/RTR/error/FD flags,
positive and negative angles, the ±180 boundary, exact version bytes, missing
fault validity, ordered and out-of-order complete sets, duplicate/expired/partial
sets, clock reversal, request cutoffs, reset generations, freshness of the oldest
part, malformed decoded fields and lack of state changes after rejected input.
