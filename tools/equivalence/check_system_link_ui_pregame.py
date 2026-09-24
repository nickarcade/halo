import json
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, "tools/equivalence")
from unicorn.x86_const import UC_X86_REG_ESP
from stubs import StubManager
import unicorn_diff

original_execute = StubManager.execute_stub
calls = []


def capture(self, uc, address):
    name = self._resolve_name(address).lower().lstrip("_")
    esp = uc.reg_read(UC_X86_REG_ESP)
    if name == "ui_widget_load_by_name_or_tag":
        ptr = int.from_bytes(uc.mem_read(esp + 4, 4), "little")
        raw = bytes(uc.mem_read(ptr, 180)).split(b"\0", 1)[0]
        calls.append(("widget", raw.decode("ascii")))
    elif name == "network_game_server_pause_countdown":
        args = tuple(int.from_bytes(uc.mem_read(esp + 4 + 4 * i, 4), "little")
                     for i in range(2))
        calls.append(("pause", args))
    elif name == "error":
        ptr = int.from_bytes(uc.mem_read(esp + 8, 4), "little")
        raw = bytes(uc.mem_read(ptr, 100)).split(b"\0", 1)[0]
        calls.append(("error", raw.decode("ascii")))
    return original_execute(self, uc, address)


StubManager.execute_stub = capture
cases = [
    ("ui_pregame_split", "split_screen\\pregame\\splitscreen_pregame_wrapper_normal",
     "failed to load pregame screen after quickstart match", False),
    ("ui_pregame_split_postgame", "split_screen\\splitscreen_map_select_postgame_wrapper",
     "failed to load map select postgame screen", False),
    ("ui_pregame_connected_host", "connected\\connected_map_select_postgame_wrapper",
     "failed to load map select postgame screen", True),
    ("ui_pregame_connected_client", "connected\\pregame\\connected_pregame_screen",
     "failed to load networked pregame status screen", False),
]
with tempfile.TemporaryDirectory() as temporary:
    for case, suffix, failure_text, host in cases:
        base = Path(__file__).with_name("regression_snapshots") / (case + ".json")
        for fails in (False, True):
            calls.clear()
            snapshot = base
            if fails:
                data = json.loads(base.read_text())
                data["stub_returns"]["ui_widget_load_by_name_or_tag"] = [0]
                data["stub_returns"]["error"] = [0]
                snapshot = Path(temporary) / (case + "_failure.json")
                snapshot.write_text(json.dumps(data))
            sys.argv = ["unicorn_diff.py", "network_game_reset_to_pregame_ui",
                        "--allow-stubs", "--real-callees", "--no-leaf-cache",
                        "--state-snapshot", str(snapshot), "--seeds", "1",
                        "--no-concolic", "--no-stub-arg-trace", "-q"]
            try:
                unicorn_diff.main()
            except SystemExit as exc:
                if exc.code != 0:
                    raise
            expected = [("widget", "ui\\shell\\main_menu\\multiplayer_type_select\\" + suffix)]
            if host:
                expected.insert(0, ("pause", (0x700000, 1)))
            if fails:
                expected.append(("error", failure_text))
            if calls != expected + expected:
                raise AssertionError((case, fails, calls, expected + expected))
            print(case, "failure" if fails else "success", "calls match")
