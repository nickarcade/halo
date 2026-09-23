"""Check 2276 postgame removal dispatch and its active helper."""

import subprocess
import sys
from pathlib import Path


snapshots = Path(__file__).with_name("regression_snapshots")
handler = "network_game_server_handle_message_client_remove_player_request_postgame"
helper = "network_game_server_remove_player_from_game"
player = bytes((i * 13 + 5) & 0xff for i in range(0x20))

if len(sys.argv) == 2 and sys.argv[1] == "--handler":
    sys.path.insert(0, "tools/equivalence")
    from unicorn import Uc, UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_ESP
    import unicorn_diff

    captures = []
    old_hook_add = Uc.hook_add

    def capture_call(uc, address, size, user_data):
        try:
            if bytes(uc.mem_read(address, 1))[0] != 0xe8:
                return
            esp = uc.reg_read(UC_X86_REG_ESP)
            args = tuple(int.from_bytes(uc.mem_read(esp + 4 * i, 4), "little")
                         for i in range(3))
            if args[:2] != (0x700000, 0x710000):
                return
            record = bytes(uc.mem_read(args[2], 0x20))
            captures.append((args[0], args[1], record))
        except Exception:
            pass

    def add_capture_hook(self, hook_type, callback, *args, **kwargs):
        handle = old_hook_add(self, hook_type, callback, *args, **kwargs)
        if hook_type & UC_HOOK_CODE:
            old_hook_add(self, UC_HOOK_CODE, capture_call)
        return handle

    Uc.hook_add = add_capture_hook
    sys.argv = [
        "unicorn_diff.py", handler, "--allow-stubs", "--real-callees",
        "--seeds", "1", "--state-snapshot",
        str(snapshots / "system_link_remove_player_postgame.json"),
        "--no-concolic", "--no-leaf-cache", "--no-stub-arg-trace", "-q",
    ]
    try:
        unicorn_diff.main()
    except SystemExit as exc:
        if exc.code:
            sys.exit(exc.code)
    expected = (0x700000, 0x710000, player)
    if captures != [expected, expected]:
        print("Postgame removal calls differ:", captures)
        sys.exit(1)
    print("Postgame removal calls match with the decoded 0x20-byte player")
else:
    outer = subprocess.run([sys.executable, __file__, "--handler"], check=False)
    if outer.returncode:
        sys.exit(outer.returncode)
    for filename in ("system_link_remove_player_postgame_decode_fail.json",
                     "system_link_remove_player_postgame_wrong_state.json"):
        branch = subprocess.run([
            sys.executable, "tools/equivalence/unicorn_diff.py", handler,
            "--allow-stubs", "--real-callees", "--seeds", "1",
            "--state-snapshot", str(snapshots / filename), "--no-concolic",
            "--no-leaf-cache", "--no-stub-arg-trace", "-q",
        ], check=False)
        if branch.returncode:
            sys.exit(branch.returncode)
    inner = subprocess.run([
        sys.executable, "tools/equivalence/unicorn_diff.py", helper,
        "--allow-stubs", "--real-callees", "--seeds", "1",
        "--state-snapshot", str(snapshots / "system_link_remove_player_helper.json"),
        "--no-concolic", "--no-leaf-cache", "--no-stub-arg-trace", "-q",
    ], check=False)
    if inner.returncode:
        sys.exit(inner.returncode)
    print("Postgame removal helper differential matches")
