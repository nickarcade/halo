#!/usr/bin/env python3
"""Capture a whole-game-state equivalence snapshot from a live MP/SP game.

Boots the PRISTINE build (cachebeta.xbe) straight into a map + game type with
one local player (tools/xbox/boot_gametype.py), waits for a chosen game tick,
then atomically captures -- inside one QMP stop/cont window -- every byte of
writable game memory:

  * the XBE .data/.bss section (all engine globals: current_game_engine,
    oddball_globals, ctf_globals, game_time_globals, pool pointers, ...),
    bounds read from the XBE section table;
  * the game-state allocation (players, objects, object bodies, AI, ...),
    base and size read from game_state_globals at capture time.

Nothing is guessed about which globals a target reads, so one snapshot serves
every function that runs in that game state.  Output is the
{"regions": {hexaddr: hexbytes}} JSON that unicorn_diff --state-snapshot loads:

    capture_gametype_snapshot.py --variant team_oddball --at-tick 300 \\
        --out artifacts/equivalence/snapshots/oddball_t300.json
    unicorn_diff.py oddball_engine_update --allow-stubs --real-callees \\
        --state-snapshot artifacts/equivalence/snapshots/oddball_t300.json

Captured state is verified before it is written: map_name and
current_game_engine must match the request, the halt guard must be clear, and
the object and player tables must resolve to live data_t headers inside the
captured game-state allocation.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools" / "xbox"))
sys.path.insert(0, str(ROOT / "tools" / "equivalence"))

import boot_gametype as bg  # noqa: E402
from qmp_capture import QMPCapture, DATA_T_MAGIC  # noqa: E402

PRISTINE_XBE = ROOT / "halo-patched" / "cachebeta.xbe"
PRISTINE_MD5 = "c7869590a1c64ad034e49a5ee0c02465"

# game_state_globals_t (src/types.h): base_address @+0x04, cpu_allocation_size @+0x08
GSG_BASE_OFF = 0x04
GSG_CPU_SIZE_OFF = 0x08
# data_t header (qmp_capture): magic @+0x28
DATA_T_MAGIC_OFF = 0x28


def xbe_section(path: Path, name: str) -> tuple[int, int]:
    """(virtual address, virtual size) of an XBE section, from its header."""
    d = path.read_bytes()
    base = struct.unpack_from("<I", d, 0x104)[0]
    count, headers = struct.unpack_from("<II", d, 0x11C)
    for i in range(count):
        o = headers - base + i * 0x38
        _flags, va, vsize, _raw, _rsize, name_addr = struct.unpack_from("<IIIIII", d, o)
        sec = d[name_addr - base:name_addr - base + 16].split(b"\0")[0].decode()
        if sec == name:
            return va, vsize
    raise KeyError(f"no section {name!r} in {path}")


def _read(regions: dict[int, bytes], addr: int, size: int) -> bytes | None:
    for base, blob in regions.items():
        if base <= addr and addr + size <= base + len(blob):
            return blob[addr - base:addr - base + size]
    return None


def _u32(regions, addr):
    b = _read(regions, addr, 4)
    return struct.unpack("<I", b)[0] if b else None


def verify(regions: dict[int, bytes], variant: str, map_name: str) -> list[str]:
    problems = []
    raw_map = _read(regions, bg.kb_data_addr("map_name"), 0x40)
    got_map = raw_map.split(b"\0")[0].decode(errors="replace") if raw_map else None
    if got_map != map_name:
        problems.append(f"map_name {got_map!r} != {map_name!r}")
    engine = _u32(regions, bg.kb_data_addr("current_game_engine"))
    if engine != bg.ENGINE_RECORDS.get(variant, 0):
        problems.append(f"current_game_engine {engine:#x} unexpected for {variant}")
    guard = _read(regions, bg.HALT_GUARD, 1)
    if guard != b"\0":
        problems.append(f"halt guard set ({guard!r})")
    for table in ("object_header_data", "player_data"):
        hdr = _u32(regions, bg.kb_data_addr(table))
        magic = _u32(regions, hdr + DATA_T_MAGIC_OFF) if hdr else None
        if magic != DATA_T_MAGIC:
            problems.append(f"{table} -> {hdr!r}: no data_t magic in captured state")
    return problems


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--variant", required=True)
    ap.add_argument("--map", default=None)
    ap.add_argument("--xbe", default="cachebeta.xbe",
                    help="title-dir XBE to boot (default: pristine cachebeta.xbe)")
    ap.add_argument("--at-tick", type=int, required=True,
                    help="capture at the first poll where game time >= this tick")
    ap.add_argument("--timeout", type=float, default=240.0)
    ap.add_argument("--description", default="")
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args()

    if hashlib.md5(PRISTINE_XBE.read_bytes()).hexdigest() != PRISTINE_MD5:
        sys.exit(f"{PRISTINE_XBE} is not the pristine 2276 build")
    map_name = args.map or bg.DEFAULT_MP_MAP
    data_va, data_size = xbe_section(PRISTINE_XBE, ".data")

    box = bg.Box(args.host)
    bg.boot(box, args.variant, map_name, args.xbe, settle=5)
    try:
        with QMPCapture(settle_s=10.0) as cap:
            deadline = time.monotonic() + args.timeout
            tick = None
            while time.monotonic() < deadline:
                try:
                    tick = cap.tick()
                except Exception:
                    tick = None
                    cap._gt_ptr = None
                if tick is not None and args.at_tick <= tick < args.at_tick + 100000:
                    break
                time.sleep(0.05)
            else:
                sys.exit(f"game time never reached {args.at_tick} (last {tick})")
            gsg = bg.kb_data_addr("game_state_globals")
            pool_base = cap.read_u32(gsg + GSG_BASE_OFF)
            pool_size = cap.read_u32(gsg + GSG_CPU_SIZE_OFF)
            specs = [(data_va, data_size), (pool_base, pool_size)]
            regions = cap.capture_regions(specs)
    finally:
        box.delete_init()

    captured_tick = _u32(regions, _u32(regions, bg.kb_data_addr("game_time_globals")) + 0x0C)
    problems = verify(regions, args.variant, map_name)
    if problems:
        for p in problems:
            print("VERIFY FAIL:", p)
        return 1

    snap = {
        "description": args.description or f"{args.variant} on {map_name} at tick {captured_tick}",
        "build_label": f"cachebeta.xbe {PRISTINE_MD5} (pristine 2276)",
        "captured_at": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "meta": {"variant": args.variant, "map_name": map_name, "tick": captured_tick,
                 "regions": [[hex(a), hex(len(b))] for a, b in regions.items()]},
        "regions": {hex(a): b.hex() for a, b in regions.items()},
    }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(snap), encoding="utf-8")
    print(f"captured {args.variant} tick {captured_tick}: "
          + ", ".join(f"{a:#x}+{len(b):#x}" for a, b in regions.items())
          + f" -> {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
