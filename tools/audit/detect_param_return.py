#!/usr/bin/env python3
"""Report-only detector: functions whose original binary returns a stack param.

WHAT IT FINDS
-------------
When the original code leaves one of its own stack parameters in EAX at every
exit, the original source almost certainly returned that parameter -- the
classic out-pointer idiom:

    float *vector_add(const float *a, const float *b, float *result)
    { ...; return result; }

A lift that declares such a function `void` drops the final
`mov eax, [esp+N]` and loses bytes (FUN_00012140 went byte-identical under
VC7.1 once its `void` became `float *` + `return result;`).  It is also a
latent correctness risk (lift-silent-bugs §16) when an unported caller reads
EAX.

This tool NEVER edits code or kb.json.  Changing a return type changes the ABI
declaration and needs caller review, so its output is a proposal list only.

HOW
---
1. Bytes come from the pristine XBE via tools/verify/xbe_reference.py
   (function_bytes, bounded by tools/verify/function_bounds.json).
2. Recursive-descent decode with capstone from the entry (direct branches and
   MSVC `jmp [reg*4 + table]` switch tables are followed; other indirect jumps
   leave the CFG marked incomplete).
3. For every exit -- `ret`/`ret N`, or a tail `jmp` leaving the function -- walk
   BACKWARDS over all predecessor paths to the reaching EAX writer.  A `call`
   counts as an EAX writer (callee result).  ESP depth is reconstructed
   backwards from the `ret`, where it is known to be 0 (ESP == entry ESP), by
   undoing push/pop/add esp/sub esp; crossing `leave`, `mov esp, ...`, or any
   other ESP rewrite makes the depth unknown.  So no callee convention has to
   be guessed: the writer always lies after the last call on its path.
4. The writer is a param load when it is a plain dword `mov eax, [esp + N]`
   with (N - depth - 4) / 4 >= 0, or `mov eax, [ebp + N]` (N >= 8) in a
   function with a `push ebp; mov ebp, esp` prologue.
5. Function class:
     param_return   every exit path returns the SAME param index
     partial        some paths return a param, others do not / differ
     unknown_frame  every path is a param-shaped load, but at least one ESP
                    depth could not be reconstructed
     not_param      no path returns a param
     no_exit        no ret / tail jump reached
     no_bytes       no reference bytes for the address

Usage:
    detect_param_return.py                    # summary + void candidates
    detect_param_return.py --json out.json    # full results
    detect_param_return.py --address 0x12140  # one function, verbose
    detect_param_return.py --all-functions    # include unported kb entries
"""
import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "tools" / "verify"))
sys.path.insert(0, str(REPO_ROOT / "tools" / "audit"))

from capstone import CS_ARCH_X86, CS_MODE_32, Cs, CS_OP_IMM, CS_OP_MEM, CS_OP_REG
from capstone import x86_const as X

EAX_REGS = {X.X86_REG_EAX, X.X86_REG_AX, X.X86_REG_AL, X.X86_REG_AH}
ESP_REGS = {X.X86_REG_ESP, X.X86_REG_SP}

_MD = Cs(CS_ARCH_X86, CS_MODE_32)
_MD.detail = True

# MAX_JUMP_TABLE bounds a switch-table read; real MSVC tables in this binary
# are far smaller, and a runaway read would only add bogus in-range edges.
MAX_JUMP_TABLE = 512


# ---------------------------------------------------------------------------
# Decoding
# ---------------------------------------------------------------------------

def _is_ret(ins):
    return ins.id in (X.X86_INS_RET, X.X86_INS_RETF)


def _is_jmp(ins):
    return ins.id == X.X86_INS_JMP


def _is_jcc(ins):
    return (X.X86_GRP_JUMP in ins.groups and not _is_jmp(ins))


def _is_call(ins):
    return ins.id == X.X86_INS_CALL


def _direct_target(ins):
    if ins.operands and ins.operands[0].type == CS_OP_IMM:
        return ins.operands[0].imm & 0xFFFFFFFF
    return None


