#!/usr/bin/env python3
"""Check tracked and new Markdown relative file links offline (not remote URLs/anchors)."""
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
if errors:
    print("\n".join(errors), file=sys.stderr)
    sys.exit(1)
print(f"Checked local file links in {len(set(paths))} Markdown files (remote URLs and anchors excluded).")
