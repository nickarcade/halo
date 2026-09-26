#!/usr/bin/env python3
"""Byte-match rule-mining loop over the raw-XBE structural audit.

The structural audit (tools/verify/raw_xbe_structural.py) records, for every
aligned instruction that differs from the original, a difference class such as
``operand_order``, ``register`` or ``stack_offset``.  This tool turns those
records into a verified search loop:

``queue``
    Rank near-exact functions by how few non-knock-on differences remain.

``search``
    For one function, enumerate semantics-preserving source rewrites located
    through the libclang AST, compile each variant with the same VC7.1
    invocation the audit uses, and keep a variant only when its aligned
    matching bytes strictly increase.  Greedy: the best variant of a round
    becomes the base of the next.  Every trial is appended to the ledger.

``rules``
    Summarize the ledger: per (difference class, transform) success rates.
    ``search`` orders its trials by these rates, so proven rules run first.

Transforms (all preserve the expression tree; moved operands that are
themselves operator expressions are parenthesized, so association never
changes):

* ``swap_commutative``: ``a OP b`` to ``b OP a`` for ``== != + * & | ^``.
* ``flip_relational``: ``a < b`` to ``b > a`` (and ``<= > >=``).
* ``invert_if``: ``if (c) S1 else S2`` to ``if (!(c)) S2 else S1``.

Operands that assign (``=``, ``++``, ``--``, compound assignment) are never
moved, and two operands are never swapped when both call functions.  C leaves
the evaluation order of these operands unspecified, so one call beside a
side-effect-free operand may already run in either order.  ``invert_if`` is refused when either branch contains an
assert or ``__LINE__``, because moving a block changes the line number an
implicit assert records.

Nothing is written to the source unless ``--apply`` is given.  An applied
rewrite still needs the normal build, hazard scan and equivalence checks.
"""

import argparse
import concurrent.futures
import hashlib
import json
import os
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "verify"))

import raw_xbe_structural as raw  # noqa: E402
import vc71_verify as vc71  # noqa: E402

RECORDS = ROOT / "artifacts" / "raw_xbe_structural"
OUT_DIR = ROOT / "artifacts" / "bytematch"
LEDGER = OUT_DIR / "ledger.jsonl"

# Classes that are consequences of an earlier size change, not causes.
KNOCK_ON_CLASSES = frozenset(("branch_target",))
COMMUTATIVE_OPS = ("==", "!=", "+", "*", "&", "|", "^")
RELATIONAL_FLIP = {"<": ">", ">": "<", "<=": ">=", ">=": "<="}
BINARY_OPS = sorted(set(COMMUTATIVE_OPS) | set(RELATIONAL_FLIP) |
                    {"-", "/", "%", "<<", ">>", "&&", "||"}, key=len, reverse=True)
# Which transforms plausibly address which difference class, before the
# ledger has evidence.  The ledger's measured rates take precedence.
PRIOR = {
    "operand_order": ("swap_commutative", "flip_relational"),
    "register": ("swap_commutative", "flip_relational", "invert_if"),
    "operands": ("swap_commutative", "flip_relational", "invert_if"),
    "candidate_only:instruction": ("invert_if", "swap_commutative"),
    "reference_only:instruction": ("invert_if", "swap_commutative"),
}
_ASSIGN_RE = re.compile(r"(?<![=!<>])=(?!=)|\+=|-=|\*=|/=|%=|&=|\|=|\^=|<<=|>>=")
_CALL_RE = re.compile(r"[A-Za-z_]\w*\s*\(")


# ---------------------------------------------------------------- records

def load_records(directory=RECORDS):
    """Latest structural record per address, skipping stale duplicates."""
    latest = {}
    for path in directory.glob("*.json"):
        try:
            record = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            continue
        address = record.get("address")
        if not address or record.get("lane") != "raw_xbe_structural":
            continue
        current = latest.get(address)
        if current is None or record.get("generated_at", "") > current.get("generated_at", ""):
            latest[address] = record
    return latest


