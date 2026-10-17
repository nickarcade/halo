"""Compare 2276 and lifted advertisement payload and send arguments.

Run after building halo. The synthetic snapshot pins helper effects so the
original out-of-line join-token helper and the candidate's inline copy have
the same input. The network write is intercepted, so delivery is untested.
"""
import hashlib
import sys
from pathlib import Path

sys.path.insert(0, "tools/equivalence")
from unicorn.x86_const import UC_X86_REG_ESP
from stubs import StubManager
import unicorn_diff

captured = []
writes = []
original_execute = StubManager.execute_stub


def capture_execute(self, uc, address):
    name = self._resolve_name(address).lower().lstrip("_")
    if name == "create_network_game_message":
        esp = uc.reg_read(UC_X86_REG_ESP)
        ptr = int.from_bytes(uc.mem_read(esp + 8, 4), "little")
        size = int.from_bytes(uc.mem_read(esp + 12, 4), "little")
        captured.append(bytes(uc.mem_read(ptr, size)))
    elif name == "network_connection_write":
        esp = uc.reg_read(UC_X86_REG_ESP)
        args = [int.from_bytes(uc.mem_read(esp + 4 + i * 4, 4), "little")
                for i in range(5)]
        destination = bytes(uc.mem_read(args[3], 0x14))
        writes.append((args, destination))
    return original_execute(self, uc, address)


StubManager.execute_stub = capture_execute
snapshot = Path(__file__).with_name("regression_snapshots") / "system_link_broadcast_valid_send.json"
sys.argv = [
    "unicorn_diff.py",
    "handle_message_client_broadcast_game_search",
    "--allow-stubs", "--real-callees", "--no-stub-arg-trace",
    "--mem-trace", "--seeds", "1", "--state-snapshot", str(snapshot),
    "--no-concolic", "--no-leaf-cache", "-q",
]
try:
    unicorn_diff.main()
except SystemExit as exc:
    result = exc.code
else:
    result = 0

if len(captured) != 2:
    print(f"Expected oracle and candidate payloads, captured {len(captured)}")
    sys.exit(1)
oracle, candidate = captured
differences = [(i, a, b) for i, (a, b) in enumerate(zip(oracle, candidate)) if a != b]
expected_token = b"message in a bottle"[:16]
if (result != 0 or len(oracle) != 0x114 or len(candidate) != 0x114
        or differences or oracle[0x102:0x104] != b"\x0e\x00"
        or oracle[0x104:0x114] != expected_token):
    print(f"Payload mismatch: lengths={[len(x) for x in captured]} "
          f"differences={differences[:12]} result={result}")
    sys.exit(1)
if len(writes) != 2:
    print(f"Expected oracle and candidate network writes, captured {len(writes)}")
    sys.exit(1)
for args, destination in writes:
    if (args[0] != 0x730000 or args[1] != 0x720000
            or (args[2] & 0xffff) != 0x10 or args[4] != 0
            or destination[:4] != b"\xff" * 4
            or destination[0x10:0x14] != bytes.fromhex("04001f14")):
        print(f"Advertisement send mismatch: args={args}, destination={destination.hex()}")
        sys.exit(1)
print(f"Advertisement payload matches: 0x114 bytes, "
      f"sha256={hashlib.sha256(oracle).hexdigest()}; send target matches")
