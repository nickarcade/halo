"""Compare 2276 and lifted ping reply arguments at creation and send.

Run after building halo. The effective send size is a 16-bit parameter; the
unused high half of its stack slot can differ between compiler output.
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
    if name in ("create_network_game_message", "network_connection_write"):
        esp = uc.reg_read(UC_X86_REG_ESP)
        args = [int.from_bytes(uc.mem_read(esp + 4 + i * 4, 4), "little")
                for i in range(5)]
        if name == "create_network_game_message":
            data = bytes(uc.mem_read(args[1], args[2]))
            captures.append((name, args[0], data))
        else:
            address_data = bytes(uc.mem_read(args[3], 0x14))
            captures.append((name, args, address_data))
    return old_execute(self, uc, address)


StubManager.execute_stub = capture_execute
snapshot = Path(__file__).with_name("regression_snapshots") / "system_link_ping_valid_send.json"
sys.argv = [
    "unicorn_diff.py", "handle_message_client_ping",
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
    print(f"Expected four matching creation/send captures, got {len(captures)}")
    sys.exit(1)
create_oracle, write_oracle, create_candidate, write_candidate = captures
expected_address = bytes.fromhex("c0a8011100000000000000000000000004001f14")
for create, write in ((create_oracle, write_oracle),
                      (create_candidate, write_candidate)):
    create_name, message_type, payload = create
    write_name, args, destination = write
    if (create_name != "create_network_game_message" or message_type != 3
            or payload != bytes.fromhex("78563412")
            or write_name != "network_connection_write"
            or args[0] != 0x730000 or args[1] != 0x720000
            or (args[2] & 0xffff) != 1 or args[4] != 0
            or destination != expected_address):
        print(f"Ping reply mismatch: create={create}, write={write}")
        sys.exit(1)
print("Ping reply matches: timestamp, message type, connection, "
      "16-bit send size, IPv4 destination and port")