def fingerprint(record):
    """The difference classes that are causes, not knock-on effects."""
    aligned = record.get("aligned_byte_match") or {}
    classes = aligned.get("difference_classes") or {}
    causes = {kind: count for kind, count in classes.items() if kind not in KNOCK_ON_CLASSES}
    # A branch that differs with nothing else differing jumps somewhere else:
    # that is block layout, a cause in its own right.
    return causes or dict(classes)


def queue(records, minimum_accuracy=0.9, limit=50):
    rows = []
    for record in records.values():
        aligned = record.get("aligned_byte_match") or {}
        if record.get("verdict") != "structural differ" or aligned.get("status") != "scored":
            continue
        accuracy = aligned.get("byte_accuracy")
        if accuracy is None or accuracy < minimum_accuracy:
            continue
        if "difference_classes" not in aligned:
            continue  # audited before difference classes were recorded
        causes = fingerprint(record)
        rows.append({"function": record["function"], "address": record["address"],
                     "source": (record.get("source") or {}).get("path"),
                     "accuracy": accuracy, "causes": causes,
                     "cause_count": sum(causes.values()),
                     "register_argument": bool(record.get("register_argument"))})
    rows.sort(key=lambda row: (row["register_argument"], row["cause_count"], -row["accuracy"]))
    return rows[:limit]


# ---------------------------------------------------------------- AST sites

def read_source(path):
    """File text with its line endings intact (CRLF files stay CRLF)."""
    with open(path, encoding="utf-8", errors="surrogateescape", newline="") as handle:
        return handle.read()


def _clang_args():
    return ["-target", "i386-pc-win32", "-fms-extensions", "-fms-compatibility",
            "-std=gnu89", "-DMSVC", "-DXDK_BUILD", "-DHDATA=",
            "-include", str(ROOT / "src" / "xdk_common.h"),
            "-I", str(ROOT / "build" / "generated"), "-I", str(ROOT / "src"),
            "-I", str(ROOT / "third_party" / "xbox"), "-Wno-everything"]


def _no_assignment(text):
    return "++" not in text and "--" not in text and not _ASSIGN_RE.search(text)


def _movable_pair(left, right):
    """Whether two operands may trade places.

    C leaves the evaluation order of these operands unspecified, so the
    compiler may already evaluate either first.  Swapping is refused when an
    operand assigns, or when both call functions, since two calls could then
    observe each other's effects in a different order.
    """
    if not (_no_assignment(left) and _no_assignment(right)):
        return False
    return not (_CALL_RE.search(left) and _CALL_RE.search(right))


_PRIMARY_RE = re.compile(r"[A-Za-z_0-9.\[\]]+(?:->[A-Za-z_0-9.\[\]]+)*")


def _enclosed(text):
    """True when ``text`` is one parenthesized group, like ``(a + b)``."""
    if not (text.startswith("(") and text.endswith(")")):
        return False
    depth = 0
    for index, char in enumerate(text):
        depth += (char == "(") - (char == ")")
        if depth == 0 and index != len(text) - 1:
            return False
    return True


_LOOSE_KINDS = ("BINARY_OPERATOR", "CONDITIONAL_OPERATOR", "COMPOUND_ASSIGNMENT_OPERATOR")


def _peeled_kind(cursor):
    """The operand's expression kind, looking through implicit wrappers."""
    while cursor is not None and cursor.kind.name == "UNEXPOSED_EXPR":
        children = list(cursor.get_children())
        if len(children) != 1:
            break
        cursor = children[0]
    return cursor.kind.name if cursor is not None else None


def _needs_parens(text, kind=None):
    """Parenthesize a moved operand that binds more loosely than a primary.

    Calls, casts, unary and postfix expressions bind tighter than any binary
    operator and move as they are.  Anything the AST cannot vouch for is
    wrapped, which never changes the tree.
    """
    stripped = text.strip()
    if _PRIMARY_RE.fullmatch(stripped) or _enclosed(stripped):
        return False
    if kind is None:
        return True
    return kind in _LOOSE_KINDS or kind == "UNEXPOSED_EXPR"


