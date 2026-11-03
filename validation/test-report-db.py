"""Ensure dependency filtering never silently removes project translation units."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

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
            result = prepare.prepare(root, [database])
            self.assertEqual(len(result), 2)
            self.assertIn("-I", result[0]["arguments"])
            self.assertIn("/one", result[0]["arguments"])
            self.assertIn("-I/two", result[0]["arguments"])
            database.write_text(json.dumps(commands[1:]))
            with self.assertRaises(ValueError):
                prepare.prepare(root, [database])


if __name__ == "__main__":
    unittest.main()
