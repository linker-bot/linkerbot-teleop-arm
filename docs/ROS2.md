[English](ROS2.md) | [简体中文](zh/ROS2.md)

# Using the ROS2 node

[Documentation](README.md) · [Deployment](INSTALL.md) · [Troubleshooting](TROUBLESHOOTING.md)

CI uses ROS2 Humble; other distributions have not received equivalent validation. The default node name is `linkerta_node`. A single-threaded executor handles timers, parameters, and services; a separate worker performs potentially blocking CAN requests.

## Startup and configuration

Build and install from the repository root:

```sh
source /opt/ros/humble/setup.bash
./build.sh ros2
./startup.sh
```

The launch file loads `share/linkerta/config/lta.yaml` from the installation and, by default, restarts an exited process after three seconds. To use an external configuration without recompiling:

```sh
source install/setup.bash
ros2 run linkerta linkerta_node --ros-args --params-file /absolute/path/lta.yaml
```

Launch does not follow changes to the source YAML. Update the installed copy or reinstall it, then restart the node. These parameters are validated before opening hardware and cannot change at runtime:

- `device="master_arm"`: used for discovery when `can_interface` is empty; ambiguous candidates are rejected.
- `can_interface=""`: a nonempty value selects a preconfigured interface; it does not configure bitrate.
- `poll_rate_hz=100.0`: finite, 1..200; a scheduling upper bound, not guaranteed device throughput.
- `request_timeout_ms=250`, `max_snapshot_age_ms=250`: each 1..10000 ms.
- `negation=[-1,-1,-1,-1,-1,-1,-1,-1,1,-1,1,-1,-1,-1]`: 14 entries, each -1 or 1, applied to ROS2 output.
- `publish_legacy_topics=true`: controls both legacy arm topics and the legacy fault string.
- `calibration=0`: only 0 is allowed; startup calibration is prohibited.
- `allow_calibration=false`: permits explicit service requests when enabled; does not initiate calibration.
- `calibration_joint=255`: 0..13 or 255, used by the one-shot service.

## Topics and units

- `/joint_states`, `sensor_msgs/msg/JointState`: 14 joints, positions in radians.
- `/left_arm_joint_control` and `/right_arm_joint_control`, the same type: seven joints each, still in degrees. The legacy word “control” does not imply commands; these topics publish measured state.
- `/joint_error_code`, `std_msgs/msg/String`: legacy fault text; missing bits are `unknown`.
- `/diagnostics`, `diagnostic_msgs/msg/DiagnosticArray`: completeness, staleness, timeouts, fault validity, snapshot sequence/generation, and calibration send status.

Left joint names are `joint61..joint67`; right names are `joint51..joint57`. No derived velocity or torque is published. Topic names are absolute in the source: multiple-node deployments need explicit remapping; a namespace alone does not change them.

Positions are published only for a new, complete, fresh snapshot, once per result. Missing frames, timeouts, and stale results update diagnostics instead. The three position messages from one snapshot share a stamp. This stamp subtracts the latest constituent frame's steady-clock age from ROS now at publication, approximating host reception time, **not device sampling time**. A paused, scaled, or jumping simulation clock affects the mapping; negative values are clamped to zero. Diagnostics use ROS now at their publication time.

## One-shot calibration service

`~/calibrate_once` uses `std_srvs/srv/Trigger` and is disabled by default. After manual confirmation, set `allow_calibration=true` at startup and call it explicitly:

```sh
ros2 service call /linkerta_node/calibrate_once std_srvs/srv/Trigger '{}'
```

Once the service accepts a request, that process cannot attempt calibration again, even if sending fails. Here, `success=false` includes “queued, but device execution cannot be confirmed”; read `message` and diagnostics for send status. No device-completion ACK has been implemented and verified, so the boolean alone cannot tell whether calibration ran. Restarting resets the process-local attempt guard but does not calibrate automatically. External callers must not retry automatically because the response is false.