def _wrap(text, cursor=None):
    kind = _peeled_kind(cursor) if cursor is not None else None
    return "(%s)" % text if _needs_parens(text, kind) else text


def find_sites(source_path, function, text=None):
    """Transform sites inside ``function``'s definition, as byte spans.

    Each site is a dict with ``kind``, ``start``, ``end`` (offsets into the
    file text), ``replacement`` and a human ``label``.  Spans come from the
    AST and are re-checked against the file text, so a site produced inside a
    macro expansion (whose extent does not spell the operator) is dropped.
    """
    import clang.cindex as clang
    source_path = Path(source_path)
    if text is None:
        text = read_source(source_path)
    data = text.encode("utf-8", errors="surrogateescape")
    index = clang.Index.create()
    tu = index.parse(str(source_path), args=_clang_args(),
                     unsaved_files=[(str(source_path), text)])
    target = None
    for cursor in tu.cursor.get_children():
        if (cursor.kind == clang.CursorKind.FUNCTION_DECL and cursor.spelling == function and
                cursor.is_definition() and cursor.location.file and
                Path(cursor.location.file.name) == source_path):
            target = cursor
    if target is None:
        return [], text

    def span(cursor):
        return cursor.extent.start.offset, cursor.extent.end.offset

    def in_file(cursor):
        start, end = cursor.extent.start, cursor.extent.end
        return (start.file is not None and end.file is not None and
                Path(start.file.name) == source_path and Path(end.file.name) == source_path)

    line_macros = set(re.findall(r"#\s*define\s+(\w+)(?:[^\n]*\\\n)*[^\n]*__LINE__", text))
    sites = []

    def piece(start, end):
        return data[start:end].decode("utf-8", errors="surrogateescape")

    def visit(cursor):
        for child in cursor.get_children():
            visit(child)
        if not in_file(cursor):
            return
        if cursor.kind == clang.CursorKind.BINARY_OPERATOR:
            children = list(cursor.get_children())
            if len(children) != 2 or not all(in_file(child) for child in children):
                return
            (ls, le), (rs, re_) = span(children[0]), span(children[1])
            start, end = span(cursor)
            if not (start == ls and le <= rs and re_ == end):
                return
            between = piece(le, rs)
            operator = between.strip()
            if operator not in BINARY_OPS or between.count(operator) != 1:
                return
            left, right = piece(ls, le), piece(rs, re_)
            if not _movable_pair(left, right):
                return
            line = cursor.extent.start.line
            if operator in COMMUTATIVE_OPS:
                sites.append({"kind": "swap_commutative", "start": start, "end": end,
                              "replacement": "%s%s%s" % (_wrap(right, children[1]), between,
                                                         _wrap(left, children[0])),
                              "label": "line %d: %s %s %s" % (line, left, operator, right)})
            if operator in RELATIONAL_FLIP:
                flipped = between.replace(operator, RELATIONAL_FLIP[operator])
                sites.append({"kind": "flip_relational", "start": start, "end": end,
                              "replacement": "%s%s%s" % (_wrap(right, children[1]), flipped,
                                                         _wrap(left, children[0])),
                              "label": "line %d: %s %s %s" % (line, left, operator, right)})
        elif cursor.kind == clang.CursorKind.IF_STMT:
            children = list(cursor.get_children())
            if len(children) != 3 or not all(in_file(child) for child in children):
                return
            condition, then_branch, else_branch = children
            if else_branch.kind == clang.CursorKind.IF_STMT:
                return  # else-if chains: inverting reorders the chain
            (cs, ce), (ts, te), (es, ee) = span(condition), span(then_branch), span(else_branch)
            then_text, else_text = piece(ts, te), piece(es, ee)
            for branch in (then_text, else_text):
                if ("assert" in branch or "__LINE__" in branch or
                        any(re.search(r"\b%s\b" % name, branch) for name in line_macros)):
                    return
            if then_text.count("\n") != else_text.count("\n"):
                return  # swapping would move lines after the if/else
            if not (then_branch.kind == clang.CursorKind.COMPOUND_STMT and
                    else_branch.kind == clang.CursorKind.COMPOUND_STMT):
                return
            middle = piece(te, es)
            if middle.strip() != "else":
                return
            start = cs
            replacement = "!(%s)%s%s%s%s" % (
                piece(cs, ce), piece(ce, ts), else_text, middle, then_text)
            sites.append({"kind": "invert_if", "start": start, "end": ee,
                          "replacement": replacement,
                          "label": "line %d: if (%s)" % (cursor.extent.start.line,
                                                         piece(cs, ce).strip())})

    visit(target)
    return sites, text


