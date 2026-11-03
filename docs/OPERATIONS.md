[English](OPERATIONS.md) | [简体中文](zh/OPERATIONS.md)

# Upgrades, rollback, and issue reports

[Documentation](README.md) · [Troubleshooting](TROUBLESHOOTING.md) · [Validation](VALIDATION.md)

## Record versions and installation sources

Before upgrading, record the application commit, CMake configuration, compiler, ROS distribution, and transport installation source. The source fallback pins a transport commit in CMake. `find_package(linkerbot-comm 0.3)` instead uses an installed package and checks only a minimum version; that package need not match the fallback commit. Confirm the package actually found and run installed-consumer and integration tests.

Install the new build into a separate prefix to avoid mixing library and header versions. Stop the old node, switch installation environments, then start one instance and check its version, diagnostics, and fresh snapshots. Roll back using the entire previous prefix and matching configuration, not a single binary. The program does not configure interface bitrate or calibrate automatically after restarting.

ROS2 parameters are read-only startup configuration. Identify whether they come from the installation or an external `--params-file`; save a copy and test it with the new version. Legacy arm topics use degrees, while `/joint_states` uses radians. When migrating consumers, account for units as well as topic names.

## Disconnection and shutdown

A device may already have executed a command when the request times out. Stop new operations, then let the Session owner decide whether to close or explicitly recover it. Do not retry calibration in a failure loop. Reconstruct old arms after recovering a shared Session. During shutdown, stop callers first, destroy the arm, then close the Session owned by the application.

The default launch file restarts exited processes but does not manage connection recovery for the application. If a process repeatedly exits, inspect its startup error, parameters, and interface state.

## Information for reproduction

Include the following in an issue report:

- Application and dependency commits, build command, platform/architecture, compiler, and ROS2 version.
- Device, firmware, adapter, interface, bitrate, and whether the Session is shared.
- Minimal reproduction steps, expected and actual behavior, exit code, and original error text.
- `/diagnostics`, Session/route state, configuration, and raw CAN captures for the relevant time window.
- For performance issues, CSV and metadata identifying synthetic, vcan, or physical-device data.

A simulation result does not rule out a hardware cause for an issue seen on one device. Preserve a minimal FakeTransport reproduction when possible. For failing tests, include the exact assertion and full failure log so code and environment failures can be distinguished.
