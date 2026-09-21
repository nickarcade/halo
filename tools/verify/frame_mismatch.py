"""Rank lifted functions by prologue frame-size mismatch against the raw XBE.

Byte accuracy is dominated by the prologue: a function whose `sub esp, N`
differs from the original has every later stack offset shifted, so the rest of
the body cannot match however correct its logic is.  Measured across the
repository, functions whose frame size differs sit near 11% byte accuracy while
the overall median is above 80%.

The frame size is a property of the source's local variables, so this tool does
not fix anything by itself.  It produces the worklist: which functions differ,
by how much, and which direction, so the lift can be corrected.

Reads the records written by ``raw_xbe_structural.py populate``; it never
recompiles and never writes to them.
"""

import argparse
import json
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))

import raw_xbe_structural as structural
import xbe_reference as xref

RECORDS = ROOT / "artifacts" / "raw_xbe_structural"
PROLOGUE_INSTRUCTIONS = 8


def frame_size(code, base):
    """Return the first `sub esp, imm` in the prologue, or None.

    Only the first few instructions are considered: a later `sub esp` is a
    dynamic allocation (alloca, a variable-length copy), not the frame.
    """
    try:
        from capstone import CS_ARCH_X86, CS_MODE_32, Cs
    except ImportError:
        return None
    decoder = Cs(CS_ARCH_X86, CS_MODE_32)
    for index, instruction in enumerate(decoder.disasm(bytes(code), base)):
        if index > PROLOGUE_INSTRUCTIONS:
            break
        if instruction.mnemonic != "sub" or not instruction.op_str.startswith("esp,"):
            continue
        try:
            return int(instruction.op_str.split(",")[1].strip(), 0)
        except ValueError:
            return None
    return None


def survey(limit=None):
    """Compare candidate and reference frame sizes for every scored record."""
    rows = []
    counts = {"records": 0, "both_have_frame": 0, "same": 0, "different": 0}
    for path in sorted(RECORDS.glob("*.json")):
        if path.name == "summary.json":
            continue
        try:
            record = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            continue
        if record.get("byte_accuracy") is None:
            continue
        counts["records"] += 1
        if limit and counts["records"] > limit:
            break
        candidate_obj = pathlib.Path((record.get("candidate") or {}).get("path", ""))
        if not candidate_obj.is_file():
            continue
        try:
            candidate, _relocs, _prov = structural._parse_coff(candidate_obj, record["function"])
        except Exception:
            continue
        address = int(record["address"], 16)
        reference, _error = xref.function_bytes(address)
        if not reference:
            continue
        ours = frame_size(candidate, 0)
        theirs = frame_size(reference, address)
        if ours is None or theirs is None:
            continue
        counts["both_have_frame"] += 1
        if ours == theirs:
            counts["same"] += 1
            continue
        counts["different"] += 1
        rows.append({
            "function": record["function"], "address": record["address"],
            "our_frame": ours, "original_frame": theirs, "delta": ours - theirs,
            "byte_accuracy": record["byte_accuracy"],
            "register_argument": bool(record.get("register_argument")),
            "source": (record.get("source") or {}).get("path"),
        })
    rows.sort(key=lambda row: (-abs(row["delta"]), row["byte_accuracy"]))
    return rows, counts


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--limit", type=int, default=None,
                        help="stop after this many records (for a quick sample)")
    parser.add_argument("--top", type=int, default=25, help="rows to print")
    parser.add_argument("--json", type=pathlib.Path, help="write the full worklist here")
    args = parser.parse_args(argv)

    rows, counts = survey(args.limit)
    if not counts["both_have_frame"]:
        print("no records with a comparable prologue frame; run populate first")
        return 1
    print("records scanned          : %d" % counts["records"])
    print("both sides have a frame  : %d" % counts["both_have_frame"])
    print("frame size matches       : %d" % counts["same"])
    print("frame size DIFFERS       : %d (%.0f%%)" % (
        counts["different"], counts["different"] / counts["both_have_frame"] * 100))
    if rows:
        import statistics
        print("median byte accuracy when the frame differs: %.1f%%" % (
            statistics.median(row["byte_accuracy"] for row in rows) * 100))
        print("\n%-40s %-10s %8s %8s %7s  %s" % (
            "function", "address", "ours", "original", "bytes", "source"))
        for row in rows[:args.top]:
            print("%-40s %-10s %8s %8s %6.1f%%  %s" % (
                row["function"][:40], row["address"],
                "0x%x" % row["our_frame"], "0x%x" % row["original_frame"],
                row["byte_accuracy"] * 100, row["source"] or "?"))
    if args.json:
        args.json.write_text(json.dumps({"counts": counts, "functions": rows}, indent=2),
                             encoding="utf-8")
        print("\nworklist written to %s" % args.json)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