def apply_site(text, site):
    data = text.encode("utf-8", errors="surrogateescape")
    replacement = site["replacement"].encode("utf-8", errors="surrogateescape")
    return (data[:site["start"]] + replacement + data[site["end"]:]).decode(
        "utf-8", errors="surrogateescape")


# ---------------------------------------------------------------- scoring

def _score(aligned):
    """Order variants: exact first, then matching bytes, then exact insns."""
    if not aligned or aligned.get("status") != "scored":
        return (-1, -1.0, -1)
    exact = int(aligned.get("byte_accuracy") == 1.0 and not aligned.get("mismatched_relocations"))
    return (exact, aligned.get("byte_accuracy") or 0.0,
            aligned.get("normalized_exact_instructions") or 0)


def evaluate(source, function, address, text, tag, opt):
    """Compile ``text`` in place of ``source`` and audit ``function``.

    The variant is written next to the original so relative includes resolve,
    with a #line directive so __FILE__ and __LINE__ match a compile of the
    original file exactly.  Temps are always removed.
    """
    source = Path(source)
    digest = hashlib.sha256(text.encode("utf-8", errors="surrogateescape")).hexdigest()[:10]
    variant = source.parent / (".bytematch_%s_%s_%s" % (tag, digest, source.name))
    candidate = OUT_DIR / "obj" / ("%s_%s.obj" % (function, digest))
    candidate.parent.mkdir(parents=True, exist_ok=True)
    win_path = vc71.wsl_to_win(source).replace("\\", "\\\\")
    try:
        with open(variant, "w", encoding="utf-8", errors="surrogateescape", newline="") as handle:
            handle.write('#line 1 "%s"\n' % win_path)
            handle.write(text)
        if not vc71.compile_vc71(variant, candidate, opt=opt):
            return {"status": "compile_failed"}
        record = raw.audit(candidate, function, address, source)
        return record.get("aligned_byte_match") or {"status": "unavailable"}
    except Exception as exc:  # a broken variant must not stop the search
        return {"status": "error", "reason": str(exc)}
    finally:
        variant.unlink(missing_ok=True)
        candidate.unlink(missing_ok=True)


# ---------------------------------------------------------------- ledger

def read_ledger(path=LEDGER):
    rows = []
    if path.is_file():
        for line in path.read_text(encoding="utf-8").splitlines():
            try:
                rows.append(json.loads(line))
            except ValueError:
                continue
    return rows


def append_ledger(rows, path=LEDGER):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "a", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")


def rule_table(rows):
    """{(class, transform): {"trials", "improved", "rate"}} from ledger rows."""
    table = {}
    for row in rows:
        for kind in row.get("causes") or {}:
            entry = table.setdefault((kind, row["transform"]), {"trials": 0, "improved": 0})
            entry["trials"] += 1
            entry["improved"] += int(bool(row.get("improved")))
    for entry in table.values():
        # Laplace-smoothed so an unseen pair neither dominates nor vanishes.
        entry["rate"] = (entry["improved"] + 1) / (entry["trials"] + 2)
    return table


def order_sites(sites, causes, table):
    def priority(site):
        rates = [table.get((kind, site["kind"]), {}).get("rate") for kind in causes]
        measured = max([rate for rate in rates if rate is not None], default=None)
        prior = any(site["kind"] in PRIOR.get(kind, ()) for kind in causes)
        return (-(measured if measured is not None else (0.5 if prior else 0.25)), site["start"])
    return sorted(sites, key=priority)


