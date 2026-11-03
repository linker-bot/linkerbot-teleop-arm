# C++ development and documentation

English · [简体中文](zh/CPP_GUIDE.md) · [Documentation](README.md)

The public core targets C++17. Keep headers usable without ROS2 or Linux headers;
platform code belongs in adapters. New code should build with the project's
`-Wall -Wextra -Wpedantic` flags and pass formatting, analyzer and runtime tests.

## Public interfaces

Document each operation's inputs, units, ownership, side effects and failure modes.
Distinguish a new request from a cache read. State when a timeout starts, what it
covers and whether an underlying operation can continue after the caller stops waiting.
Do not mark an operation `noexcept` unless every path meets that guarantee.

Public headers include their own dependencies, use qualified names and avoid
namespace-wide `using` directives. Prefer value types and fixed-size arrays for
protocol data. Validate lengths and ranges before indexing or narrowing values.
Keep wire decoding independent of transport and ROS message conversion.

## Resources and concurrency

Use RAII for connections and threads. Give each resource one lifecycle owner;
sharing an object does not grant every client permission to close or recover it.
Document borrowed lifetimes and which calls can run concurrently. Stop callers
before destroying their target, and never hold a state lock while invoking user code.

Separate transport acceptance, a protocol reply and device completion. Preserve
error details rather than converting a failure to an apparently valid zero value.
Retries need an operation-specific policy; calibration is not automatically replayable.

## Changes and tests

A regression test should exercise production code and fail for the defect it guards.
Use FakeTransport gates or observable state for concurrency tests instead of hoping
a sleep creates the intended order. Keep hardware tests explicitly selected.

For a public behavior change, update both language editions, examples and tests in
the same PR. Check installed consumers as well as in-tree builds. Report analyzer
findings and runtime validation separately: a clean static report does not establish
thread safety, protocol correctness or device behavior.

[API](API.md), [Session](SESSION.md) and [tests](TESTING.md) describe the implemented contracts.
