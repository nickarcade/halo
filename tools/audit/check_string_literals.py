#!/usr/bin/env python3
"""
check_string_literals.py — Cross-check the string literals a lift writes
against the strings the pristine XBE function actually references.

Every call-site gate compares code: callee identity, argument counts, types.
None compares DATA the function names by address.  A log or error message is
the most common such datum, and a lift that retypes it by hand can drift from
the original without moving a single instruction: the literal lands in our
.rdata at a new address, VC71 sees the same `push imm32` shape, and nothing
fails.  Example: network_game_server_adjust_machine_settings logged
"... failed in network_game_server_get_client_connection()" — the name of a
different function — and survived every existing audit.

Method
------
For each `ported: true` kb.json function with a same-named definition in src/:

  1. Linearly disassemble [addr, function_bounds.json end) and collect every
     32-bit immediate and memory displacement.  A value that points at a
     NUL-terminated printable run in the XBE is a string the function
     references.
  2. Collect the string literals in the C body, joining adjacent literals the
     way the compiler does and decoding C escapes.  Wide (L"...") literals are
     skipped.
  3. A literal is MATCHED when some operand points at exactly those bytes plus
     a NUL.  Otherwise it is a finding:

  HIGH  unmatched, and a referenced-but-unmatched binary string (an "orphan")
        is similar (difflib ratio >= 0.75).  The transcription-error signature:
        the original says one thing, the lift says nearly the same thing.
  WARN  unmatched, the literal exists nowhere in the XBE, no similar orphan.
        Invented text, or text the original built at runtime.
  INFO  unmatched, but the exact string exists elsewhere in the XBE.  Usually
        an inlined callee's string or one reached through a table — shown
        with -v only.

Known false positives: strings reached through a global pointer or table
(no immediate in the body), stale or short function_bounds.json `end`, and
literals that the original assembled differently (sprintf pieces).  Assert
text produced by assert_halt()'s #cond is not a literal and is not checked
here; recover_assert_sites.py owns that.

Usage:
    python3 tools/audit/check_string_literals.py                 # HIGH + WARN
    python3 tools/audit/check_string_literals.py -v              # + INFO
    python3 tools/audit/check_string_literals.py --function network_game_server_adjust_machine_settings
    python3 tools/audit/check_string_literals.py --files src/halo/networking/network_server_manager.c
    python3 tools/audit/check_string_literals.py --changed-only
    python3 tools/audit/check_string_literals.py --check         # exit 1 on HIGH
    python3 tools/audit/check_string_literals.py --json <path>
"""

from __future__ import annotations

import argparse
import difflib
import json
import re
import sys
from pathlib import Path
from typing import Dict, List, NamedTuple, Optional, Set, Tuple

import capstone
from capstone import x86 as cs_x86

sys.path.insert(0, str(Path(__file__).resolve().parent))

import check_arg_counts as cac  # noqa: E402  (XBE loader, kb loader)
import check_callee_identity as cci  # noqa: E402  (bounds, source walking)

REPO_ROOT = cac.REPO_ROOT

_SIMILAR_MIN = 0.75

# Deliberate, reviewed divergences from the original text: {function: reason}.
ALLOWLIST = {
    "display_assert": "e841c5112 appends [rev=%s] (build revision) to the "
                      "exception message on purpose",
}

# Deliberate project-side literals (debug tooling, not transcription):
# {function: (reason, (literal prefixes...))}.
ALLOWED_LITERALS = {
    "unattached_impulse_sound_new": ("72768df8e scale-OOB diagnostic logging", (b"DIAG ",)),
    "object_impulse_sound_new": ("72768df8e scale-OOB diagnostic logging", (b"DIAG ",)),
    "main_loop": ("649b86787 die-to-core fixture hook", (b"d:\\die_to_core.xts",)),
    "input_check_state_mode": ("649b86787 fixture core-loop hook", (b"d:\\core_loop.xts",)),
}

_ORPHAN_MIN_LEN = 4
_MAX_STRING = 1024

_cs: Optional[capstone.Cs] = None


def _get_cs() -> capstone.Cs:
    global _cs
    if _cs is None:
        _cs = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        _cs.detail = True
        _cs.skipdata = True
    return _cs


# ---------------------------------------------------------------------------
# Pristine-XBE side
# ---------------------------------------------------------------------------