# ---------------------------------------------------------------- search

def search(function, address, source, rounds=3, max_trials=60, workers=4, log=print):
    source = Path(source)
    address_int = int(address, 0) if isinstance(address, str) else address
    opt = raw._function_opt(source, function)
    text = read_source(source)
    if not vc71.regen_decl_header(quiet=True):
        raise SystemExit("generated declaration header regeneration failed")
    base = evaluate(source, function, address_int, text, "base", opt)
    if base.get("status") != "scored":
        raise SystemExit("baseline did not score: %s" % base)
    causes = fingerprint({"aligned_byte_match": base})
    log("baseline %s: aligned %.4f, exact insns %d/%d, causes %s" % (
        function, base["byte_accuracy"], base["normalized_exact_instructions"],
        base["aligned_instruction_pairs"], causes))
    table = rule_table(read_ledger())
    applied = []
    trials = 0
    ledger_rows = []
    current_text, current = text, base
    for round_index in range(rounds):
        if _score(current)[0] == 1:
            break
        sites, _ = find_sites(source, function, current_text)
        sites = order_sites(sites, causes, table)[:max(0, max_trials - trials)]
        if not sites:
            break
        log("round %d: %d candidate rewrites" % (round_index + 1, len(sites)))
        # compile_vc71 writes a per-process declaration shadow. Threads would
        # share that path and race; processes get distinct PIDs and shadows.
        with concurrent.futures.ProcessPoolExecutor(max_workers=workers) as executor:
            futures = {executor.submit(evaluate, source, function, address_int,
                                       apply_site(current_text, site), "r%d" % index, opt): site
                       for index, site in enumerate(sites)}
            results = [(futures[future], future.result())
                       for future in concurrent.futures.as_completed(futures)]
        trials += len(results)
        best = None
        for site, aligned in results:
            improved = _score(aligned) > _score(current)
            ledger_rows.append({
                "at": datetime.now(timezone.utc).isoformat(), "function": function,
                "address": "0x%08x" % address_int, "source": str(source.relative_to(ROOT)),
                "transform": site["kind"], "site": site["label"], "causes": causes,
                "status": aligned.get("status"),
                "before": current.get("byte_accuracy"), "after": aligned.get("byte_accuracy"),
                "improved": improved})
            if improved and (best is None or _score(aligned) > _score(best[1])):
                best = (site, aligned)
        if best is None:
            log("round %d: no rewrite improved the aligned bytes" % (round_index + 1))
            break
        site, aligned = best
        log("round %d: keep %s (%s): %.4f -> %.4f" % (
            round_index + 1, site["kind"], site["label"],
            current["byte_accuracy"], aligned["byte_accuracy"]))
        applied.append({"transform": site["kind"], "site": site["label"],
                        "before": current["byte_accuracy"], "after": aligned["byte_accuracy"]})
        current_text, current = apply_site(current_text, site), aligned
    append_ledger(ledger_rows)
    return {"function": function, "address": "0x%08x" % address_int,
            "source": str(source), "baseline": base.get("byte_accuracy"),
            "final": current.get("byte_accuracy"), "exact": _score(current)[0] == 1,
            "trials": trials, "applied": applied,
            "text": current_text if applied else None, "original_text": text}


# ---------------------------------------------------------------- param returns

