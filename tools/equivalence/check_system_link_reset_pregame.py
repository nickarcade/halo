"""Focused 2276 reset-to-pregame differential across server-state branches."""

import subprocess
import sys
from pathlib import Path


SNAPSHOTS = (
    "reset_pregame_create_fail",
    "reset_pregame_send_fail",
    "reset_pregame_success",
    "reset_pregame_teams",
    "reset_pregame_teams_invalid",
    "reset_pregame_settings_create_fail",
    "reset_pregame_settings_send_fail",
    "reset_pregame_playlist_end",
    "reset_pregame_playlist_handle_fail",
    "reset_pregame_initial",
)
MESSAGE_COUNTS = {
    "reset_pregame_create_fail": 1,
    "reset_pregame_send_fail": 1,
    "reset_pregame_success": 2,
    "reset_pregame_teams": 2,
    "reset_pregame_teams_invalid": 2,
    "reset_pregame_settings_create_fail": 2,
    "reset_pregame_settings_send_fail": 2,
    "reset_pregame_playlist_end": 2,
    "reset_pregame_playlist_handle_fail": 2,
    "reset_pregame_initial": 0,
}
MESSAGE_TYPES = {
    "reset_pregame_create_fail": (0x1e,),
    "reset_pregame_send_fail": (0x1e,),
    "reset_pregame_success": (0x1e, 6),
    "reset_pregame_teams": (0x1e, 6),
    "reset_pregame_teams_invalid": (0x1e, 6),
    "reset_pregame_settings_create_fail": (0x1e, 6),
    "reset_pregame_settings_send_fail": (0x1e, 6),
    "reset_pregame_playlist_end": (0x1e, 9),
    "reset_pregame_playlist_handle_fail": (0x1e, 9),
    "reset_pregame_initial": (),
}
snapshot_dir = Path(__file__).with_name("regression_snapshots")

