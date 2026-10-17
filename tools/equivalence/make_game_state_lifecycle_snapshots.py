#!/usr/bin/env python3
"""Write deterministic game-state lifecycle equivalence snapshots.

The lifecycle entry points in game_state.obj take no parameters and return
void, so a seeded run varies nothing: every seed drives the same globals down
the same branch and the differential reports `vacuous_output` at 100% coverage.
Each fixture here pins one concrete outcome instead -- one branch, one set of
stub returns -- so the run is a deliberate single-input probe rather than an
unvaried sweep.

Companion to make_game_state_header_snapshots.py, which covers
game_state_validate_core_header's five rejection predicates.
"""

import argparse
import json
import struct
from pathlib import Path


HERE = Path(__file__).resolve().parent
DEFAULT_OUTPUT = HERE / "regression_snapshots"

# Globals the lifecycle routines branch on (game_state.c).
SAVED_FLAG = 0x004EA9A5        # uint8: a save exists
RESTART_FLAG = 0x005054E8      # uint8: map flagged for restart
VERIFIED_TICK = 0x004EA9A8     # int32: tick the save was taken at
MAP_TYPE = 0x0031FA94          # int16: 1 == single player
ALLOCATION_CHECKSUM = 0x004EA9A0
STATE_BUFFER = 0x004EA994      # void *: committed game-state buffer
SAVE_HEADER = 0x004EA9AC       # void *: 0x14c-byte save header

# Fixed addresses used as synthetic allocator results.  They are outside the
# XBE image span, so nothing the loader supplies is disturbed.
BUFFER_VA = 0x80061000         # what FUN_001c00c0 commits and returns
HEADER_VA_A = 0x00700000
HEADER_VA_B = 0x00701000
SCENARIO_NAME_VA = 0x00702000  # what the tag_get_name stub hands back

GAME_TICK = 0x00001234


def _u8(v):
    return struct.pack("<B7x", v).hex()


def _u16(v):
    return struct.pack("<h6x", v).hex()


def _u32(v):
    return struct.pack("<II", v, 0).hex()


def _snapshot(description, regions=None, stub_returns=None, arg_overrides=None):
    snap = {
        "description": description,
        "build_label": "halo-debug-2276-synthetic",
        "verified": True,
        "regions": regions or {},
    }
    if stub_returns:
        snap["stub_returns"] = stub_returns
    if arg_overrides:
        snap["arg_overrides"] = arg_overrides
    return snap