def param_return_variant(source, function, param_index, text=None):
    """Source text where ``function`` returns its parameter ``param_index``.

    Returns ``(text, return_type, param_name)`` or ``(None, reason, None)``.
    The return type becomes the parameter's own type, every bare ``return;``
    becomes ``return <param>;``, and a body that can fall off its end gets
    ``return <param>;`` before its closing brace, on the same line so no
    later ``__LINE__`` moves.
    """
    import clang.cindex as clang
    source = Path(source)
    if text is None:
        text = read_source(source)
    data = text.encode("utf-8", errors="surrogateescape")
    tu = clang.Index.create().parse(str(source), args=_clang_args(),
                                    unsaved_files=[(str(source), text)])
    target = None
    for cursor in tu.cursor.get_children():
        if (cursor.kind == clang.CursorKind.FUNCTION_DECL and cursor.spelling == function and
                cursor.is_definition() and cursor.location.file and
                Path(cursor.location.file.name) == source):
            target = cursor
    if target is None:
        return None, "definition not found", None
    params = [child for child in target.get_children() if child.kind == clang.CursorKind.PARM_DECL]
    if param_index >= len(params):
        return None, "parameter %d not in definition" % param_index, None
    param = params[param_index]
    name, ptype = param.spelling, param.type.spelling
    if not name or "(" in ptype or "[" in ptype:
        return None, "parameter type %r cannot be a return type as written" % ptype, None
    body = [child for child in target.get_children() if child.kind == clang.CursorKind.COMPOUND_STMT]
    if len(body) != 1:
        return None, "no single body", None
    body = body[0]
    header = data[target.extent.start.offset:body.extent.start.offset].decode(
        "utf-8", errors="surrogateescape")
    match = re.match(r"(\s*(?:static\s+)?)void\b", header)
    if not match:
        return None, "definition does not start with void", None
    edits = [(target.extent.start.offset + match.start(0) + len(match.group(1)),
              target.extent.start.offset + match.end(0),
              ptype if ptype.endswith("*") else ptype + " ")]
    if not ptype.endswith("*"):
        edits[0] = (edits[0][0], edits[0][1], ptype)

    def returns(cursor):
        for child in cursor.get_children():
            if child.kind == clang.CursorKind.RETURN_STMT:
                yield child
            else:
                yield from returns(child)

    for ret in returns(body):
        if list(ret.get_children()):
            return None, "function already returns a value somewhere", None
        start = ret.extent.start.offset
        if data[start:start + 6] != b"return":
            return None, "return inside a macro", None
        edits.append((start + 6, start + 6, " " + name))
    statements = list(body.get_children())
    if not statements or statements[-1].kind != clang.CursorKind.RETURN_STMT:
        close = body.extent.end.offset - 1
        if data[close:close + 1] != b"}":
            return None, "body end not found", None
        edits.append((close, close, "return %s; " % name))
    out = data
    for start, end, replacement in sorted(edits, reverse=True):
        out = out[:start] + replacement.encode("utf-8") + out[end:]
    return out.decode("utf-8", errors="surrogateescape"), ptype, name


def _prototype_shadow(function, return_type):
    """Wrap vc71's declaration shadow so ``function`` is declared returning
    ``return_type``.  Returns an undo callable."""
    original = vc71._make_fastcall_decl_shadow
    pattern = re.compile(r"\bvoid(\s+(?:__\w+\s+)*)(%s\s*\()" % re.escape(function))
    spelled = return_type if return_type.endswith("*") else return_type + " "

    def shadow(names):
        directory = original(names)
        source = (directory / "decl.h") if directory else vc71.BUILD_DIR / "generated" / "decl.h"
        text = source.read_text()
        new_text, count = pattern.subn(lambda m: spelled.rstrip() + (
            " " if not spelled.endswith("*") else "") + m.group(1).lstrip() + m.group(2), text)
        if directory is None:
            directory = OUT_DIR / "decl_shadow" / ("p%d" % os.getpid())
            directory.mkdir(parents=True, exist_ok=True)
        (directory / "decl.h").write_text(new_text if count else text)
        return directory

    vc71._make_fastcall_decl_shadow = shadow

    def undo():
        vc71._make_fastcall_decl_shadow = original
    return undo


