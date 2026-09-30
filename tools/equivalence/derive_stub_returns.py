#!/usr/bin/env python3
"""Add state-derived stub returns for a target's pure getter callees.

With a live game-state snapshot the oracle still calls every direct callee
through a sentinel stub that returns 0, so a getter such as
game_engine_get_variant (`mov eax,0x456af8; ret`) sends the target down a
null-pointer path the real game never takes.

For each direct callee of the target (CALL rel32 in the pristine XBE body)
whose kb.json declaration takes no parameters, this runs the callee's ORIGINAL
code in Unicorn over the snapshot memory.  If it returns without making a call
and without writing anything outside its own stack frame, its EAX (AL for a
bool/char return) is exactly what that callee returns in the captured state,
and it is written to the snapshot's "stub_returns".  Callees that call out,
write memory or take parameters are left alone.

    derive_stub_returns.py --target ctf_engine_update \\
        --snapshot artifacts/equivalence/snapshots/ctf_t700.json \\
        --out artifacts/equivalence/snapshots/ctf_t700_ctf_engine_update.json
"""
from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from pathlib import Path

from capstone import CS_ARCH_X86, CS_MODE_32, Cs
from unicorn import (UC_ARCH_X86, UC_HOOK_CODE, UC_HOOK_MEM_WRITE, UC_MODE_32,
                     Uc, UcError)
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP

ROOT = Path(__file__).resolve().parent.parent.parent
XBE = ROOT / "halo-patched" / "cachebeta.xbe"
BOUNDS = ROOT / "tools" / "verify" / "function_bounds.json"

STACK_TOP = 0x0FF00000
STACK_SIZE = 0x10000
RETURN_SENTINEL = 0x0FEFFFF0
MAX_INSNS = 500
_NAME_RE = re.compile(r"(\w+)\s*\(([^)]*)\)")


def load_xbe_sections(data: bytes):
    base = struct.unpack_from("<I", data, 0x104)[0]
    count, headers = struct.unpack_from("<II", data, 0x11C)
    out = []
    for i in range(count):
        o = headers - base + i * 0x38
        _fl, va, vsize, raw, rsize, _name = struct.unpack_from("<IIIIII", data, o)
        out.append((va, vsize, data[raw:raw + min(rsize, vsize)]))
    return out


def kb_functions() -> dict[int, dict]:
    kb = json.loads((ROOT / "kb.json").read_text(encoding="utf-8"))
    out = {}
    for obj in kb["objects"]:
        for f in obj.get("functions", []):
            m = _NAME_RE.search(re.sub(r"@<\w+>", "", f["decl"]))
            if m:
                out[int(f["addr"], 16)] = {
                    "name": m.group(1),
                    "params": m.group(2).strip(),
                    "ret": f["decl"][:m.start()].strip(),
                }
    return out


def function_extent(addr: int) -> int | None:
    b = json.loads(BOUNDS.read_text(encoding="utf-8"))
    ent = b.get(hex(addr)) or b.get("0x%x" % addr)
    if isinstance(ent, dict):
        end = ent.get("end") or ent.get("end_addr")
        return int(end, 16) if isinstance(end, str) else end
    return None


def read_va(sections, va: int, size: int) -> bytes:
    for sva, vsize, blob in sections:
        if sva <= va < sva + vsize:
            return blob[va - sva:va - sva + size]
    raise KeyError(hex(va))


def direct_callees(sections, start: int, end: int) -> list[int]:
    code = read_va(sections, start, end - start)
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    out = []
    for ins in md.disasm(code, start):
        if ins.mnemonic == "call" and ins.op_str.startswith("0x"):
            tgt = int(ins.op_str, 16)
            if tgt not in out:
                out.append(tgt)
    return out


def emulate_getter(sections, regions: dict[int, bytes], addr: int) -> int | None:
    """EAX after running a no-arg callee on the snapshot; None if impure."""
    uc = Uc(UC_ARCH_X86, UC_MODE_32)
    mapped = []

    def map_bytes(va, blob, size=None):
        size = size or len(blob)
        lo = va & ~0xFFF
        hi = (va + size + 0xFFF) & ~0xFFF
        for mlo, mhi in mapped:
            if lo < mhi and hi > mlo:
                lo = max(lo, mhi) if lo >= mlo else lo
                hi = min(hi, mlo) if hi <= mhi else hi
        if hi > lo:
            uc.mem_map(lo, hi - lo)
            mapped.append((lo, hi))
        uc.mem_write(va, blob)

    for va, vsize, blob in sections:
        map_bytes(va, blob, vsize)
    for va, blob in regions.items():
        map_bytes(va, blob)
    uc.mem_map(STACK_TOP - STACK_SIZE, STACK_SIZE)
    esp = STACK_TOP - 0x100
    uc.mem_write(esp, struct.pack("<I", RETURN_SENTINEL))
    uc.reg_write(UC_X86_REG_ESP, esp)

    md = Cs(CS_ARCH_X86, CS_MODE_32)
    impure = []

    def on_code(uc_, address, size, _):
        if address == RETURN_SENTINEL:
            uc_.emu_stop()
            return
        ins = next(md.disasm(bytes(uc_.mem_read(address, size)), address), None)
        if ins is not None and ins.mnemonic.startswith("call"):
            impure.append("call")
            uc_.emu_stop()

    def on_write(uc_, _access, address, _size, _value, _):
        if not STACK_TOP - STACK_SIZE <= address < STACK_TOP:
            impure.append("write %#x" % address)
            uc_.emu_stop()

    uc.hook_add(UC_HOOK_CODE, on_code)
    uc.hook_add(UC_HOOK_MEM_WRITE, on_write)
    try:
        uc.emu_start(addr, RETURN_SENTINEL, count=MAX_INSNS)
    except UcError:
        return None
    if impure:
        return None
    return uc.reg_read(UC_X86_REG_EAX)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--target", required=True, help="kb.json function name")
    ap.add_argument("--snapshot", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args()

    fns = kb_functions()
    by_name = {v["name"]: a for a, v in fns.items()}
    if args.target not in by_name:
        sys.exit(f"{args.target} not in kb.json")
    start = by_name[args.target]
    end = function_extent(start)
    if end is None:
        sys.exit(f"no committed bound for {args.target} in {BOUNDS.name}")

    sections = load_xbe_sections(XBE.read_bytes())
    snap = json.loads(args.snapshot.read_text(encoding="utf-8"))
    regions = {int(a, 16): bytes.fromhex(b) for a, b in snap["regions"].items()}
    stub_returns = dict(snap.get("stub_returns", {}))

    for callee in direct_callees(sections, start, end):
        info = fns.get(callee)
        if info is None:
            print(f"  skip {callee:#x}: not in kb.json")
            continue
        name = info["name"]
        if info["params"] not in ("", "void"):
            print(f"  skip {name}: takes parameters")
            continue
        if info["ret"] == "void" or name in stub_returns:
            continue
        eax = emulate_getter(sections, regions, callee)
        if eax is None:
            print(f"  skip {name}: calls out or writes memory")
            continue
        value = eax & 0xFF if info["ret"] in ("bool", "boolean", "char", "unsigned char") else eax
        stub_returns[name] = value
        print(f"  {name} -> {value:#x}")

    snap["stub_returns"] = stub_returns
    args.out.write_text(json.dumps(snap), encoding="utf-8")
    print(f"wrote {len(stub_returns)} stub return(s) -> {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
