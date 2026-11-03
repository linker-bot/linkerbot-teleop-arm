"""Exercise a launched production node with only the transport replaced by a test fixture."""
import math
import time
import unittest

import launch
import launch_testing
import launch_testing.actions
import launch_testing.asserts
import pytest
import rclpy
from diagnostic_msgs.msg import DiagnosticArray, DiagnosticStatus
from sensor_msgs.msg import JointState
from std_msgs.msg import UInt32
from std_srvs.srv import SetBool, Trigger


@pytest.mark.launch_test
def generate_test_description():
    fixture = launch.actions.ExecuteProcess(
        cmd=[launch.substitutions.LaunchConfiguration("fixture_path"),
             "--ros-args", "-p", "poll_rate_hz:=20.0",
             "-p", "request_timeout_ms:=30", "-p", "max_snapshot_age_ms:=150",
             "-p", "negation:=[1,1,1,1,1,1,1,1,1,1,1,1,1,1]"],
        output="screen",
    )
    return launch.LaunchDescription([
        launch.actions.DeclareLaunchArgument("fixture_path"),
        fixture,
        launch_testing.actions.ReadyToTest(),
    ]), {"fixture": fixture}


class TestLaunchedNode(unittest.TestCase):
    def test_publication_timeout_and_disabled_calibration(self):
        rclpy.init()
        node = rclpy.create_node("launch_contract_observer")
        samples, legacy, diagnostics, calibration_counts = [], [], [], []
        subscriptions = [
            node.create_subscription(JointState, "/joint_states", samples.append, 100),
            node.create_subscription(JointState, "/left_arm_joint_control", legacy.append, 100),
            node.create_subscription(DiagnosticArray, "/diagnostics", diagnostics.append, 100),
            node.create_subscription(UInt32, "/test_calibration_count", calibration_counts.append, 10),
        ]

        def spin_until(predicate, timeout=5.0):
            deadline = time.monotonic() + timeout
            while not predicate() and time.monotonic() < deadline:
                rclpy.spin_once(node, timeout_sec=0.02)
            self.assertTrue(predicate())

        def spin_for(duration):
            deadline = time.monotonic() + duration
            while time.monotonic() < deadline:
                rclpy.spin_once(node, timeout_sec=0.02)

        try:
            spin_until(lambda: len(samples) >= 6 and len(legacy) >= 6 and calibration_counts)
            self.assertEqual(len(samples[-1].position), 14)
            self.assertTrue(all(abs(p - math.pi / 2) < 1e-6 for p in samples[-1].position))
            self.assertTrue(all(p == 90.0 for p in legacy[-1].position))
            self.assertTrue(any(a.header.stamp == b.header.stamp for a in samples for b in legacy))
            before = len(samples)
            spin_for(0.5)
            self.assertGreater(len(samples) - before, 1)
            self.assertLessEqual(len(samples) - before, 25)
            self.assertTrue(all(count.data == 0 for count in calibration_counts))

            calibrate = node.create_client(Trigger, "/linkerta_node/calibrate_once")
            self.assertTrue(calibrate.wait_for_service(timeout_sec=5.0))
            result = calibrate.call_async(Trigger.Request())
            spin_until(result.done)
            self.assertFalse(result.result().success)
            spin_for(0.1)
            self.assertEqual(calibration_counts[-1].data, 0)

            control = node.create_client(SetBool, "/test_respond")
            self.assertTrue(control.wait_for_service(timeout_sec=5.0))
            stop = control.call_async(SetBool.Request(data=False))
            spin_until(stop.done)
            diagnostics.clear()
            spin_until(lambda: any(status.level == DiagnosticStatus.ERROR
                                   for msg in diagnostics for status in msg.status))
            spin_for(0.1)  # Drain a successful result already queued when the fault was injected.
            stopped_count = len(samples)
            spin_for(0.3)
            self.assertEqual(len(samples), stopped_count)
        finally:
            for subscription in subscriptions:
                node.destroy_subscription(subscription)
            node.destroy_node()
            rclpy.shutdown()


@launch_testing.post_shutdown_test()
class TestFixtureExit(unittest.TestCase):
    def test_clean_shutdown(self, proc_info, fixture):
        launch_testing.asserts.assertExitCodes(proc_info, process=fixture)
