"""Compare 2276 server join decisions, helper arguments, and state writes."""

import json
import subprocess
import sys
from pathlib import Path


CASES = ("closed", "full", "invalid_address", "remote_rejected",
         "loopback_accepted", "remote_accepted", "accept_failed", "slot_two")
TARGETS = {
    0x12c100: "game_open",
    0x1283c0: "get_address", 0x40004000: "get_address",
    0x12a160: "remote_allowed", 0x40008000: "remote_allowed",
    0x12acb0: "invalidate", 0x40010000: "invalidate",
    0x1285c0: "accept", 0x40014000: "accept",
    0x81b90: "address_string", 0x4000c000: "address_string",
    0x12b650: "event", 0x40000000: "event",
}
SYMBOL_TARGETS = {
    "network_connection_get_address": "get_address",
    "network_game_should_accept_remote_connections": "remote_allowed",
    "network_game_invalidate_machine": "invalidate",
    "network_connection_server_accept_client_connection": "accept",
    "transport_address_to_string": "address_string",
    "network_event": "event",
}
EXPECTED = {
    "closed": ("event",),
    "full": ("event",),
    "invalid_address": ("get_address", "event"),
    "remote_rejected": ("get_address", "remote_allowed", "address_string", "event"),
    "loopback_accepted": ("get_address", "remote_allowed", "invalidate",
                          "accept", "address_string", "event"),
    "remote_accepted": ("get_address", "remote_allowed", "invalidate",
                        "accept", "address_string", "event"),
    "accept_failed": ("get_address", "remote_allowed", "invalidate", "accept"),
    "slot_two": ("get_address", "remote_allowed", "invalidate",
                 "accept", "address_string", "event"),
}
snapshots = Path(__file__).with_name("regression_snapshots")

if len(sys.argv) == 3 and sys.argv[1] == "--single":
    case = sys.argv[2]
    sys.path.insert(0, "tools/equivalence")
    from unicorn import Uc, UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX
    import unicorn_diff
    from stubs import StubManager

    calls = [[], []]
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
        side = 0 if 0x12d880 <= address < 0x12da00 else (
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
            target = address + 5 + displacement
            if side == 1 and stub_managers:
                symbol = stub_managers[-1]._stub_names.get(target, "").lstrip("_")
                name = SYMBOL_TARGETS.get(symbol)
            else:
                name = TARGETS.get(target)
            if name is None or name == "game_open":
                return
            esp = uc.reg_read(UC_X86_REG_ESP)
            args = tuple(int.from_bytes(uc.mem_read(esp + 4 * i, 4), "little")
                         for i in range(2))
            if name == "get_address":
                record = (name, args[0])
            elif name in ("remote_allowed",):
                record = (name,)
            elif name in ("invalidate", "accept"):
                record = (name, args[0], args[1])
            elif name == "address_string":
                record = (name, bytes(uc.mem_read(args[0], 0x18)))
            else:
                fmt = bytes(uc.mem_read(args[0], 200)).split(b"\0")[0]
                record = (name, fmt, args[1] if b"%s" in fmt else None)
            calls[side].append(record)
        except Exception:
            pass

    def add_capture_hook(self, hook_type, callback, *args, **kwargs):
        handle = old_hook_add(self, hook_type, callback, *args, **kwargs)
        if hook_type & UC_HOOK_CODE:
            old_hook_add(self, UC_HOOK_CODE, capture_code)
        return handle

    Uc.hook_add = add_capture_hook
    snapshot = snapshots / ("system_link_add_new_client_" + case + ".json")
    sys.argv = [
        "unicorn_diff.py", "network_game_server_add_new_client",
        "--allow-stubs", "--real-callees", "--seeds", "1",
        "--state-snapshot", str(snapshot), "--no-concolic", "--no-leaf-cache",
        "--no-stub-arg-trace", "-q",
    ]
    try:
        unicorn_diff.main()
    except SystemExit as exc:
        if exc.code:
            sys.exit(exc.code)

    names = tuple(record[0] for record in calls[0])
    if calls[0] != calls[1] or names != EXPECTED[case]:
        print("Add-new-client helper calls differ", case, calls)
        sys.exit(1)
    if len(states) != 2 or states[0][0] != 0 or states[1][0] != 1 or states[0][1:] != states[1][1:]:
        print("Add-new-client returns or server states differ", case, len(states))
        sys.exit(1)
    result, server = states[0][1:]
    before = bytes.fromhex(json.loads(snapshot.read_text())["regions"]["0x700000"])
    accepted = case in ("loopback_accepted", "remote_accepted", "accept_failed", "slot_two")
    index = 2 if case == "slot_two" else 0
    if bool(result) != (accepted and case != "accept_failed"):
        print("Add-new-client return differs from expected", case, result)
        sys.exit(1)
    if accepted:
        slot = 0x43c + 0x10 * index
        expected = bytearray(before)
        expected[slot:slot + 4] = (0x720000).to_bytes(4, "little")
        expected[slot + 0xc:slot + 0xe] = index.to_bytes(2, "little")
        expected[slot + 0xe:slot + 0x10] = (1).to_bytes(2, "little")
        if server != expected:
            print("Add-new-client machine slot differs", case)
            sys.exit(1)
        if ("invalidate", 0x700008, index) not in calls[0] or (
                "accept", 0x710000, 0x720000) not in calls[0]:
            print("Add-new-client game or connection arguments differ", case)
            sys.exit(1)
    elif server != before:
        print("Add-new-client rejection changed server state", case)
        sys.exit(1)
    print("Add-new-client calls and state match", case)
else:
    for case in CASES:
        result = subprocess.run([sys.executable, __file__, "--single", case], check=False)
        if result.returncode:
            sys.exit(result.returncode)
    print("Add-new-client focused branches match")