def _cstring_at(va: int) -> Optional[bytes]:
    """NUL-terminated bytes at va (without the NUL), or None if unmapped."""
    cac._load_xbe()
    for i, (vaddr, vsize, _raw, _rsize) in enumerate(cac._xbe_sections):
        if vaddr <= va < vaddr + vsize:
            data = cac._xbe_bytes_cache[i]
            off = va - vaddr
            end = data.find(b"\0", off, off + _MAX_STRING)
            if end < 0:
                return None
            return data[off:end]
    return None


def _printable(b: bytes) -> bool:
    return all(0x20 <= c < 0x7F or c in (0x09, 0x0A, 0x0D) for c in b)


_xbe_blob: Optional[bytes] = None


def _in_xbe(s: bytes) -> bool:
    """True when s followed by NUL appears anywhere in the mapped sections."""
    global _xbe_blob
    if _xbe_blob is None:
        cac._load_xbe()
        _xbe_blob = b"\0".join(cac._xbe_bytes_cache[i]
                               for i in range(len(cac._xbe_sections)))
    return (s + b"\0") in _xbe_blob


def referenced_strings(start: int, end: int) -> Tuple[Dict[int, bytes], int]:
    """{va: bytes} for every operand value in [start, end) that points at a
    NUL-terminated string, plus the count of decoded instructions."""
    code = cci._va_bytes(start, end)
    if not code:
        return {}, 0
    out: Dict[int, bytes] = {}
    decoded = 0
    for ins in _get_cs().disasm(code, start):
        decoded += 1
        try:
            ops = ins.operands
        except capstone.CsError:
            continue
        for op in ops:
            if op.type == cs_x86.X86_OP_IMM:
                v = op.imm & 0xFFFFFFFF
            elif op.type == cs_x86.X86_OP_MEM and op.mem.base == 0 and op.mem.index == 0:
                v = op.mem.disp & 0xFFFFFFFF
            else:
                continue
            if v < 0x10000 or v in out:
                continue
            s = _cstring_at(v)
            if s is not None:
                out[v] = s
    return out, decoded


# ---------------------------------------------------------------------------
# Source side
# ---------------------------------------------------------------------------

_ESC = {"n": "\n", "t": "\t", "r": "\r", "0": "\0", "\\": "\\", "'": "'",
        '"': '"', "?": "?", "a": "\a", "b": "\b", "f": "\f", "v": "\v"}


def _decode_c(body: str) -> bytes:
    out = bytearray()
    i = 0
    while i < len(body):
        c = body[i]
        if c != "\\" or i + 1 >= len(body):
            out += c.encode("latin-1", "replace")
            i += 1
            continue
        n = body[i + 1]
        if n == "x":
            m = re.match(r"[0-9a-fA-F]+", body[i + 2:])
            if m:
                out.append(int(m.group(0), 16) & 0xFF)
                i += 2 + len(m.group(0))
                continue
        if n in "01234567":
            m = re.match(r"[0-7]{1,3}", body[i + 1:])
            out.append(int(m.group(0), 8) & 0xFF)
            i += 1 + len(m.group(0))
            continue
        out += _ESC.get(n, n).encode("latin-1", "replace")
        i += 2
    return bytes(out)


class Literal(NamedTuple):
    value: bytes
    line:  int       # 1-based
    text:  str       # source spelling, for the report


_ASM_RE = re.compile(r"(?:__asm__|__asm|asm)\b\s*(?:__volatile__|volatile)?\s*\(")

# src/common.h assert macros stringify their condition (#cond); a literal
# inside it is part of the assert text, not a string of its own.
_ASSERT_RE = re.compile(r"\b(assert_halt(?:_msg)?(?:_at)?)\s*\(")
_COND_ARG = {"assert_halt": 0, "assert_halt_msg": 0,
             "assert_halt_at": -1, "assert_halt_msg_at": -1}


def _assert_cond_spans(blank: str, lo: int, hi: int) -> Dict[int, int]:
    """{start: end} of every assert-macro condition argument in [lo, hi)."""
    spans: Dict[int, int] = {}
    for m in _ASSERT_RE.finditer(blank, lo, hi):
        close = cci._match_delims(blank, m.end() - 1, "(", ")")
        if close < 0:
            continue
        args, depth, start = [], 0, m.end()
        for k in range(m.end(), close):
            c = blank[k]
            if c in "([{":
                depth += 1
            elif c in ")]}":
                depth -= 1
            elif c == "," and depth == 0:
                args.append((start, k)); start = k + 1
        args.append((start, close))
        a, b = args[_COND_ARG[m.group(1)]]
        spans[a] = b
    return spans


