"""Import one broad behavior result and explicitly labeled supplemental cases.

The dashboard consumes the same per-target result contract as batch_verify. This
small adapter keeps historical pilot artifacts out of the default scan until a
human names the main result and each supplemental case explicitly.
"""

import argparse
import json
import os
import re
import sys
from pathlib import Path


_COUNT_FIELDS = ("passed", "failed", "errors", "seeds")
_VALID_STATUSES = {"pass", "fail", "error", "inconclusive", "not_applicable"}


def _read_result(path):
    try:
        with open(path, encoding="utf-8") as handle:
            record = json.load(handle)
    except (OSError, json.JSONDecodeError) as exc:
        raise ValueError("could not read result %s: %s" % (path, exc))
    if not isinstance(record, dict):
        raise ValueError("result %s is not an object" % path)
    target = record.get("target")
    status = record.get("status")
    if not isinstance(target, str) or not target.strip():
        raise ValueError("result %s has no target" % path)
    if status not in _VALID_STATUSES:
        raise ValueError("result %s has invalid status %r" % (path, status))
    for field in _COUNT_FIELDS:
        value = record.get(field)
        if value is not None and (isinstance(value, bool) or
                                  not isinstance(value, int) or value < 0):
            raise ValueError("result %s has invalid %s" % (path, field))
    seeds = record.get("seeds")
    if seeds is not None:
        total = sum(record.get(field) or 0 for field in ("passed", "failed", "errors"))
        if total > seeds:
            raise ValueError("result %s has disjoint counts greater than seeds" % path)
        for field in ("passed", "failed", "errors"):
            value = record.get(field)
            if value is not None and value > seeds:
                raise ValueError("result %s has %s greater than seeds" % (path, field))
    return record


def _relative(path, root):
    return os.path.relpath(os.path.abspath(path), os.path.abspath(root)).replace(os.sep, "/")


def _case_spec(spec):
    if "=" not in spec:
        raise ValueError("case must use LABEL=PATH: %s" % spec)
    label, path = spec.split("=", 1)
    label = label.strip()
    path = path.strip()
    if not label or not path:
        raise ValueError("case must use a non-empty LABEL=PATH: %s" % spec)
    return label, path


def import_bundle(main_path, case_specs=(), output_dir=None, root_dir=None):
    """Write one normalized dashboard result and return its output path."""
    root_dir = os.path.abspath(root_dir or os.getcwd())
    main_path = os.path.abspath(main_path)
    main = _read_result(main_path)
    target = main["target"]
    cases = []
    labels = set()
    for spec in case_specs:
        label, case_path = _case_spec(spec)
        if label in labels:
            raise ValueError("duplicate case label: %s" % label)
        labels.add(label)
        case_path = os.path.abspath(case_path)
        case = _read_result(case_path)
        if case["target"] != target:
            raise ValueError("case %s targets %s, expected %s" %
                             (label, case["target"], target))
        cases.append({
            "label": label,
            "artifact": _relative(case_path, root_dir),
            "target_reference": {
                "target": case["target"],
                "address": case.get("address"),
            },
            "result": case,
        })

    bundle = dict(main)
    bundle["target_reference"] = {
        "target": target,
        "address": main.get("address"),
    }
    bundle["main_artifact"] = _relative(main_path, root_dir)
    bundle["targeted_cases"] = cases
    bundle["import_provenance"] = {
        "kind": "explicit_behavior_result_import_v1",
        "main_artifact": _relative(main_path, root_dir),
        "case_count": len(cases),
    }

    if output_dir is None:
        output_dir = os.path.join(root_dir, "artifacts", "batch_verify", "imported")
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    filename = re.sub(r"[^A-Za-z0-9_.-]+", "_", target).strip("._") or "behavior_result"
    output = output_dir / (filename + ".json")
    temporary = output.with_name("." + output.name + ".tmp")
    temporary.write_text(json.dumps(bundle, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    os.replace(temporary, output)
    return output


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--main", required=True, help="broad result JSON")
    parser.add_argument("--case", action="append", default=[], metavar="LABEL=PATH",
                        help="one distinct supplemental result; repeat for each case")
    parser.add_argument("--output-dir", help="destination, default artifacts/batch_verify/imported")
    parser.add_argument("--root-dir", default=os.getcwd(), help="path base for artifact references")
    args = parser.parse_args(argv)
    try:
        output = import_bundle(args.main, args.case, args.output_dir, args.root_dir)
    except ValueError as exc:
        parser.error(str(exc))
    print(output)
    return 0


if __name__ == "__main__":
    sys.exit(main())
