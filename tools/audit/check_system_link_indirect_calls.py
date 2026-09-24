"""Audit indirect control transfers in the 2276 system-link object inventory.

Static function bounds and linear disassembly can miss data-driven dispatch
outside these functions. This gate classifies known indirect sites and checks
that the computed jump-table targets begin inside their owning functions.
"""

import hashlib
import json
import re
import struct
import sys
from collections import Counter
from pathlib import Path

from capstone import Cs, CS_ARCH_X86, CS_MODE_32, CS_OP_IMM, CS_OP_MEM, CS_OP_REG

sys.path.insert(0, "tools/equivalence")
from xbe_image import load_xbe, read_va_raw
from system_link_inventory import SCOPED_OBJECTS

EXPECTED_CALL_SITES = {
    "0x11ba6a", "0x11bae0", "0x11bd1e", "0x11c4a9", "0x11c517",
    "0x129c07",
}
graph = json.loads(Path("artifacts/ntsc_callgraph/callgraph.json").read_text())["functions"]
addresses = sorted(int(addr, 16) for addr, info in graph.items()
                   if info["object"] in SCOPED_OBJECTS)
image, sections = load_xbe("halo-patched/cachebeta.xbe")
if hashlib.md5(image).hexdigest() != "c7869590a1c64ad034e49a5ee0c02465":
    raise RuntimeError("wrong 2276 reference XBE")
if len(addresses) != 391:
    raise RuntimeError(f"expected 391 scoped functions, found {len(addresses)}")
disassembler = Cs(CS_ARCH_X86, CS_MODE_32)
disassembler.detail = True
rows = []
for address in addresses:
    info = graph.get(hex(address))
    if not info:
        continue
    end = int(info["end"], 16)
    code = read_va_raw(image, sections, address, end - address)
    for insn in disassembler.disasm(code, address):
        if insn.mnemonic not in ("call", "jmp"):
            continue
        if not insn.operands or insn.operands[0].type == CS_OP_IMM:
            continue
        operand = insn.operands[0]
        if operand.type == CS_OP_REG:
            kind = "register"
        elif operand.type == CS_OP_MEM:
            kind = "absolute_memory" if operand.mem.base == 0 and operand.mem.index == 0 else "computed_memory"
        else:
            kind = "other"
        rows.append({
            "caller": hex(address), "caller_name": info["name"],
            "object": info["object"], "at": hex(insn.address),
            "instruction": f"{insn.mnemonic} {insn.op_str}",
            "kind": kind,
        })
Path("artifacts/equivalence/system_link_indirect_calls.json").write_text(
    json.dumps(rows, indent=2))
print("functions", len(addresses), "indirect_edges", len(rows))
print("kinds", dict(Counter(row["kind"] for row in rows)))
print("objects", dict(Counter(row["object"] for row in rows)))
tables = []
for row in rows:
    if not row["instruction"].startswith("jmp dword ptr ["):
        continue
    match = re.search(r"0x([0-9a-f]+)\]$", row["instruction"])
    if not match:
        continue
    base = int(match.group(1), 16)
    info = graph[row["caller"]]
    start, end = int(info["addr"], 16), int(info["end"], 16)
    raw = read_va_raw(image, sections, base, 4 * 32)
    targets = [struct.unpack_from("<I", raw, i)[0]
               for i in range(0, len(raw) - 3, 4)]
    prefix = []
    for target in targets:
        if not start <= target < end:
            break
        prefix.append(hex(target))
    tables.append({"at": row["at"], "caller": row["caller"],
                   "object": row["object"], "base": hex(base),
                   "internal_prefix": prefix,
                   "first_outside": hex(targets[len(prefix)]) if len(prefix) < len(targets) else None})
Path("artifacts/equivalence/system_link_jump_tables.json").write_text(
    json.dumps(tables, indent=2))
print("jump_tables", len(tables), "all_internal_prefix", all(t["internal_prefix"] for t in tables))
print("short_prefix", [(t["at"], len(t["internal_prefix"]), t["first_outside"])
                       for t in tables if len(t["internal_prefix"]) < 2])
call_sites = {row["at"] for row in rows if row["instruction"].startswith("call ")}
if call_sites != EXPECTED_CALL_SITES:
    raise RuntimeError(f"indirect call sites changed: {sorted(call_sites)}")
if len(tables) != 29 or any(len(t["internal_prefix"]) < 2 for t in tables):
    raise RuntimeError("jump-table classification changed")
print("PASS: six known indirect calls and 29 in-function jump tables")