def verify_param_returns(detector_json, limit=None, log=print):
    """Compile each void param-return candidate returning its parameter."""
    detected = json.loads(Path(detector_json).read_text(encoding="utf-8"))
    rows = [row for row in detected.get("functions", [])
            if row.get("class") == "param_return" and row.get("decl_status") == "void" and
            row.get("confidence") in ("high", "medium")]
    records = {int(record["address"], 16): record for record in load_records().values()}
    rows.sort(key=lambda row: (
        (records.get(int(row["addr"], 16)) or {}).get("verdict") == "structural exact",
        int(row["addr"], 16)))
    if not vc71.regen_decl_header(quiet=True):
        raise SystemExit("generated declaration header regeneration failed")
    results = []
    for row in rows[:limit]:
        address = int(row["addr"], 16)
        record = records.get(address)
        name = row["name"]
        source = (record or {}).get("source", {}).get("path")
        entry = {"function": name, "address": "0x%08x" % address, "param": row["param"]}
        if not source:
            entry["status"] = "no audited source"
            results.append(entry)
            continue
        if record.get("verdict") == "structural exact":
            entry["status"] = "already_exact"
            results.append(entry)
            continue
        text = read_source(source)
        variant, detail, param_name = param_return_variant(source, name, row["param"], text)
        if variant is None:
            entry.update(status="not rewritable", reason=detail)
            results.append(entry)
            continue
        opt = raw._function_opt(Path(source), name)
        # Use the already-populated baseline.  Recompiling the original after
        # changing the prototype shadow would not be the same experiment.
        before = record.get("aligned_byte_match") or {"status": "unavailable"}
        undo = _prototype_shadow(name, detail)
        try:
            after = evaluate(source, name, address, variant, "pra", opt)
        finally:
            undo()
        entry.update(return_type=detail, param_name=param_name,
                     before=before.get("byte_accuracy"), after=after.get("byte_accuracy"),
                     after_status=after.get("status"),
                     exact=_score(after)[0] == 1)
        entry["status"] = ("confirmed" if _score(after) > _score(before) else
                           "no_gain" if after.get("status") == "scored" else "compile_failed")
        if entry["status"] == "confirmed":
            relative = str(Path(source).resolve().relative_to(ROOT))
            patch_path = OUT_DIR / "param_returns" / ("%s.patch" % name)
            patch_path.parent.mkdir(parents=True, exist_ok=True)
            patch_path.write_text(_diff(text, variant, relative), encoding="utf-8")
            entry["patch"] = str(patch_path.relative_to(ROOT))
        log("%-44s %-15s %s -> %s" % (name, entry["status"], entry.get("before"), entry.get("after")))
        results.append(entry)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    (OUT_DIR / "param_returns.json").write_text(json.dumps(results, indent=1), encoding="utf-8")
    return results


# Differences no expression rewrite can change: a wrong constant or a wrong
# relocation target is a source fact, found by reading, not by search.
UNSEARCHABLE_CLASSES = frozenset(("relocation_target", "immediate", "displacement"))


def run_batch(args):
    records = load_records()
    rows = [row for row in queue(records, args.min_accuracy, 100000)
            if not row["register_argument"] and row["source"] and
            not set(row["causes"]) <= UNSEARCHABLE_CLASSES]
    rows = rows[:args.limit]
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    results = []
    for index, row in enumerate(rows, 1):
        print("[%d/%d] %s %s" % (index, len(rows), row["function"], row["causes"]), flush=True)
        try:
            result = search(row["function"], row["address"], row["source"], rounds=3,
                            max_trials=args.max_trials, workers=args.workers,
                            log=lambda message: print("  " + message, flush=True))
        except SystemExit as exc:
            print("  skipped: %s" % exc, flush=True)
            results.append({"function": row["function"], "skipped": str(exc)})
            continue
        entry = {key: result[key] for key in ("function", "address", "baseline", "final",
                                               "exact", "trials", "applied")}
        if result["text"] is not None:
            relative = str(Path(result["source"]).resolve().relative_to(ROOT))
            patch_path = OUT_DIR / ("%s.patch" % row["function"])
            patch_path.write_text(_diff(result["original_text"], result["text"], relative),
                                  encoding="utf-8")
            entry["patch"] = str(patch_path.relative_to(ROOT))
        results.append(entry)
    (OUT_DIR / "batch.json").write_text(json.dumps(results, indent=1), encoding="utf-8")
    improved = [entry for entry in results if entry.get("applied")]
    print("improved %d of %d; byte-exact %d; results in %s" % (
        len(improved), len(results), sum(1 for entry in improved if entry["exact"]),
        (OUT_DIR / "batch.json").relative_to(ROOT)))
    return 0