def body_literals(raw: str, blank: str, lo: int, hi: int) -> Tuple[List[Literal], int]:
    """Narrow string literals in raw[lo:hi], adjacent ones joined.

    `blank` is raw with comments and literals blanked (same offsets); it
    locates inline-asm statements, whose templates and constraints are not
    data and are skipped.  Returns (literals, wide_count)."""
    lits: List[Literal] = []
    wide = 0
    i = lo
    cur: Optional[List] = None      # [bytes, line, text]
    skip = _assert_cond_spans(blank, lo, hi)
    while i < hi:
        if i in skip:
            if cur:
                lits.append(Literal(cur[0], cur[1], cur[2])); cur = None
            i = skip[i]
            continue
        ch = raw[i]
        nxt = raw[i + 1] if i + 1 < hi else ""
        if ch == "/" and nxt == "/":
            j = raw.find("\n", i)
            i = hi if j < 0 else j
            continue
        if ch == "/" and nxt == "*":
            j = raw.find("*/", i + 2)
            i = hi if j < 0 else j + 2
            continue
        if ch in " \t\r\n":
            i += 1
            continue
        if ch in "a_" and (i == 0 or not (raw[i - 1].isalnum() or raw[i - 1] == "_")):
            am = _ASM_RE.match(blank, i)
            if am:
                close = cci._match_delims(blank, am.end() - 1, "(", ")")
                if close > 0:
                    if cur:
                        lits.append(Literal(cur[0], cur[1], cur[2])); cur = None
                    i = close + 1
                    continue
        if ch == "'":
            j = i + 1
            while j < hi and raw[j] != "'":
                j += 2 if raw[j] == "\\" else 1
            i = j + 1
            if cur:
                lits.append(Literal(cur[0], cur[1], cur[2])); cur = None
            continue
        is_wide = ch == "L" and nxt == '"' and (i == 0 or not (raw[i - 1].isalnum() or raw[i - 1] == "_"))
        if ch == '"' or is_wide:
            q = i + 1 if is_wide else i
            j = q + 1
            while j < hi and raw[j] != '"':
                j += 2 if raw[j] == "\\" else 1
            piece = raw[q + 1:j]
            i = j + 1
            if is_wide:
                wide += 1
                if cur:
                    lits.append(Literal(cur[0], cur[1], cur[2])); cur = None
                continue
            if cur is None:
                cur = [b"", raw.count("\n", 0, q) + 1, ""]
            cur[0] += _decode_c(piece)
            cur[2] += '"' + piece + '"'
            continue
        if cur:
            lits.append(Literal(cur[0], cur[1], cur[2])); cur = None
        i += 1
    if cur:
        lits.append(Literal(cur[0], cur[1], cur[2]))
    return lits, wide


class SrcFunc(NamedTuple):
    name: str
    path: Path
    literals: List[Literal]
    wide: int


def parse_file(path: Path) -> List[SrcFunc]:
    try:
        raw = path.read_text(errors="replace")
    except OSError:
        return []
    text = cci.cac_blank(raw)
    out: List[SrcFunc] = []
    pos = 0
    while True:
        m = cci._DEF_HEAD_RE.search(text, pos)
        if not m:
            break
        name = m.group(1)
        pos = m.end()
        if name in cci._NOT_CALLS:
            continue
        close_paren = cci._match_delims(text, m.end() - 1, "(", ")")
        if close_paren < 0:
            continue
        k = close_paren + 1
        while k < len(text) and text[k] in " \t\r\n":
            k += 1
        if k >= len(text) or text[k] != "{":
            continue
        close_brace = cci._match_delims(text, k, "{", "}")
        if close_brace < 0:
            continue
        lits, wide = body_literals(raw, text, k, close_brace)
        out.append(SrcFunc(name, path, lits, wide))
        pos = close_brace + 1
    return out


# ---------------------------------------------------------------------------
# Analysis
# ---------------------------------------------------------------------------

class Finding(NamedTuple):
    severity:  str       # HIGH / WARN / INFO
    function:  str
    addr:      str
    path:      str
    line:      int
    literal:   str       # decoded source value
    candidate: str       # most similar orphan, "" if none
    cand_addr: str
    ratio:     float


def _show(b: bytes) -> str:
    return b.decode("latin-1").encode("unicode_escape").decode("ascii")


