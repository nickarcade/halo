"""Compare 2276 server machine calls, packet sizes, and removal paths."""

import os
import subprocess
import sys
from pathlib import Path


CASES = (
    "inactive", "inactive_index2", "inactive_remove_fail", "idle_fail",
    "disconnected", "read_empty", "read_message", "message_fail_remove",
    "message_fail_fallback", "two_slots",
)
TARGETS = {
    0x128660: "active", 0x129cf0: "idle",
    0x128360: "connected", 0x1298f0: "read",
    0x130580: "handle", 0x12e090: "remove",
    0x12df50: "remove_client", 0x12de20: "dump",
    0x12b650: "event",
}
SYMBOL_TARGETS = {
    "network_connection_active": "active",
    "network_connection_idle": "idle",
    "network_connection_connected": "connected",
    "network_connection_read": "read",
    "network_game_server_handle_client_message": "handle",
    "network_game_server_remove_machine_from_game": "remove",
    "network_game_server_remove_client_machine_from_game": "remove_client",
    "network_game_server_dump": "dump",
    "network_event": "event",
}
EXPECTED = {
    "inactive": ("active", "remove", "dump"),
    "inactive_index2": ("active", "remove", "dump"),
    "two_slots": ("active", "idle", "connected", "read", "active", "remove", "dump"),
    "inactive_remove_fail": ("active", "remove", "dump"),
    "idle_fail": ("active", "idle", "remove"),
    "disconnected": ("active", "idle", "connected", "remove"),
    "read_empty": ("active", "idle", "connected", "read"),
    "read_message": ("active", "idle", "connected", "read", "handle", "read"),
    "message_fail_remove": ("active", "idle", "connected", "read", "handle", "remove"),
    "message_fail_fallback": ("active", "idle", "connected", "read", "handle",
                              "remove", "remove_client"),
}
snapshots = Path(__file__).with_name("regression_snapshots")

if len(sys.argv) == 3 and sys.argv[1] == "--single":
    case = sys.argv[2]
    sys.path.insert(0, "tools/equivalence")
    from unicorn import Uc, UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_ESP
    import unicorn_diff
    from stubs import StubManager

    calls = [[], []]
    observed = [[], []]
    states = []
    old_hook_add = Uc.hook_add
    old_prepare_stubs = StubManager.prepare_stubs
    stub_managers = []

    def capture_prepare_stubs(self, *args, **kwargs):
        result = old_prepare_stubs(self, *args, **kwargs)
        stub_managers.append(self)
        return result

    StubManager.prepare_stubs = capture_prepare_stubs

    def capture_code(uc, address, size, user_data):
        side = 0 if 0x12e580 <= address < 0x12e743 else (
            1 if 0x9000000 <= address < 0x9001000 else -1)
        if side < 0:
            return
        try:
            op = bytes(uc.mem_read(address, 1))[0]
            if op in (0xc3, 0xc2):
                states.append(bytes(uc.mem_read(0x700000, 0x500)))
            if op != 0xe8:
                return
            displacement = int.from_bytes(uc.mem_read(address + 1, 4), "little", signed=True)
            target = address + 5 + displacement
            if side == 1 and stub_managers:
                symbol = stub_managers[-1]._stub_names.get(target, "").lstrip("_")
                name = SYMBOL_TARGETS.get(symbol)
            else:
                name = TARGETS.get(target)
            observed[side].append((hex(address), hex(target), name))
            if name is None:
                return
            esp = uc.reg_read(UC_X86_REG_ESP)
            args = tuple(int.from_bytes(uc.mem_read(esp + 4 * i, 4), "little")
                         for i in range(4))
            if name in ("active", "connected"):
                record = (name, args[0])
            elif name == "idle":
                record = (name, args[0], args[1], args[2])
            elif name == "read":
                record = (name, args[0],
                          int.from_bytes(uc.mem_read(args[2], 4), "little"), args[3])
            elif name == "handle":
                record = (name, args[0], args[1], args[3],
                          bytes(uc.mem_read(args[2], args[3])))
            elif name in ("remove", "remove_client"):
                record = (name, args[0], args[1])
            elif name == "dump":
                record = (name, args[0])
            else:
                message = bytes(uc.mem_read(args[0], 200)).split(b"\0")[0]
                record = (name, message)
            calls[side].append(record)
        except Exception:
            pass

    def add_capture_hook(self, hook_type, callback, *args, **kwargs):
        handle = old_hook_add(self, hook_type, callback, *args, **kwargs)
        if hook_type & UC_HOOK_CODE:
            old_hook_add(self, UC_HOOK_CODE, capture_code)
        return handle

    Uc.hook_add = add_capture_hook
    # Stub same-object callees on both sides so the snapshot's helper returns
    # apply equally when the candidate's source reshapes the outer loop.
    os.environ["BIPED_SIBLING_RESOLVE"] = "1"
    sys.argv = [
        "unicorn_diff.py", "network_game_server_handle_client_machines",
        "--allow-stubs", "--no-real-callees", "--seeds", "1",
        "--state-snapshot", str(snapshots / ("system_link_client_machines_" + case + ".json")),
        "--no-concolic", "--no-leaf-cache", "--no-stub-arg-trace", "-q",
    ]
    try:
        unicorn_diff.main()
    except SystemExit as exc:
        if exc.code:
            sys.exit(exc.code)

    if calls[0] != calls[1]:
        print("Client-machine calls differ", case, calls, observed)
        sys.exit(1)
    names = tuple(record[0] for record in calls[0] if record[0] != "event")
    if names != EXPECTED[case] or len(states) != 2 or states[0] != states[1]:
        print("Client-machine path or state differs", case, names, len(states))
        sys.exit(1)
    pointer = 0x7001a4 if case in ("inactive_index2", "two_slots") else 0x70011c
    if any(record[2] != pointer for record in calls[0] if record[0] == "remove"):
        print("Client-machine game slot differs", case)
        sys.exit(1)
    if any(record[2] != 0x800 for record in calls[0] if record[0] == "read"):
        print("Client-machine receive buffer size differs", case)
        sys.exit(1)
    if any(record[3] != 0x24 or record[4] != bytes((i * 7 + 3) & 0xff
                                                 for i in range(0x24))
           for record in calls[0] if record[0] == "handle"):
        print("Client-machine decoded packet differs", case)
        sys.exit(1)
    print("Client-machine calls and state match", case)
else:
    for case in CASES:
        result = subprocess.run([sys.executable, __file__, "--single", case], check=False)
        if result.returncode:
            sys.exit(result.returncode)
    print("Client-machine focused branches match")
