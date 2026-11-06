# Release notes

## 2.0.1

- Require an installed transport package compatible with v0.3.4 and pin source fallback to its release commit, `0e190032d65ed0b315337b45b3779cf4f650ed68`.
- Verify the published v0.3.4 Ubuntu x86_64 and ARM64 SDKs by checksum, then build and test with dependency downloads disabled.
- Update installation, Session and operation documentation. The classic CAN protocol, Session ownership, scheduling, control parameters and ROS2 topic units are unchanged by this dependency update.

Rebuild the driver and downstream consumers with matching transport headers and libraries in a separate installation prefix. Transport v0.3.4 adds public structure fields; binary ABI compatibility with earlier transport SDKs is not promised. The installed-package lookup permits compatible newer patches; record the actual dependency found.

This is a source release. Build/test and analysis evidence covers the release source; source archives are not prebuilt ROS2 or device binaries. FakeTransport, virtual CAN, ROS2 tests and synthetic benchmarks do not establish electrical behavior, physical ACK, bus-off recovery, device throughput/latency or a physical-device soak. No new physical hardware acceptance is claimed. Firmware-specific reply details and calibration completion still require device documentation and captures.
