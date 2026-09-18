#!/usr/bin/env python3
"""Gate a snapshot-verification report against the reviewed-exception policy.

game_state_verify.py reports every target but always exits 0, so a target that
starts failing reads exactly like one that always failed.  This turns the report
into a pass/fail decision:

  pass             -> ok
  not_applicable   -> ok (harness/reference-data gap, already labelled as such)
  inconclusive     -> ok by default (this lane drives one captured snapshot, so
                      a parameterless routine cannot vary its own inputs; the
                      per-branch proof lives in regression_targets.json)
  fail / error     -> FAILS the job unless the target is listed in the policy
                      file with a reason

A target listed in the policy that comes back PASS is also reported -- the
exception has been fixed and the entry should be removed.
"""

import argparse
from collections import Counter
import json
import os
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_POLICY = Path(__file__).resolve().parent / "snapshot_verify_policy.json"


def classify(result: dict) -> str:
    """Reduce one result row to pass/not_applicable/inconclusive/fail/error."""
    if result.get("error"):
        return "error"
    status = result.get("status")
    if status == "not_applicable" or result.get("applicable") is False:
        return "not_applicable"
    if status in ("inconclusive", "error"):
        return status
    total = result.get("total_seeds", 0)
    if total > 0 and result.get("passed", 0) == total and not result.get("errors", 0):
        return "pass"
    if result.get("errors", 0):
        return "error"
    return "fail"


def write_github_summary(report: dict, policy: dict, excused: list, stale: list,
                         unexpected: list) -> None:
    """Append the policy-aware result tally to a GitHub Actions summary."""
    summary_path = os.environ.get("GITHUB_STEP_SUMMARY")
    if not summary_path:
        return

    results = report.get("results", [])
    counts = Counter(classify(result) for result in results)
    summary = report.get("summary", {})
    rows = ["## Snapshot Verification", "",
            f"**{summary.get('passed', 0)}/{summary.get('total', 0)} targets passed** "
            f"({summary.get('avg_coverage_pct', 0):.1f}% average coverage)", "",
            "| Result | Count |", "|---|---:|"]
    for verdict in ("pass", "inconclusive", "not_applicable", "fail", "error"):
        if counts[verdict]:
            rows.append(f"| {verdict} | {counts[verdict]} |")
    if excused:
        rows.append("\n### Reviewed exceptions")
        for name, verdict, reason in excused:
            rows.append(f"- `{name}` — `{verdict}`: {reason}")
    if stale:
        rows.append("\n### Stale policy entries")
        for name, _entry in stale:
            rows.append(f"- `{name}` now passes; remove its exception.")
    if unexpected:
        rows.append("\n### Unexpected outcomes")
        for name, verdict, reason in unexpected:
            rows.append(f"- `{name}` — `{verdict}`: {reason}")
    informational = []
    if policy.get("allow_inconclusive", True):
        informational.append("`inconclusive`")
    if policy.get("allow_not_applicable", True):
        informational.append("`not_applicable`")
    if informational:
        rows.append("\n%s are informational by policy." % " and ".join(informational))
    with open(summary_path, "a", encoding="utf-8") as f:
        f.write("\n".join(rows) + "\n")


def check(report: dict, policy: dict, github_summary: bool = False) -> int:
    expected = policy.get("expected_non_pass", {})
    allow_inconclusive = policy.get("allow_inconclusive", True)
    allow_not_applicable = policy.get("allow_not_applicable", True)

    unexpected = []
    excused = []
    stale = []
    informational = []

    for result in report.get("results", []):
        name = result.get("func") or result.get("name") or "?"
        verdict = classify(result)
        entry = expected.get(name)

        if verdict == "pass":
            if entry:
                stale.append((name, entry))
            continue
        if verdict == "not_applicable" and allow_not_applicable:
            informational.append((name, verdict, result.get("reason")))
            continue
        if verdict == "inconclusive" and allow_inconclusive:
            informational.append((name, verdict, result.get("reason")))
            continue
        if entry and verdict in entry.get("statuses", []):
            excused.append((name, verdict, entry.get("reason", "")))
            continue
        unexpected.append((name, verdict, result.get("reason") or result.get("error") or ""))

    summary = report.get("summary", {})
    print(f"snapshot verification: {summary.get('passed', 0)}/"
          f"{summary.get('total', 0)} passed")

    if informational:
        print(f"\n  informational ({len(informational)}):")
        for name, verdict, reason in informational:
            print(f"    {verdict:<16s} {name}  {reason or ''}".rstrip())
    if excused:
        print(f"\n  reviewed exceptions ({len(excused)}):")
        for name, verdict, reason in excused:
            print(f"    {verdict:<16s} {name}")
            print(f"      {reason}")
    if stale:
        print(f"\n  STALE POLICY ENTRIES ({len(stale)}) -- these now pass, "
              f"remove them from the policy file:")
        for name, _entry in stale:
            print(f"    {name}")
    if github_summary:
        write_github_summary(report, policy, excused, stale, unexpected)
    if unexpected:
        print(f"\n  UNEXPECTED ({len(unexpected)}):")
        for name, verdict, reason in unexpected:
            print(f"    {verdict:<16s} {name}  {reason}".rstrip())
        return 1

    return 0


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("report", help="JSON report from game_state_verify.py")
    p.add_argument("--policy", default=str(DEFAULT_POLICY),
                   help="Reviewed-exception policy file")
    p.add_argument("--strict-inconclusive", action="store_true",
                   help="Treat inconclusive as a failure too")
    p.add_argument("--github-summary", action="store_true",
                   help="Append a policy-aware table to GITHUB_STEP_SUMMARY")
    args = p.parse_args()

    report_path = Path(args.report)
    if not report_path.exists():
        print(f"no report at {report_path}; the verification step did not finish")
        return 1

    report = json.loads(report_path.read_text(encoding="utf-8"))
    policy = json.loads(Path(args.policy).read_text(encoding="utf-8"))
    if args.strict_inconclusive:
        policy["allow_inconclusive"] = False
    return check(report, policy, github_summary=args.github_summary)


if __name__ == "__main__":
    sys.exit(main())
