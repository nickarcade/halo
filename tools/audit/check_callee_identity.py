#!/usr/bin/env python3
"""
check_callee_identity.py — Cross-check WHICH function a lift calls against the
function the pristine XBE actually calls at that site.

Every existing call-site gate checks the *shape* of a call: argument count
(check_arg_counts.py), float/int param and return types (check_param_types.py),
register-arg annotations (check_callee_reg_args.py), raw-cast conventions
(check_xcall_types.py).  None of them checks callee IDENTITY — whether the name
written in the C source resolves to the address the original binary calls.

That gap is real and was exploited once already (lift-learnings §61):
`FUN_000dedf0` in src/halo/interface/interface.c called `unit_get_weapon`
(0x1ab940) where the original calls `unit_inventory_get_weapon` (0x1adeb0).
Two similarly-named kb.json functions were confused at lift time; a later
automated rebase fixup then REORDERED the arguments to conform to the wrong
callee's decl, which made the error type-check cleanly and hid it from every
shape-based audit.  It survived ~7 days and cost 4.2pp of VC71 match.

Method
------
For each `ported: true` kb.json function that has a same-named definition in
src/:

  1. Linearly disassemble the original bytes over
     [addr, function_bounds.json end) and collect every direct `CALL imm32` /
     `JMP imm32` target (the JMP set covers MSVC tail calls).  Every target
     that is a bare one-instruction `JMP imm32` stub is followed to the
     function it fronts, so the `thunk_rasterizer_*` stubs at 0x17c8c0+ and the
     XAPI import stubs at 0x2235c4+ (`GetLastError` -> `xapi_GetLastError`) do
     not read as mismatches.  This is what CLAUDE.md's "Functional
     Re-implementation" clause means by acceptable import indirection.
  2. Parse the C body and collect every call to a NAMED kb.json function
     (`FUN_xxxxxxxx` callees are skipped — there is no better name to compare
     against, so a mismatch carries no information).
  3. A source callee whose kb.json address is absent from the binary's target
     set is a finding.  Targets present in the binary but named nowhere in the
     source are "orphans" — the candidates for what the source *should* have
     said.

Severity
--------
  HIGH  missing callee + an orphan target whose kb.json name is lexically
        similar (token subset, Jaccard >= 0.5, or character ratio >= 0.88).
        This is the name-confusion signature: the interface.c bug scores HIGH,
        as does `csstrcat` where the original calls `csstrncat`.
  WARN  missing callee + orphan targets exist, none similar.  Either a wrong
        callee with an unrelated name, or one of the false-positive mechanisms
        below.  Dominated by inlined cseries/CRT helpers.
  INFO  missing callee, no orphan targets (`no-orphans`), or every orphan that
        justified the severity is itself called by the missing callee's own
        body (`inlined`) — the signature of MSVC having inlined that callee
        into the caller.  Off by default; shown with -v.

Known false-positive mechanisms (all verified to occur)
-------------------------------------------------------
  1. Inlining.  MSVC inlined the callee into the original, so there is no CALL
     to find.  Dominates the INFO and WARN tiers.  (Our lift calling it
     out-of-line is a separate, "Preserve Inline Schedule" fidelity question —
     not this gate's.)  The `inlined` downgrade catches the subset where the
     inlined body's own callees surface as the orphans.
  2. Indirect calls.  `call dword ptr [...]` / `call reg` — vtables, callbacks
     and function tables.  No immediate to compare, so the site is invisible
     here; the result records `indirect=N` so a finding against a function with
     indirect calls can be discounted.
  3. Duplicate COMDATs.  A helper emitted out-of-line more than once has
     several addresses; kb.json usually names only one, so a call to the other
     copy reads as missing with the sibling as an orphan.
  4. Stale or wrong `function_bounds.json` `end` — calls past a short `end` are
     never seen.  Entries with `"kind": "auto"` are derived, not proven.
  5. Macro-shaped callees.  A kb.json name also defined as a macro (XCALL or
     otherwise) expands to something other than a direct call.
  6. Our lift legitimately restructured a helper call the original reached by
     a different path.

Known false NEGATIVE: the `inlined` downgrade would mask a genuinely wrong
callee that happens to call the right one.  It does not mask the §61 bug
(`unit_get_weapon` never calls `unit_inventory_get_weapon`), and the downgraded
findings stay visible under -v.

Because of (1) the INFO and WARN tiers are noise-dominated by construction and
are NOT gated.  Only HIGH is gated by --check.

Usage:
    python3 tools/audit/check_callee_identity.py                # HIGH + WARN
    python3 tools/audit/check_callee_identity.py -v             # + INFO tier
    python3 tools/audit/check_callee_identity.py --function FUN_000dedf0
    python3 tools/audit/check_callee_identity.py --function 0xdedf0
    python3 tools/audit/check_callee_identity.py --files src/halo/interface/interface.c
    python3 tools/audit/check_callee_identity.py --changed-only
    python3 tools/audit/check_callee_identity.py --recent-commits 3
    python3 tools/audit/check_callee_identity.py --check         # exit 1 on new HIGH
    python3 tools/audit/check_callee_identity.py --update-baseline
    python3 tools/audit/check_callee_identity.py --json <path>
    python3 tools/audit/check_callee_identity.py --self-test
"""

