"""Verify actual CSV/metadata accounting through the real Session/FakeTransport path."""
import csv
import json
import math
import re
import signal
from pathlib import Path
import subprocess
import sys
import tempfile
import time


def check(executable, path, drop_every):
    command = [executable, "--simulate", "--count", "9", "--rate-hz", "50",
               "--timeout-ms", "1000", "--output", str(path)]
    if drop_every:
        command += ["--drop-every", str(drop_every)]
    result = subprocess.run(command, text=True, capture_output=True, timeout=20)
    expected_failures = 9 // drop_every if drop_every else 0
    assert result.returncode == (1 if expected_failures else 0), result.stderr + result.stdout
    rows = list(csv.DictReader(path.open()))
    metadata = json.loads(Path(str(path) + ".metadata.json").read_text())
    assert len(rows) == 9
    assert metadata["synthetic"] is True and metadata["complete"] is True
    successes = [row for row in rows if row["success"] == "1"]
    assert len(successes) == 9 - expected_failures
    assert metadata["successful_count"] == len(successes)
    assert metadata["failed_count"] == expected_failures
    assert metadata["performed_count"] == len(rows)
    assert metadata["success_ratio"] == len(successes) / len(rows)
    summary_ratio = re.search(r"success_ratio=([0-9.e+-]+)", result.stdout)
    assert summary_ratio and float(summary_ratio.group(1)) == len(successes) / len(rows)
    durations = sorted(int(row["duration_us"]) for row in successes)
    for name, percentile in [("p50_us", 0.50), ("p95_us", 0.95), ("p99_us", 0.99)]:
        assert metadata[name] == durations[math.ceil(percentile * len(durations)) - 1]
    for row in rows:
        if row["success"] == "0":
            assert row["sequence"] == "" and row["oldest_age_us"] == ""
            assert row["error"]
        else:
            assert int(row["sequence"]) > 0
            assert int(row["oldest_age_us"]) >= int(row["skew_us"]) >= 0
    # Explicit outputs must not silently replace earlier measurements.
    original = path.read_bytes()
    repeated = subprocess.run(command, text=True, capture_output=True, timeout=5)
    assert repeated.returncode == 2
    assert path.read_bytes() == original


def check_interruption(executable, path):
    requested = 100000
    process = subprocess.Popen(
        [executable, "--simulate", "--count", str(requested), "--rate-hz", "20",
         "--timeout-ms", "1000", "--output", str(path)],
        text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    metadata_path = Path(str(path) + ".metadata.json")
    try:
        # Wait for the installed signal handler, initial metadata, and a flushed
        # complete row. Sending SIGTERM before main starts would not test cleanup.
        deadline = time.monotonic() + 10
        ready = False
        while time.monotonic() < deadline:
            assert process.poll() is None, "benchmark exited before interruption"
            if path.exists() and metadata_path.exists():
                try:
                    initial = json.loads(metadata_path.read_text())
                    with path.open() as stream:
                        recorded = list(csv.DictReader(stream))
                    ready = initial["complete"] is False and len(recorded) >= 1
                except (json.JSONDecodeError, OSError):
                    ready = False
                if ready:
                    break
            time.sleep(0.01)
        assert ready, "benchmark did not produce a row before the deadline"
        process.send_signal(signal.SIGTERM)
        stdout, stderr = process.communicate(timeout=5)
        assert process.returncode == 128 + signal.SIGTERM, stdout + stderr
        with path.open() as stream:
            rows = list(csv.DictReader(stream))
        metadata = json.loads(metadata_path.read_text())
        assert metadata["synthetic"] is True and metadata["complete"] is False
        assert 0 < len(rows) < requested
        assert metadata["performed_count"] == len(rows)
        successes = [row for row in rows if row["success"] == "1"]
        assert metadata["successful_count"] == len(successes)
        assert metadata["failed_count"] == len(rows) - len(successes)
        assert metadata["success_ratio"] == len(successes) / len(rows)
        assert [int(row["request"]) for row in rows] == list(range(1, len(rows) + 1))
        sequences = [int(row["sequence"]) for row in successes]
        assert sequences == sorted(set(sequences))  # Never repeat cached success.
        for row in rows:
            if row["success"] == "1":
                assert int(row["sequence"]) > 0 and not row["error"]
                assert int(row["oldest_age_us"]) >= int(row["skew_us"]) >= 0
            else:
                assert not row["sequence"] and not row["oldest_age_us"] and row["error"]
    finally:
        if process.poll() is None:
            process.kill()
            process.communicate(timeout=5)


with tempfile.TemporaryDirectory() as temporary:
    check(sys.argv[1], Path(temporary) / "normal.csv", 0)
    check(sys.argv[1], Path(temporary) / "missing.csv", 3)
    check_interruption(sys.argv[1], Path(temporary) / "interrupted.csv")
