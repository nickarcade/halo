"""Compare 2276 and lifted client-loaded handler's transition call.

The loading-complete helper is intercepted; this checks its two arguments,
while its state effects require separate or runtime validation.
"""
import sys
from pathlib import Path

sys.path.insert(0, "tools/equivalence")
from unicorn.x86_const import UC_X86_REG_ESP
from stubs import StubManager
import unicorn_diff

captures = []
old_execute = StubManager.execute_stub


def capture_execute(self, uc, address):
    name = self._resolve_name(address).lower().lstrip("_")
    if name == "network_game_server_client_machine_game_loading_complete":
        esp = uc.reg_read(UC_X86_REG_ESP)
        captures.append(tuple(int.from_bytes(uc.mem_read(esp + 4 + 4 * i, 4), "little")
                              for i in range(2)))
    return old_execute(self, uc, address)


StubManager.execute_stub = capture_execute
snapshot = Path(__file__).with_name("regression_snapshots") / "system_link_client_loaded.json"
sys.argv = [
    "unicorn_diff.py", "network_game_server_handle_message_client_loaded",
    "--allow-stubs", "--real-callees", "--no-stub-arg-trace",
    "--seeds", "1", "--state-snapshot", str(snapshot),
    "--no-concolic", "--no-leaf-cache", "-q",
]
try:
    unicorn_diff.main()
except SystemExit as exc:
    result = exc.code
else:
    result = 0
if result != 0 or captures != [(0x700000, 0x710000), (0x700000, 0x710000)]:
    print(f"Loading-complete calls differ: {captures}")
    sys.exit(1)
print("Client-loaded transition call matches: server=0x700000, machine=0x710000")
