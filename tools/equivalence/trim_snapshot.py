#!/usr/bin/env python3
"""Trim a whole-game-state snapshot to the pages one target depends on.

capture_gametype_snapshot.py saves all of .data/.bss and the game-state
allocation (~13 MB of JSON), which is too large to commit.  This runs the
target's unicorn_diff invocation once with HALO_EQUIV_TOUCHED_PAGES set, so
every 4 KB snapshot page that the oracle or the candidate reads or writes (on
any seed, including real-callee code) is recorded.  It then writes a snapshot
that keeps only those pages, plus the original description, stub_returns and
arg_overrides, and re-runs the same invocation on the trimmed file.  The
trimmed snapshot is only written if both runs report identical verdict
counts, coverage and compared writes and calls.

    trim_snapshot.py oddball_engine_update \\
        --snapshot artifacts/equivalence/snapshots/oddball_t300_spawn.json \\
        --out tests/equivalence/snapshots/oddball_engine_update_spawn.json \\
        -- --allow-stubs --real-callees --trace-all-stubs --pinned-state \\
           --seeds 20 --mem-trace

Pages the target never touches are zero in the trimmed run, exactly as they
would be if they were unmapped, so a lift that starts reading new state shows
up as a divergence rather than silently seeing live data.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
UNICORN_DIFF = ROOT / "tools" / "equivalence" / "unicorn_diff.py"
PAGE = 0x1000

# Summary lines whose numbers must agree between the full and trimmed runs.
_SUMMARY_RE = re.compile(
    r"(.*RESULTS.*|.*[Cc]overage.*|.*matched.*|.*compared.*|.*mismatch.*"
    r"|.*pinned-state.*|.*vacuous.*)")


def run_diff(target: str, snapshot: Path, diff_args: list[str],
             pages_out: Path | None = None) -> tuple[int, list[str]]:
    env = dict(os.environ)
    if pages_out is not None:
        env["HALO_EQUIV_TOUCHED_PAGES"] = str(pages_out)
    argv = [sys.executable, str(UNICORN_DIFF), target, *diff_args,
            "--state-snapshot", str(snapshot)]
    r = subprocess.run(argv, capture_output=True, text=True, env=env, cwd=ROOT)
    out = (r.stdout or "") + (r.stderr or "")
    summary = [m.group(0).strip() for m in map(_SUMMARY_RE.fullmatch, out.splitlines()) if m]
    # Timings and file names differ run to run; compare numbers and words only.
    summary = [re.sub(r"\(?\d+\.\d+s\)?|/\S+\.json", "", s) for s in summary]
    return r.returncode, summary


def trim(snap: dict, pages: list[int]) -> dict:
    regions = {int(a, 16): bytes.fromhex(b) for a, b in snap["regions"].items()}
    keep: dict[int, bytearray] = {}
    last_end = None
    for page in pages:
        for base, blob in regions.items():
            lo = max(page, base)
            hi = min(page + PAGE, base + len(blob))
            if lo >= hi:
                continue
            chunk = blob[lo - base:hi - base]
            if last_end == lo:
                start = max(keep)
                keep[start] += chunk
            else:
                keep[lo] = bytearray(chunk)
            last_end = hi
    out = {k: v for k, v in snap.items() if k not in ("regions", "meta")}
    out["meta"] = dict(snap.get("meta", {}))
    out["meta"]["trimmed_from"] = [[hex(a), hex(len(b))] for a, b in regions.items()]
    out["meta"]["regions"] = [[hex(a), hex(len(b))] for a, b in keep.items()]
    out["regions"] = {hex(a): bytes(b).hex() for a, b in keep.items()}
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("target")
    ap.add_argument("--snapshot", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    argv = sys.argv[1:]
    split = argv.index("--") if "--" in argv else len(argv)
    args = ap.parse_args(argv[:split])
    diff_args = argv[split + 1:]

    with tempfile.TemporaryDirectory() as td:
        pages_file = Path(td) / "pages.json"
        rc_full, full = run_diff(args.target, args.snapshot, diff_args, pages_file)
        if not pages_file.exists():
            sys.exit("unicorn_diff recorded no pages (did the snapshot load?)")
        pages = json.loads(pages_file.read_text(encoding="utf-8"))
        snap = json.loads(args.snapshot.read_text(encoding="utf-8"))
        trimmed = trim(snap, pages)
        trial = Path(td) / "trimmed.json"
        trial.write_text(json.dumps(trimmed), encoding="utf-8")
        rc_trim, small = run_diff(args.target, trial, diff_args)

    if rc_full != rc_trim or full != small:
        print(f"MISMATCH: full rc={rc_full}, trimmed rc={rc_trim}")
        for a in full:
            print("  full   :", a)
        for b in small:
            print("  trimmed:", b)
        return 1
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(trimmed, indent=1), encoding="utf-8")
    size = sum(len(b) // 2 for b in trimmed["regions"].values())
    print(f"{len(pages)} page(s), {size:#x} bytes in {len(trimmed['regions'])} region(s) "
          f"-> {args.out} (rc={rc_trim}; results identical to the full snapshot)")
    for line in small:
        print("  ", line)
    return 0


if __name__ == "__main__":
    sys.exit(main())