def analyze(files: Optional[List[str]] = None,
            changed_only: bool = False,
            func_filter: Optional[str] = None) -> Tuple[List[Finding], Dict]:
    entries = cac._load_kb()
    name_addrs: Dict[str, Set[int]] = {}
    ported: Dict[int, bool] = {}
    for e in entries:
        if e.name:
            name_addrs.setdefault(e.name, set()).add(e.addr)
        ported[e.addr] = e.ported
    bounds = cci._load_bounds()

    funcs: Dict[str, SrcFunc] = {}
    dups: Set[str] = set()
    for p in cci._collect_src_files(files, changed_only, 0):
        if not p.is_file():
            continue
        for sf in parse_file(p):
            if sf.name in funcs:
                dups.add(sf.name)
            else:
                funcs[sf.name] = sf

    stats = {"source_functions": len(funcs), "analyzed": 0, "no_kb_entry": 0,
             "not_ported": 0, "no_bounds": 0, "duplicate_names": len(dups),
             "literals": 0, "matched": 0, "wide_skipped": 0,
             "high": 0, "warn": 0, "info": 0}
    findings: List[Finding] = []

    for name, sf in sorted(funcs.items()):
        if name in dups or name in ALLOWLIST:
            continue
        addrs = name_addrs.get(name)
        if not addrs:
            stats["no_kb_entry"] += 1
            continue
        addr = min(addrs)
        if func_filter is not None and func_filter not in (name, "0x%x" % addr):
            continue
        if not ported.get(addr):
            stats["not_ported"] += 1
            continue
        if not sf.literals:
            continue
        end = bounds.get(addr)
        if end is None or end <= addr:
            stats["no_bounds"] += 1
            continue
        refs, decoded = referenced_strings(addr, end)
        if not decoded:
            stats["no_bounds"] += 1
            continue
        stats["analyzed"] += 1
        stats["wide_skipped"] += sf.wide

        ref_values = set(refs.values())
        used: Set[bytes] = set()
        unmatched: List[Literal] = []
        for lit in sf.literals:
            stats["literals"] += 1
            # An embedded NUL (a memcmp key) ends the C string the operand
            # points at; only the leading run is comparable.
            if lit.value.split(b"\0", 1)[0] in ref_values:
                stats["matched"] += 1
                used.add(lit.value)
            elif any(lit.value.startswith(pre)
                     for pre in ALLOWED_LITERALS.get(name, ("", ()))[1]):
                stats["matched"] += 1
            else:
                unmatched.append(lit)
        orphans = {va: s for va, s in refs.items()
                   if s not in used and len(s) >= _ORPHAN_MIN_LEN and _printable(s)}

        for lit in unmatched:
            best_va, best_s, best_r = 0, b"", 0.0
            for va, s in orphans.items():
                r = difflib.SequenceMatcher(None, lit.value, s).ratio()
                if r > best_r:
                    best_va, best_s, best_r = va, s, r
            if best_r >= _SIMILAR_MIN:
                sev = "HIGH"
            elif _in_xbe(lit.value):
                sev = "INFO"
            else:
                sev = "WARN"
            stats[sev.lower()] += 1
            findings.append(Finding(
                sev, name, "0x%x" % addr, cci._relpath(sf.path), lit.line,
                _show(lit.value), _show(best_s) if best_r >= _SIMILAR_MIN else "",
                "0x%x" % best_va if best_r >= _SIMILAR_MIN else "", round(best_r, 3)))

    return findings, stats


def report(findings: List[Finding], stats: Dict, verbose: bool) -> None:
    order = {"HIGH": 0, "WARN": 1, "INFO": 2}
    for f in sorted(findings, key=lambda f: (order[f.severity], f.path, f.line)):
        if f.severity == "INFO" and not verbose:
            continue
        print("%-4s %s:%d  %s@%s" % (f.severity, f.path, f.line, f.function, f.addr))
        print("       ours:     \"%s\"" % f.literal)
        if f.candidate:
            print("       original: \"%s\"  (@%s, ratio %.2f)" % (f.candidate, f.cand_addr, f.ratio))
    print("\n" + " ".join("%s=%d" % kv for kv in stats.items()))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--files", nargs="*")
    ap.add_argument("--function")
    ap.add_argument("--changed-only", action="store_true")
    ap.add_argument("--check", action="store_true", help="exit 1 on any HIGH")
    ap.add_argument("--json")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    findings, stats = analyze(args.files, args.changed_only, args.function)
    report(findings, stats, args.verbose)
    if args.json:
        Path(args.json).write_text(json.dumps(
            {"stats": stats, "findings": [f._asdict() for f in findings]}, indent=2))
    return 1 if args.check and stats["high"] else 0


if __name__ == "__main__":
    sys.exit(main())
