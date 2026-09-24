"""Compare 2276 server pregame tick calls, message payloads, and state."""

import json
import subprocess
import sys
from pathlib import Path


CASES = (
    "quiet", "keepalive", "keepalive_null", "dead_client", "countdown_invalid",
    "countdown_wait", "countdown_send", "countdown_create_failed",
    "countdown_send_failed", "start_success", "loading_wait", "loading_pending",
    "loading_timeout", "loading_loaded",
)
TARGETS = {
    0x8e370: "clock", 0x40000000: "clock",
    0x128660: "active", 0x40014000: "active",
    0x12b650: "event", 0x40008000: "event",
    0x12df50: "remove", 0x4000c000: "remove",
    0x12caa0: "all_loaded", 0x40010000: "all_loaded",
    0x19f3a0: "wide", 0x40004000: "wide",
    0x12d150: "enough", 0x40018000: "enough",
    0x12d0c0: "each", 0x4001c000: "each",
    0x12d040: "teams", 0x40020000: "teams",
    0x8db80: "clear", 0x40024000: "clear",
    0x12b700: "create", 0x40028000: "create",
    0x12f430: "send", 0x4002c000: "send",
    0x12bdb0: "timer", 0x40030000: "timer",
    0x12dbb0: "precached", 0x40034000: "precached",
    0x12c0b0: "close", 0x40038000: "close",
    0x12c290: "start", 0x4003c000: "start",
}
EXPECTED = {
    "quiet": ("clock",),
    "keepalive": ("clock", "create", "send"),
    "keepalive_null": ("clock", "create", "send"),
    "dead_client": ("clock", "active", "event", "remove", "create", "send"),
    "countdown_invalid": ("clock", "enough", "clear", "create", "send"),
    "countdown_wait": ("clock", "enough", "each", "teams", "timer"),
    "countdown_send": ("clock", "enough", "each", "teams", "timer",
                       "timer", "create", "send"),
    "countdown_create_failed": ("clock", "enough", "each", "teams", "timer",
                                "timer", "create"),
    "countdown_send_failed": ("clock", "enough", "each", "teams", "timer",
                              "timer", "create", "send", "event"),
    "start_success": ("clock", "enough", "each", "teams", "timer", "precached",
                      "close", "start"),
    "loading_wait": ("clock",),
    "loading_pending": ("clock", "clock"),
    "loading_timeout": ("clock", "clock", "wide", "event", "remove", "all_loaded"),
    "loading_loaded": ("clock", "clock", "all_loaded"),
}
snapshots = Path(__file__).with_name("regression_snapshots")

