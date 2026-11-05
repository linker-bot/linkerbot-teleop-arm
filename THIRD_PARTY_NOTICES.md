# Third-party notices

English · [简体中文](THIRD_PARTY_NOTICES.zh-CN.md)

LinkerBot Teleoperation Arm is licensed under [Apache-2.0](LICENSE). Dependencies retain
their own licenses and copyright notices.

## linkerbot-transport-cpp

- Role: runtime transport library, linked by `linkerta::core`.
- License: [MIT](LICENSES/linkerbot-transport-cpp.MIT.txt).
- Upstream notice: [NOTICE](LICENSES/linkerbot-transport-cpp.NOTICE.txt).
- Pinned fallback source: [e514b832e09ecd60a56d2c67e39c54285793fff8](https://github.com/soulteary/linkerbot-transport-cpp/tree/e514b832e09ecd60a56d2c67e39c54285793fff8).

The files above are copied verbatim from that revision. An installed transport
package may differ from the fallback revision; keep the notices supplied with
the package actually redistributed. Transport's own tests and optional vendor
binaries are not included by this project's fallback build.

## GoogleTest

- Role: test framework; not linked into the installed driver or core library.
- License: [BSD-3-Clause](LICENSES/GoogleTest.BSD-3-Clause.txt).
- Pinned fallback source: [v1.14.0, f8d7d77c06936315286eb55f8de22cd23c188571](https://github.com/google/googletest/tree/f8d7d77c06936315286eb55f8de22cd23c188571).

The license file is copied verbatim from that revision. Keep it when distributing
GoogleTest sources or test binaries that contain GoogleTest.

## Installed files

CMake installs this file, the project LICENSE and the LICENSES directory under
`<prefix>/share/licenses/linkerta` (or the configured GNUInstallDirs data path).
Keep that directory with binary distributions. ROS2, system libraries and any
separately supplied packages keep their own notices; this directory is not an
inventory of every package in the build machine or operating system.
