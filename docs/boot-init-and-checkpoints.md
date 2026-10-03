# Boot Control: init.txt and Saved States (Cores)

This document covers three things:

1. What `init.txt` is and how the deploy uses it.
2. How to boot a map fresh from its start.
3. **How to boot straight to a saved in-game checkpoint** — the "core" mechanism. This is the main reason to read this doc.

> ⚠️ **Critical caveat for AI / encounter bugs:** a restored core **and** a console `map_name` boot do **NOT**
> faithfully reproduce script-driven squad behavior (advance / engage / target orders). Those are issued by the
> scenario **main script thread**, which runs only on a real **New Game**. If you are debugging "grunts won't
> advance/aim"-type bugs, read **§4** first — a degenerate repro will make *every* build (patched or pristine)
> look broken and waste days.

---

## 1. What is `init.txt`?

`init.txt` is a text file the debug build reads and executes at startup.

- Source: `src/halo/main/console.c`, `console_startup()` (~line 326).
- The build opens `d:\init.txt` (or `editor_d:\init.txt` in editor mode).
- It reads each line and evaluates it as a HaloScript console command, the same as if you typed it in the in-game console (`~` key).
- Lines starting with `;` are treated as comments (silently skipped).

### Where `d:\` maps on Xbox

On the Xbox devkit, `d:\` is the running title's own directory. For our deploy that is:

```
E:\GAMES\halo-patched\
```

So:

| Path in HaloScript | Physical location on Xbox |
|--------------------|--------------------------|
| `d:\init.txt` | `E:\GAMES\halo-patched\init.txt` |
| `d:\core\core.bin` | `E:\GAMES\halo-patched\core\core.bin` |
| `d:\core\<name>` | `E:\GAMES\halo-patched\core\<name>` |

### How the deploy handles it

`tools/xbox/deploy_xbox.py`, function `deploy_init_txt()` (~line 819):

- If `<repo>/init.txt` **exists**: uploads it to the Xbox on every deploy.
- If `<repo>/init.txt` **does not exist**: deletes the remote one.

**You control boot behavior simply by editing `<repo>/init.txt`.** No other config is needed — just save the file and run the next deploy.

---

## 2. Booting a Map Fresh

To load a level from its start, put `map_name <level>` in `init.txt`. The level names match the file names in `maps/`.

**Example `init.txt` — boot the Pillar of Autumn at Legendary:**

```
game_difficulty_set impossible
map_name a10
```

Common level names: `a10`, `a30`, `a50`, `b30`, `b40`, `c10`, `c20`, `c40`, `d20`, `d40`.

**`game_difficulty_set` values:** `easy`, `normal`, `hard`, `impossible` (Legendary).

If you omit `game_difficulty_set`, the build uses whatever difficulty was last set.

---

## 3. Booting to a Saved Checkpoint (the Core Mechanism)

This is the primary workflow for iterating on a build at a specific in-game moment.

### What is a "core"?

A core (`core.bin` or a named `.bin` file) is a **snapshot of the game-state heap only — not the XBE code**. This matters:

- You can rebuild and redeploy the patched XBE without touching the core file.
- The new code runs on the restored state.
- You can reproduce the exact same in-game scenario (enemies in the same positions, same health, same encounter state) every boot, without replaying the level.

The core is saved to and loaded from `d:\core\` on the Xbox.

### The recipe: boot straight to your checkpoint

1. **Play to the spot you want to re-test.** Get to the encounter, cutscene, or game state you care about.

2. **Open the console** (`~` key) and save a core:
   - Default slot: `core_save` — saves to `d:\core\core.bin`.
   - Named slot (recommended if you want to keep it): `core_save_name <name>` — saves to `d:\core\<name>`.

3. **Edit `<repo>/init.txt`** to load the core on next boot.

4. **Deploy** — every subsequent deploy will boot that map and overlay your saved state, dropping you straight to your checkpoint.

### Default slot (`core.bin`)

```
game_difficulty_set impossible
map_name a10
core_load_at_startup
```

### Named slot

```
game_difficulty_set impossible
map_name a10
core_load_name_at_startup a10_flood_encounter.bin
```

### Why `core_load_at_startup` must come AFTER `map_name`

A core is game-state data. The map must be fully initialized before the core can be overlaid onto it. `core_load_at_startup` (and `core_load_name_at_startup`) defer the load until after the map finishes initializing. If you put the `core_load_at_startup` line **before** `map_name`, the map will not be loaded yet and the command will have nothing to load into.

**Correct order:**
```
map_name a10           ← load the map first
core_load_at_startup   ← then overlay the saved state
```

**Wrong order — will boot a fresh map, not the saved state:**
```
core_load_at_startup   ← no map loaded yet, does nothing
map_name a10
```

### Core commands reference

| Command | What it does |
|---------|-------------|
| `core_save` | Saves game state to `d:\core\core.bin` |
| `core_save_name <name>` | Saves game state to `d:\core\<name>` |
| `core_load` | Loads `d:\core\core.bin` immediately (use in-console) |
| `core_load_name <name>` | Loads `d:\core\<name>` immediately (use in-console) |
| `core_load_at_startup` | Loads `d:\core\core.bin` after map initializes (use in init.txt) |
| `core_load_name_at_startup <name>` | Loads `d:\core\<name>` after map initializes (use in init.txt) |
| `game_revert` | Reverts to the last in-session campaign autosave (no core needed) |

### Troubleshooting

**The map loads fresh instead of my saved state:**
- Check that `core_load_at_startup` (or `core_load_name_at_startup`) appears **after** `map_name` in `init.txt`.
- Check that the core file exists on the Xbox at `E:\GAMES\halo-patched\core\core.bin` (or the named path). If you saved it in a previous session on a different Xbox partition or with a different name, it will not be there.
- Confirm that `init.txt` was actually uploaded — the deploy script prints `init.txt (N bytes)` when it uploads. If it prints `deleting init.txt...`, the local file is missing.

**The core was saved at a different difficulty / the wrong map loads:**
- `core_load_at_startup` restores the map that was active when `core_save` was called. If `map_name` in `init.txt` names a different map, the core load will not match and the behavior is undefined. Use the same map name as when you saved.

**The engine generated a `d:\a10_init.txt` (slow-frame core):**
- When the engine detects sustained slow frames, it auto-saves a core and writes `d:\<map>_init.txt` containing `map_name <map>` and a commented `;core_load_name_at_startup <name>` line. This is for diagnostic replay, not normal use. Copy the relevant lines to `<repo>/init.txt` if you want to boot from it.

---

## 4. Faithful repro for script-driven AI behavior (a10 grunt advance, etc.)

Some bugs are about **scripted squad behavior** — e.g. the a10 first-doorway grunts that should advance + aim +
fire. That behavior is issued by the scenario **main script thread**, which runs only on a genuine **New Game**.
Two common shortcuts silently break the repro and make *every* build look broken:

- **Console `map_name a10`** loads the BSP and statically spawns the encounters but does **not** run the script
  thread → grunts shuffle to static firing posts, never acquire the player, never aim (only return fire when shot).
- **A restored core** snapshots the heap but not live script-thread state. A core saved "just before the trigger"
  does not re-issue the advance/engage orders on reload → the same false-passive, in patched *and* pristine builds.

**Faithful method:** empty `init.txt` → boot to the main menu → **New Game → Pillar of Autumn → Impossible** →
play to the doorway. Confirmed 2026-06-27: pristine-unpatched grunts advance + aim + fire + cycle guard↔fight here;
the degenerate core/console repros showed passive grunts in *both* builds.

### Deploying the PRISTINE (unpatched) original for an A/B

`deploy_xbox.py --xbe-only` re-runs the `patched_xbe` target whenever the build ELF is newer
(`deploy_xbox.py:901-908`), so `cp cachebeta.xbe default.xbe && deploy` silently uploads the **patched** XBE.
Upload the original **directly**, bypassing the patch step:

```
python3 tools/xbox/xbdm_rdcp.py --sendfile halo-patched/cachebeta.xbe 'E:\GAMES\halo-patched\default.xbe'
```

then magicboot it: send `magicboot title=E:\GAMES\halo-patched\default.xbe debug` over XBDM (port 731).

### Verify which build is actually running (it flip-flops — always check)

- **XBE header section count** @ VA `0x1011C`: **24 = unpatched**, **30 = patched**. Readable even at the menu
  (header magic `XBEH` is live at `0x10000`).
- **In-game byte** @ a ported function (e.g. `0x2cdb0`, AI pages load only in-game): `55 8b ec` = unpatched
  prologue; `68 .. .. .. 00 c3` (`push <impl>; ret`) = patched redirect.

---

## 5. Booting straight into a multiplayer game type (one player)

`game_variant` selects the multiplayer engine for the next `map_name`. Put both in `init.txt` and the box boots
into a live MP game with **one local player**: no menus, no second box, no controller input. The engine runs from
tick 0: spawns, per-tick update, objective items. For example, the oddball ball spawns at tick 450.

```
game_variant team_oddball
map_name levels\test\bloodgulch\bloodgulch
```

Proven variants (2026-09-30): `ctf`, `team_oddball`, `team_king`, `team_race`. `slayer` also works. Any string that
is not a game type (e.g. `none`) returns to single player.

`tools/xbox/boot_gametype.py` does the whole loop: stage `init.txt`, upload, magicboot, prove, delete `init.txt`:

```
python3 tools/xbox/boot_gametype.py --variant team_oddball --check                  # patched default.xbe
python3 tools/xbox/boot_gametype.py --variant ctf --xbe cachebeta.xbe --check       # pristine original
python3 tools/xbox/boot_gametype.py --variant none --map 'levels\a10\a10' --check
```

The title dir already holds the pristine `cachebeta.xbe`, so `--xbe cachebeta.xbe` magicboots the original
directly. The deploy-path trap in section 4 does not apply.

**Always prove the scenario.** A failed `init.txt` upload still boots, straight to the main menu. `--check` reads
back:

| Check | Address | Pass |
|---|---|---|
| `map_name` (kb.json) | `char[255]` | equals the requested map |
| `current_game_engine` (kb.json) | engine record | ctf `0x2efe88`, king `0x2eff10`, oddball `0x2effe8`, race `0x2f0070`, SP `0` |
| halt guard | byte `0x46e392` | `0` (1 = assert; text at `0x5aa8e8`) |
| `game_time_globals`+0xc | tick | advances between two samples |

**Transport gotcha:** on WSL `xbdm_rdcp.py` re-execs under Windows Python. The staged `init.txt` must sit on a
drive Windows can see (the tool uses `<repo>/tmp/`). A `/tmp/...` path fails with `local file not found`, and the
box then boots with no `init.txt`. Bridged xemu guests also need `HALO_WINDOWS_REEXEC=1`.

Uses: per-engine smoke tests for game-engine lifts, golden captures, and live-state equivalence snapshots
(section 6). MP maps do not depend on the scenario script thread, so the section 4 caveat about console
`map_name` does not affect MP game-engine behavior.

## 6. Live game-state snapshots for equivalence tests

`tools/equivalence/capture_gametype_snapshot.py` boots the **pristine** build into a game type, waits for a game
tick, then captures all writable game memory inside one QMP `stop`/`cont` window:

- the XBE `.data`/`.bss` section (every engine global), with bounds read from the XBE section table;
- the game-state allocation (players, objects, object bodies), with base and size read from `game_state_globals`.

It verifies the captured bytes (map, engine, halt guard, object/player `data_t` magic), then writes the
`{"regions": ...}` JSON that `unicorn_diff.py --state-snapshot` loads:

```
python3 tools/equivalence/capture_gametype_snapshot.py --variant team_oddball --at-tick 300 \
    --out artifacts/equivalence/snapshots/oddball_t300.json
