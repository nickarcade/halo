"""Reject an unexplained large drop in dashboard behavioral matches."""

import argparse
import json
from pathlib import Path


def behavioral_matches(report):
    count = 0
    for unit in report.get("units", []):
        if unit.get("synthetic"):
            continue
        for function in unit.get("functions", []):
            if not function.get("ported"):
                continue
            confidence = function.get("equiv_confidence")
            status = function.get("equiv_status")
            if (status == "fail" and function.get("equiv_reason") == "divergence"
                    and confidence in ("high", "moderate")):
                continue
            equivalence = status == "pass" and (
                function.get("equiv_proven") is True
                or confidence in ("high", "moderate"))
            if (equivalence or function.get("snapshot_passed") is True
                    or function.get("runtime_oracle_passed") is True):
                count += 1
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("previous", type=Path)
    parser.add_argument("current", type=Path)
    args = parser.parse_args()
    if not args.previous.is_file():
        print("No previous report; behavioral-count comparison skipped")
        return 0
    old = behavioral_matches(json.loads(args.previous.read_text(encoding="utf-8")))
    new = behavioral_matches(json.loads(args.current.read_text(encoding="utf-8")))
    drop = old - new
    limit = max(50, int(old * 0.05))
    print(f"Behavioral matches: {old} -> {new} (drop limit {limit})")
    if drop > limit:
        print("Large behavioral-match drop requires investigation before publish")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
