"""Verify the 2276 system-link inventory redirects in a patched XBE.

The static call graph defines the 15-object scope.  The current KB determines
whether each entry is meant to be active; the XBE bytes prove what was built.
"""

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, "tools/equivalence")
from xbe_image import load_xbe, read_va_raw

from system_link_inventory import SCOPED_OBJECTS


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--patched", default="halo-patched/default.xbe")
    args = parser.parse_args()

    graph = json.loads(Path("artifacts/ntsc_callgraph/callgraph.json").read_text())["functions"]
    kb = json.loads(Path("kb.json").read_text())
    nested = {int(function["addr"], 16): function
              for obj in kb["objects"] for function in obj.get("functions", [])
              if "addr" in function}
    addresses = sorted(int(addr, 16) for addr, info in graph.items()
                       if info["object"] in SCOPED_OBJECTS)
    if len(addresses) != 391:
        raise RuntimeError(f"expected 391 scoped functions, found {len(addresses)}")

    pristine, pristine_sections = load_xbe("halo-patched/cachebeta.xbe")
    if hashlib.md5(pristine).hexdigest() != "c7869590a1c64ad034e49a5ee0c02465":
        raise RuntimeError("wrong 2276 reference XBE")
    patched, patched_sections = load_xbe(args.patched)

    failures = []
    for address in addresses:
        key = hex(address)
        entry = kb.get(key, nested.get(address))
        if entry is not None and "ported" not in entry:
            entry = nested.get(address, entry)
        if entry is None or entry.get("ported") is not True:
            failures.append(f"{key}: KB entry missing or not ported:true")
            continue
        original = read_va_raw(pristine, pristine_sections, address, 6)
        current = read_va_raw(patched, patched_sections, address, 6)
        if current == original:
            failures.append(f"{key}: original body still active")
        elif len(current) != 6 or current[0] != 0x68 or current[5] != 0xc3:
            failures.append(f"{key}: expected push-target; ret redirect, got {current.hex()}")
        else:
            target = struct.unpack_from("<I", current, 1)[0]
            if not any(section.name in {".text.patch", ".thunks.patch", ".rvthunks"}
                       and section.raw_contains_va(target)
                       for section in patched_sections):
                failures.append(f"{key}: redirect target {target:#x} is outside patched code")

    print(f"system-link redirects: {len(addresses) - len(failures)}/{len(addresses)} valid")
    for failure in failures:
        print(failure)
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
