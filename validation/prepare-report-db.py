"""Keep every project translation unit and reject incomplete analysis databases."""
import argparse
import json
from pathlib import Path
import shlex
import re
import subprocess


STDLIB_MACROS = {"_LIBCPP_VERSION", "_GLIBCXX_RELEASE", "__GLIBCXX__"}


def preprocessing_command(command, directory, source):
    """Use the original target's compiler/options without writing object/dependency files."""
    source = source.resolve()
    result = [command[0]]
    consumes_value = {"-o", "--output", "-MF", "-MT", "-MQ", "-MJ"}
    skip = False
    for item in command[1:]:
        if skip:
            skip = False
            continue
        if item in consumes_value:
            skip = True
            continue
        if item in {"-c", "-S", "-E", "-MD", "-MMD", "-M", "-MM", "-MG", "-MP", "-fsyntax-only"}:
            continue
        if item.startswith(("-o", "-MF", "-MT", "-MQ", "-MJ")):
            continue
        candidate = Path(item)
        if not candidate.is_absolute():
            candidate = directory / candidate
        if candidate.resolve() == source:
            continue
        # Includes, preprocessor definitions, --sysroot/-isysroot, --target,
        # -stdlib and -std remain exactly as used to compile this translation unit.
        result.append(item)
    if skip:
        raise ValueError("Compilation command ends with an option missing its value")
    return result + ["-dM", "-E", "-x", "c++", "-"]


def compiler_stdlib_macros(command, directory, source):
    probe = preprocessing_command(command, directory, source)
    result = subprocess.run(probe, input="#include <functional>\n", cwd=directory,
                            capture_output=True, text=True, check=False, timeout=60)
    if result.returncode:
        raise ValueError("Cannot determine standard library from the actual compiler: " + result.stderr)
    definitions = {}
    for line in result.stdout.splitlines():
        match = re.fullmatch(r"#define\s+(\w+)\s+(.+)", line)
        if match and match[1] in STDLIB_MACROS:
            value = match[2].strip()
            if not value.isdecimal():
                raise ValueError(f"Unsupported standard-library macro {match[1]}={value!r}")
            definitions[match[1]] = value
    libcxx = "_LIBCPP_VERSION" in definitions
    libstdcxx = "__GLIBCXX__" in definitions
    if libcxx == libstdcxx:
        raise ValueError("Expected exactly one identifiable libc++ or libstdc++ from <functional>")
    if "_GLIBCXX_RELEASE" in definitions and not libstdcxx:
        raise ValueError("Inconsistent standard-library version macros")
    return [f"-D{name}={value}" for name, value in sorted(definitions.items())]


def prepare(root, databases):
    source_root = (root / "src/linkerta").resolve()
    expected = {path.resolve() for path in source_root.rglob("*.cpp")}
    if not expected:
        raise ValueError("No project C++ translation units were found")
    entries = {}
    macros_by_probe = {}
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
            probe = tuple(preprocessing_command(command, directory, source))
            key = (str(directory), probe)
            if key not in macros_by_probe:
                macros_by_probe[key] = compiler_stdlib_macros(command, directory, source)
            # Cppcheck models standard headers instead of retaining implementation
            # version macros. Reintroduce only values observed from this exact
            # compiler/stdlib/sysroot context, never guessed version numbers.
            command = command + macros_by_probe[key]
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