if len(sys.argv) == 3 and sys.argv[1] == "--single":
    case = sys.argv[2]
    sys.path.insert(0, "tools/equivalence")
    import abi
    from unicorn import Uc, UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_ESP
    from stubs import StubManager

    # The server reaches +0x4b9; the default 0x400-byte pointer slot is short.
    abi.POINTER_SLOT = 0x1000
    copies = []
    messages = []
    server_states = []
    old_execute = StubManager.execute_stub
    old_hook_add = Uc.hook_add

    def capture_code(uc, address, size, user_data):
        in_oracle = 0x12eca0 <= address < 0x12eee8
        in_candidate = 0x9000000 <= address < 0x9001000
        if not (in_oracle or in_candidate):
            return
        try:
            op = bytes(uc.mem_read(address, 1))[0]
            if op in (0xc3, 0xc2):
                server_states.append(bytes(uc.mem_read(0x10000000, 0x4c0)))
            if op != 0xe8:
                return
            esp = uc.reg_read(UC_X86_REG_ESP)
            args = tuple(int.from_bytes(uc.mem_read(esp + 4 * i, 4), "little")
                         for i in range(3))
            if (args[0], args[2]) not in ((0x1e, 4), (6, 0x434), (9, 4)):
                return
            messages.append((args[0], bytes(uc.mem_read(args[1], args[2]))))
        except Exception:
            pass

    def add_capture_hook(self, hook_type, callback, *args, **kwargs):
        handle = old_hook_add(self, hook_type, callback, *args, **kwargs)
        if hook_type & UC_HOOK_CODE:
            old_hook_add(self, UC_HOOK_CODE, capture_code)
        return handle

    Uc.hook_add = add_capture_hook

    def capture_copy(self, uc, address):
        name = self._resolve_name(address).lower().lstrip("_")
        if name == "csmemcpy":
            esp = uc.reg_read(UC_X86_REG_ESP)
            args = tuple(int.from_bytes(uc.mem_read(esp + 4 + 4 * i, 4), "little")
                         for i in range(3))
            if args[2] == 0x434:
                source = bytes(uc.mem_read(args[1], 0x434))
                result = old_execute(self, uc, address)
                copies.append((source, bytes(uc.mem_read(args[0], 0x434))))
                return result
        return old_execute(self, uc, address)

    StubManager.execute_stub = capture_copy
    import unicorn_diff

    sys.argv = [
        "unicorn_diff.py", "network_game_server_reset_to_pregame",
        "--allow-stubs", "--real-callees", "--seeds", "1",
        "--state-snapshot", str(snapshot_dir / ("system_link_" + case + ".json")),
        "--no-concolic", "--no-leaf-cache", "--no-stub-arg-trace", "-q",
    ]
    try:
        unicorn_diff.main()
    except SystemExit as exc:
        if exc.code:
            sys.exit(exc.code)
    count = MESSAGE_COUNTS[case]
    if len(messages) != 2 * count or messages[:count] != messages[count:]:
        print("Reset message payloads differ", case, [(kind, len(data)) for kind, data in messages])
        sys.exit(1)
    if len(server_states) != 2 or server_states[0] != server_states[1]:
        print("Reset final server states differ", case, len(server_states))
        sys.exit(1)
    final_server = server_states[0]
    expected_state = 0 if case in ("reset_pregame_success", "reset_pregame_teams",
                                   "reset_pregame_teams_invalid",
                                   "reset_pregame_initial") else 2
    if (int.from_bytes(final_server[4:6], "little") != expected_state
            or int.from_bytes(final_server[0x434:0x438], "little") != 0x10203041
            or final_server[0x47c:0x480] != b"\x00" * 4
            or final_server[0x484:0x498] != b"\x00" * 0x14
            or final_server[0x4b8:0x4ba] != b"\x00\x00"):
        print("Reset expected server fields differ", case)
        sys.exit(1)
    expected_machine_flag = 0xfb if case in (
        "reset_pregame_success", "reset_pregame_teams", "reset_pregame_teams_invalid",
        "reset_pregame_settings_create_fail", "reset_pregame_settings_send_fail",
        "reset_pregame_playlist_end", "reset_pregame_playlist_handle_fail") else 0xff
    if any(final_server[0x44a + 0x10 * index] != expected_machine_flag
           for index in range(4)):
        print("Reset machine flags differ", case)
        sys.exit(1)
    if case in ("reset_pregame_teams", "reset_pregame_initial"):
        if final_server[0x24c] != 1 or final_server[0x26c] != 0:
            print("Reset team rotation differs", case)
            sys.exit(1)
    if case == "reset_pregame_teams_invalid":
        if final_server[0x24c] != 0 or final_server[0x26c] != 1:
            print("Reset invalid players changed teams")
            sys.exit(1)
    payloads = messages[:count]
    if tuple(kind for kind, data in payloads) != MESSAGE_TYPES[case]:
        print("Reset message kinds differ", case)
        sys.exit(1)
    for kind, data in payloads:
        expected = final_server[8:8 + 0x434] if kind == 6 else b"\x00" * 4
        if data != expected:
            print("Reset message body differs", case, kind)
            sys.exit(1)
    print("Message payloads and final server state match", case)
    if case in ("reset_pregame_success", "reset_pregame_teams",
                "reset_pregame_teams_invalid",
                "reset_pregame_settings_create_fail",
                "reset_pregame_settings_send_fail"):
        if len(copies) != 2 or copies[0][0] != copies[1][0] or any(
                source != destination for source, destination in copies):
            print("Reset game-settings payload copy differs", case)
            sys.exit(1)
        print("Game-settings payload copy matches on both sides", case)
else:
    for name in SNAPSHOTS:
        result = subprocess.run([sys.executable, __file__, "--single", name], check=False)
        if result.returncode:
            sys.exit(result.returncode)
    print("Reset-to-pregame focused state branches match")
