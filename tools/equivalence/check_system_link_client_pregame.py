"""Compare 2276 client pregame paths with the C lift and their event text."""

import contextlib
import io
import json
import os
import sys
import tempfile
from pathlib import Path

os.environ.setdefault("BIPED_SIBLING_RESOLVE", "1")
sys.path.insert(0, "tools/equivalence")
import abi
abi.POINTER_SLOT = 0x1000
from unicorn import Uc, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP
import unicorn_diff

CASES = {
    "success": {},
    "local_bypass": {"network_game_is_splitscreen_local": 1,
                     "transport_network_available": 0},
    "network_down": {"transport_network_available": 0},
    "inactive": {"network_connection_active": 0},
    "not_connected": {"network_connection_connected": 0},
    "idle_failed": {"network_connection_idle": 0},
    "messages_failed": {"network_game_client_process_incoming_messages": 0,
                        "network_connection_read": 1,
                        "network_game_client_handle_message": 0},
}
ORIGINAL_TARGETS = {
    0x8f390: "error",
    0xe44d0: "display_error_when_main_menu_loaded",
    0x128660: "network_connection_active",
    0x128360: "network_connection_connected",
    0x129cf0: "network_connection_idle",
    0x12b650: "network_event",
}
CONNECTION = 0x710000
old_run = unicorn_diff._run_function
old_hook_add = Uc.hook_add
states = []
calls = [[], []]
side = 0
manager = None
covered = set()


def capture_run(*args, **kwargs):
    global side, manager
    side = len(states)
    manager = kwargs.get("stub_manager")
    result = old_run(*args, **kwargs)
    states.append(result)
    return result


def read_string(uc, address):
    return bytes(uc.mem_read(address, 160)).split(b"\x00", 1)[0].decode("latin1")


def capture_call(uc, address, size, user_data):
    if not (0x126ce0 <= address < 0x126db0 or
            0x9000000 <= address < 0x9001000):
        return
    if bytes(uc.mem_read(address, 1))[0] != 0xe8:
        return
    displacement = int.from_bytes(uc.mem_read(address + 1, 4), "little",
                                  signed=True)
    target = address + 5 + displacement
    name = (ORIGINAL_TARGETS.get(target) if side == 0 else
            manager._resolve_name(target))
    if name not in ORIGINAL_TARGETS.values():
        return
    esp = uc.reg_read(UC_X86_REG_ESP)
    arguments = [int.from_bytes(uc.mem_read(esp + i * 4, 4), "little")
                 for i in range(3)]
    if name == "error":
        calls[side].append((name, arguments[0], read_string(uc, arguments[1])))
    elif name == "network_event":
        calls[side].append((name, read_string(uc, arguments[0])))
    elif name == "network_connection_idle":
        calls[side].append((name, *arguments))
    else:
        calls[side].append((name, arguments[0]))


def add_capture_hook(self, hook_type, callback, *args, **kwargs):
    handle = old_hook_add(self, hook_type, callback, *args, **kwargs)
    if hook_type & UC_HOOK_CODE:
        old_hook_add(self, UC_HOOK_CODE, capture_call)
    return handle


unicorn_diff._run_function = capture_run
Uc.hook_add = add_capture_hook

for case, overrides in CASES.items():
    states.clear()
    calls = [[], []]
    server = bytearray(0xd00)
    server[0x82c:0x830] = CONNECTION.to_bytes(4, "little")
    returns = {
        "network_game_is_splitscreen_local": 0,
        "transport_network_available": 1,
        "network_connection_active": 1,
        "network_connection_connected": 1,
        "network_game_client_update_precache_status": 1,
        "network_connection_idle": 1,
        "network_game_client_process_incoming_messages": 1,
    }
    returns.update(overrides)
    snapshot = {
        "description": "client pregame " + case,
        "build_label": "synthetic",
        "regions": {"0x710000": "00" * 0x40},
        "arg_overrides": {"eax": server.hex()},
        "stub_returns": returns,
    }
    with tempfile.TemporaryDirectory() as temporary:
        snapshot_path = Path(temporary) / "client_pregame.json"
        report_path = Path(temporary) / "result.json"
        snapshot_path.write_text(json.dumps(snapshot))
        sys.argv = ["unicorn_diff.py", "network_game_client_idle_pregame",
                    "--allow-stubs", "--no-real-callees", "--no-concolic",
                    "--no-leaf-cache", "--no-stub-arg-trace", "--seeds", "1",
                    "--state-snapshot", str(snapshot_path), "--output-json",
                    str(report_path), "-q"]
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            try:
                unicorn_diff.main()
            except SystemExit as exc:
                result = exc.code
            else:
                result = 0
        report = json.loads(report_path.read_text())
        covered.update(int(address, 16) for address in report["covered_pcs"])
    if result or len(states) != 2 or any(state.error for state in states):
        raise AssertionError((case, result, [state.error for state in states],
                              output.getvalue(), calls))
    expected_return = int(case in ("success", "local_bypass"))
    if ((states[0].eax & 0xff) != expected_return or
            (states[1].eax & 0xff) != expected_return or
            states[0].scratch_data != states[1].scratch_data):
        raise AssertionError((case, "return or client state differs"))
    candidate_calls = list(calls[1])
    if case == "messages_failed":
        # The candidate inlines the message loop. The oracle stubs that helper;
        # its real 2276 body logs this inner event at 0x126113 on handler failure.
        inner = ("network_event", "network_game_client_handle_message() failed in "
                 "network_game_client_process_incoming_messages()")
        outer = ("network_event", "network_game_client_process_incoming_messages() "
                 "failed in network_game_client_idle_pregame()")
        if inner not in candidate_calls or outer not in candidate_calls or \
                candidate_calls.index(inner) >= candidate_calls.index(outer):
            raise AssertionError((case, "inlined message failure events", candidate_calls))
        candidate_calls.remove(inner)
    if calls[0] != candidate_calls:
        raise AssertionError((case, "helper call arguments differ", calls))
    events = [entry[1] for entry in calls[0] if entry[0] == "network_event"]
    expected_events = {
        "idle_failed": ["network_connection_idle() failed in network_game_client_idle_pregame()"],
        "messages_failed": ["network_game_client_process_incoming_messages() failed in network_game_client_idle_pregame()"],
    }.get(case, [])
    if events != expected_events:
        raise AssertionError((case, "event path", events))
    idle_calls = [entry for entry in calls[0]
                  if entry[0] == "network_connection_idle"]
    expected_idle = int(case in ("success", "local_bypass", "idle_failed",
                                 "messages_failed"))
    if idle_calls != ([("network_connection_idle", CONNECTION, 15000, 0)]
                      if expected_idle else []):
        raise AssertionError((case, "connection idle arguments", idle_calls))
    print(case, "matches")

required_branches = {0x126d01, 0x126d5d, 0x126d6c, 0x126d77,
                     0x126d86, 0x126d9b, 0x126daa}
missing = required_branches - covered
if missing:
    raise AssertionError(("unreached reference branches", sorted(missing)))
print(len(covered), "reference instruction addresses covered across cases")
