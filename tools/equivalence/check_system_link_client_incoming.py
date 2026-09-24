"""Compare 2276 client receive-loop capacity, packets, calls, and result."""

import subprocess
import sys
from pathlib import Path


CASES = {
    "empty": ("read",),
    "handler_fail": ("read", "handle", "event"),
    "one_success": ("read", "handle", "read"),
    "two_messages": ("read", "handle", "read", "handle", "event"),
}
SNAPSHOTS = Path(__file__).with_name("regression_snapshots")

if len(sys.argv) == 3 and sys.argv[1] == "--single":
    case = sys.argv[2]
    sys.path.insert(0, str(Path(__file__).parent))
    from unicorn import Uc, UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_ESP
    import abi

    abi.POINTER_SLOT = 0x1000
    import unicorn_diff
    from stubs import StubManager

    oracle_targets = {0x1298f0: "read", 0x127ea0: "handle", 0x12b650: "event"}
    candidate_names = {
        "network_connection_read": "read",
        "network_game_client_handle_message": "handle",
        "network_event": "event",
    }
    calls = [[], []]
    stub_managers = []
    old_hook_add = Uc.hook_add
    old_prepare_stubs = StubManager.prepare_stubs

    def capture_prepare_stubs(self, *args, **kwargs):
        result = old_prepare_stubs(self, *args, **kwargs)
        stub_managers.append(self)
        return result

    def capture_code(uc, address, size, user_data):
        side = 0 if 0x1260c0 <= address < 0x126132 else (
            1 if 0x9000000 <= address < 0x9001000 else -1)
        if side < 0 or bytes(uc.mem_read(address, 1))[0] != 0xe8:
            return
        displacement = int.from_bytes(uc.mem_read(address + 1, 4),
                                      "little", signed=True)
        target = address + 5 + displacement
        if side == 0:
            name = oracle_targets.get(target)
        else:
            symbol = stub_managers[-1]._stub_names.get(target, "").lstrip("_")
            name = candidate_names.get(symbol)
        if name is None:
            return
        esp = uc.reg_read(UC_X86_REG_ESP)
        args = [int.from_bytes(uc.mem_read(esp + 4 * i, 4), "little")
                for i in range(4)]
        if name == "read":
            record = (name, args[0],
                      int.from_bytes(uc.mem_read(args[2], 4), "little"))
        elif name == "handle":
            record = (name, args[0], bytes(uc.mem_read(args[1], args[2])),
                      bytes(uc.mem_read(args[3], 24)))
        else:
            record = (name, bytes(uc.mem_read(args[0], 160)).split(b"\0")[0])
        calls[side].append(record)

    def add_capture_hook(self, hook_type, callback, *args, **kwargs):
        handle = old_hook_add(self, hook_type, callback, *args, **kwargs)
        if hook_type & UC_HOOK_CODE:
            old_hook_add(self, UC_HOOK_CODE, capture_code)
        return handle

    StubManager.prepare_stubs = capture_prepare_stubs
    Uc.hook_add = add_capture_hook
    sys.argv = [
        "unicorn_diff.py", "network_game_client_process_incoming_messages",
        "--allow-stubs", "--no-real-callees", "--seeds", "1",
        "--state-snapshot", str(SNAPSHOTS / f"system_link_client_incoming_{case}.json"),
        "--no-concolic", "--no-leaf-cache", "-q",
    ]
    try:
        unicorn_diff.main()
    except SystemExit as exc:
        if exc.code:
            sys.exit(exc.code)

    if calls[0] != calls[1] or tuple(call[0] for call in calls[0]) != CASES[case]:
        raise AssertionError((case, "helper sequence or arguments differ", calls))
    for call in calls[0]:
        if call[0] == "read" and call[1:] != (0x710000, 0x800):
            raise AssertionError((case, "receive capacity or connection differs", call))
        if call[0] == "handle" and call[1] != 0x10000000:
            raise AssertionError((case, "client argument differs", call))
    expected_packets = {
        "empty": (), "handler_fail": (bytes.fromhex("01020304"),),
        "one_success": (bytes.fromhex("01020304"),),
        "two_messages": (bytes.fromhex("01020304"),
                         bytes.fromhex("1020304050607080")),
    }
    packets = tuple(call[2] for call in calls[0] if call[0] == "handle")
    if packets != expected_packets[case]:
        raise AssertionError((case, "received packet differs", packets))
    print(case, "matches", len(calls[0]), "helper calls")
else:
    for case in CASES:
        result = subprocess.run([sys.executable, __file__, "--single", case],
                                check=False)
        if result.returncode:
            sys.exit(result.returncode)
