"""Compare 2276 client search packet paths with the active C lift.

The server argument is @<eax>; the synthetic snapshot must override `eax`,
which the equivalence ABI parser names `p0`. Packet creation, network delivery,
and the downstream join helper are modeled at their call sites.
"""

import contextlib
import io
import json
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, "tools/equivalence")
import abi
abi.POINTER_SLOT = 0x1000
from unicorn import Uc, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP
import unicorn_diff

CASES = (
    "quiet", "splitscreen_high_byte", "network_down", "idle_failed", "broadcast",
    "broadcast_create_failed", "broadcast_write_failed",
    "ping", "ping_create_failed", "ping_write_failed",
    "local_join", "local_join_failed",
)
TARGETS = {0x12b700: "create_network_game_message",
           0x128e00: "network_connection_write",
           0x124aa0: "network_game_client_initiate_join_game"}
NONCE = bytes.fromhex("1122334455667788")
TOKEN = b"message in a bot"
PING_ADDRESS = bytes.fromhex("c0a8011100000000000000000000000004001e14")
old_run = unicorn_diff._run_function
old_hook_add = Uc.hook_add
states = []
calls = [[], []]
seen = [[], []]
side = 0
manager = None


def capture_run(*args, **kwargs):
    global side, manager
    side = len(states)
    manager = kwargs.get("stub_manager")
    result = old_run(*args, **kwargs)
    states.append(result)
    return result


def capture_call(uc, address, size, user_data):
    if not (0x1268a0 <= address < 0x126b60 or
            0x9000000 <= address < 0x9001000):
        return
    if bytes(uc.mem_read(address, 1))[0] != 0xe8:
        return
    displacement = int.from_bytes(uc.mem_read(address + 1, 4), "little",
                                  signed=True)
    target = address + 5 + displacement
    name = (TARGETS.get(target) if side == 0 else manager._resolve_name(target))
    # The synthetic COFF loader leaves this same-object helper call at E8 0.
    # It follows token generation on the local-host path in both binaries.
    if (side == 1 and case.startswith("local_join") and name == "" and
            target == address + 5 and
            any(item[2] == "network_game_generate_join_game_token"
                for item in seen[side])):
        name = "network_game_client_initiate_join_game"
    if case.startswith("local_join"):
        seen[side].append((hex(address), hex(target), name))
    if name not in TARGETS.values():
        return
    esp = uc.reg_read(UC_X86_REG_ESP)
    args = [int.from_bytes(uc.mem_read(esp + i * 4, 4), "little")
            for i in range(5)]
    if name == "create_network_game_message":
        calls[side].append((name, args[0], bytes(uc.mem_read(args[1], args[2]))))
    elif name == "network_game_client_initiate_join_game":
        game = bytes(uc.mem_read(args[1], 0xe4))
        params = bytes(uc.mem_read(args[2], 0x22))
        dest = bytes(uc.mem_read(args[3], 0x14))
        calls[side].append((name, args[0], game, params[2:4],
                            params[0x12:0x22], dest[:4] + dest[16:20]))
        uc.reg_write(UC_X86_REG_EAX, int(case != "local_join_failed"))
        uc.reg_write(UC_X86_REG_EIP, address + 5)
    else:
        destination = bytes(uc.mem_read(args[3], 0x14))
        if destination[:4] == b"\xff" * 4:
            destination = destination[:4] + destination[16:20]
        calls[side].append((name, args[0], args[1], args[2] & 0xffff,
                            destination, args[4]))


def add_capture_hook(self, hook_type, callback, *args, **kwargs):
    handle = old_hook_add(self, hook_type, callback, *args, **kwargs)
    if hook_type & UC_HOOK_CODE:
        old_hook_add(self, UC_HOOK_CODE, capture_call)
    return handle


unicorn_diff._run_function = capture_run
Uc.hook_add = add_capture_hook

