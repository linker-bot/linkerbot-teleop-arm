# Measurement harness and outstanding hardware validation

`linkerta_benchmark` is an opt-in measurement tool. It does not configure CAN,
calibrate joints, recover the transport, or change the polling implementation.
The synthetic mode exercises the production `LinkerArm` and Session code through
`FakeTransport`; its timings are **not physical-device latency or an SLO**.

## Build and synthetic accounting check

```sh
cmake -S src/linkerta -B build-benchmark -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON -DLINKERTA_BUILD_BENCHMARKS=ON
cmake --build build-benchmark --parallel 2
ctest --test-dir build-benchmark --output-on-failure --no-tests=error
./build-benchmark/linkerta_benchmark --simulate --count 100 --rate-hz 20 \
  --timeout-ms 250 --output synthetic.csv
```

Use a new output path for each run. Existing CSV or metadata files are rejected.
An additional simulation-only `--drop-every 10` omits every tenth complete reply;
it should produce failure rows, not repeat the previous snapshot. The process
exits `0` for all-success, `1` if any requests fail, `2` for argument/I/O/setup errors,
and `128 + signal` after SIGINT/SIGTERM. A failed request does not stop accounting.
No automatic retries are performed. SIGINT/SIGTERM stop before the next request;
an in-flight request can finish or reach its deadline first.

## Hardware run (not performed for this PR)

Configure the existing CAN interface using the deployment steps in `INSTALL.md`.
Supply a real device identifier and firmware label; the tool refuses implicit
hardware selection or a hardware run without these labels:

```sh
./build-benchmark/linkerta_benchmark --interface can0 \
  --device-label arm-A-adapter-A --firmware recorded-firmware-version \
  --count 10000 --rate-hz 20 --timeout-ms 250 --output hardware-arm-A.csv
```

`--simulate` and `--interface` are mutually exclusive. The hardware path uses the
existing configured interface (the core constructor receives the legacy expected
bitrate of 1000000 but does not apply it). Record the actual externally configured
bitrate, adapter, bus load, host, CPU governor and wiring alongside the result.
Both paths retain the driver's 5000 microsecond transmit gap. Request frequency
is an upper bound, with no catch-up burst after a slow request.

## Output contract

The CSV contains one row per attempted request: elapsed host steady time,
request duration in microseconds, success, snapshot sequence/generation, oldest
frame age and inter-frame skew in microseconds, route pending/high-water/overflow,
Session pending/high-water/overflow, and a quoted error. Failed requests have no
snapshot sequence or age fields; generation still records current Session lifetime. Counters are cumulative snapshots, not per-request deltas.
A successful request means the driver assembled a complete host-consistent
snapshot; it does not prove simultaneous hardware sampling.

`OUTPUT.metadata.json` records `synthetic`, `complete`, UTC start, build revision,
supplied device/firmware labels, observed version text, counts, parameters and
p50/p95/p99. Percentiles use the nearest-rank method on **successful requests
only**; no successful samples yields JSON `null`. Always report failures and
success ratio alongside percentiles. The initial metadata marks `complete=false`
until the planned number of attempts finishes; interruption or abnormal process
termination must not be mistaken for a complete run. Revision is `unknown` when
building outside a Git checkout; record the source revision manually in that case.

## Acceptance still requiring a physical bench

- Capture raw frames and confirm each firmware's 6/7/8-byte response layouts,
  version encoding, command ACK behavior and minimum request spacing.
- Fix device, firmware, adapter, host, bus load and request settings; archive raw
  CSV/metadata and external capture together. Repeat baseline runs before any
  performance change.
- Run the agreed long-duration workload (the original plan proposes at least
  eight hours), monitoring complete-snapshot rate, failures, latency percentiles,
  thread/file-descriptor counts and memory growth.
- Validate disconnect, bus-off and recovery on actual hardware. A vcan or
  FakeTransport result cannot establish electrical behavior or physical ACK.
- Define a hardware SLO only after these measurements. This PR contains tooling
  and synthetic accounting checks, not a claim that the physical bench is complete.