def _jump_table(ins, start, end, read_dword):
    """Targets of an MSVC `jmp dword ptr [reg*4 + table]`, or None."""
    if not ins.operands or ins.operands[0].type != CS_OP_MEM:
        return None
    m = ins.operands[0].mem
    if m.base != 0 or m.index == 0 or m.scale != 4:
        return None
    table = m.disp & 0xFFFFFFFF
    targets = []
    for i in range(MAX_JUMP_TABLE):
        v = read_dword(table + 4 * i)
        if v is None or not (start <= v < end):
            break
        targets.append(v)
    return targets or None


def decode(code, base, read_dword=None):
    """Recursive-descent decode.

    Returns (insns: {addr: CsInsn}, info) where info has
      succ: {addr: [successor addrs inside the function]}
      exits: [(addr, kind)] kind in {"ret", "tail_jump"}
      incomplete: [addr of unresolved indirect jumps]
    """
    end = base + len(code)

    def rd(va):
        if base <= va and va + 4 <= end:
            return int.from_bytes(code[va - base:va - base + 4], "little")
        return read_dword(va) if read_dword else None

    insns = {}
    succ = {}
    exits = []
    incomplete = []
    work = [base]
    while work:
        a = work.pop()
        while base <= a < end and a not in insns:
            off = a - base
            ins = next(_MD.disasm(code[off:off + 16], a, 1), None)
            if ins is None:
                break
            insns[a] = ins
            nxt = a + ins.size
            if _is_ret(ins):
                succ[a] = []
                exits.append((a, "ret"))
                break
            if _is_jmp(ins):
                t = _direct_target(ins)
                if t is not None:
                    if base <= t < end:
                        succ[a] = [t]
                        work.append(t)
                    else:
                        succ[a] = []
                        exits.append((a, "tail_jump"))
                else:
                    tbl = _jump_table(ins, base, end, rd)
                    if tbl:
                        succ[a] = sorted(set(tbl))
                        work.extend(tbl)
                    else:
                        succ[a] = []
                        incomplete.append(a)
                break
            if _is_jcc(ins):
                t = _direct_target(ins)
                s = [nxt]
                if t is not None and base <= t < end:
                    s.append(t)
                    work.append(t)
                succ[a] = s
                a = nxt
                continue
            succ[a] = [nxt]
            a = nxt
    # drop fallthrough edges that run off the decoded region
    for a, s in succ.items():
        succ[a] = [t for t in s if t in insns]
    return insns, {"succ": succ, "exits": exits, "incomplete": incomplete}


# ---------------------------------------------------------------------------
# Instruction semantics
# ---------------------------------------------------------------------------

def writes_eax(ins):
    if _is_call(ins):
        return True
    try:
        _r, w = ins.regs_access()
    except Exception:
        w = []
    return any(r in EAX_REGS for r in w)


def esp_effect(ins):
    """Change in DEPTH (entry_esp - esp) caused by `ins`, or None if unknown.

    push -> +4, pop -> -4, sub esp,imm -> +imm, add esp,imm -> -imm.
    Instructions that do not write ESP -> 0.
    """
    i = ins.id
    if i == X.X86_INS_PUSH:
        return 4 if ins.operands and ins.operands[0].size != 2 else 2
    if i == X.X86_INS_POP:
        return -4 if ins.operands and ins.operands[0].size != 2 else -2
    if i == X.X86_INS_PUSHAL:
        return 32
    if i == X.X86_INS_POPAL:
        return -32
    if i in (X.X86_INS_PUSHFD,):
        return 4
    if i in (X.X86_INS_POPFD,):
        return -4
    try:
        _r, w = ins.regs_access()
    except Exception:
        w = []
    if not any(r in ESP_REGS for r in w):
        return 0
    if (i in (X.X86_INS_SUB, X.X86_INS_ADD) and len(ins.operands) == 2
            and ins.operands[0].type == CS_OP_REG
            and ins.operands[0].reg == X.X86_REG_ESP
            and ins.operands[1].type == CS_OP_IMM):
        imm = ins.operands[1].imm
        return imm if i == X.X86_INS_SUB else -imm
    return None                                   # leave, mov esp, lea esp, and esp...


def has_ebp_frame(insns, base):
    a = insns.get(base)
    if a is None or a.id != X.X86_INS_PUSH or not a.operands:
        return False
    if a.operands[0].type != CS_OP_REG or a.operands[0].reg != X.X86_REG_EBP:
        return False
    b = insns.get(base + a.size)
    return (b is not None and b.id == X.X86_INS_MOV and len(b.operands) == 2
            and b.operands[0].type == CS_OP_REG and b.operands[0].reg == X.X86_REG_EBP
            and b.operands[1].type == CS_OP_REG and b.operands[1].reg == X.X86_REG_ESP)


