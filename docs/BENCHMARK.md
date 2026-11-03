# Measuring query latency

Build `linkerta_benchmark` to record query results and latency. It uses the
existing polling implementation and does not configure CAN, calibrate joints or
recover the transport. Synthetic mode runs `LinkerArm` and Session through
`FakeTransport`; use it to check the tool, not to set a hardware latency target.

## Build and check synthetic output

```sh
cmake -S src/linkerta -B build-benchmark -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON -DLINKERTA_BUILD_BENCHMARKS=ON
cmake --build build-benchmark --parallel 2
ctest --test-dir build-benchmark --output-on-failure --no-tests=error
./build-benchmark/linkerta_benchmark --simulate --count 100 --rate-hz 20 \
  --timeout-ms 250 --output synthetic.csv
```

Choose a new output path for each run; existing CSV or metadata files are
rejected. In simulation, `--drop-every 10` omits every tenth complete reply. Each
omission should produce a failure row rather than repeat the previous snapshot.

Failed requests are recorded without retry, and the run continues. Exit codes
are `0` when all requests succeed, `1` when any fail, `2` for argument/I/O/setup
errors, and `128 + signal` after SIGINT/SIGTERM. A signal stops the run before
the next request; an in-flight request can finish or reach its deadline first.

## Run on hardware

Configure the CAN interface using the deployment steps in `INSTALL.md`. Supply
its name, a device identifier and a firmware label; all three are required for a
hardware run:

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

The CSV records one row per attempted request, with elapsed host steady time,
request duration in microseconds, success, snapshot sequence/generation, oldest
frame age, inter-frame skew in microseconds, route pending/high-water/overflow,
Session pending/high-water/overflow, and a quoted error. Failure rows leave
snapshot sequence and age fields empty but retain the current Session generation.
Counters are cumulative snapshots rather than per-request deltas.

Success means the driver assembled a complete snapshot under its host-side
rules. The frames may still come from different hardware sampling cycles.

`OUTPUT.metadata.json` records `synthetic`, `complete`, UTC start, build revision,
device/firmware labels, observed version text, counts, parameters and p50/p95/p99.
Percentiles use the nearest-rank method on successful requests only and are JSON
`null` when none succeed. Report failures and success ratio with the percentiles.

Metadata starts with `complete=false` and changes only after all planned attempts
finish, so interrupted runs remain identifiable. A build outside Git records
revision `unknown`; supply the source revision with the results.

## Hardware measurements still needed

- Capture raw frames and confirm each firmware's 6/7/8-byte response layouts,
  version encoding, command ACK behavior and minimum request spacing.
- Keep device, firmware, adapter, host, bus load and request settings fixed. Save
  raw CSV/metadata with the external capture and repeat baseline runs before
  changing performance-sensitive code.
- Run the agreed long-duration workload (the original plan proposes at least
  eight hours), monitoring complete-snapshot rate, failures, latency percentiles,
  thread/file-descriptor counts and memory growth.
- Validate disconnect, bus-off and recovery on actual hardware. A vcan or
  FakeTransport result cannot establish electrical behavior or physical ACK.
- Set hardware latency and throughput targets from these measurements. The tool
  and its synthetic checks do not include completed physical bench results.
