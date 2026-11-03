"""Verify actual CSV/metadata accounting through the real Session/FakeTransport path."""
import csv
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile


def check(executable, path, drop_every):
    command = [executable, "--simulate", "--count", "8", "--rate-hz", "50",
               "--timeout-ms", "1000", "--output", str(path)]
    if drop_every:
        command += ["--drop-every", str(drop_every)]
    result = subprocess.run(command, text=True, capture_output=True, timeout=20)
    expected_failures = 8 // drop_every if drop_every else 0
    assert result.returncode == (1 if expected_failures else 0), result.stderr + result.stdout
    rows = list(csv.DictReader(path.open()))
    metadata = json.loads(Path(str(path) + ".metadata.json").read_text())
    assert len(rows) == 8
    assert metadata["synthetic"] is True and metadata["complete"] is True
    successes = [row for row in rows if row["success"] == "1"]
    assert len(successes) == 8 - expected_failures
    assert metadata["successful_count"] == len(successes)
    assert metadata["failed_count"] == expected_failures
    assert metadata["performed_count"] == len(rows)
    assert metadata["success_ratio"] == len(successes) / len(rows)
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


with tempfile.TemporaryDirectory() as temporary:
    check(sys.argv[1], Path(temporary) / "normal.csv", 0)
    check(sys.argv[1], Path(temporary) / "missing.csv", 3)
