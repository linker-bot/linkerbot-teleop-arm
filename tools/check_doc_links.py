#!/usr/bin/env python3
"""Check local Markdown file links, language pairs and executable examples offline."""
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import unquote, urlsplit

root = Path(__file__).resolve().parents[1]
paths = subprocess.check_output(
    ["git", "ls-files", "--cached", "--others", "--exclude-standard", "--", "*.md"],
    cwd=root, text=True,
).splitlines()
errors = []
for name in sorted(set(paths)):
    path = root / name
    content = re.sub(r"```.*?```", "", path.read_text(encoding="utf-8"), flags=re.S)
    for match in re.finditer(r"!?\[[^\]]*\]\(([^)]+)\)", content):
        target = match.group(1).strip()
        if target.startswith("<") and ">" in target:
            target = target[1:target.index(">")]
        else:
            target = target.split(' "', 1)[0]
        parsed = urlsplit(target)
        if parsed.scheme or parsed.netloc or not parsed.path:
            continue
        destination = (path.parent / unquote(parsed.path)).resolve()
        if not destination.exists():
            line = content.count("\n", 0, match.start()) + 1
            errors.append(f"{name}:{line}: missing local target {target}")
def examples(path):
    blocks = re.findall(r"```[^\n]*\n(.*?)```", path.read_text(encoding="utf-8"), re.S)
    # Translated full-line comments may differ; commands and C++ directives may not.
    return ["\n".join(line for line in block.splitlines()
                      if not line.lstrip().startswith(("//", "# ")))
            for block in blocks]

pairs = [(root / "README.md", root / "README.zh-CN.md"),
         (root / "THIRD_PARTY_NOTICES.md", root / "THIRD_PARTY_NOTICES.zh-CN.md"),
         (root / "src/linkerta/test/README.md", root / "src/linkerta/test/README.zh-CN.md")]
for english in sorted((root / "docs").glob("*.md")):
    pairs.append((english, root / "docs/zh" / english.name))
for chinese in sorted((root / "docs/zh").glob("*.md")):
    if not (root / "docs" / chinese.name).is_file():
        errors.append(f"Orphan Chinese guide: {chinese.relative_to(root)}")
for english, chinese in pairs:
    if not english.is_file() or not chinese.is_file():
        errors.append(f"Missing language pair: {english.relative_to(root)} / {chinese.relative_to(root)}")
        continue
    for source, target in [(english, chinese), (chinese, english)]:
        # Check the actual relative destination, not the display language label.
        links = re.findall(r"\[[^\]]*\]\(([^)]+)\)", source.read_text(encoding="utf-8"))
        if not any((source.parent / link).resolve() == target.resolve() for link in links
                   if not urlsplit(link).scheme and not link.startswith("#")):
            errors.append(f"Missing language switch: {source.relative_to(root)} -> {target.relative_to(root)}")
    if examples(english) != examples(chinese):
        errors.append(f"Examples differ between {english.relative_to(root)} and {chinese.relative_to(root)}")

if errors:
    print("\n".join(errors), file=sys.stderr)
    sys.exit(1)
print(f"Checked {len(set(paths))} Markdown files and {len(pairs)} language pairs; remote URLs and anchors excluded.")
