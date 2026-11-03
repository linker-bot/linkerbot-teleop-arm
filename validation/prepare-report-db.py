"""Keep every project translation unit and reject incomplete analysis databases."""
import argparse
import json
from pathlib import Path
import shlex


def prepare(root, databases):
    source_root = (root / "src/linkerta").resolve()
    expected = {path.resolve() for path in source_root.rglob("*.cpp")}
    if not expected:
        raise ValueError("No project C++ translation units were found")
    entries = {}
    for database in databases:
        for original in json.loads(database.read_text()):
            directory = Path(original["directory"]).resolve()
            source = Path(original["file"])
            if not source.is_absolute():
                source = directory / source
            source = source.resolve()
            if not source.is_relative_to(source_root):
                continue
            command = original.get("arguments") or shlex.split(original["command"])
            # Cppcheck 2.13 does not resolve every -isystem entry in compile DBs.
            # Preserve each include directory; do not drop external headers.
            command = ["-I" if item == "-isystem" else
                       "-I" + item[len("-isystem"):] if item.startswith("-isystem") else item
                       for item in command]
            entry = {"directory": str(directory), "file": str(source), "arguments": command}
            # The same source can occur in more than one identical target build.
            # Distinct command variants are retained, not silently deduplicated.
            key = (str(source), tuple(command))
            entries[key] = entry
    found = {Path(entry["file"]) for entry in entries.values()}
    missing = expected - found
    if missing:
        raise ValueError("Missing project translation units: " +
                         ", ".join(str(path.relative_to(root)) for path in sorted(missing)))
    return sorted(entries.values(), key=lambda entry: (entry["file"], entry["arguments"]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("databases", nargs="+", type=Path)
    args = parser.parse_args()
    entries = prepare(args.root.resolve(), args.databases)
    args.output.write_text(json.dumps(entries, indent=2) + "\n")
    print(f"Prepared {len(entries)} commands for {len({entry['file'] for entry in entries})} "
          "project translation units (production, ROS2, CLI, benchmark and tests)")


if __name__ == "__main__":
    main()
