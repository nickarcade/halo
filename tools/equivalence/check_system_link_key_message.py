"""Check the 2276 key-agreement builder's successful packet path.

The packet encoder and message allocator are intercepted on both sides.
The fixture supplies a distinctive 24-byte encoded packet; this checks
the builder's call arguments, returned pointer, and key-agreement flag.
"""

import sys
from pathlib import Path

sys.path.insert(0, "tools/equivalence")
from unicorn.x86_const import UC_X86_REG_ESP
from stubs import StubManager
import unicorn_diff

calls = []
states = []
old_execute = StubManager.execute_stub
old_run = unicorn_diff._run_function


def capture_stub(self, uc, address):
    name = self._resolve_name(address).lower().lstrip("_")
    esp = uc.reg_read(UC_X86_REG_ESP)
    if name == "encode_packet_group":
        args = [int.from_bytes(uc.mem_read(esp + 4 + 4 * i, 4), "little")
                for i in range(6)]
        data = bytes(uc.mem_read(args[1], 24))
        capacity = int.from_bytes(uc.mem_read(args[3], 2), "little")
        calls.append((name, args[0], data, capacity, args[4], args[5]))
    elif name == "create_message":
        args = [int.from_bytes(uc.mem_read(esp + 4 + 4 * i, 4), "little")
                for i in range(5)]
        payload = bytes(uc.mem_read(args[1], args[2]))
        calls.append((name, args[0], payload, args[3], args[4]))
    return old_execute(self, uc, address)


def capture_run(*args, **kwargs):
    state = old_run(*args, **kwargs)
    states.append(state)
    return state


StubManager.execute_stub = capture_stub
unicorn_diff._run_function = capture_run
snapshot = Path(__file__).with_name("regression_snapshots") / "system_link_key_agreement_success.json"
sys.argv = [
    "unicorn_diff.py", "key_agreement_build_message",
    "--allow-stubs", "--real-callees", "--mem-trace", "--no-leaf-cache",
    "--state-snapshot", str(snapshot), "--seeds", "1", "--no-concolic",
    "--no-stub-arg-trace", "-q",
]
try:
    unicorn_diff.main()
except SystemExit as exc:
    if exc.code != 0:
        raise
packet = bytes(range(1, 25))
expected = [
    ("encode_packet_group", 0x2ee588, packet, 0x80, 0, 1),
    ("create_message", 3, packet, 0x700000, 0x100),
]
if calls != expected + expected:
    raise AssertionError(("call arguments", calls, expected + expected))
if len(states) != 2 or any(state.error or state.eax != 0x700000 for state in states):
    raise AssertionError(("return state", states))
for state in states:
    writes = [(write.address, write.size, write.value)
              for write in state.mem_writes if write.address == 0x700000]
    if writes != [(0x700000, 2, 2)]:
        raise AssertionError(("message header flag", writes))
print("Key-agreement message success matches: 24-byte payload, type 3, flag 2")