if len(sys.argv) == 3 and sys.argv[1] == "--single":
    case = sys.argv[2]
    sys.path.insert(0, "tools/equivalence")
    from unicorn import Uc, UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX
    import unicorn_diff

    calls = [[], []]
    states = []
    old_hook_add = Uc.hook_add

    def capture_code(uc, address, size, user_data):
        side = 0 if 0x12e750 <= address < 0x12ea00 else (
            1 if 0x9000000 <= address < 0x9001000 else -1)
        if side < 0:
            return
        try:
            op = bytes(uc.mem_read(address, 1))[0]
            if op in (0xc3, 0xc2):
                states.append((side, uc.reg_read(UC_X86_REG_EAX) & 0xff,
                               bytes(uc.mem_read(0x700000, 0x500))))
            if op != 0xe8:
                return
            displacement = int.from_bytes(uc.mem_read(address + 1, 4),
                                          "little", signed=True)
            name = TARGETS.get(address + 5 + displacement)
            if name is None:
                return
            esp = uc.reg_read(UC_X86_REG_ESP)
            args = tuple(int.from_bytes(uc.mem_read(esp + 4 * i, 4), "little")
                         for i in range(3))
            if name == "clock":
                record = (name,)
            elif name == "event":
                fmt = bytes(uc.mem_read(args[0], 200)).split(b"\0")[0]
                if b"%s" in fmt:
                    value = bytes(uc.mem_read(args[1], 100)).split(b"\0")[0]
                elif b"%d" in fmt:
                    value = args[1]
                else:
                    value = None
                record = (name, fmt, value)
            elif name == "wide":
                record = (name, args[0], bytes(uc.mem_read(args[0], 0x20)), args[2])
            elif name == "create":
                record = (name, args[0], args[2], bytes(uc.mem_read(args[1], args[2])))
            elif name == "precached":
                record = (name, uc.reg_read(UC_X86_REG_EAX) if side == 0 else args[0])
            elif name in ("active", "all_loaded", "enough", "each", "teams",
                          "timer", "close", "start"):
                record = (name, args[0])
            elif name in ("remove", "send"):
                record = (name, args[0], args[1])
            else:
                record = (name, args[0], args[1], args[2])
            calls[side].append(record)
        except Exception:
            pass

    def add_capture_hook(self, hook_type, callback, *args, **kwargs):
        handle = old_hook_add(self, hook_type, callback, *args, **kwargs)
        if hook_type & UC_HOOK_CODE:
            old_hook_add(self, UC_HOOK_CODE, capture_code)
        return handle

    Uc.hook_add = add_capture_hook
    snapshot = snapshots / ("system_link_pregame_tasks_" + case + ".json")
    sys.argv = [
        "unicorn_diff.py", "network_game_server_idle_pregame_tasks",
        "--allow-stubs", "--real-callees", "--seeds", "1",
        "--state-snapshot", str(snapshot), "--no-concolic", "--no-leaf-cache",
        "--no-stub-arg-trace", "-q",
    ]
    try:
        unicorn_diff.main()
    except SystemExit as exc:
        if exc.code and case != "loading_wait":
            sys.exit(exc.code)

    names = tuple(record[0] for record in calls[0])
    if calls[0] != calls[1] or names != EXPECTED[case]:
        print("Pregame tick helper calls differ", case, calls)
        sys.exit(1)
    if len(states) != 2 or states[0][0] != 0 or states[1][0] != 1 or states[0][1:] != states[1][1:]:
        print("Pregame tick return or server state differs", case, states)
        sys.exit(1)
    before = bytes.fromhex(json.loads(snapshot.read_text())["regions"]["0x700000"])
    after = states[0][2]
    if case in ("quiet", "countdown_wait", "start_success", "loading_wait",
                "loading_pending", "loading_timeout", "loading_loaded"):
        if after != before:
            print("Pregame tick unexpectedly changed server state", case)
            sys.exit(1)
    if case.startswith("countdown"):
        messages = [record for record in calls[0] if record[0] == "create"]
        if messages and messages[0] != ("create", 7, 2,
                                        b"\xff\xff" if case == "countdown_invalid"
                                        else b"\x03\x00"):
            print("Pregame countdown payload differs", case, messages)
            sys.exit(1)
    if case in ("keepalive", "keepalive_null", "dead_client"):
        if ("create", 10, 2, b"\0\0") not in calls[0]:
            print("Pregame keepalive payload differs", case)
            sys.exit(1)
        if int.from_bytes(after[0x480:0x484], "little") != 10000:
            print("Pregame keepalive time differs", case)
            sys.exit(1)
    if case in ("countdown_invalid", "countdown_send"):
        if int.from_bytes(after[0x490:0x494], "little") != 10000:
            print("Pregame countdown update time differs", case)
            sys.exit(1)
    if case == "countdown_invalid" and after[0x494] != 0:
        print("Pregame invalid countdown remains active")
        sys.exit(1)
    if case in ("countdown_create_failed", "countdown_send_failed"):
        if after[0x490:0x494] != before[0x490:0x494]:
            print("Pregame failed countdown advanced time", case)
            sys.exit(1)
    print("Pregame tick calls, packet, and state match", case)
else:
    for case in CASES:
        result = subprocess.run([sys.executable, __file__, "--single", case], check=False)
        if result.returncode:
            sys.exit(result.returncode)
    print("Pregame focused branches match")
