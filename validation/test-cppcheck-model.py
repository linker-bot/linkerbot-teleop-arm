"""Fail if Cppcheck's GoogleTest model loses test bodies or assertion operands."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import tempfile
import xml.etree.ElementTree as ET


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compile-commands", required=True, type=Path)
    args = parser.parse_args()
    database = json.loads(args.compile_commands.read_text())
    template = next((entry for entry in database
                     if Path(entry["file"]).name == "test_runtime.cpp"), None)
    if template is None:
        raise ValueError("The analysis database must include the real GoogleTest runtime tests")
    command = template.get("arguments") or shlex.split(template["command"])
    cases = {
        "test_body": ('TEST(Probe, Body) { int *p = nullptr; int n = *p; (void)n; }', "nullPointer"),
        "expect_operand": ('TEST(Probe, Operand) { int *p = nullptr; EXPECT_EQ(*p, 1); }', "nullPointer"),
        "assert_operand": ('TEST(Probe, Operand) { int *p = nullptr; ASSERT_EQ(*p, 1); }', "nullPointer"),
        "nonfatal_continues": ('TEST(Probe, Continued) { int *p = nullptr; EXPECT_NE(p, nullptr); int n = *p; (void)n; }', "nullPointer"),
        "fatal_stops": ('TEST(Probe, Stops) { int *p = nullptr; ASSERT_NE(p, nullptr); int n = *p; (void)n; }', None),
        "stream_operand": ('TEST(Probe, Stream) { int *p = nullptr; EXPECT_TRUE(false) << *p; }', "nullPointer"),
        "exception_operand": ('TEST(Probe, Exception) { int *p = nullptr; EXPECT_THROW((void)*p, int); }', "nullPointer"),
        "array_operand": ('TEST(Probe, Array) { int n[1] = {0}; EXPECT_EQ(n[2], 0); }', "arrayIndexOutOfBounds"),
        "fixture_body": ('class Fixture : public testing::Test { protected: int *member = nullptr; };\nTEST_F(Fixture, Member) { int n = *member; (void)n; }', "nullPointer"),
        "later_body": ('TEST(Probe, First) { EXPECT_TRUE(true); }\nTEST(Probe, Second) { int *p = nullptr; int n = *p; (void)n; }', "nullPointer"),
    }
    wrapper = Path(__file__).with_name("cppcheck-report.sh").resolve()
    with tempfile.TemporaryDirectory(prefix="linkerta-google-model-") as temporary:
        directory = Path(temporary)
        for name, (body, expected) in cases.items():
            source = directory / f"{name}.cpp"
            source.write_text('#include <gtest/gtest.h>\n' + body + '\n')
            entry = {**template, "file": str(source)}
            entry.pop("command", None)
            entry["arguments"] = [str(source) if value == template["file"] else value
                                  for value in command]
            manifest = directory / "compile_commands.json"
            manifest.write_text(json.dumps([entry]))
            result = subprocess.run([str(wrapper), f"--project={manifest}",
                                     "--enable=warning,style,performance,portability", "--xml", "--xml-version=2"],
                                    capture_output=True, text=True, check=False)
            if result.returncode:
                raise RuntimeError(f"{name}: Cppcheck tool failed: {result.stderr}")
            diagnostics = ET.fromstring(result.stderr).findall("./errors/error")
            ids = {item.attrib["id"] for item in diagnostics}
            unexpected = {item.attrib["id"] for item in diagnostics
                          if item.attrib.get("severity") == "error" and item.attrib["id"] != expected}
            if unexpected:
                raise AssertionError(f"{name}: unexpected errors: {unexpected}")
            if expected is not None and expected not in ids:
                raise AssertionError(f"{name}: expected {expected}, got {ids}")
            if expected is None and ids & {"nullPointer", "arrayIndexOutOfBounds"}:
                raise AssertionError(f"{name}: fatal assertion control flow was lost: {ids}")
            print(f"PASS {name}")


if __name__ == "__main__":
    main()
