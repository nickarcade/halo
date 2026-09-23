"""Focused 2276 ingame add-player record and queue-helper check."""

import sys
import subprocess
from pathlib import Path

sys.path.insert(0, "tools/equivalence")
from unicorn.x86_const import UC_X86_REG_ESP
from stubs import StubManager
import unicorn_diff


PLAYER = bytes((i * 13 + 5) & 0xff for i in range(0x20))
calls = []
old_execute = StubManager.execute_stub


def capture_execute(self, uc, address):
    name = self._resolve_name(address).lower().lstrip("_")
    if name == "csmemcpy":
        esp = uc.reg_read(UC_X86_REG_ESP)
        args = tuple(int.from_bytes(uc.mem_read(esp + 4 + 4 * i, 4), "little")
                     for i in range(3))
        if args[2] == 0x20:
            calls.append((args[0], args[2], bytes(uc.mem_read(args[1], 0x20))))
    return old_execute(self, uc, address)


StubManager.execute_stub = capture_execute
snapshot = Path(__file__).with_name("regression_snapshots") / "system_link_add_player_ingame.json"
sys.argv = [
    "unicorn_diff.py", "network_game_server_handle_message_client_add_player_request_ingame",
    "--allow-stubs", "--real-callees", "--seeds", "1",
    "--state-snapshot", str(snapshot), "--no-concolic", "--no-leaf-cache",
    "--no-stub-arg-trace",
]
try:
    unicorn_diff.main()
except SystemExit as exc:
    if exc.code:
        sys.exit(exc.code)

expected = (0x700498, 0x20, PLAYER)
if calls != [expected]:
    print("Ingame add-player copy differs:", calls)
    sys.exit(1)
print("Ingame add-player copy matches: server+0x498, 0x20 decoded bytes")

queue_snapshot = snapshot.with_name("system_link_queue_player_copy.json")
queue_check = subprocess.run([
    sys.executable, "tools/equivalence/unicorn_diff.py",
    "network_game_server_queue_player_for_addition", "--allow-stubs",
    "--real-callees", "--seeds", "1", "--state-snapshot", str(queue_snapshot),
    "--no-concolic", "--no-leaf-cache", "--mem-trace",
], check=False)
if queue_check.returncode:
    sys.exit(queue_check.returncode)
print("Standalone queue-helper differential matches")
