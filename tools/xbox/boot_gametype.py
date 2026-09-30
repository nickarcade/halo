#!/usr/bin/env python3
"""Boot a Halo title on xemu straight into a map + game type, one local player.

The debug build runs E:\\GAMES\\halo-patched\\init.txt as console commands at
boot.  Two lines drop a single local player into a live multiplayer game with
the chosen engine running (spawns, per-tick update, objective items):

    game_variant team_oddball
    map_name levels\\test\\bloodgulch\\bloodgulch

No menus, no second box, no controller input.  `--variant none` (any string
that is not a game type) boots a single-player map.

After booting, the scenario is PROVEN, never assumed: map_name must equal the
request, current_game_engine must point at the requested engine record, the
halt guard must be clear, and game time must advance between two samples.

    boot_gametype.py --variant team_oddball --check            # patched build
    boot_gametype.py --variant ctf --xbe cachebeta.xbe --check # pristine
    boot_gametype.py --variant none --map 'levels\\a10\\a10' --check
    boot_gametype.py --restore                                  # delete init.txt

Transport: XBDM over xbdm_rdcp.py.  On WSL it re-execs under Windows Python,
so the init.txt staging file must live on a drive Windows can see (the repo,
on G:), never /tmp.  Bridged xemu guests need HALO_WINDOWS_REEXEC=1.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
RDCP = ROOT / "tools" / "xbox" / "xbdm_rdcp.py"
TITLE_ROOT = r"E:\GAMES\halo-patched"
DEFAULT_MP_MAP = r"levels\test\bloodgulch\bloodgulch"

# Game-engine definition records (struct game_engine, stride 0x88).  Not named
# in kb.json; each value is what current_game_engine held after booting the
# matching variant (2026-09-30).
ENGINE_RECORDS = {
    "ctf": 0x2EFE88,
    "team_ctf": 0x2EFE88,
    "team_king": 0x2EFF10,
    "king": 0x2EFF10,
    "team_oddball": 0x2EFFE8,
    "oddball": 0x2EFFE8,
    "team_race": 0x2F0070,
    "race": 0x2F0070,
}

# Byte set to 1 by the halt path (assert / plain blue screen); the assert text
# is left in the error buffer.  Not named in kb.json.
HALT_GUARD = 0x46E392
ERROR_TEXT = 0x5AA8E8


def kb_data_addr(name: str) -> int:
    """Address of a kb.json global by its declared name."""
    kb = json.loads((ROOT / "kb.json").read_text(encoding="utf-8"))
    pat = re.compile(r"[\s\*]%s\s*(\[|;)" % re.escape(name))
    for obj in kb["objects"]:
        for d in obj.get("data", []):
            if pat.search(" " + d["decl"]):
                return int(d["addr"], 16)
    raise KeyError(f"no kb.json data entry named {name!r}")


def _win_path(p: Path) -> str:
    s = str(p.resolve())
    m = re.match(r"^/mnt/([a-z])/(.*)$", s)
    return f"{m.group(1).upper()}:/{m.group(2)}" if m else s


class Box:
    def __init__(self, host: str):
        self.host = host

    def rdcp(self, *args: str, timeout: int = 60) -> str:
        argv = [sys.executable, str(RDCP), "--host", self.host, *args]
        try:
            r = subprocess.run(argv, capture_output=True, text=True, timeout=timeout)
        except subprocess.TimeoutExpired:
            return "TIMEOUT"
        return (r.stdout or "") + (r.stderr or "")

    def getmem(self, addr: int, length: int) -> bytes | None:
        out = self.rdcp("getmem addr=0x%x length=0x%x" % (addr, length))
        hexs = "".join(re.findall(r"^[0-9a-fA-F?]+$", out, re.M))
        if not hexs or "?" in hexs or len(hexs) != 2 * length:
            return None
        return bytes.fromhex(hexs)

    def u32(self, addr: int) -> int | None:
        b = self.getmem(addr, 4)
        return int.from_bytes(b, "little") if b else None

    def write_init(self, lines: list[str]) -> None:
        stage = ROOT / "tmp" / "boot_gametype_init.txt"
        stage.parent.mkdir(parents=True, exist_ok=True)
        stage.write_bytes(("\r\n".join(lines) + "\r\n").encode("ascii"))
        out = self.rdcp("--sendfile", _win_path(stage), TITLE_ROOT + r"\init.txt",
                        timeout=120)
        stage.unlink()
        if "=>" not in out:
            raise RuntimeError(f"init.txt upload failed: {out.strip()[-200:]}")

    def delete_init(self) -> str:
        return self.rdcp("delete name=%s\\init.txt" % TITLE_ROOT).strip()

    def magicboot(self, xbe: str) -> None:
        out = self.rdcp("magicboot title=%s\\%s debug" % (TITLE_ROOT, xbe))
        if "200" not in out:
            raise RuntimeError(f"magicboot failed: {out.strip()[-200:]}")


def init_lines(variant: str, map_name: str) -> list[str]:
    return [f"game_variant {variant}", f"map_name {map_name}"]


def game_tick(box: Box) -> int | None:
    gt = box.u32(kb_data_addr("game_time_globals"))
    if gt is None or not 0x80000000 <= gt < 0x84000000:
        return None
    return box.u32(gt + 0x0C)


def prove(box: Box, variant: str, map_name: str) -> dict:
    """Read back what is actually running; ok=True only if it is the request."""
    t1 = game_tick(box)
    time.sleep(3)
    t2 = game_tick(box)
    guard = box.getmem(HALT_GUARD, 1)
    raw_map = box.getmem(kb_data_addr("map_name"), 0x40)
    running_map = raw_map.split(b"\0")[0].decode(errors="replace") if raw_map else None
    engine = box.u32(kb_data_addr("current_game_engine"))
    want_engine = ENGINE_RECORDS.get(variant, 0)
    res = {
        "map_name": running_map,
        "current_game_engine": engine,
        "halt_guard": guard[0] if guard else None,
        "tick": [t1, t2],
    }
    res["ok"] = (running_map == map_name and engine == want_engine
                 and guard == b"\0" and t1 is not None and t2 is not None
                 and t2 > t1)
    if guard and guard != b"\0":
        err = box.getmem(ERROR_TEXT, 0x600)
        res["error_text"] = err.split(b"\0")[0].decode(errors="replace") if err else None
    return res


def boot(box: Box, variant: str, map_name: str, xbe: str, settle: float) -> None:
    box.write_init(init_lines(variant, map_name))
    box.magicboot(xbe)
    time.sleep(settle)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default=os.environ.get("XBOX_HOST", "127.0.0.1"))
    ap.add_argument("--variant", default="team_oddball",
                    help="game_variant string (ctf, team_oddball, team_king, "
                         "team_race, slayer, ...); 'none' = single player")
    ap.add_argument("--map", default=None,
                    help=f"map_name (default {DEFAULT_MP_MAP} for MP variants)")
    ap.add_argument("--xbe", default="default.xbe",
                    help="XBE in the title dir: default.xbe (patched) or cachebeta.xbe (pristine)")
    ap.add_argument("--settle", type=float, default=45.0,
                    help="seconds to wait after magicboot before checking")
    ap.add_argument("--check", action="store_true", help="prove the scenario after boot")
    ap.add_argument("--keep-init", action="store_true",
                    help="leave init.txt on the box (default: delete after boot)")
    ap.add_argument("--restore", action="store_true", help="only delete init.txt and exit")
    args = ap.parse_args()

    box = Box(args.host)
    if args.restore:
        print(box.delete_init())
        return 0
    map_name = args.map or DEFAULT_MP_MAP
    boot(box, args.variant, map_name, args.xbe, args.settle)
    rc = 0
    if args.check:
        res = prove(box, args.variant, map_name)
        print(json.dumps(res, indent=2))
        rc = 0 if res["ok"] else 1
    if not args.keep_init:
        box.delete_init()
    return rc


if __name__ == "__main__":
    sys.exit(main())
