#!/usr/bin/env python3
"""Pin the 15 remaining decals.obj 5-byte JMP thunks to their shipped C.

Each wrapper in src/halo/effects/decals.c must tail-call the JMP target
decoded from the pristine 2276 XBE (not an empty stub). kb.json must keep
the recovered 0-arg cdecl and ported=true.

Run: rtk python3 tools/verify/test_decals_obj_remaining_thunks.py
"""
from __future__ import annotations

import json
import re
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "equivalence"))
from xbe_image import load_xbe, read_va  # type: ignore

XBE = REPO / "halo-patched" / "cachebeta.xbe"
KB = REPO / "kb.json"
SRC = REPO / "src" / "halo" / "effects" / "decals.c"

# Inventory from the remaining-unported decals.obj thunk cluster.
THUNKS = [
    0x17CBA0,
    0x17CBE0,
    0x17CBF0,
    0x17CC00,
    0x17CC40,
    0x17CC50,
    0x17CC80,
    0x17CC90,
    0x17CCA0,
    0x17CCE0,
    0x17CD00,
    0x17CD10,
    0x17CD20,
    0x17CD40,
    0x17CD50,
]


def _kb_funcs():
    kb = json.loads(KB.read_text())
    out = {}
    for obj in kb.get("objects") or []:
        if not isinstance(obj, dict):
            continue
        for fn in obj.get("functions") or []:
            out[int(fn["addr"], 16)] = fn
    return out


def _decl_name(decl: str) -> str:
    m = re.search(r"\b(\w+)\s*\(", decl)
    assert m, decl
    return m.group(1)


def _arity(decl: str) -> int:
    inner = decl[decl.find("(") + 1 : decl.rfind(")")]
    inner = inner.strip()
    if inner in ("", "void"):
        return 0
    return inner.count(",") + 1


def test_fifteen_thunks_forward_xbe_jmp_targets():
    raw, secs = load_xbe(XBE)
    src = SRC.read_text()
    funcs = _kb_funcs()
    assert SRC.is_file()

    for addr in THUNKS:
        body = read_va(raw, secs, addr, 5)
        assert body[0] == 0xE9, "0x%x bytes %s" % (addr, body.hex())
        rel = struct.unpack_from("<i", body, 1)[0]
        jmp_tgt = addr + 5 + rel

        thunk = funcs[addr]
        target = funcs[jmp_tgt]
        assert thunk.get("ported") is True, hex(addr)
        assert _arity(thunk["decl"]) == 0, thunk["decl"]
        assert _arity(target["decl"]) == 0, target["decl"]

        tname = _decl_name(thunk["decl"])
        cname = _decl_name(target["decl"])
        m = re.search(
            r"\b%s\s*\(\s*void\s*\)\s*\{([^}]*)\}" % re.escape(tname), src
        )
        assert m, "missing C body for %s" % tname
        body_c = m.group(1)
        assert cname + "(" in body_c, "%s must call %s; got %r" % (
            tname, cname, body_c)
        assert "return;" not in body_c
        assert body_c.strip() != ""


if __name__ == "__main__":
    test_fifteen_thunks_forward_xbe_jmp_targets()
    print("PASS: 15 decals.obj thunks tail-call their XBE JMP targets")
    sys.exit(0)