python3 tools/equivalence/unicorn_diff.py oddball_engine_update --allow-stubs --mem-trace \
    --state-snapshot artifacts/equivalence/snapshots/oddball_t300.json
```

Snapshots are about 13 MB and live under the gitignored `artifacts/equivalence/`. A stubbed pure getter still
returns 0 from a snapshot, because the oracle calls a sentinel. Add `"stub_returns"` computed from the same
captured state; for example, `game_engine_get_variant` is `mov eax,0x456af8; ret`.
`tools/equivalence/derive_stub_returns.py` does this. For each no-parameter direct callee of a target, it runs
the original XBE code on the snapshot. It keeps EAX (AL for a bool return) only when the callee returns
without calling out or writing memory.

```
python3 tools/equivalence/derive_stub_returns.py --target oddball_engine_update \
    --snapshot artifacts/equivalence/snapshots/oddball_t1200.json --out /tmp/oddball_update.json
python3 tools/equivalence/unicorn_diff.py oddball_engine_update --allow-stubs --real-callees \
    --trace-all-stubs --pinned-state --mem-trace --state-snapshot /tmp/oddball_update.json
```

`--real-callees` stops same-TU callees that the candidate calls, but does not inline, from crashing as
`eip=0x8d`. `--pinned-state` treats the fixed snapshot as the input, so output that does not vary across seeds
is not flagged as vacuous. That exemption applies only when writes or stub calls were actually compared.

To commit a scenario as a regression test, trim the snapshot to the 4 KB pages the target reads or writes on
either side:

```
python3 tools/equivalence/trim_snapshot.py oddball_engine_update \
    --snapshot /tmp/oddball_update.json \
    --out tools/equivalence/regression_snapshots/oddball_engine_update_x.json \
    -- --allow-stubs --real-callees --trace-all-stubs --pinned-state --seeds 20 --mem-trace \
       --no-concolic --no-leaf-cache
```

The trimmer reruns the target on the trimmed file. It writes output only if verdicts, coverage and
compared writes/calls are identical, so a result of 13 MB -> 4-60 KB is safe. Then add a
`regression_targets.json` entry with the same flags. Known gap: when the candidate inlines a same-TU callee
the oracle stubs (the king update inlines `game_engine_set_goal_position`), the call sequences cannot align.
That shows up as "call-seq diverged at index 0" plus lifted-only writes, and it is a harness artifact.

---

## See also

- `docs/debug-commands-keyboard.md` — full HaloScript command reference, console keys, cheats.
- `docs/xbdm.md` — XBDM/RDCP workflow, deploy commands, real Xbox probing.
- `.claude/skills/halo-xbdm/SKILL.md` — deploy skill, covers `init.txt` upload in context of the full deploy flow.