def _diff(original, updated, path):
    import difflib
    return "".join(difflib.unified_diff(original.splitlines(True), updated.splitlines(True),
                                        "a/" + path, "b/" + path))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    q = sub.add_parser("queue", help="rank near-exact functions")
    q.add_argument("--min-accuracy", type=float, default=0.9)
    q.add_argument("--limit", type=int, default=50)
    q.add_argument("--json", action="store_true")
    s = sub.add_parser("search", help="search source rewrites for one function")
    s.add_argument("function")
    s.add_argument("--rounds", type=int, default=3)
    s.add_argument("--max-trials", type=int, default=60)
    s.add_argument("--workers", type=int, default=4)
    s.add_argument("--apply", action="store_true", help="write the best variant to the source")
    sub.add_parser("rules", help="summarize the ledger")
    bt = sub.add_parser("batch", help="search the top of the queue; never applies")
    bt.add_argument("--limit", type=int, default=20)
    bt.add_argument("--min-accuracy", type=float, default=0.9)
    bt.add_argument("--max-trials", type=int, default=40)
    bt.add_argument("--workers", type=int, default=4)
    pr = sub.add_parser("verify-returns",
                        help="compile detect_param_return.py candidates returning their param")
    pr.add_argument("detector_json")
    pr.add_argument("--limit", type=int)
    args = parser.parse_args(argv)

    if args.command == "verify-returns":
        results = verify_param_returns(args.detector_json, args.limit)
        counts = {}
        for entry in results:
            counts[entry["status"]] = counts.get(entry["status"], 0) + 1
        print(counts, "->", (OUT_DIR / "param_returns.json").relative_to(ROOT))
        return 0
    if args.command == "batch":
        return run_batch(args)

    if args.command == "queue":
        rows = queue(load_records(), args.min_accuracy, args.limit)
        if args.json:
            print(json.dumps(rows, indent=1))
        else:
            for row in rows:
                print("%-44s %s %6.2f%%  %s%s" % (
                    row["function"][:44], row["address"], 100 * row["accuracy"],
                    ", ".join("%s=%d" % item for item in sorted(row["causes"].items())) or "-",
                    "  [reg-arg]" if row["register_argument"] else ""))
        return 0
    if args.command == "rules":
        table = rule_table(read_ledger())
        for (kind, transform), entry in sorted(table.items(), key=lambda item: -item[1]["rate"]):
            print("%-32s %-18s %4d trials %4d improved  rate %.2f" % (
                kind, transform, entry["trials"], entry["improved"], entry["rate"]))
        return 0

    records = load_records()
    matches = [record for record in records.values() if record.get("function") == args.function]
    if len(matches) != 1 or not (matches[0].get("source") or {}).get("path"):
        raise SystemExit("need exactly one audited record with a source for %s (found %d)" % (
            args.function, len(matches)))
    record = matches[0]
    result = search(args.function, record["address"], record["source"]["path"],
                    args.rounds, args.max_trials, args.workers)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    print("%s: %.4f -> %.4f after %d trials%s" % (
        args.function, result["baseline"], result["final"], result["trials"],
        " (byte-exact)" if result["exact"] else ""))
    if result["text"] is None:
        return 1
    relative = str(Path(result["source"]).resolve().relative_to(ROOT))
    patch = _diff(result["original_text"], result["text"], relative)
    patch_path = OUT_DIR / ("%s.patch" % args.function)
    patch_path.write_text(patch, encoding="utf-8")
    print(patch)
    print("patch: %s" % patch_path)
    if args.apply:
        with open(result["source"], "w", encoding="utf-8", errors="surrogateescape",
                  newline="") as handle:
            handle.write(result["text"])
        print("applied to %s; run the build, hazard scan and equivalence gate next" % relative)
    return 0


if __name__ == "__main__":
    sys.exit(main())