def classify_writer(ins, depth, ebp_frame):
    """Classify the reaching EAX writer.  Returns dict(kind, param?, detail)."""
    text = f"{ins.mnemonic} {ins.op_str}".strip()
    if _is_call(ins):
        return {"kind": "call_result", "detail": text}
    ops = ins.operands
    if (ins.id == X.X86_INS_MOV and len(ops) == 2 and ops[0].type == CS_OP_REG
            and ops[0].reg == X.X86_REG_EAX and ops[1].type == CS_OP_MEM
            and ops[1].size == 4 and ops[1].mem.index == 0
            and ops[1].mem.segment == 0):
        m = ops[1].mem
        if m.base == X.X86_REG_ESP:
            if depth is None:
                return {"kind": "unknown_frame", "detail": text}
            rel = m.disp - depth - 4
            if rel >= 0 and rel % 4 == 0:
                return {"kind": "param", "param": rel // 4, "detail": text}
            return {"kind": "local_load", "detail": f"{text} (depth {depth})"}
        if m.base == X.X86_REG_EBP:
            if not ebp_frame:
                return {"kind": "ebp_nonframe_load", "detail": text}
            rel = m.disp - 8
            if rel >= 0 and rel % 4 == 0:
                return {"kind": "param", "param": rel // 4, "detail": text}
            return {"kind": "local_load", "detail": text}
        return {"kind": "mem_load", "detail": text}
    if ins.id == X.X86_INS_XOR and len(ops) == 2 and all(
            o.type == CS_OP_REG and o.reg == X.X86_REG_EAX for o in ops):
        return {"kind": "const", "detail": text}
    if ins.id in (X.X86_INS_MOV, X.X86_INS_OR) and len(ops) == 2 and \
            ops[1].type == CS_OP_IMM:
        return {"kind": "const", "detail": text}
    if ins.id == X.X86_INS_POP:
        return {"kind": "pop", "detail": text}
    return {"kind": "other", "detail": text}


# ---------------------------------------------------------------------------
# Backward reaching-definition search
# ---------------------------------------------------------------------------

def _predecessors(insns, succ):
    preds = {a: [] for a in insns}
    for a, s in succ.items():
        for t in s:
            if t in preds:
                preds[t].append(a)
    return preds


def reaching_eax(exit_addr, exit_kind, insns, preds, base, ebp_frame,
                 max_states=4096):
    """All (classification, path_tail) pairs reaching `exit_addr`.

    ESP depth is known to be 0 at a `ret` (ESP points at the return address,
    exactly as at entry) and is reconstructed backwards from there.
    """
    if exit_kind != "ret":
        # A tail jump hands control to another function; whatever EAX holds
        # here is overwritten by that function's own return value.
        return [({"kind": "tail_jump", "detail": insns[exit_addr].op_str},
                 (exit_addr,))]
    results = []
    start_depth = 0
    # state: (addr of instruction whose PREDECESSORS we examine, depth AFTER
    # the predecessor, i.e. depth at entry of `addr`)
    stack = [(exit_addr, start_depth, (exit_addr,))]
    seen = set()
    states = 0
    while stack:
        addr, depth, trail = stack.pop()
        states += 1
        if states > max_states:
            results.append(({"kind": "search_limit", "detail": ""}, trail))
            break
        ps = preds.get(addr, [])
        if not ps:
            if addr == base:
                results.append(({"kind": "eax_from_entry", "detail": ""}, trail))
            else:
                results.append(({"kind": "unknown_pred",
                                 "detail": f"0x{addr:x} has no known predecessor"},
                                trail))
            continue
        for p in ps:
            ins = insns[p]
            key = (p, depth)
            if key in seen:
                continue
            seen.add(key)
            if writes_eax(ins):
                # depth at the writer: the writer itself does not move ESP
                # (a `pop eax` does, but it is classified 'pop' regardless).
                eff = esp_effect(ins)
                d_before = None if (depth is None or eff is None) else depth - eff
                results.append((classify_writer(ins, d_before, ebp_frame),
                                (p,) + trail))
                continue
            eff = esp_effect(ins)
            nd = None if (depth is None or eff is None) else depth - eff
            stack.append((p, nd, (p,) + trail))
    return results


_X87_STORE = {"fst", "fstp", "fist", "fistp", "fisttp", "fbstp", "fnstsw",
              "fstsw", "fnstcw", "fstcw", "fnsave", "fsave", "fnstenv", "fstenv"}
# x87 ops after which the register stack is normally empty again (a value
# stored/compared away).  A different LAST x87 op before `ret` suggests a float
# result may be live in ST0 -- i.e. the function probably returns float and
# EAX only coincidentally holds a param it used as a pointer.
_X87_DRAINS = {"fstp", "fistp", "fisttp", "fbstp", "fcompp", "fucompp", "fcomp",
               "fucomp", "ficomp", "fnstsw", "fstsw", "fnstcw", "fstcw", "fldcw",
               "fninit", "finit", "fnclex", "fclex", "ffree", "ffreep", "wait",
               "fwait", "fcomip", "fucomip"}


def _reads_eax(ins):
    try:
        r, _w = ins.regs_access()
    except Exception:
        r = []
    return any(x in EAX_REGS for x in r)


def _writes_mem_slot(ins, base_reg, disp):
    """True when `ins` writes the dword at [base_reg + disp]."""
    for i, op in enumerate(ins.operands):
        if op.type != CS_OP_MEM or op.mem.base != base_reg or op.mem.index != 0:
            continue
        if not (disp - 3 <= op.mem.disp <= disp + 3):
            continue
        if ins.mnemonic.startswith("f"):
            # capstone marks x87 memory destinations as READ; use the mnemonic
            if ins.mnemonic in _X87_STORE:
                return True
            continue
        if op.access & 2:                           # CS_AC_WRITE
            return True
        if i == 0 and ins.id in (X.X86_INS_MOV, X.X86_INS_ADD, X.X86_INS_SUB,
                                 X.X86_INS_AND, X.X86_INS_OR, X.X86_INS_XOR,
                                 X.X86_INS_INC, X.X86_INS_DEC, X.X86_INS_POP):
            return True
    return False


def _path_facts(trail, insns):
    """Facts about the straight path from the param load (trail[0]) to ret."""
    after = [insns[a] for a in trail[1:-1]]
    eax_reused = any(_reads_eax(i) for i in after)
    eax_as_pointer = any(
        op.type == CS_OP_MEM and (op.mem.base == X.X86_REG_EAX
                                  or op.mem.index == X.X86_REG_EAX)
        for i in after for op in i.operands)
    last_x87 = None
    for i in after:
        if i.mnemonic.startswith("f") and X.X86_GRP_FPU in i.groups:
            last_x87 = i.mnemonic
    return {"eax_reused": eax_reused,
            "eax_as_pointer": eax_as_pointer,
            "st0_maybe_live": last_x87 is not None and last_x87 not in _X87_DRAINS}


def classify_bytes(code, base=0x10000, read_dword=None):
    """Classify one function from its raw bytes.  Pure; used by the tests."""
    insns, info = decode(code, base, read_dword)
    preds = _predecessors(insns, info["succ"])
    ebp_frame = has_ebp_frame(insns, base)
    exits = []
    for addr, kind in sorted(info["exits"]):
        defs = reaching_eax(addr, kind, insns, preds, base, ebp_frame)
        for d, trail in defs:
            if d["kind"] == "param":
                d.update(_path_facts(trail, insns))
        exits.append({
            "addr": addr,
            "kind": kind,
            "insn": f"{insns[addr].mnemonic} {insns[addr].op_str}".strip(),
            "defs": [dict(d, writer=trail[0] if d["kind"] not in (
                "eax_from_entry", "unknown_pred", "search_limit",
                "tail_jump") else None)
                for d, trail in defs],
            "_trails": [trail for _d, trail in defs],
        })

    all_defs = [d for e in exits for d in e["defs"]]
    params = {d["param"] for d in all_defs if d["kind"] == "param"}
    kinds = {d["kind"] for d in all_defs}
    if not insns:
        cls = "no_bytes"
    elif not exits:
        cls = "no_exit"
    elif kinds == {"param"} and len(params) == 1:
        cls = "param_return"
    elif kinds <= {"param", "unknown_frame"} and len(params) <= 1:
        cls = "unknown_frame"
    elif "param" in kinds:
        cls = "partial"
    else:
        cls = "not_param"

    param = next(iter(params)) if len(params) == 1 else None
    # Was the param's stack slot ever written?  MSVC reuses a dead param slot
    # as a local, so `mov eax, [ebp+8]` can return a COMPUTED value parked in
    # param_1's slot (hs_short_to_real) -- not the out-pointer idiom.  Only
    # provable for EBP frames; ESP-framed slots need forward depth tracking.
    slot_written = None
    if param is not None and ebp_frame:
        disp = 8 + 4 * param
        slot_written = False
        for i in insns.values():
            if (i.id == X.X86_INS_LEA and i.operands[1].mem.base == X.X86_REG_EBP
                    and i.operands[1].mem.disp == disp):
                slot_written = "address_taken"     # e.g. &out_size parked in the slot
                break
            if _writes_mem_slot(i, X.X86_REG_EBP, disp):
                slot_written = "store"
                break
    if cls == "param_return" and slot_written:
        cls = "slot_reused"

    pdefs = [d for d in all_defs if d["kind"] == "param"]
    if cls != "param_return":
        confidence = None
    elif any(d["st0_maybe_live"] for d in pdefs):
        confidence = "low"            # float result likely live in ST0
    elif any(d["eax_reused"] for d in pdefs):
        confidence = "medium"         # EAX also used after the load (pointer/copy)
    else:
        confidence = "high"           # load exists only to set EAX for ret
    return {
        "class": cls,
        "confidence": confidence,
        "param": param,
        "params_seen": sorted(params),
        "slot_written": slot_written,
        "ebp_frame": ebp_frame,
        "n_rets": sum(1 for e in exits if e["kind"] == "ret"),
        "n_tail_jumps": sum(1 for e in exits if e["kind"] == "tail_jump"),
        "incomplete_cfg": [f"0x{a:x}" for a in info["incomplete"]],
        "exits": exits,
        "_insns": insns,
    }


# ---------------------------------------------------------------------------
# Source side: kb.json decls
# ---------------------------------------------------------------------------

_CC = re.compile(r'\b(__cdecl|__stdcall|__fastcall|__thiscall|__declspec\([^)]*\)'
                 r'|static|extern|inline|__inline|__forceinline)\b')


def _split_top(s, sep=","):
    out, depth, cur = [], 0, []
    for ch in s:
        if ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        if ch == sep and depth == 0:
            out.append("".join(cur))
            cur = []
        else:
            cur.append(ch)
    out.append("".join(cur))
    return out


def parse_decl(decl):
    """-> {ret, name, params:[{type,name,reg,text}], stack_params:[...]} or None."""
    s = decl.strip().rstrip(";").strip()
    if not s.endswith(")"):
        return None
    # find the opening paren of the LAST top-level group (the parameter list)
    depth = 0
    open_i = None
    for i in range(len(s) - 1, -1, -1):
        if s[i] == ")":
            depth += 1
        elif s[i] == "(":
            depth -= 1
            if depth == 0:
                open_i = i
                break
    if open_i is None:
        return None
    head, plist = s[:open_i].rstrip(), s[open_i + 1:-1]
    m = re.search(r'([A-Za-z_]\w*)\s*$', head)
    if not m:
        return None
    name = m.group(1)
    ret = _CC.sub(" ", head[:m.start()])
    ret = re.sub(r'@<\w+>', ' ', ret)
    ret = re.sub(r'\s+', ' ', ret).strip()
    params = []
    if plist.strip() not in ("", "void"):
        for p in _split_top(plist):
            p = p.strip()
            reg = None
            rm = re.search(r'@\s*<\s*(\w+)\s*>', p)
            if rm:
                reg = rm.group(1)
                p2 = (p[:rm.start()] + p[rm.end():]).strip()
            else:
                p2 = p
            if p2 == "...":
                params.append({"type": "...", "name": "...", "reg": None, "text": p})
                continue
            nm = re.search(r'([A-Za-z_]\w*)\s*(\[[^\]]*\])*\s*$', p2)
            if nm and nm.start() > 0 and "(" not in p2:
                ptype = p2[:nm.start()].strip()
                pname = nm.group(1)
            else:
                ptype, pname = p2, None          # unnamed / function pointer
            params.append({"type": re.sub(r'\s+', ' ', ptype), "name": pname,
                           "reg": reg, "text": p})
    stack = [p for p in params if p["reg"] is None and p["type"] != "..."]
    return {"ret": ret, "name": name, "params": params, "stack_params": stack,
            "varargs": any(p["type"] == "..." for p in params)}


def _norm_type(t):
    t = re.sub(r'\bconst\b|\bvolatile\b', ' ', t or "")
    return re.sub(r'\s+', '', t)


def kb_functions(ported_only=True):
    filt = ('.objects[] | .name as $o | .functions[]? | '
            + ('select(.ported==true) | ' if ported_only else '')
            + '{addr, decl, ported, object: $o}')
    out = subprocess.run(["jq", "-c", filt, str(REPO_ROOT / "kb.json")],
                         capture_output=True, text=True, check=True)
    return [json.loads(l) for l in out.stdout.splitlines() if l.strip()]


# ---------------------------------------------------------------------------
# Optional source check for the non-void agreement metric
# ---------------------------------------------------------------------------

_SRC_CACHE = None


def _src_files():
    global _SRC_CACHE
    if _SRC_CACHE is None:
        _SRC_CACHE = []
        for p in sorted((REPO_ROOT / "src").rglob("*.c")):
            try:
                _SRC_CACHE.append((p, p.read_text(errors="replace")))
            except OSError:
                pass
    return _SRC_CACHE


def source_returns(name, full_index):
    """'yes' if every `return` in NAME's C definition returns its parameter at
    position FULL_INDEX (the name is read from the C definition itself, since
    kb.json decl names and source names often differ), 'no' if a definition
    was found and some return differs / none exist, None if no definition was
    found."""
    if full_index is None:
        return None
    rx =re.compile(r'^[^\s#/][^;{}\n]*\b' + re.escape(name) + r'\s*\(', re.M)
    for path, text in _src_files():
        for m in rx.finditer(text):
            # find the matching ')' then require '{' before any ';'
            i, depth = m.end() - 1, 0
            while i < len(text):
                if text[i] == "(":
                    depth += 1
                elif text[i] == ")":
                    depth -= 1
                    if depth == 0:
                        break
                i += 1
            j = i + 1
            while j < len(text) and text[j] in " \t\r\n":
                j += 1
            if j >= len(text) or text[j] != "{":
                continue
            k, depth = j, 0
            while k < len(text):
                if text[k] == "{":
                    depth += 1
                elif text[k] == "}":
                    depth -= 1
                    if depth == 0:
                        break
                k += 1
            body = text[j:k + 1]
            plist = _split_top(text[m.end():i])
            if full_index >= len(plist):
                return "no"
            pm = re.search(r'([A-Za-z_]\w*)\s*(\[[^\]]*\])*\s*$', plist[full_index])
            if not pm:
                return None
            param_name = pm.group(1)
            rets = re.findall(r'\breturn\b([^;]*);', body)
            if not rets:
                return "no"
            want = re.compile(r'^\s*(\(\s*[^()]*\)\s*)?\(?\s*' + re.escape(param_name)
                              + r'\s*\)?\s*$')
            return "yes" if all(want.match(r) for r in rets) else "no"
    return None


# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------

_XBE_READER = None


def _xbe_read_dword():
    global _XBE_READER
    if _XBE_READER is None:
        import xbe_reference as xr
        from check_delinked_bounds import va_to_off
        data, secs = xr._xbe()

        def rd(va):
            off = va_to_off(secs, va)
            if off is None or off + 4 > len(data):
                return None
            return int.from_bytes(data[off:off + 4], "little")
        _XBE_READER = rd
    return _XBE_READER


def analyze_address(addr):
    import xbe_reference as xr
    code, err = xr.function_bytes(addr)
    if code is None:
        return {"class": "no_bytes", "error": err, "exits": [], "_insns": {},
                "param": None, "params_seen": [], "n_rets": 0,
                "n_tail_jumps": 0, "incomplete_cfg": [], "ebp_frame": False}
    r = classify_bytes(code, addr, _xbe_read_dword())
    r["size"] = len(code)
    return r


def _public(r):
    out = {k: v for k, v in r.items() if not k.startswith("_")}
    out["exits"] = [
        {"addr": f"0x{e['addr']:x}", "kind": e["kind"], "insn": e["insn"],
         "defs": [dict(d, writer=(f"0x{d['writer']:x}" if d.get("writer") is not None
                                  else None)) for d in e["defs"]]}
        for e in r["exits"]]
    return out


def enrich(fn, r):
    """Attach decl-side facts to a classification result."""
    pd = parse_decl(fn.get("decl") or "")
    rec = {"addr": fn["addr"], "object": fn.get("object"), "decl": fn.get("decl"),
           "name": pd["name"] if pd else None,
           "ret_type": pd["ret"] if pd else None}
    rec.update(_public(r))
    if pd is None:
        rec["decl_status"] = "unparsed"
        return rec
    rec["decl_status"] = "void" if pd["ret"] == "void" else "non_void"
    k = r.get("param")
    if k is not None:
        sp = pd["stack_params"]
        if k < len(sp):
            if (rec.get("confidence") in ("high", "medium")
                    and re.fullmatch(r'(const\s+)?(float|real|double)', sp[k]["type"])):
                rec["confidence"] = "low"
                rec["confidence_note"] = "param declared float (EAX likely a copy)"
            rec["param_decl"] = {"index": k, "type": sp[k]["type"],
                                 "name": sp[k]["name"],
                                 "full_index": next(i for i, p in enumerate(pd["params"])
                                                    if p is sp[k])}
        else:
            rec["param_decl"] = {"index": k, "type": None, "name": None,
                                 "note": f"decl has only {len(sp)} stack params"
                                 + (" (varargs)" if pd["varargs"] else "")}
    return rec


def print_verbose(addr):
    r = analyze_address(addr)
    fn = next((f for f in kb_functions(False) if int(f["addr"], 16) == addr), None)
    print(f"0x{addr:x}  {fn['decl'] if fn else '(not in kb.json)'}")
    if fn and not fn.get("ported"):
        print("  (kb.json: not ported)")
    if r.get("error"):
        print(f"  no bytes: {r['error']}")
        return 0
    print(f"  class={r['class']} param={r['param']} ebp_frame={r['ebp_frame']} "
          f"rets={r['n_rets']} tail_jumps={r['n_tail_jumps']} size={r.get('size')}"
          + (f" incomplete_cfg={r['incomplete_cfg']}" if r["incomplete_cfg"] else ""))
    if fn:
        rec = enrich(fn, r)
        if rec.get("param_decl"):
            print(f"  decl param: {rec['param_decl']}")
    insns = r["_insns"]
    for e in r["exits"]:
        print(f"  exit 0x{e['addr']:x} [{e['kind']}] {e['insn']}")
        for d, trail in zip(e["defs"], e["_trails"]):
            print(f"    def: {d['kind']}"
                  + (f" param={d['param']}" if "param" in d else "")
                  + (f"  ({d['detail']})" if d.get("detail") else ""))
            # the trail runs from the reaching EAX writer to the exit
            shown = list(trail) if len(trail) <= 30 else (
                list(trail[:6]) + [None] + list(trail[-20:]))
            for a in shown:
                if a is None:
                    print(f"      ... {len(trail) - 26} instructions ...")
                    continue
                ins = insns[a]
                print(f"      {a:08x}  {ins.mnemonic} {ins.op_str}")
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--address", help="analyze one function verbosely")
    ap.add_argument("--json", help="write full results to PATH")
    ap.add_argument("--all-functions", action="store_true",
                    help="include kb.json functions that are not ported")
    ap.add_argument("--limit", type=int, default=0,
                    help="print at most N void candidates (0 = all)")
    args = ap.parse_args(argv)

    if args.address:
        return print_verbose(int(args.address, 16))

    fns = kb_functions(not args.all_functions)
    seen = set()
    recs = []
    for fn in fns:
        try:
            addr = int(fn["addr"], 16)
        except (TypeError, ValueError):
            continue
        if addr in seen:
            continue
        seen.add(addr)
        recs.append(enrich(fn, analyze_address(addr)))

    counts = {}
    for r in recs:
        counts[r["class"]] = counts.get(r["class"], 0) + 1
    by_decl = {}
    for r in recs:
        if r["class"] in ("param_return", "partial", "unknown_frame", "slot_reused"):
            key = (r["class"], r.get("decl_status"), r.get("confidence") or "-")
            by_decl[key] = by_decl.get(key, 0) + 1

    # Sanity check: param_return functions whose decl already returns a value.
    # Agreement = the decl's return type equals the returned param's type, and
    # (stronger) the C definition's every `return` names that parameter.
    nonvoid = [r for r in recs if r["class"] == "param_return"
               and r.get("decl_status") == "non_void"]
    for r in nonvoid:
        pdcl = r.get("param_decl") or {}
        r["type_agrees"] = (pdcl.get("type") is not None
                            and _norm_type(pdcl["type"]) == _norm_type(r["ret_type"]))
        r["source_returns_param"] = source_returns(r["name"], pdcl.get("full_index"))
    sanity = {}
    for tier in ("high", "medium", "low", "all"):
        rows = [r for r in nonvoid if tier == "all" or r["confidence"] == tier]
        src = [r for r in rows if r["source_returns_param"] is not None]
        sanity[tier] = {"n": len(rows),
                        "type_agrees": sum(1 for r in rows if r["type_agrees"]),
                        "source_located": len(src),
                        "source_returns_param": sum(
                            1 for r in src if r["source_returns_param"] == "yes")}

    def pct(a, b):
        return f"{100.0 * a / b:.1f}%" if b else "n/a"

    print(f"functions analyzed: {len(recs)}"
          + ("" if args.all_functions else " (ported)"))
    for k in ("param_return", "slot_reused", "partial", "unknown_frame",
              "not_param", "no_exit", "no_bytes"):
        print(f"  {k:<14} {counts.get(k, 0)}")
    print("  by decl / confidence:")
    for (c, d, conf), n in sorted(by_decl.items()):
        print(f"    {c:<14} {d or '?':<9} {conf:<7} {n}")
    print("sanity: param_return with a non-void decl "
          "(type == decl ret type | C body returns that param)")
    for tier, v in sanity.items():
        print(f"  {tier:<7} n={v['n']:<4} type {v['type_agrees']}/{v['n']} "
              f"({pct(v['type_agrees'], v['n'])})   source "
              f"{v['source_returns_param']}/{v['source_located']} "
              f"({pct(v['source_returns_param'], v['source_located'])})")

    cands = sorted((r for r in recs if r["class"] == "param_return"
                    and r.get("decl_status") == "void"),
                   key=lambda r: ({"high": 0, "medium": 1, "low": 2}[r["confidence"]],
                                  int(r["addr"], 16)))
    tiers = {}
    for r in cands:
        tiers[r["confidence"]] = tiers.get(r["confidence"], 0) + 1
    print(f"\nvoid-declared param_return candidates: {len(cands)}  "
          + " ".join(f"{k}={v}" for k, v in sorted(tiers.items())))
    print("  (high: the load only feeds ret; medium: EAX is also used after the"
          " load;\n   low: ST0 may hold a float result, or the param is declared"
          " float -- a returned\n   float would travel in ST0, so EAX is likely"
          " a coincidental copy)")
    shown = cands if not args.limit else cands[:args.limit]
    for r in shown:
        pdcl = r.get("param_decl") or {}
        flag = " incomplete_cfg" if r["incomplete_cfg"] else ""
        ptxt = (f"{pdcl.get('type')} {pdcl.get('name')}" if pdcl.get("type")
                else pdcl.get("note", "?"))
        print(f"  {r['addr']:<10} {r['confidence']:<6} {r['name']:<44} "
              f"p{r['param']}: {ptxt:<30} rets={r['n_rets']}{flag}")
    if args.limit and len(cands) > args.limit:
        print(f"  ... {len(cands) - args.limit} more (use --json)")

    if args.json:
        Path(args.json).parent.mkdir(parents=True, exist_ok=True)
        with open(args.json, "w") as f:
            json.dump({"counts": counts,
                       "sanity": sanity,
                       "void_candidates": [r["addr"] for r in cands],
                       "functions": recs}, f, indent=1)
        print(f"\nwrote {args.json}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
