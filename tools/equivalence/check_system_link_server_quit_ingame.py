"""Compare the 2276 and lifted in-game player quit message payload."""

import json
import sys
from pathlib import Path

from unicorn import Uc, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP

sys.path.insert(0, "tools/equivalence")
import unicorn_diff


snapshot = (Path(__file__).with_name("regression_snapshots") /
            "system_link_server_quit_ingame.json")
server = bytes.fromhex(json.loads(snapshot.read_text())["regions"]["0x700000"])
expected = server[0x22e:0x24e] + (100 + 0x21).to_bytes(4, "little")
payloads = []
old_hook_add = Uc.hook_add


def capture_message(uc, address, size, user_data):
    if not (0x12c1c0 <= address < 0x12c284 or
            0x9000000 <= address < 0x9001000):
        return
    if bytes(uc.mem_read(address, 1))[0] != 0xe8:
        return
    esp = uc.reg_read(UC_X86_REG_ESP)
    args = [int.from_bytes(uc.mem_read(esp + 4 * index, 4), "little")
            for index in range(3)]
    if args[0] == 0x16 and args[2] == 0x24:
        payloads.append(bytes(uc.mem_read(args[1], 0x24)))


def add_capture_hook(self, hook_type, callback, *args, **kwargs):
    handle = old_hook_add(self, hook_type, callback, *args, **kwargs)
    if hook_type & UC_HOOK_CODE:
        old_hook_add(self, UC_HOOK_CODE, capture_message)
    return handle


Uc.hook_add = add_capture_hook
sys.argv = [
    "unicorn_diff.py", "network_game_server_remove_players_from_machine_ingame",
    "--allow-stubs", "--real-callees", "--seeds", "1", "--state-snapshot",
    str(snapshot), "--no-concolic", "--no-leaf-cache", "--no-stub-arg-trace", "-q",
]
try:
    unicorn_diff.main()
except SystemExit as exc:
    if exc.code:
        sys.exit(exc.code)

if payloads != [expected, expected]:
    print("In-game quit message payload differs", [item.hex() for item in payloads],
          "expected", expected.hex())
    sys.exit(1)
print("In-game quit message payload matches 2276")
