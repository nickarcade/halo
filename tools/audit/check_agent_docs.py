#!/usr/bin/env python3
"""Check the always-loaded agent instructions in AGENTS.md.

CLAUDE.md is not tracked. Where a local copy exists (normally a symlink to
AGENTS.md), it must read identically so both agents see the same rules.
"""

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parent.parent.parent
AGENTS = ROOT / "AGENTS.md"
CLAUDE = ROOT / "CLAUDE.md"
MAX_LINES = 300


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def line_count(text: str) -> int:
    return len(text.splitlines())


def main() -> int:
    if not AGENTS.exists():
        print(f"Missing required file: {AGENTS}")
        return 1

    agents_text = read_text(AGENTS)

    failed = False

    lines = line_count(agents_text)
    if lines > MAX_LINES:
        print(f"{AGENTS.name} has {lines} lines (limit: {MAX_LINES}).")
        failed = True

    if CLAUDE.exists() and read_text(CLAUDE) != agents_text:
        print("Local CLAUDE.md differs from AGENTS.md; make it a symlink:")
        print("  ln -sf AGENTS.md CLAUDE.md")
        failed = True

    if failed:
        return 1

    print(f"OK: AGENTS.md is <= {MAX_LINES} lines.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