from __future__ import annotations

import argparse
import difflib
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path
from typing import Dict, List, NamedTuple, Optional, Set, Tuple

import capstone

sys.path.insert(0, str(Path(__file__).resolve().parent))

import check_arg_counts as cac  # noqa: E402  (XBE loader, kb loader, comment helpers)

REPO_ROOT = cac.REPO_ROOT
SRC_DIR = REPO_ROOT / "src"
BOUNDS_PATH = REPO_ROOT / "tools" / "verify" / "function_bounds.json"
BASELINE_PATH = REPO_ROOT / "tools" / "audit" / "callee_identity_baseline.json"
REPORT_DIR = REPO_ROOT / "artifacts" / "audit"

_FUN_NAME_RE = re.compile(r"^FUN_[0-9a-fA-F]{6,8}$")

# Identifiers that are followed by '(' but are not calls.
_NOT_CALLS = {
    "if", "while", "for", "switch", "return", "sizeof", "do", "else",
    "case", "defined", "offsetof", "va_start", "va_arg", "va_end",
    "typedef", "struct", "union", "enum", "static", "const", "volatile",
    "extern", "inline", "register", "signed", "unsigned", "void", "char",
    "short", "int", "long", "float", "double", "_Bool", "__try", "__except",
    "__finally", "__declspec", "__asm", "asm", "goto", "catch",
}


# ---------------------------------------------------------------------------
# Pristine-XBE side: direct call/jmp targets of one function body
# ---------------------------------------------------------------------------

_cs_lin: Optional[capstone.Cs] = None


def _get_cs() -> capstone.Cs:
    global _cs_lin
    if _cs_lin is None:
        _cs_lin = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        _cs_lin.detail = False
        _cs_lin.skipdata = True
    return _cs_lin


def _va_bytes(start: int, end: int) -> bytes:
    """Raw bytes for [start, end) out of the pristine XBE, or b'' if unmapped."""
    cac._load_xbe()
    for i, (vaddr, vsize, _raw_addr, _raw_size) in enumerate(cac._xbe_sections):
        if vaddr <= start < vaddr + vsize:
            data = cac._xbe_bytes_cache[i]
            off = start - vaddr
            n = min(end - start, len(data) - off)
            return data[off:off + n] if n > 0 else b""
    return b""


_thunk_cache: Dict[int, int] = {}


def resolve_thunk(addr: int, depth: int = 4) -> int:
    """Follow a one-instruction `JMP imm32` thunk to the function it fronts.

    The original reaches many functions through a `.text` jump stub: the
    `thunk_rasterizer_*` stubs at 0x17c8c0+, and the XAPI import stubs at
    0x2235c4+ (`GetLastError` -> `xapi_GetLastError`).  Both are transparent —
    a lift that names the real function is correct, so the raw CALL target must
    be resolved before comparing.  Binary evidence, not a name heuristic.
    """
    if addr in _thunk_cache:
        return _thunk_cache[addr]
    cur = addr
    for _ in range(depth):
        code = _va_bytes(cur, cur + 8)
        if not code:
            break
        first = next(_get_cs().disasm(code, cur), None)
        if first is None or first.mnemonic != "jmp":
            break
        try:
            nxt = int(first.op_str, 16)
        except ValueError:
            break
        if nxt == cur:
            break
        cur = nxt
    _thunk_cache[addr] = cur
    return cur


