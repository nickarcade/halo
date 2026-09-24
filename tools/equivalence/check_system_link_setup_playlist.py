"""Compare 2276 playlist setup side effects with the lifted server body."""

import json
import subprocess
import sys
from pathlib import Path


cases = ("team", "no_team", "failure")
snapshots = Path(__file__).with_name("regression_snapshots")

if len(sys.argv) == 3 and sys.argv[1] == "--single":
    case = sys.argv[2]
    sys.path.insert(0, "tools/equivalence")
    from unicorn import Uc, UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_ESP
    import unicorn_diff

    states = []
    name_sources = []
    old_hook_add = Uc.hook_add

    def capture_return(uc, address, size, user_data):
        in_oracle = 0x12dc20 <= address < 0x12dcf0
        in_candidate = 0x9000000 <= address < 0x9001000
        if not (in_oracle or in_candidate):
            return
        try:
            op = bytes(uc.mem_read(address, 1))[0]
            if op in (0xc3, 0xc2):
                states.append(bytes(uc.mem_read(0x700000, 0x4c0)))
            if op == 0xe8:
                esp = uc.reg_read(UC_X86_REG_ESP)
                args = tuple(int.from_bytes(uc.mem_read(esp + 4 * i, 4), "little")
                             for i in range(3))
                if args[0] == 0x700008 and args[2] == 0xf:
                    name_sources.append(bytes(uc.mem_read(args[1], 0x40)))
        except Exception:
            pass

    def add_capture_hook(self, hook_type, callback, *args, **kwargs):
        handle = old_hook_add(self, hook_type, callback, *args, **kwargs)
        if hook_type & UC_HOOK_CODE:
            old_hook_add(self, UC_HOOK_CODE, capture_return)
        return handle

    Uc.hook_add = add_capture_hook
    snapshot = snapshots / ("system_link_setup_playlist_" + case + ".json")
    sys.argv = [
        "unicorn_diff.py", "network_game_server_setup_game_from_playlist",
        "--allow-stubs", "--real-callees", "--seeds", "1",
        "--state-snapshot", str(snapshot), "--no-concolic", "--no-leaf-cache",
        "--no-stub-arg-trace", "-q",
    ]
    try:
        unicorn_diff.main()
    except SystemExit as exc:
        if exc.code:
            sys.exit(exc.code)
    if len(states) != 2 or states[0] != states[1]:
        differences = ([(hex(index), a, b) for index, (a, b) in
                        enumerate(zip(states[0], states[1])) if a != b][:20]
                       if len(states) == 2 else [])
        print("Playlist setup server states differ", case, len(states), differences)
        sys.exit(1)
    server = states[0]
    if case == "failure":
        before = bytes.fromhex(json.loads(snapshot.read_text())["regions"]["0x700000"])
        if server != before[:0x4c0]:
            print("Playlist failure changed server state")
            sys.exit(1)
    else:
        name = ("LINKCHECK" + "\0").encode("utf-16le")
        expected_teams = 2 if case == "team" else 1
        if (len(name_sources) != 2 or name_sources[0] != name_sources[1]
                or name_sources[0][:len(name)] != name):
            print("Playlist generated name differs", case, len(name_sources))
            sys.exit(1)
        if (server[8:8 + len(name)] != name
                or server[0x26:0x28] != b"\x00\x00"
                or server[0x28:0x2c] != b"\x00" * 4
                or server[0x115:0x118] != bytes((2, 0x10, expected_teams))
                or server[6] & 1 != 1):
            print("Playlist setup expected game fields differ", case,
                  server[8:8 + len(name)].hex(), server[0x26:0x2c].hex(),
                  server[0x115:0x118].hex(), server[6:8].hex())
            sys.exit(1)
    print("Playlist setup server fields match", case)
else:
    for case in cases:
        result = subprocess.run([sys.executable, __file__, "--single", case], check=False)
        if result.returncode:
            sys.exit(result.returncode)
    print("Playlist setup focused branches match")
