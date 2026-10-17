"""Compare the 2276 server tick's dispatch, calls, and full server state."""

import json
import subprocess
import sys
from pathlib import Path


CASES = (
    "network_down", "split_local", "invalid_game", "idle_failed",
    "no_client_pregame", "new_client_accepted", "new_client_rejected",
    "public_failed", "ingame", "postgame", "unknown_state",
)
TARGETS = {
    0x82300: "transport", 0x40000000: "transport",
    0x12a170: "split", 0x40004000: "split",
    0xe44d0: "display", 0x4001c000: "display",
    0x8f390: "error", 0x40020000: "error",
    0x12c160: "game_valid", 0x40048000: "game_valid",
    0x129cf0: "idle", 0x4000c000: "idle",
    0x12d880: "add", 0x40010000: "add",
    0x1283c0: "address", 0x40014000: "address",
    0x81b90: "address_string", 0x40018000: "address_string",
    0x12b650: "event", 0x40008000: "event",
    0x129130: "close", 0x40024000: "close",
    0x12d9f0: "public", 0x40028000: "public",
    0x12e580: "machines", 0x4002c000: "machines",
    0x12e750: "pregame", 0x40030000: "pregame",
    0x12db60: "postgame", 0x4004c000: "postgame",
    0x40034000: "postgame_inlined",
}
EXPECTED = {
    "network_down": ("transport", "split", "display", "error"),
    "split_local": ("transport", "split", "idle", "public", "machines", "pregame"),
    "invalid_game": ("transport", "event"),
    "idle_failed": ("transport", "idle", "event"),
    "no_client_pregame": ("transport", "idle", "public", "machines", "pregame"),
    "new_client_accepted": ("transport", "idle", "add", "address", "address_string",
                            "event", "public", "machines", "pregame"),
    "new_client_rejected": ("transport", "idle", "add", "event", "close",
                            "public", "machines", "pregame"),
    "public_failed": ("transport", "idle", "public", "event"),
    "ingame": ("transport", "idle", "public", "machines"),
    "postgame": ("transport", "idle", "public", "machines", "postgame"),
    "unknown_state": ("transport", "idle", "public", "machines", "event"),
}
snapshots = Path(__file__).with_name("regression_snapshots")

if len(sys.argv) == 3 and sys.argv[1] == "--single":
    case = sys.argv[2]
    sys.path.insert(0, "tools/equivalence")
    import abi
    abi.POINTER_SLOT = 0x1000
    from unicorn import Uc, UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX
    from unicorn.x86_const import UC_X86_REG_ESI, UC_X86_REG_EBX
    import unicorn_diff

    calls = [[], []]
    states = []
    old_hook_add = Uc.hook_add

    def capture_code(uc, address, size, user_data):
        side = 0 if 0x12eb20 <= address < 0x12eca0 else (
            1 if 0x9000000 <= address < 0x9001000 else -1)
        if side < 0:
            return
        try:
            op = bytes(uc.mem_read(address, 1))[0]
            if op in (0xc3, 0xc2):
                states.append((side, uc.reg_read(UC_X86_REG_EAX) & 0xff,
                               bytes(uc.mem_read(0x10000000, 0x4c0))))
            if op != 0xe8:
                return
            displacement = int.from_bytes(uc.mem_read(address + 1, 4),
                                          "little", signed=True)
            name = TARGETS.get(address + 5 + displacement)
            if name is None or name == "game_valid":
                return
            esp = uc.reg_read(UC_X86_REG_ESP)
            args = tuple(int.from_bytes(uc.mem_read(esp + 4 * i, 4), "little")
                         for i in range(3))
            if name in ("transport", "split"):
                record = (name,)
            elif name == "display":
                record = (name, args[0])
            elif name == "error":
                record = (name, args[0],
                          bytes(uc.mem_read(args[1], 100)).split(b"\0")[0])
            elif name == "idle":
                record = (name, args[0], args[1])
            elif name == "add":
                record = ((name, uc.reg_read(UC_X86_REG_EAX), args[0]) if side == 0
                          else (name, args[0], args[1]))
            elif name == "address":
                record = (name, args[0], args[2])
            elif name == "address_string":
                record = (name, bytes(uc.mem_read(args[0], 0x18)))
            elif name == "event":
                fmt = bytes(uc.mem_read(args[0], 200)).split(b"\0")[0]
                value = (bytes(uc.mem_read(args[1], 100)).split(b"\0")[0]
                         if b"%s" in fmt and args[1] else None)
                record = (name, fmt, value)
            elif name == "close":
                record = (name, args[0], args[1])
            elif name in ("public", "pregame", "postgame"):
                record = (name, uc.reg_read(UC_X86_REG_ESI) if side == 0 else args[0])
            elif name == "machines":
                record = (name, uc.reg_read(UC_X86_REG_EBX) if side == 0 else args[0])
            elif name == "postgame_inlined":
                record = ("postgame", 0x10000000)
            else:
                record = (name, args[0])
            calls[side].append(record)
        except Exception:
            pass

    def add_capture_hook(self, hook_type, callback, *args, **kwargs):
        handle = old_hook_add(self, hook_type, callback, *args, **kwargs)
        if hook_type & UC_HOOK_CODE:
            old_hook_add(self, UC_HOOK_CODE, capture_code)
        return handle

    Uc.hook_add = add_capture_hook
    snapshot = snapshots / ("system_link_server_idle_" + case + ".json")
    sys.argv = [
        "unicorn_diff.py", "network_game_server_idle", "--allow-stubs",
        "--real-callees", "--seeds", "1", "--state-snapshot", str(snapshot),
        "--no-concolic", "--no-leaf-cache", "--no-stub-arg-trace", "-q",
    ]
    try:
        unicorn_diff.main()
    except SystemExit as exc:
        if exc.code:
            sys.exit(exc.code)

    if calls[0] != calls[1] or tuple(r[0] for r in calls[0]) != EXPECTED[case]:
        print("Server idle helper calls differ", case, calls)
        sys.exit(1)
    if len(states) != 2 or states[0][0] != 0 or states[1][0] != 1 or states[0][1:] != states[1][1:]:
        print("Server idle return or state differs", case, len(states))
        sys.exit(1)
    before = bytes.fromhex(json.loads(snapshot.read_text())["arg_overrides"]["server"])
    after = states[0][2]
    if after != before[:0x4c0]:
        print("Server idle changed state unexpectedly", case)
        sys.exit(1)
    if states[0][1] != (0 if case in ("network_down", "idle_failed",
                                   "public_failed", "unknown_state") else 1):
        print("Server idle expected result differs", case)
        sys.exit(1)
    if case == "new_client_rejected" and ("close", 0x710000, 0x720000) not in calls[0]:
        print("Server idle rejected connection was not closed")
        sys.exit(1)
    print("Server idle calls and state match", case)
else:
    for case in CASES:
        result = subprocess.run([sys.executable, __file__, "--single", case], check=False)
        if result.returncode:
            sys.exit(result.returncode)
    print("Server idle focused branches match")