for case in CASES:
    states.clear()
    calls = [[], []]
    seen = [[], []]
    server = bytearray(0xd00)
    server[0x808:0x81c] = PING_ADDRESS
    server[0x820:0x824] = (0 if case.startswith("ping") else 10000).to_bytes(4, "little")
    server[0x82a] = int(case.startswith("ping"))
    server[0x82c:0x830] = (0x710000).to_bytes(4, "little")
    server[0xc94:0xc98] = (0 if case.startswith("broadcast") else 10000).to_bytes(4, "little")
    snapshot = {
        "description": "client search " + case, "build_label": "synthetic",
        "regions": {"0x46e8bc": "01000000" if case.startswith("local_join") else "00000000",
                    "0x710000": "00" * 0x40,
                    "0x720000": ("a000" if case.startswith("ping") else "e000") + "00" * 0x40},
        "arg_overrides": {"eax": server.hex()},
        "stub_returns": {
            "system_milliseconds": 10000,
            "network_game_is_splitscreen_local": 0x100 if case == "splitscreen_high_byte" else 0,
            "transport_network_available": int(case != "network_down"),
            "network_connection_idle": int(case != "idle_failed"),
            "network_game_client_process_incoming_messages": 1,
            "create_network_game_message": 0 if case.endswith("create_failed") else 0x720000,
            "network_connection_write": int(not case.endswith("write_failed")),
            "global_network_game_server_get": int(case.startswith("local_join")),
        },
        "stub_writes": {"transport_get_nonce": [[{"arg": 0, "data": NONCE.hex()}]],
                        "network_game_generate_join_game_token": [[{"arg": 0, "data": TOKEN.hex()}]]},
    }
    with tempfile.TemporaryDirectory() as temporary:
        snapshot_path = Path(temporary) / "client_search.json"
        snapshot_path.write_text(json.dumps(snapshot))
        sys.argv = ["unicorn_diff.py", "network_game_client_idle_searching",
                    "--allow-stubs", "--no-stub-arg-trace", "--seeds", "1",
                    "--state-snapshot", str(snapshot_path), "--no-concolic",
                    "--no-leaf-cache", "-q"]
        with contextlib.redirect_stdout(io.StringIO()):
            try:
                unicorn_diff.main()
            except SystemExit as exc:
                result = exc.code
            else:
                result = 0
    if result or len(states) != 2 or any(state.error for state in states):
        raise AssertionError((case, result, [state.error for state in states], seen,
                              [len(item) for item in calls]))
    expected_return = int(case not in ("network_down", "idle_failed", "local_join_failed",
                                       "broadcast_write_failed"))
    if ((states[0].eax & 0xff) != expected_return or
            (states[1].eax & 0xff) != expected_return or
            states[0].scratch_data != states[1].scratch_data):
        raise AssertionError((case, "return or client state differs"))
    if calls[0] != calls[1]:
        raise AssertionError((case, "packet call arguments differ", calls))
    expected_names = (("network_game_client_initiate_join_game",) if case.startswith("local_join")
                      else () if case in ("quiet", "splitscreen_high_byte", "network_down", "idle_failed")
                      else ("create_network_game_message",) if case.endswith("create_failed")
                      else ("create_network_game_message", "network_connection_write"))
    if tuple(call[0] for call in calls[0]) != expected_names:
        raise AssertionError((case, "wrong packet call path", calls[0]))
    if calls[0]:
        create = calls[0][0]
        if case.startswith("local_join"):
            expected_game = bytearray(0xe4)
            expected_game[0x24:0x2c] = NONCE
            if create != ("network_game_client_initiate_join_game", 0x10000000,
                          bytes(expected_game), b"\x00\x00", TOKEN,
                          bytes.fromhex("0100007f04001e14")):
                raise AssertionError((case, "local join arguments", create))
        elif case.startswith("broadcast"):
            expected_payload = bytes.fromhex("1f140100") + NONCE
            if create != ("create_network_game_message", 0, expected_payload):
                raise AssertionError((case, "broadcast payload", create))
        elif create[0:2] != ("create_network_game_message", 1) or create[2][:6] != bytes.fromhex("102700001f14"):
            raise AssertionError((case, "ping payload", create))
    if len(calls[0]) == 2:
        write = calls[0][1]
        expected_destination = (bytes.fromhex("ffffffff04001e14")
                                if case.startswith("broadcast") else PING_ADDRESS)
        expected_size = 14 if case.startswith("broadcast") else 10
        if write != ("network_connection_write", 0x710000, 0x720000,
                     expected_size, expected_destination, 0):
            raise AssertionError((case, "network write", write))
    after = states[0].scratch_data
    broadcast_stamp = int.from_bytes(after[0xc94:0xc98], "little")
    ping_stamp = int.from_bytes(after[0x820:0x824], "little")
    expected_broadcast_stamp = (10000 if case == "broadcast" else
                                0 if case.startswith("broadcast") else 10000)
    expected_ping_stamp = (10000 if case == "ping" else
                           0 if case.startswith("ping") else 10000)
    if (broadcast_stamp, ping_stamp) != (expected_broadcast_stamp, expected_ping_stamp):
        raise AssertionError((case, "timestamps", broadcast_stamp, ping_stamp))
    print(case, "matches")
