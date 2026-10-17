#!/usr/bin/env python3
"""Write deterministic core-header validation equivalence snapshots.

Each fixture drives one outcome of game_state_validate_core_header with the
header passed in ESI, as the original register ABI requires.  String and map
checksum predicates are controlled by symmetric stub returns; the header and
global fields still drive the concrete checksum and player-count predicates.
"""

import argparse
import json
import struct
from pathlib import Path


HERE = Path(__file__).resolve().parent
DEFAULT_OUTPUT = HERE / "regression_snapshots"
HEADER_SIZE = 0x14C
ALLOCATION_CHECKSUM = 0x3523FBF8
PLAYER_COUNT = 1


def _header():
    data = bytearray(HEADER_SIZE)
    data[0x04:0x0D] = b"test-map\0"
    data[0x104:0x114] = b"01.10.12.2276\0"
    struct.pack_into("<I", data, 0x00, ALLOCATION_CHECKSUM)
    struct.pack_into("<h", data, 0x124, PLAYER_COUNT)
    return data


def _snapshot(description, header, strcmp_returns, map_checksum):
    return {
        "description": description,
        "build_label": "halo-debug-2276-synthetic",
        "verified": True,
        "regions": {
            "0x00326a08": "0000000000000000",
            "0x004ea9a0": struct.pack("<II", ALLOCATION_CHECKSUM, 0).hex(),
            "0x0031fa94": struct.pack("<h6x", PLAYER_COUNT).hex(),
        },
        "stub_returns": {
            "csstrcmp": strcmp_returns,
            "FUN_001b9920": map_checksum,
        },
        "arg_overrides": {
            "header": bytes(header).hex(),
            "fatal": 0,
        },
    }


def build_snapshots():
    valid = _header()

    allocation_mismatch = bytearray(valid)
    struct.pack_into("<I", allocation_mismatch, 0x00, 0x13579BDF)

    player_count_mismatch = bytearray(valid)
    struct.pack_into("<h", player_count_mismatch, 0x124, PLAYER_COUNT + 1)

    map_checksum_mismatch = bytearray(valid)
    struct.pack_into("<I", map_checksum_mismatch, 0x128, 1)

    return {
        "game_state_validate_core_header_valid.json": _snapshot(
            "core header validates: matching build, scenario, allocation, player, and map checksums",
            valid, [0, 0], 0),
        "game_state_validate_core_header_build_mismatch.json": _snapshot(
            "core header rejects a build-version mismatch without the fatal halt path",
            valid, [1], 0),
        "game_state_validate_core_header_scenario_mismatch.json": _snapshot(
            "core header rejects a scenario-name mismatch without the fatal halt path",
            valid, [0, 1], 0),
        "game_state_validate_core_header_allocation_mismatch.json": _snapshot(
            "core header rejects an allocation-checksum mismatch",
            allocation_mismatch, [0, 0], 0),
        "game_state_validate_core_header_player_mismatch.json": _snapshot(
            "core header rejects a player-count mismatch",
            player_count_mismatch, [0, 0], 0),
        "game_state_validate_core_header_map_mismatch.json": _snapshot(
            "core header rejects a map-file checksum mismatch",
            map_checksum_mismatch, [0, 0], 0),
    }


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
