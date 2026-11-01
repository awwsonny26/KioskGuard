#!/usr/bin/env python3
"""Content address the actual uncommitted firmware; never substitute HEAD alone."""
from pathlib import Path
import hashlib
import subprocess
ROOT = Path(__file__).resolve().parents[1]

def inputs():
    for path in sorted((ROOT / "firmware").rglob("*")):
        if not path.is_file() or "build" in path.parts or "Drivers" in path.parts:
            continue
        if path.name != "version.h" and path.suffix in (".c", ".h", ".s", ".ld", ".ioc", ""):
            yield path
    for name in ("firmware/Makefile", "diagram.json", "wokwi.toml"):
        yield ROOT / name

def source_hash():
    digest = hashlib.sha256()
    for path in sorted(set(inputs())):
        digest.update(str(path.relative_to(ROOT)).encode() + b"\0")
        digest.update(path.read_bytes())
    return digest.hexdigest()

if __name__ == "__main__":
    base = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"], cwd=ROOT, text=True).strip()
    value = base + "-tree-" + source_hash()[:16]
    path = ROOT / "firmware/Platform/version.h"
    text = '#define KG_VERSION "' + value + '"\n'
    if not path.exists() or path.read_text() != text:
        path.write_text(text)
    print(value)