class BodyTargets(NamedTuple):
    call_imm:  Set[int]   # direct CALL imm32 targets
    jmp_imm:   Set[int]   # direct JMP imm32 targets landing OUTSIDE the body (tail calls)
    indirect:  int        # count of indirect CALLs (no immediate to compare)
    decoded:   int        # instructions decoded (0 == nothing to analyse)


def function_targets(start: int, end: int) -> BodyTargets:
    """Direct call/tail-jmp targets of the original function body [start, end).

    Linear disassembly from a known function entry, bounded by
    function_bounds.json, is more accurate than the section-wide scan
    check_arg_counts.py uses: that one starts at a section base and can desync
    on data-in-code, which is tolerable for a callee-centric histogram but not
    for "did THIS function call THAT address".
    """
    code = _va_bytes(start, end)
    if not code:
        return BodyTargets(set(), set(), 0, 0)

    calls: Set[int] = set()
    jmps: Set[int] = set()
    indirect = 0
    decoded = 0
    for ins in _get_cs().disasm(code, start):
        decoded += 1
        if ins.mnemonic == "call":
            try:
                calls.add(int(ins.op_str, 16))
            except ValueError:
                indirect += 1
        elif ins.mnemonic == "jmp":
            try:
                t = int(ins.op_str, 16)
            except ValueError:
                continue
            if not (start <= t < end):
                jmps.add(t)           # tail call out of the body

    # Keep both the raw stub address and the function it fronts, so a source
    # naming either spelling matches.
    calls |= {resolve_thunk(t) for t in calls}
    jmps |= {resolve_thunk(t) for t in jmps}
    return BodyTargets(calls, jmps, indirect, decoded)


# ---------------------------------------------------------------------------
# Source side: function definitions and the named callees inside them
# ---------------------------------------------------------------------------

class SourceFunc(NamedTuple):
    name:     str
    path:     Path
    line:     int            # 1-based line of the definition
    calls:    List[Tuple[str, int]]   # (callee name, 1-based line)


def _match_delims(text: str, i: int, opener: str, closer: str) -> int:
    """Index of the delimiter matching text[i], or -1."""
    depth = 0
    while i < len(text):
        c = text[i]
        if c == opener:
            depth += 1
        elif c == closer:
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


_DEF_HEAD_RE = re.compile(
    r"(?m)^(?:[A-Za-z_][A-Za-z0-9_]*[ \t\*\n]+)*?([A-Za-z_][A-Za-z0-9_]*)[ \t]*\(")