def build_snapshots():
    snaps = {}

    # --- game_state_save (0x1bf880) ------------------------------------
    # Branch-free, but it stores the write_to_file result to SAVED_FLAG, so
    # the two stub returns give two distinct write traces.
    for label, wrote in (("saved", 1), ("save_failed", 0)):
        snaps[f"game_state_save_{label}.json"] = _snapshot(
            f"game_state_save records a {'successful' if wrote else 'failed'} "
            f"write to the saved flag at 0x4ea9a5",
            regions={f"0x{SAVED_FLAG:08x}": _u8(0 if wrote else 1)},
            stub_returns={"game_state_write_to_file": wrote},
        )

    # --- game_state_revert (0x1bf8a0) ----------------------------------
    # Writes nothing; the branch is visible only as a call sequence
    # (main_reset_map vs read_from_file + the 13-callback loop).
    snaps["game_state_revert_restart.json"] = _snapshot(
        "game_state_revert restarts the map when no save exists and the "
        "restart flag is clear",
        regions={
            f"0x{SAVED_FLAG:08x}": _u8(0),
            f"0x{RESTART_FLAG:08x}": _u8(0),
        },
    )
    snaps["game_state_revert_reload.json"] = _snapshot(
        "game_state_revert reloads the save and runs the 13 after-load "
        "callbacks when a save exists",
        regions={
            f"0x{SAVED_FLAG:08x}": _u8(1),
            f"0x{RESTART_FLAG:08x}": _u8(0),
        },
    )

    # --- game_state_save_to_persistent_storage (0x1bf8e0) --------------
    snaps["game_state_save_to_persistent_storage_single_player.json"] = _snapshot(
        "single-player map type reverts then writes the header to persistent "
        "storage",
        regions={
            f"0x{MAP_TYPE:08x}": _u16(1),
            f"0x{SAVED_FLAG:08x}": _u8(1),
            f"0x{RESTART_FLAG:08x}": _u8(0),
            f"0x{STATE_BUFFER:08x}": _u32(BUFFER_VA),
            f"0x{SAVE_HEADER:08x}": _u32(HEADER_VA_A),
        },
    )
    snaps["game_state_save_to_persistent_storage_multiplayer.json"] = _snapshot(
        "a non-single-player map type writes nothing to persistent storage",
        regions={
            f"0x{MAP_TYPE:08x}": _u16(2),
            f"0x{STATE_BUFFER:08x}": _u32(BUFFER_VA),
            f"0x{SAVE_HEADER:08x}": _u32(HEADER_VA_A),
        },
    )

    # --- game_state_reverted (0x1bf9e0) --------------------------------
    # Returns verified_tick == game_time_get(); one fixture per outcome.
    snaps["game_state_reverted_true.json"] = _snapshot(
        "the verified tick equals the current game time, so the state counts "
        "as reverted",
        regions={f"0x{VERIFIED_TICK:08x}": _u32(GAME_TICK)},
        stub_returns={"game_time_get": GAME_TICK},
    )
    snaps["game_state_reverted_false.json"] = _snapshot(
        "game time has advanced past the verified tick, so the state is not "
        "reverted",
        regions={f"0x{VERIFIED_TICK:08x}": _u32(GAME_TICK)},
        stub_returns={"game_time_get": GAME_TICK + 1},
    )

    # --- game_state_lruv_cache_new (0x1c0070) --------------------------
    # FUN_001c00c0 returns its `address` argument, so its stub return must be
    # BUFFER_VA or the oracle's store to 0x4ea994 disagrees with the
    # candidate's (clang forwards the constant through the call).
    for label, header_va in (("header_a", HEADER_VA_A), ("header_b", HEADER_VA_B)):
        snaps[f"game_state_lruv_cache_new_{label}.json"] = _snapshot(
            f"game-state buffer committed at 0x{BUFFER_VA:08x} and the save "
            f"header allocated at 0x{header_va:08x}",
            regions={
                f"0x{STATE_BUFFER:08x}": _u32(0),
                f"0x{SAVE_HEADER:08x}": _u32(0),
            },
            stub_returns={
                "FUN_001c00c0": BUFFER_VA,
                "game_state_malloc": header_va,
            },
        )

    # --- game_state_initialize_for_new_map (0x1bf7c0) ------------------
    # Clears the 0x14c-byte save header through the pointer at 0x4ea9ac and
    # refills it, so the whole fixture is about the resulting write trace.
    for label, difficulty, map_checksum in (("easy", 0, 0x11111111),
                                            ("legendary", 3, 0x22222222)):
        snaps[f"game_state_initialize_for_new_map_{label}.json"] = _snapshot(
            f"new-map header rebuilt at difficulty {difficulty} with map "
            f"checksum 0x{map_checksum:08x}",
            regions={
                f"0x{SAVE_HEADER:08x}": _u32(HEADER_VA_A),
                f"0x{HEADER_VA_A:08x}": "00" * 0x14C,
                f"0x{SCENARIO_NAME_VA:08x}": b"test-map\0\0\0\0\0\0\0\0".hex(),
                f"0x{ALLOCATION_CHECKSUM:08x}": _u32(0x3523FBF8),
                f"0x{MAP_TYPE:08x}": _u16(1),
                "0x00326a08": _u32(0),
            },
            stub_returns={
                "tag_get_name": SCENARIO_NAME_VA,
                "game_difficulty_level_get": difficulty,
                "FUN_001b9920": map_checksum,
            },
        )

    # --- game_state_dispose (0x1bf7b0) ---------------------------------
    snaps["game_state_dispose.json"] = _snapshot(
        "game_state_dispose releases the buffer then closes the file",
    )

    # --- game_state_call_after_load_procs (0x1bf790) -------------------
    snaps["game_state_call_after_load_procs.json"] = _snapshot(
        "the after-load callback table at 0x32eaa8 is walked exactly 13 times",
    )

    return snaps


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    for name, snapshot in build_snapshots().items():
        path = args.output_dir / name
        path.write_text(json.dumps(snapshot, indent=2) + "\n", encoding="utf-8")
        print(path)


if __name__ == "__main__":
    main()
