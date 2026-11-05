[English](VALIDATION.md) | [简体中文](zh/VALIDATION.md)

# Validation results and outstanding tests

[Documentation](README.md) · [Running tests](TESTING.md) · [Measurements](BENCHMARK.md)

## Automated coverage

CI covers Linux x86-64/ARM64, macOS core builds, ASan/UBSan, TSan, relocated installed consumers, ROS2 Humble, and Linux vcan. FakeTransport replaces the transport backend; tests still exercise the production Session, protocol decoder, and snapshot assembler.

The fixed evidence commit before PR8 was merged is `3fa95b4e90e76b27a2d556950f64b0a05ba359ca`:

- [PR CI](https://github.com/soulteary/linkerhand-tele-arm/actions/runs/41648503034): 58 tests each on Linux/TSan/ASan, 59 with vcan, and 53 macOS core tests.
- ROS2 colcon reported 73 tests with no errors, failures, or skips; four lifecycle and shutdown tests were also repeated three times each.
- With benchmark tests enabled: 62 tests on Linux and 57 on macOS, covering missing-response accounting, non-integer success-ratio precision, overwrite prevention, and partial-result integrity after SIGTERM.

These counts apply to that commit. Tests may be added or removed later; check the relevant commit's results in [Actions](https://github.com/linker-bot/linkerbot-teleop-arm/actions) when evaluating a newer version.

## Physical-device validation still needed

- vcan does not validate wiring, termination, physical ACK, adapter drivers, or real bus-off behavior.
- Synthetic baselines are explicitly marked `synthetic=true` and cannot establish physical-device throughput or latency SLOs.
- Without device cycle IDs, host assembly cannot strictly prove simultaneous sampling or completely exclude late replies from an earlier request.
- The actual 6/7/8-byte reply distribution across firmware versions, version encoding, and calibration completion ACK need device documentation and captures.
- No eight-hour physical-device soak was performed in this round; see [SOAK.md](SOAK.md) for the procedure.

For release validation, record the device, firmware, adapter, interface, bitrate, load, host, and source commit, and retain raw CAN captures, CSV, and metadata. Tie hardware conclusions to those records; automated results do not replace outstanding physical tests.
