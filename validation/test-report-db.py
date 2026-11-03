"""Guard complete source coverage and real compiler-derived stdlib selection."""
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("prepare", Path(__file__).with_name("prepare-report-db.py"))
prepare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prepare)


class DatabaseTests(unittest.TestCase):
    def test_complete_source_coverage_and_include_preservation(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "src/linkerta/src"
            source.mkdir(parents=True)
            one, two = source / "one.cpp", source / "two.cpp"
            one.touch()
            two.touch()
            database = root / "db.json"
            commands = [{"directory": str(root), "file": str(path),
                         "arguments": ["clang++", "-isystem", "/one", "-isystem/two", "-c", str(path)]}
                        for path in (one, two, root / "_deps/vendor.cpp")]
            database.write_text(json.dumps(commands))
            with patch.object(prepare, "compiler_stdlib_macros", return_value=["-D_LIBCPP_VERSION=190102"]):
                result = prepare.prepare(root, [database])
                self.assertEqual(len(result), 2)
                self.assertIn("-I", result[0]["arguments"])
                self.assertIn("/one", result[0]["arguments"])
                self.assertIn("-I/two", result[0]["arguments"])
                self.assertIn("-D_LIBCPP_VERSION=190102", result[0]["arguments"])
                database.write_text(json.dumps(commands[1:]))
                with self.assertRaises(ValueError):
                    prepare.prepare(root, [database])

    def test_probe_retains_actual_target_stdlib_and_sysroot_flags(self):
        root = Path("/tmp")
        command = ["clang++-18", "--target=aarch64-linux-gnu", "-stdlib=libc++", "--sysroot=/sdk",
                   "-isysroot", "/sdk-two", "-std=c++17", "-isystem", "/third-party",
                   "-DREAL_TARGET=1", "-c", "/tmp/probe.cpp", "-o", "probe.o", "-MD",
                   "-MF", "probe.d", "-MT", "probe.o"]
        expected = command[:10] + ["-dM", "-E", "-x", "c++", "-"]
        self.assertEqual(prepare.preprocessing_command(command, root, root / "probe.cpp"), expected)

    def test_only_observed_standard_library_macros_are_injected(self):
        outputs = [
            ("#define __GLIBCXX__ 20240829\n#define _GLIBCXX_RELEASE 14\n#define OTHER 3\n",
             ["-D_GLIBCXX_RELEASE=14", "-D__GLIBCXX__=20240829"]),
            ("#define _LIBCPP_VERSION 190102\n", ["-D_LIBCPP_VERSION=190102"]),
        ]
        for stdout, expected in outputs:
            with self.subTest(stdout=stdout), patch.object(prepare.subprocess, "run") as run:
                run.return_value = subprocess.CompletedProcess([], 0, stdout, "")
                self.assertEqual(prepare.compiler_stdlib_macros(
                    ["clang++", "-c", "/tmp/probe.cpp"], Path("/tmp"), Path("/tmp/probe.cpp")), expected)
                self.assertEqual(run.call_args.kwargs["input"], "#include <functional>\n")

    def test_missing_ambiguous_or_invalid_macros_fail(self):
        for stdout in ("", "#define __GLIBCXX__ 123\n#define _LIBCPP_VERSION 456\n",
                       "#define _GLIBCXX_RELEASE 14\n", "#define _LIBCPP_VERSION (190000 + 2)\n"):
            with self.subTest(stdout=stdout), patch.object(prepare.subprocess, "run") as run:
                run.return_value = subprocess.CompletedProcess([], 0, stdout, "")
                with self.assertRaises(ValueError):
                    prepare.compiler_stdlib_macros(["clang++"], Path("/tmp"), Path("/tmp/probe.cpp"))
        with patch.object(prepare.subprocess, "run") as run:
            run.return_value = subprocess.CompletedProcess([], 1, "", "missing target sysroot")
            with self.assertRaisesRegex(ValueError, "missing target sysroot"):
                prepare.compiler_stdlib_macros(["clang++"], Path("/tmp"), Path("/tmp/probe.cpp"))

    def test_real_compiler_probe_identifies_the_installed_library(self):
        compiler = shutil.which("clang++-18") or shutil.which("clang++") or shutil.which("c++")
        self.assertIsNotNone(compiler, "A C++ compiler is required for report validation")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            definitions = prepare.compiler_stdlib_macros(
                [compiler, "-std=c++17", "-c", str(root / "probe.cpp"), "-o", str(root / "probe.o")],
                root, root / "probe.cpp")
            self.assertTrue(any(item.startswith(("-D_LIBCPP_VERSION=", "-D__GLIBCXX__="))
                                for item in definitions))
            self.assertFalse((root / "probe.o").exists())


if __name__ == "__main__":
    unittest.main()