_CALL_RE = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)[ \t\n]*\(")


def parse_source_file(path: Path) -> List[SourceFunc]:
    """Top-level function definitions in one .c file, with their callee names."""
    try:
        raw = path.read_text(errors="replace")
    except OSError:
        return []
    text = cac_blank(raw)

    funcs: List[SourceFunc] = []
    pos = 0
    while True:
        m = _DEF_HEAD_RE.search(text, pos)
        if not m:
            break
        name = m.group(1)
        open_paren = m.end() - 1
        pos = m.end()
        if name in _NOT_CALLS:
            continue
        close_paren = _match_delims(text, open_paren, "(", ")")
        if close_paren < 0:
            continue
        k = close_paren + 1
        while k < len(text) and text[k] in " \t\r\n":
            k += 1
        if k >= len(text) or text[k] != "{":
            continue               # prototype, not a definition
        close_brace = _match_delims(text, k, "{", "}")
        if close_brace < 0:
            continue

        body = text[k:close_brace]
        body_line0 = text.count("\n", 0, k) + 1
        calls: List[Tuple[str, int]] = []
        for cm in _CALL_RE.finditer(body):
            cname = cm.group(1)
            if cname in _NOT_CALLS or cname == name:
                continue
            # `p->fn(...)` / `s.fn(...)` are indirect through a field.
            j = cm.start() - 1
            while j >= 0 and body[j] in " \t\n":
                j -= 1
            if j >= 0 and (body[j] == "." or (j >= 1 and body[j - 1:j + 1] == "->")):
                continue
            calls.append((cname, body_line0 + body.count("\n", 0, cm.start())))

        funcs.append(SourceFunc(name, path, text.count("\n", 0, m.start()) + 1, calls))
        pos = close_brace + 1

    return funcs


def cac_blank(raw: str) -> str:
    """Blank comments and string/char literals, preserving offsets."""
    # check_lift_hazards owns the canonical implementation; importing that
    # 3.5k-line module for one helper is not worth the load time, and the
    # routine is short enough to keep position-identical here.
    out = []
    i = 0
    state = "code"
    n = len(raw)
    while i < n:
        ch = raw[i]
        nxt = raw[i + 1] if i + 1 < n else ""
        if state == "code":
            if ch == "/" and nxt == "/":
                out.extend((" ", " ")); i += 2; state = "line"; continue
            if ch == "/" and nxt == "*":
                out.extend((" ", " ")); i += 2; state = "block"; continue
            if ch == '"':
                out.append(" "); i += 1; state = "string"; continue
            if ch == "'":
                out.append(" "); i += 1; state = "char"; continue
            out.append(ch); i += 1; continue
        if state == "line":
            out.append("\n" if ch == "\n" else " ")
            i += 1
            if ch == "\n":
                state = "code"
            continue
        if state == "block":
            if ch == "*" and nxt == "/":
                out.extend((" ", " ")); i += 2; state = "code"; continue
            out.append("\n" if ch == "\n" else " "); i += 1; continue
        quote = '"' if state == "string" else "'"
        if ch == "\\" and nxt:
            out.extend((" ", "\n" if nxt == "\n" else " ")); i += 2; continue
        out.append("\n" if ch == "\n" else " ")
        i += 1
        if ch == quote:
            state = "code"
    return "".join(out)


# ---------------------------------------------------------------------------
# Name similarity — the name-confusion signature
# ---------------------------------------------------------------------------

def _tokens(name: str) -> Set[str]:
    return {t for t in name.lower().split("_") if t}


# Character-ratio fallback for names that do not tokenise usefully.  Tuned on
# the real corpus: 0.941 `csstrcat`/`csstrncat` passes, 0.842 `csprintf`/
# `crt_sprintf` and 0.800 `csstricmp`/`crt_stricmp` (both real inline-vs-CRT
# false positives) do not.
_CHAR_RATIO_MIN = 0.88


def names_similar(a: str, b: str) -> bool:
    """True when two kb.json names are close enough to be confused at lift time."""
    if a == b:
        return True
    ta, tb = _tokens(a), _tokens(b)
    if ta and tb:
        if ta <= tb or tb <= ta:
            return True
        if len(ta & tb) / len(ta | tb) >= 0.5:
            return True
    return difflib.SequenceMatcher(None, a, b).ratio() >= _CHAR_RATIO_MIN


# ---------------------------------------------------------------------------
# Analysis
# ---------------------------------------------------------------------------

class Finding(NamedTuple):
    severity:    str        # HIGH / WARN / INFO
    reason:      str        # "" / "inlined" / "no-orphans"
    caller:      str
    caller_addr: str
    path:        str
    line:        int
    callee:      str
    callee_addr: str
    candidates:  List[str]  # "name@0xaddr" orphan targets (similar ones first)
    indirect:    int


def _callee_own_targets(addrs: Set[int], bounds: Dict[int, int]) -> Set[int]:
    """Direct targets of the named-but-uncalled callee's own body.

    When MSVC inlined that callee into the caller, the addresses the inlined
    body reaches surface as orphans in the caller.  Containment is therefore a
    binary-backed "this was inlined, not misnamed" signal — and it does not
    mask the §61 bug: unit_get_weapon never calls unit_inventory_get_weapon.
    """
    out: Set[int] = set()
    for a in addrs:
        end = bounds.get(a)
        if end is None or end <= a:
            continue
        bt = function_targets(a, end)
        out |= bt.call_imm | bt.jmp_imm
    return out


def _relpath(p: Path) -> str:
    try:
        return str(p.resolve().relative_to(REPO_ROOT))
    except (ValueError, OSError):
        return str(p)


def _load_bounds() -> Dict[int, int]:
    if not BOUNDS_PATH.exists():
        return {}
    data = json.loads(BOUNDS_PATH.read_text())
    out: Dict[int, int] = {}
    for k, v in data.items():
        if not isinstance(v, dict):
            continue
        try:
            out[int(k, 16)] = int(v["end"], 16)
        except (KeyError, ValueError, TypeError):
            continue
    return out


def _collect_src_files(files: Optional[List[str]],
                       changed_only: bool,
                       recent_commits: int) -> List[Path]:
    if files:
        return [Path(f) if os.path.isabs(f) else REPO_ROOT / f for f in files]

    if changed_only or recent_commits:
        if recent_commits:
            cmd = ["git", "diff", "--name-only", f"HEAD~{recent_commits}..HEAD",
                   "--", "src/"]
        else:
            cmd = ["git", "diff", "--name-only", "HEAD", "--", "src/"]
        try:
            r = subprocess.run(cmd, capture_output=True, text=True, cwd=str(REPO_ROOT))
            names = [ln.strip() for ln in r.stdout.splitlines() if ln.strip().endswith(".c")]
        except (subprocess.SubprocessError, FileNotFoundError):
            names = []
        return [REPO_ROOT / n for n in names]

    return sorted(SRC_DIR.rglob("*.c"))


def analyze(files: Optional[List[str]] = None,
            changed_only: bool = False,
            recent_commits: int = 0,
            func_filter: Optional[str] = None) -> Tuple[List[Finding], Dict]:
    entries = cac._load_kb()
    by_addr: Dict[int, cac.FuncEntry] = {e.addr: e for e in entries}

    # name -> addr.  A name appearing at several addresses is ambiguous; the
    # source cannot distinguish them either, so treat every address as valid.
    name_addrs: Dict[str, Set[int]] = {}
    for e in entries:
        if e.name:
            name_addrs.setdefault(e.name, set()).add(e.addr)

    bounds = _load_bounds()

    src_funcs: Dict[str, SourceFunc] = {}
    dup_names: Set[str] = set()
    for p in _collect_src_files(files, changed_only, recent_commits):
        if not p.is_file():
            continue
        for sf in parse_source_file(p):
            if sf.name in src_funcs:
                dup_names.add(sf.name)
                continue
            src_funcs[sf.name] = sf

    stats = {
        "source_functions":    len(src_funcs),
        "analyzed":            0,
        "no_kb_entry":         0,
        "not_ported":          0,
        "no_bounds":           0,
        "undecodable":         0,
        "duplicate_names":     len(dup_names),
        "named_call_sites":    0,
        "matched_call_sites":  0,
        "inlined_downgrades":  0,
        "high": 0, "warn": 0, "info": 0,
    }

    findings: List[Finding] = []

    for name, sf in sorted(src_funcs.items()):
        addrs = name_addrs.get(name)
        if not addrs:
            stats["no_kb_entry"] += 1
            continue
        addr = sorted(addrs)[0]
        entry = by_addr[addr]
        if not entry.ported:
            stats["not_ported"] += 1
            continue
        if func_filter is not None and func_filter not in (name, entry.addr_str,
                                                           "0x%x" % addr):
            continue
        end = bounds.get(addr)
        if end is None or end <= addr:
            stats["no_bounds"] += 1
            continue

        bt = function_targets(addr, end)
        if bt.decoded == 0:
            stats["undecodable"] += 1
            continue
        stats["analyzed"] += 1

        bin_targets = bt.call_imm | bt.jmp_imm

        # Which source callees are named kb functions?
        named_calls: List[Tuple[str, int, Set[int]]] = []
        claimed: Set[int] = set()
        for cname, cline in sf.calls:
            cand = name_addrs.get(cname)
            if not cand or _FUN_NAME_RE.match(cname):
                continue
            named_calls.append((cname, cline, cand))
            claimed |= (cand & bin_targets)

        # Orphans: addresses the original calls that the source never names.
        orphans: List[Tuple[int, str]] = []
        for t in sorted(bin_targets - claimed):
            te = by_addr.get(t)
            if te is None or _FUN_NAME_RE.match(te.name or ""):
                continue
            if te.name in {c[0] for c in named_calls}:
                continue
            orphans.append((t, te.name))

        seen: Set[Tuple[str, int]] = set()
        for cname, cline, cand in named_calls:
            stats["named_call_sites"] += 1
            if cand & bin_targets:
                stats["matched_call_sites"] += 1
                continue
            if (cname, cline) in seen:
                continue
            seen.add((cname, cline))

            sim_pairs = [(a, n) for a, n in orphans if names_similar(cname, n)]
            similar = [f"{n}@0x{a:x}" for a, n in sim_pairs]
            other = [f"{n}@0x{a:x}" for a, n in orphans if not names_similar(cname, n)]
            reason = ""
            if similar:
                sev = "HIGH"
            elif other:
                sev = "WARN"
            else:
                sev, reason = "INFO", "no-orphans"

            if sev != "INFO":
                justifying = {a for a, _ in (sim_pairs if similar else orphans)}
                own = _callee_own_targets(cand, bounds)
                if justifying and justifying <= own:
                    sev, reason = "INFO", "inlined"
                    stats["inlined_downgrades"] += 1
            stats[sev.lower()] += 1

            findings.append(Finding(
                severity=sev,
                reason=reason,
                caller=name,
                caller_addr=entry.addr_str,
                path=_relpath(sf.path),
                line=cline,
                callee=cname,
                callee_addr=",".join("0x%x" % a for a in sorted(cand)),
                candidates=similar + other[:4],
                indirect=bt.indirect,
            ))

    _sev = {"HIGH": 0, "WARN": 1, "INFO": 2}
    findings.sort(key=lambda f: (_sev[f.severity], f.path, f.line))
    return findings, stats


# ---------------------------------------------------------------------------
# Baseline
# ---------------------------------------------------------------------------

def _key(f: Finding) -> str:
    return f"{f.caller_addr}:{f.callee}"


def _load_baseline() -> Dict[str, Dict]:
    if not BASELINE_PATH.exists():
        return {}
    return json.loads(BASELINE_PATH.read_text()).get("entries", {})


def _write_baseline(highs: List[Finding]) -> None:
    entries = {
        _key(f): {
            "caller": f.caller,
            "callee": f.callee,
            "callee_addr": f.callee_addr,
            "candidates": f.candidates,
        }
        for f in highs
    }
    BASELINE_PATH.write_text(json.dumps({
        "_comment": (
            "Pre-existing HIGH callee-identity findings (a lift names one "
            "kb.json function where the pristine XBE calls a similarly-named "
            "other one) accepted as latent. --check gates only HIGH findings "
            "absent from this file or whose callee_addr/candidates changed. "
            "Do NOT baseline a finding without first checking the "
            "disassembly at the caller: this detector exists because that "
            "class of bug type-checks cleanly. "
            "Regenerate: check_callee_identity.py --update-baseline"
        ),
        "entries": dict(sorted(entries.items())),
    }, indent=2) + "\n")


def _new_highs(highs: List[Finding], baseline: Dict[str, Dict]) -> List[Finding]:
    out = []
    for f in highs:
        b = baseline.get(_key(f))
        if (b is None
                or b.get("callee_addr") != f.callee_addr
                or b.get("candidates") != f.candidates):
            out.append(f)
    return out


# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------

def _fmt(f: Finding) -> str:
    cand = ", ".join(f.candidates) if f.candidates else "none"
    ind = f" indirect={f.indirect}" if f.indirect else ""
    why = f" [{f.reason}]" if f.reason else ""
    return (f"  {f.severity:4s}{why} {f.path}:{f.line}: {f.caller}({f.caller_addr}) "
            f"calls {f.callee}({f.callee_addr}) but the original never calls "
            f"that address — binary calls: {cand}{ind}")


def _print_report(findings: List[Finding], stats: Dict, verbose: bool) -> None:
    shown = [f for f in findings if verbose or f.severity != "INFO"]
    if shown:
        print(f"\n=== CALLEE-IDENTITY FINDINGS ({len(shown)}) ===")
        for f in shown:
            print(_fmt(f))
    else:
        print("\nCallee-identity audit: clean")

    print("\n=== SUMMARY ===")
    print(f"  Source functions seen:     {stats['source_functions']}")
    print(f"  Analyzed (ported+bounds):  {stats['analyzed']}")
    print(f"  Named call sites checked:  {stats['named_call_sites']}")
    print(f"  Confirmed against binary:  {stats['matched_call_sites']}")
    print(f"  Skipped no kb entry:       {stats['no_kb_entry']}")
    print(f"  Skipped not ported:        {stats['not_ported']}")
    print(f"  Skipped no bounds entry:   {stats['no_bounds']}")
    print(f"  Skipped undecodable:       {stats['undecodable']}")
    print(f"  Duplicate source names:    {stats['duplicate_names']}")
    print(f"  Downgraded (inlined):      {stats['inlined_downgrades']}")
    print(f"  HIGH / WARN / INFO:        "
          f"{stats['high']} / {stats['warn']} / {stats['info']}")


# ---------------------------------------------------------------------------
# Self-test
# ---------------------------------------------------------------------------

_SELF_TEST_SRC = """
/* comment naming unit_get_weapon(0, 0) must be ignored */
int FUN_000dedf0(int32_t *out_value)
{
    int unit;
    const char *s = "unit_get_weapon(x)";
    if (out_value != NULL) {
        unit = unit_get_weapon(*(int16_t *)(out_value + 0x2a2), out_value);
    }
    return unit;
}

static int prototype_only(int a);

void helper(void) { }
"""


def _self_test() -> None:
    import tempfile

    print("=== Self-test: name similarity ===")
    sim_cases = [
        ("unit_get_weapon", "unit_inventory_get_weapon", True, "token subset"),
        ("object_get", "object_get_and_verify_type", True, "token subset"),
        ("player_get", "unit_get_weapon", False, "unrelated"),
        ("a_b", "c_d", False, "disjoint"),
        ("game_engine_update", "game_engine_render", True, "jaccard 2/4 = 0.5"),
        ("game_engine_update", "rasterizer_draw_text", False, "jaccard 0/6"),
        ("unit_get_weapon", "first_person_weapon_update", False, "jaccard 1/7"),
        ("csstrcat", "csstrncat", True, "char ratio 0.94, single token"),
        ("csstricmp", "crt_stricmp", False, "char ratio 0.80 (inline-vs-CRT FP)"),
        ("csprintf", "crt_sprintf", False, "char ratio 0.84 (inline-vs-CRT FP)"),
    ]
    ok = True
    for a, b, exp, label in sim_cases:
        got = names_similar(a, b)
        status = "PASS" if got == exp else "FAIL"
        if got != exp:
            ok = False
        print(f"  {status}: {label}: {a} ~ {b} -> {got} (expected {exp})")

    print("\n=== Self-test: source parser ===")
    with tempfile.TemporaryDirectory() as td:
        p = Path(td) / "t.c"
        p.write_text(_SELF_TEST_SRC)
        funcs = {f.name: f for f in parse_source_file(p)}
        for want in ("FUN_000dedf0", "helper"):
            status = "PASS" if want in funcs else "FAIL"
            if want not in funcs:
                ok = False
            print(f"  {status}: definition {want} found")
        status = "PASS" if "prototype_only" not in funcs else "FAIL"
        if "prototype_only" in funcs:
            ok = False
        print(f"  {status}: prototype_only not treated as a definition")
        callees = [c for c, _ in funcs.get("FUN_000dedf0", SourceFunc("", p, 0, [])).calls]
        n_ugw = callees.count("unit_get_weapon")
        status = "PASS" if n_ugw == 1 else "FAIL"
        if n_ugw != 1:
            ok = False
        print(f"  {status}: unit_get_weapon counted once "
              f"(comment and string literal ignored), got {n_ugw}")

    if not ok:
        print("\nSelf-test FAILED (unit level)")
        sys.exit(1)

    print("\n=== Self-test: real XBE, FUN_000dedf0 (lift-learnings §61) ===")
    if not cac.XBE_PATH.exists() or not cac.KB_PATH.exists() or not BOUNDS_PATH.exists():
        print("  SKIP: XBE / kb.json / function_bounds.json not available")
        print("\nUnit self-tests PASSED (binary tests skipped).")
        return

    bounds = _load_bounds()
    start, end = 0xdedf0, bounds.get(0xdedf0)
    if end is None:
        print("  SKIP: 0xdedf0 absent from function_bounds.json")
        print("\nUnit self-tests PASSED (binary tests skipped).")
        return

    bt = function_targets(start, end)
    good, bad = 0x1adeb0, 0x1ab940      # unit_inventory_get_weapon / unit_get_weapon
    assert good in bt.call_imm, \
        f"FAIL: expected 0x{good:x} among call targets of 0x{start:x}"
    assert bad not in bt.call_imm, \
        f"FAIL: 0x{bad:x} unexpectedly a call target of 0x{start:x}"
    print(f"  PASS: 0x{start:x} calls 0x{good:x}, never 0x{bad:x} "
          f"({len(bt.call_imm)} direct targets, {bt.indirect} indirect)")

    # Current (fixed) source must be clean for this function.
    cur, _ = analyze(files=["src/halo/interface/interface.c"], func_filter="FUN_000dedf0")
    bad_now = [f for f in cur if f.callee in ("unit_get_weapon",
                                              "unit_inventory_get_weapon")]
    assert not bad_now, f"FAIL: false positive on the FIXED source: {bad_now}"
    print("  PASS: current (fixed) interface.c -> no finding for this callee pair")

    print("\nAll self-tests PASSED.")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    ap = argparse.ArgumentParser(
        description="Cross-check lifted-source callee NAMES against the call "
                    "targets the pristine XBE actually uses.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    ap.add_argument("--function", metavar="NAME|ADDR",
                    help="Filter to one caller (kb name or 0x hex address)")
    ap.add_argument("--files", nargs="+", metavar="PATH",
                    help="Only scan these .c files")
    ap.add_argument("--changed-only", action="store_true",
                    help="Only scan src/*.c changed vs HEAD")
    ap.add_argument("--recent-commits", metavar="N", type=int, default=0,
                    help="Only scan src/*.c changed in the last N commits")
    ap.add_argument("--check", action="store_true",
                    help="Exit 1 on HIGH findings not covered by the baseline")
    ap.add_argument("--update-baseline", action="store_true",
                    help=f"Write current HIGH findings to {BASELINE_PATH.name}")
    ap.add_argument("--json", metavar="PATH", help="Write all findings as JSON")
    ap.add_argument("--verbose", "-v", action="store_true",
                    help="Also show the INFO tier (inlined-away callees)")
    ap.add_argument("--self-test", action="store_true", help="Run self-tests and exit")
    args = ap.parse_args()

    if args.self_test:
        _self_test()
        return 0

    for p, label in [(cac.KB_PATH, "kb.json"), (cac.XBE_PATH, "pristine XBE"),
                     (BOUNDS_PATH, "function_bounds.json")]:
        if not p.exists():
            print(f"ERROR: {label} not found at {p}", file=sys.stderr)
            return 1

    t0 = time.time()
    findings, stats = analyze(
        files=args.files,
        changed_only=args.changed_only,
        recent_commits=args.recent_commits,
        func_filter=args.function,
    )
    elapsed = time.time() - t0

    _print_report(findings, stats, verbose=args.verbose)
    print(f"  Runtime:                   {elapsed:.1f}s")

    if args.json:
        Path(args.json).parent.mkdir(parents=True, exist_ok=True)
        Path(args.json).write_text(json.dumps(
            [f._asdict() for f in findings], indent=2) + "\n")
        print(f"\nJSON written to: {args.json}")

    highs = [f for f in findings if f.severity == "HIGH"]

    if args.update_baseline:
        _write_baseline(highs)
        print(f"\nbaseline written: {BASELINE_PATH} ({len(highs)} entries)")
        return 0

    if args.check:
        new = _new_highs(highs, _load_baseline())
        if new:
            print(f"\n--check FAILED: {len(new)} non-baselined callee-identity "
                  f"mismatch(es) (of {len(highs)} HIGH total):")
            for f in new[:20]:
                print(_fmt(f))
            if len(new) > 20:
                print(f"  ... and {len(new) - 20} more")
            print("\nDecode the CALL targets in the original before touching "
                  "the source: the source name, not the argument order, is the "
                  "thing in question (lift-learnings §61).")
            return 1
        print(f"\n--check OK: {len(highs)} HIGH finding(s), all baselined.")

    return 0


if __name__ == "__main__":
    sys.exit(main())
