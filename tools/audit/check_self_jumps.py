#!/usr/bin/env python3
"""Post-link guard: `jmp $` self-loops that the original binary does not have.

When a lift polls memory that another agent updates (the DirectSound runtime,
an IO completion path, hardware) through a plain, non-volatile lvalue, clang is
allowed to assume the value never changes inside the loop.  It hoists the load
and the wait collapses to a two-byte `jmp $` (EB FE).  VC71 does not hoist such
loads, so the VC71 score stays high and nothing asserts: the game just freezes
with EIP pinned on the self-jump.

This happened for real in dsound_flush (FUN_001ca130): with the XDK helper
dsound_stream_is_active ported into the same TU, clang inlined its status read
into the stop-wait loop and save-and-quit hung forever (b0b676c1e).  The fix
is a volatile read in the poller (sound_dsound_xbox.c), not unporting it.

This script disassembles every export in the linked PE, finds each
instruction that jumps to its own address, and fails unless the original
function in cachebeta.xbe also contains a self-jump (intentional spins such as
the `for (;;);` after XLaunchNewImageA).

Exit status: 0 = clean, 1 = unexplained self-jump, 2 = usage/tooling error.
"""

import argparse
import json
import os
import sys

try:
    import pefile
except ImportError:
    print("error: pefile not installed (pip install 'pefile~=2023.2.7')",
          file=sys.stderr)
    sys.exit(2)

try:
    import capstone
except ImportError:
    print("error: capstone not installed (pip install capstone)",
          file=sys.stderr)
    sys.exit(2)

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DEFAULT_PE = os.path.join(ROOT_DIR, "build", "halo")
BOUNDS_PATH = os.path.join(ROOT_DIR, "tools", "verify", "function_bounds.json")

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

# EB FE (jmp rel8 -2) and E9 FB FF FF FF (jmp rel32 -5).
SELF_JUMP_ENCODINGS = (b"\xeb\xfe", b"\xe9\xfb\xff\xff\xff")


def _exports(pe):
    """Map export name -> RVA."""
    out = {}
    directory = getattr(pe, "DIRECTORY_ENTRY_EXPORT", None)
    if directory is None:
        return out
    for sym in directory.symbols:
        if sym.name:
            out[sym.name.decode("ascii", "replace")] = sym.address
    return out


def _original_extents():
    """Map function name -> (start VA, end VA) in cachebeta.xbe."""
    extents = {}
    try:
        import check_arg_counts as cac
        for entry in cac._load_kb():
            extents.setdefault(entry.name, (entry.addr, None))
    except Exception:  # noqa: BLE001 -- best effort, bounds table below
        pass
    try:
        with open(BOUNDS_PATH, "r") as f:
            bounds = json.load(f)
    except (OSError, ValueError):
        bounds = {}
    by_addr = {}
    for addr_str, row in bounds.items():
        try:
            by_addr[int(addr_str, 16)] = int(row["end"], 16)
        except (KeyError, TypeError, ValueError):
            continue
        name = row.get("name")
        if name:
            extents.setdefault(name, (int(addr_str, 16), None))
    return {name: (start, end if end is not None else by_addr.get(start))
            for name, (start, end) in extents.items()}


def _original_has_self_jump(name, extents, md):
    """True/False from the pristine XBE; None when the original is unknown."""
    start, end = extents.get(name, (None, None))
    if start is None or end is None:
        return None
    try:
        import check_callee_identity as cci
        body = cci._va_bytes(start, end)
    except Exception:  # noqa: BLE001 -- pristine XBE unavailable
        return None
    if not body:
        return None
    for insn in md.disasm(body, start):
        if insn.bytes in SELF_JUMP_ENCODINGS:
            return True
    return False


def check(pe_path, verbose=False):
    if not os.path.isfile(pe_path):
        print(f"error: PE not found: {pe_path}\n  build it first "
              f"(python3 tools/build/build.py)", file=sys.stderr)
        return 2

    pe = pefile.PE(pe_path, fast_load=False)
    image = pe.get_memory_mapped_image()
    exports = _exports(pe)
    if not exports:
        print(f"error: no exports in {pe_path}; cannot attribute functions",
              file=sys.stderr)
        return 2

    ordered = sorted(set(exports.values()))
    rva_to_name = {}
    for name, rva in sorted(exports.items()):
        rva_to_name.setdefault(rva, name)

    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    hits = []
    for i, rva in enumerate(ordered):
        end = ordered[i + 1] if i + 1 < len(ordered) else rva + 0x400
        end = min(end, len(image))
        if rva >= end:
            continue
        body = image[rva:end]
        # Cheap byte prefilter before disassembling.
        if not any(enc in body for enc in SELF_JUMP_ENCODINGS):
            continue
        for insn in md.disasm(body, rva):
            if insn.bytes in SELF_JUMP_ENCODINGS:
                hits.append((rva_to_name.get(rva, f"rva_{rva:#x}"), rva,
                             insn.address))
            elif insn.mnemonic in ("ret", "int3") and insn.address > rva:
                # Stop at padding after the last return only when the
                # remaining bytes are pure int3 fill.
                rest = body[insn.address + insn.size - rva:]
                if rest and not rest.strip(b"\xcc"):
                    break

    extents = _original_extents()
    failures = []
    for name, rva, site in hits:
        original = _original_has_self_jump(name, extents, md)
        if original:
            if verbose:
                print(f"ok: {name} self-jump at rva {site:#x} "
                      f"(original has one too)")
            continue
        failures.append((name, rva, site, original))

    if verbose:
        print(f"checked {len(ordered)} export(s) in {pe_path}: "
              f"{len(hits)} self-jump(s), {len(failures)} unexplained")

    if not failures:
        return 0

    print("error: `jmp $` self-loop not present in the original binary",
          file=sys.stderr)
    for name, rva, site, original in failures:
        why = ("original has none" if original is False
               else "original function not found")
        print(f"  {name} (rva {rva:#x}): self-jump at rva {site:#x} "
              f"-- {why}", file=sys.stderr)
    print("", file=sys.stderr)
    print("  A busy-wait polls memory through a non-volatile lvalue, so clang "
          "hoisted", file=sys.stderr)
    print("  the load and the loop can never exit (silent freeze).  Read the "
          "polled", file=sys.stderr)
    print("  field through a `volatile` lvalue in the lift.", file=sys.stderr)
    return 1


def main():
    parser = argparse.ArgumentParser(
        description="Fail on `jmp $` self-loops absent from the original XBE.")
    parser.add_argument("pe", nargs="?", default=DEFAULT_PE,
                        help="linked PE to inspect (default: build/halo)")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="report what was checked even when clean")
    args = parser.parse_args()
    return check(args.pe, verbose=args.verbose)


if __name__ == "__main__":
    sys.exit(main())
