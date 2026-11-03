# Documentation

English · [简体中文](zh/README.md)

Build and test without a device first, then configure CAN using the installation guide.

## Guides

- [Build and install](INSTALL.md): dependencies, offline builds, package consumers and CAN setup.
- [C++ API](API.md): requests and cache reads, units, defaults, threads and ownership.
- [ROS2](ROS2.md): parameters, topics, timestamps, diagnostics and calibration.
- [Protocol](PROTOCOL.md): frame lengths, angles, fault validity and unresolved firmware details.
- [Session](SESSION.md): routing, receipts, cancellation and recovery.
- [Troubleshooting](TROUBLESHOOTING.md) and [FAQ](FAQ.md).
- [Tests](TESTING.md) and [validation records](VALIDATION.md).
- [Benchmarks](BENCHMARK.md) and [long-running tests](SOAK.md).
- [Upgrades and issue reports](OPERATIONS.md).
- [Operating limits](SAFETY.md).
- [C++ development and documentation](CPP_GUIDE.md).
- [C++ Report Card](QUALITY.md).

## Maintaining these guides

English pages are the default; each has a Chinese counterpart under `zh/`.
Update both when commands, defaults or behavior change. Keep identifiers and
fixed-commit evidence unchanged. Code examples may translate comments, but must
perform the same operations in both languages.

Parameter definitions live in `NodeConfig.h` and `TeleArmNode.cpp`, CLI options in
`linkerta --help`, and protocol decoding in `ArmProtocol.cpp`. Run
`python3 tools/check_doc_links.py` after editing. It checks local file links and
language pairs, not remote availability or heading anchors.
