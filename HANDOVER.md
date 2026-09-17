# Handover

## Objective
Investigate the four non-pass results from GitHub Snapshot Verification run `35127188495`, especially `game_state_validate_core_header`, and obtain a useful targeted state snapshot without destabilizing xemu.

## Current State
- GitHub job `104899004450` completed successfully, but the report contained four non-pass results.
- Local reproduction matched CI: `9/20` passed; the four results were validator `FAIL`, two allocator `ERROR`s, and LRU/cache initializer `FAIL`.
- The current worktree has unrelated pre-existing modifications to `README.md` and `tools/equivalence/leaf_cache.json`. Do not overwrite or stage them.
- No session changes remain in `src/halo/saved_games/game_state.c`.

## Confirmed
- `game_state_validate_core_header` is at `0x1bfa00`, with `header@<esi>` and `fatal@<bl>`.
- `game_state_data_new` is at `0x1bfe10`; `game_state_memory_pool_new` at `0x1bfe50`; `game_state_lruv_cache_new` at `0x1c0070`.
- CI results:
  - `game_state_validate_core_header`: `FAIL`, coverage `25.4%`, confidence `weak`.
  - `game_state_data_new`: `ERROR (emulation_error)`, coverage `0%`.
  - `game_state_memory_pool_new`: `ERROR (emulation_error)`, coverage `0%`.
  - `game_state_lruv_cache_new`: `FAIL`, coverage `100%`, confidence `none`.
- Migration history in `tools/equivalence/oracle_migration_expected_deltas.json` records the validator failure as pre-existing on both delinked and raw-XBE oracles: `FAIL`, coverage `32.1%`, moderate confidence.
- All four targets have `100%` VC71 records where inspected, so these are runtime/harness results, not byte-match failures.
- `ci/snapshot.json` is a minimal snapshot with globals and callback data, not a transient core header.
- `game_state_validate_core_header` is called by explicit `game_state_load_core`; normal checkpoint restore uses `game_state_revert` / `game_state_read_from_file`.
- `game_state_load_core` reads a `0x14c`-byte local header, then calls the validator. Header fields used by the validator are offsets `+0x04`, `+0x104`, `+0x124`, and `+0x128`.
- xemu QMP virtual memory inspection worked. A loaded-game state showed object table pointer `0x800b9370` and datum magic `0x64407440`.
- QMP captured stable regions for the loaded state under `artifacts/equivalence/regions/`, including `r_004ea9a0_200_loaded.bin`, `r_00326a00_200_loaded.bin`, `r_0031fa90_200_loaded.bin`, and `r_005ab100_200_loaded.bin`.
- Direct GDB breakpoint attempts installed `Breakpoint 1 at 0x1bfa00` but never hit. The GDB client later timed out/was killed, leaving xemu's GDB chardev stale; subsequent QMP `gdbserver tcp::1234` and `xemu_attach_gdb` reported `duplicate yank instance`.
- The project memory warns that GDB halts all vCPUs, repeated stops corrupt timing/state, and dirty disconnects desynchronize the RSP stream. The MCP has no native code-breakpoint API; `xemu_attach_gdb` uses the same broken GDB path.
- A temporary source hook copied the local header to `0x5ab100`; this caused a crash and was removed. The shared scratch buffer is not proven large enough for `0x14c` bytes and must not be reused for this purpose.

## Important Changes
- `artifacts/equivalence/plan_validate_core_header.json` was generated for the validator with `--anchors --window 0x200`; it lists reloc-driven regions and QMP `memsave` commands.
- Temporary loaded-state QMP region files exist in `artifacts/equivalence/regions/`; they are scratch artifacts, not yet a validated targeted fixture.
- No source or kb.json changes were intentionally retained.

## Validation
- Ran the exact local command:
  `.venv/bin/python3 tools/equivalence/game_state_verify.py --snapshot ci/snapshot.json --objects game_state.obj --seeds 25 --output /tmp/ci_results.json`
  Result matched CI.
- QMP status/query and `info registers` worked after xemu was relaunched.
- Do not claim a header capture exists: `artifacts/equivalence/header_validate_live.bin` was never created.
- No build/deploy was completed for the temporary hook; the build command was interrupted.

## Uncertain / Risks
- The user's exact debug-console command and console response were never captured. They reported using debug console commands to load a core, but the validator breakpoint did not fire.
- It is not yet proven whether the command was `core_load`, `core_load_name`, or a different restore path, nor whether the core file was found.
- The validator's 25.4% snapshot divergence needs a first failing seed/register/memory comparison before any source change.
- Do not restart/reset xemu or re-arm GDB without explicit coordination; the current session may have a stale GDB chardev.

## Next Steps
1. Leave the current worktree changes untouched and determine the exact supported runtime command for explicit `game_state_load_core` (`core_load` versus named variant) from the console implementation/docs.
2. Prefer deterministic validator fixtures: map a dedicated safe test region for the `ESI` header, provide the four referenced globals, and test valid plus each invalid-header branch against raw-XBE and candidate.
3. If a live capture is still required, restart xemu once to clear the stale GDB chardev, use one clean GDB session, and trigger the explicit core-load path only after the breakpoint is installed. Never kill a connected GDB client; remove the breakpoint and detach cleanly.
4. Once a header address is obtained, use QMP virtual `memsave` for exactly `0x14c` bytes and verify the build/scenario/checksum/player fields before constructing a snapshot.
5. Separately fix the CI policy so unexpected `FAIL`/`ERROR` results fail the step, while explicitly allowlisted `not_applicable` harness cases do not.

## Resume Prompt
Continue the Halo equivalence investigation from `HANDOVER.md`. Do not edit the unrelated `README.md` or `tools/equivalence/leaf_cache.json` changes. Treat the four CI results as: validator pre-existing but unresolved; allocator errors likely harness limitations; LRU/cache failure likely initializer/harness artifact. Prefer synthetic validator fixtures or a single clean GDB attempt only after clearing the stale chardev; never reuse `0x5ab100` for a 0x14c-byte header copy.
