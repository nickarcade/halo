#!/usr/bin/env python3
"""consolidate_squash_msg.py — Parse, deduplicate, and standardize squashed lift commit messages.

When Git squashes multiple commits (via interactive rebase, merge --squash, etc.),
it concatenates the original commit messages together. This script detects and
consolidates those messages into the repository's standard single-commit format:
  - Exactly one synthesized subject line (with batch count or function names, object, and average VC71 score)
  - A single deduplicated 'Functions ported:' section sorted by address
  - Consolidated 'Functions renamed:' and 'Callee decls corrected:' sections
  - A single net 'Coverage:' line (e.g. start% -> end% (count/total symbols))

Usage:
  python3 tools/audit/consolidate_squash_msg.py <file>
  python3 tools/audit/consolidate_squash_msg.py --in-place <file>
  python3 tools/audit/consolidate_squash_msg.py --check <file>
  python3 tools/audit/consolidate_squash_msg.py --commit <sha>
  git log -1 --format=%B <sha> | python3 tools/audit/consolidate_squash_msg.py
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from collections import Counter
from pathlib import Path

PORTED_LINE_RE = re.compile(
    r"^-\s*(\*?\w+)\s*@\s*(0x[0-9a-fA-F]+)\s*\(([^)]+)\)(?:\s*\[([\d.]+)%\s*VC71(?:,\s*(\d+)/(\d+)\s*insns)?\])?$"
)
RENAMED_LINE_RE = re.compile(
    r"^-\s*(\w+)\s*->\s*(\w+)\s*@\s*(0x[0-9a-fA-F]+)\s*\(([^)]+)\)$"
)
DECL_FIX_LINE_RE = re.compile(
    r"^-\s*(\*?\w+)\s*@\s*(0x[0-9a-fA-F]+)\s*\(([^)]+)\)$"
)
META_LINE_RE = re.compile(
    r"^-\s*(\d+)\s+symbol\(s\)\s+updated$"
)
COVERAGE_LINE_RE = re.compile(
    r"^Coverage:\s+(?:([\d.]+)%\s*->\s*)?([\d.]+)%\s*\(([0-9]+)/([0-9]+)\s+symbols\)"
)


def has_unmerged_squash(text: str) -> bool:
    """True if text contains multiple lift commit headers or raw git squash markers."""
    # Check for git squash comments even before stripping
    if re.search(r"^# This is a combination of [0-9]+ commits", text, re.MULTILINE):
        return True

    lines = [l for l in text.splitlines() if not l.strip().startswith("#")]
    clean = "\n".join(lines).strip()

    ported_headers = len(re.findall(r"^Functions ported:", clean, re.MULTILINE))
    if ported_headers > 1:
        return True

    coverage_lines = len(re.findall(r"^Coverage:", clean, re.MULTILINE))
    if coverage_lines > 1:
        return True

    port_subjects = len(re.findall(r"^(?:Port|Update)\s+.+", clean, re.MULTILINE))
    if port_subjects > 1:
        return True

    return False


def consolidate_message(text: str) -> str | None:
    """Parse a concatenated squash commit message and format it into canonical shape."""
    raw_lines = [l for l in text.splitlines() if not l.strip().startswith("#")]
    clean_text = "\n".join(raw_lines).strip()

    if not has_unmerged_squash(text) and not clean_text.startswith("Port "):
        return None

    ports: dict[str, dict] = {}
    renames: dict[str, tuple[str, str, str, str]] = {}
    decl_fixes: dict[str, tuple[str, str, str, str, str]] = {}
    meta_count = 0
    coverages: list[tuple] = []
    equivs: list[str] = []

    current_section: str | None = None
    lines = clean_text.splitlines()
    i = 0
    while i < len(lines):
        line = lines[i]
        stripped = line.strip()

        # Subject line match (extract equiv tags if present)
        m_equiv = re.search(r"(\d+/\d+)\s+equiv", stripped)
        if m_equiv:
            equivs.append(m_equiv.group(1))

        if stripped.startswith("Functions ported:"):
            current_section = "ports"
            i += 1
            continue
        elif stripped.startswith("Functions renamed:"):
            current_section = "renames"
            i += 1
            continue
        elif stripped.startswith("Callee decls corrected"):
            current_section = "decl_fixes"
            i += 1
            continue
        elif stripped.startswith("kb_meta.json updates:"):
            current_section = "meta"
            i += 1
            continue
        elif stripped.startswith("Coverage:"):
            current_section = "coverage"
            m_cov = COVERAGE_LINE_RE.search(stripped)
            if m_cov:
                coverages.append(m_cov.groups())
            i += 1
            continue
        elif re.match(r"^(?:Port|Update)\s+", stripped):
            current_section = None
            i += 1
            continue

        if current_section == "ports":
            m_p = PORTED_LINE_RE.match(stripped)
            if m_p:
                name, addr, obj, score, insn_c, insn_r = m_p.groups()
                addr_norm = hex(int(addr, 16)).lower()
                ports[addr_norm] = {
                    "name": name,
                    "addr": addr,
                    "obj": obj,
                    "score": float(score) if score else None,
                    "insn_c": insn_c,
                    "insn_r": insn_r,
                }
        elif current_section == "renames":
            m_r = RENAMED_LINE_RE.match(stripped)
            if m_r:
                old, new, addr, obj = m_r.groups()
                addr_norm = hex(int(addr, 16)).lower()
                renames[addr_norm] = (old, new, addr, obj)
        elif current_section == "decl_fixes":
            m_df = DECL_FIX_LINE_RE.match(stripped)
            if m_df:
                name, addr, obj = m_df.groups()
                addr_norm = hex(int(addr, 16)).lower()
                old_decl, new_decl = "", ""
                if i + 1 < len(lines) and lines[i + 1].startswith("    "):
                    old_decl = lines[i + 1].strip()
                    i += 1
                if i + 1 < len(lines) and lines[i + 1].strip().startswith("->"):
                    new_decl = lines[i + 1].strip().lstrip("->").strip()
                    i += 1
                decl_fixes[addr_norm] = (name, addr, obj, old_decl, new_decl)
        elif current_section == "meta":
            m_m = META_LINE_RE.match(stripped)
            if m_m:
                meta_count += int(m_m.group(1))

        i += 1

    if not ports and not renames and not decl_fixes:
        return None

    # Equivalence tag
    equiv_tag = None
    if equivs:
        u_equivs = set(equivs)
        if len(u_equivs) == 1:
            equiv_tag = f"{list(u_equivs)[0]} equiv"
        else:
            equiv_tag = f"{equivs[-1]} equiv"

    # Compute average VC71 score
    scores = [p["score"] for p in ports.values() if p["score"] is not None]
    vc71_str = f"{sum(scores) / len(scores):.1f}% VC71" if scores else None

    match_parts = []
    if vc71_str:
        match_parts.append(vc71_str)
    if equiv_tag:
        match_parts.append(equiv_tag)
    match_tag = f" ({', '.join(match_parts)})" if match_parts else ""

    # Determine object context: primary object + object count
    port_objs = [p["obj"] for p in ports.values()]
    all_objs = sorted(list(set(port_objs + [r[3] for r in renames.values()])))

    if port_objs:
        primary_obj = Counter(port_objs).most_common(1)[0][0]
    elif all_objs:
        primary_obj = all_objs[0]
    else:
        primary_obj = "functions"

    verb = "Port" if ports else "Update"
    n_ports = len(ports)

    # Subject line synthesis
    if n_ports == 1:
        fn_name = list(ports.values())[0]["name"]
        subject = f"{verb} {fn_name} ({primary_obj}){match_tag}"
    elif n_ports == 2 and len(all_objs) == 1:
        names = [p["name"] for p in ports.values()]
        subject = f"{verb} {names[0]}, {names[1]} ({primary_obj}){match_tag}"
    elif len(all_objs) == 1:
        subject = f"{verb} {n_ports} functions from {primary_obj}{match_tag}"
    else:
        subject = f"{verb} {n_ports} functions from {primary_obj} ({len(all_objs)} objects){match_tag}"

    out = [subject, ""]

    if ports:
        out.append("Functions ported:")
        for addr, p in sorted(ports.items(), key=lambda x: int(x[0], 16)):
            insn_part = f", {p['insn_c']}/{p['insn_r']} insns" if p["insn_c"] else ""
            score_part = (
                f" [{p['score']:.1f}% VC71{insn_part}]"
                if p["score"] is not None
                else ""
            )
            out.append(f"- {p['name']} @ {p['addr']} ({p['obj']}){score_part}")
        out.append("")

    if renames:
        out.append("Functions renamed:")
        for addr, r in sorted(renames.items(), key=lambda x: int(x[0], 16)):
            out.append(f"- {r[0]} -> {r[1]} @ {r[2]} ({r[3]})")
        out.append("")

    if decl_fixes:
        out.append("Callee decls corrected (signature only, name unchanged):")
        for addr, df in sorted(decl_fixes.items(), key=lambda x: int(x[0], 16)):
            out.append(f"- {df[0]} @ {df[1]} ({df[2]})")
            out.append(f"    {df[3]}")
            out.append(f" -> {df[4]}")
        out.append("")

    if meta_count > 0:
        out.append("kb_meta.json updates:")
        out.append(f"- {meta_count} symbol(s) updated")
        out.append("")

    if coverages:
        start_pct = coverages[0][0] or coverages[0][1]
        end_pct = coverages[-1][1]
        final_count = coverages[-1][2]
        total_syms = coverages[-1][3]
        if start_pct and start_pct != end_pct:
            out.append(
                f"Coverage: {start_pct}% -> {end_pct}% ({final_count}/{total_syms} symbols)"
            )
        else:
            out.append(f"Coverage: {end_pct}% ({final_count}/{total_syms} symbols)")

    return "\n".join(out).strip() + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Standardize and consolidate squashed lift commit messages."
    )
    parser.add_argument(
        "file",
        nargs="?",
        default=None,
        help="Commit message file to read (or stdin if omitted)",
    )
    parser.add_argument(
        "-i",
        "--in-place",
        action="store_true",
        help="Modify the input file in-place",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Check if message contains unmerged squash artifacts (exit 0 if it does, 1 if clean)",
    )
    parser.add_argument(
        "--commit",
        default=None,
        help="Git commit SHA to inspect and consolidate",
    )

    args = parser.parse_args()

    if args.commit:
        proc = subprocess.run(
            ["git", "log", "-1", "--format=%B", args.commit],
            capture_output=True,
            text=True,
            check=True,
        )
        content = proc.stdout
    elif args.file:
        path = Path(args.file)
        if not path.exists():
            print(f"Error: file not found: {args.file}", file=sys.stderr)
            return 2
        content = path.read_text(encoding="utf-8", errors="replace")
    elif not sys.stdin.isatty():
        content = sys.stdin.read()
    else:
        parser.print_help()
        return 2

    if args.check:
        return 0 if has_unmerged_squash(content) else 1

    consolidated = consolidate_message(content)
    if consolidated is None:
        if args.in_place:
            return 0
        print(content.strip())
        return 0

    if args.in_place:
        if not args.file:
            print("Error: --in-place requires a file argument", file=sys.stderr)
            return 2
        Path(args.file).write_text(consolidated, encoding="utf-8")
    else:
        print(consolidated, end="")

    return 0


if __name__ == "__main__":
    sys.exit(main())
