"""Compare 2276 and lifted pregame settings payload and broadcast call.

Run after building halo. The snapshot gives the server game a distinctive
0x434-byte image. Message creation and broadcasting are intercepted.
"""
import hashlib
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
    if name in ("create_network_game_message",
                "network_game_server_send_message_to_all_machines"):
        esp = uc.reg_read(UC_X86_REG_ESP)
        args = [int.from_bytes(uc.mem_read(esp + 4 + i * 4, 4), "little")
                for i in range(3)]
        if name == "create_network_game_message":
            payload = bytes(uc.mem_read(args[1], args[2]))
            captures.append((name, args[0], payload))
        else:
            captures.append((name, args[:2]))
    return old_execute(self, uc, address)


StubManager.execute_stub = capture_execute
snapshot = Path(__file__).with_name("regression_snapshots") / "system_link_game_data_payload.json"
sys.argv = [
    "unicorn_diff.py", "network_game_server_send_game_data_pregame",
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
if result != 0 or len(captures) != 4:
    print(f"Expected four creation/broadcast captures, got {len(captures)}")
    sys.exit(1)
create_oracle, send_oracle, create_candidate, send_candidate = captures
expected_game = bytes((i * 37 + 11) & 0xff for i in range(0x434))
if (create_oracle != create_candidate
        or create_oracle != ("create_network_game_message", 6, expected_game)
        or send_oracle != send_candidate
        or send_oracle[0] != "network_game_server_send_message_to_all_machines"
        or send_oracle[1][1] != 0x710000):
    print("Pregame game-data creation or broadcast arguments differ")
    sys.exit(1)
print("Pregame settings payload matches: 0x434 bytes, "
      f"sha256={hashlib.sha256(expected_game).hexdigest()}; broadcast arguments match")
