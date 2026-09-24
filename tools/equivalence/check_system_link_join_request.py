"""Compare 2276 server join decisions, packet bytes, and network writes."""

import subprocess
import sys
from pathlib import Path


CASES = (
    "not_pregame", "already_joined", "decode_failed", "game_closed",
    "bad_token", "bad_token_create_failed", "bad_token_write_failed",
    "accept_failed", "accepted", "accepted_create_failed",
    "accepted_write_failed", "accepted_data_failed", "hosts_denied", "hosts_allowed",
)
TARGETS = {
    0x12b700: "create", 0x40018000: "create",
    0x128e00: "write", 0x4001c000: "write",
    0x12c560: "accept", 0x40044000: "accept",
    0x12f5d0: "game_data", 0x40054000: "game_data",
    0x12b650: "event", 0x40004000: "event",
    0x1d9e59: "open_hosts", 0x40030000: "open_hosts",
    0x1daeec: "read_hosts", 0x40034000: "read_hosts",
    0x1283c0: "address", 0x40010000: "address",
    0x81b90: "address_string", 0x40014000: "address_string",
}
REJECT_REASON = {
    "game_closed": 5, "bad_token": 2, "bad_token_create_failed": 2,
    "bad_token_write_failed": 2, "accept_failed": 5, "hosts_denied": 6,
}
ACCEPT_CASES = {
    "accepted", "accepted_create_failed", "accepted_write_failed",
    "accepted_data_failed", "hosts_allowed",
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
        side = 0 if 0x12f990 <= address < 0x12fd75 else (
            1 if 0x9000000 <= address < 0x9001000 else -1)
        if side < 0:
            return
        try:
            op = bytes(uc.mem_read(address, 1))[0]
            if op in (0xc3, 0xc2):
                states.append((side, uc.reg_read(UC_X86_REG_EAX) & 0xff,
                               bytes(uc.mem_read(0x700000, 0x500)),
                               bytes(uc.mem_read(0x10000000, 0x20))))
            if op != 0xe8:
                return
            displacement = int.from_bytes(uc.mem_read(address + 1, 4),
                                          "little", signed=True)
            name = TARGETS.get(address + 5 + displacement)
            if name is None:
                return
            esp = uc.reg_read(UC_X86_REG_ESP)
            args = tuple(int.from_bytes(uc.mem_read(esp + 4 * i, 4), "little")
                         for i in range(5))
            if name == "create":
                record = (name, args[0], args[2],
                          bytes(uc.mem_read(args[1], args[2])))
            elif name == "write":
                record = (name, args[0], args[1], args[2] & 0xffff, args[3], args[4])
            elif name == "accept":
                record = (name, args[0], args[1])
            elif name == "game_data":
                record = (name, args[0])
            elif name == "event":
                fmt = bytes(uc.mem_read(args[0], 200)).split(b"\0")[0]
                record = (name, fmt)
            elif name == "open_hosts":
                record = (name, bytes(uc.mem_read(args[0], 50)).split(b"\0")[0],
                          bytes(uc.mem_read(args[1], 10)).split(b"\0")[0])
            elif name == "read_hosts":
                record = (name, args[1])
            elif name == "address":
                record = (name, args[0], args[2])
            else:
                record = (name, bytes(uc.mem_read(args[0], 0x18)))
            calls[side].append(record)
        except Exception:
            pass

    def add_capture_hook(self, hook_type, callback, *args, **kwargs):
        handle = old_hook_add(self, hook_type, callback, *args, **kwargs)
        if hook_type & UC_HOOK_CODE:
            old_hook_add(self, UC_HOOK_CODE, capture_code)
        return handle

    Uc.hook_add = add_capture_hook
    sys.argv = [
        "unicorn_diff.py", "network_game_server_handle_message_client_join_game_request",
        "--allow-stubs", "--real-callees", "--seeds", "1",
        "--state-snapshot", str(snapshots / ("system_link_join_request_" + case + ".json")),
        "--no-concolic", "--no-leaf-cache", "--no-stub-arg-trace", "-q",
    ]
    try:
        unicorn_diff.main()
    except SystemExit as exc:
        if exc.code and case not in ("not_pregame", "already_joined"):
            sys.exit(exc.code)

    if case == "decode_failed":
        inner_error = ("event", b"decode_network_game_message() failed")
        if not calls[1] or calls[1][0] != inner_error:
            print("Join-request inlined decoder error differs", case, calls)
            sys.exit(1)
        calls[1].pop(0)
    if calls[0] != calls[1]:
        print("Join-request effects differ", case, calls)
        sys.exit(1)
    if len(states) != 2 or states[0][0] != 0 or states[1][0] != 1 or states[0][1:] != states[1][1:]:
        print("Join-request result or state differs", case, states)
        sys.exit(1)
    records = calls[0]
    created = [r for r in records if r[0] == "create"]
    written = [r for r in records if r[0] == "write"]
    accepts = [r for r in records if r[0] == "accept"]
    game_data = [r for r in records if r[0] == "game_data"]
    if case in REJECT_REASON:
        expected = ("create", 5, 2, REJECT_REASON[case].to_bytes(2, "little"))
    elif case in ACCEPT_CASES:
        expected = ("create", 4, 8, bytes.fromhex("7856341200000000"))
    else:
        expected = None
    if created != ([] if expected is None else [expected]):
        print("Join-request packet differs", case, created)
        sys.exit(1)
    write_expected = expected is not None and not case.endswith("create_failed")
    if written != ([("write", 0x720000, 0x730000, 0x10, 0, 1)] if write_expected else []):
        print("Join-request network write differs", case, written)
        sys.exit(1)
    if bool(accepts) != (case in ACCEPT_CASES or case == "accept_failed"):
        print("Join-request acceptance call differs", case, accepts)
        sys.exit(1)
    if accepts and accepts != [("accept", 0x700000, 0x10000000)]:
        print("Join-request acceptance arguments differ", case, accepts)
        sys.exit(1)
    if game_data != ([("game_data", 0x700000)] if case in
                     ("accepted", "accepted_data_failed", "hosts_allowed") else []):
        print("Join-request game data call differs", case, game_data)
        sys.exit(1)
    if states[0][1] != (1 if case in ("not_pregame", "already_joined",
                                        "accepted", "hosts_allowed") else 0):
        print("Join-request expected result differs", case)
        sys.exit(1)
    if case.startswith("hosts_") and ("open_hosts", b"d:\\hosts.txt", b"r") not in records:
        print("Join-request hosts file differs", case)
        sys.exit(1)
    print("Join-request packets, calls, and state match", case)
else:
    for case in CASES:
        result = subprocess.run([sys.executable, __file__, "--single", case], check=False)
        if result.returncode:
            sys.exit(result.returncode)
    print("Join-request focused branches match")
