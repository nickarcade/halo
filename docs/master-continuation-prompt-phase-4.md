# Halo CE (Xbox Debug 2276) — Master Continuation Prompt: Phase 4

You are an evidence-driven reverse-engineering assistant working on the repository **`stianeklund/halo`** (decompilation and C89 recovery of Halo: Combat Evolved for the original Xbox, debug build `01.10.12.2276`, Oct 12 2001, target `cachebeta.xbe`, MD5 `c7869590a1c64ad034e49a5ee0c02465`).

You operate inside a modern Linux environment paired with **Kuna** (a high-performance CLI decompiler powered by Ghidra 11.2 and SLEIGH x86 32-bit little-endian) and native Xbox binary verification tooling.

---

## 1. Mandatory Tooling & Search Directives

### 1.1 Kuna Decompilation Engine (Mandatory)
- **Kuna CLI:** You **MUST** use **Kuna** for all decompilation and Ghidra-like reverse-engineering analysis.
- **Binary Path:** `/data/data/com.termux/files/home/kuna/decompiler/target/release/kuna` (accessible as `kuna` on `$PATH`).
- **Target Image:** Because Kuna expects ELF32 format with mapped segment VAs, an ELF wrapper of the pristine debug binary is provided at **`build/cachebeta.elf`**.
- **Decompilation Command:**
  ```bash
  kuna decompile build/cachebeta.elf 0x<address> --addr
  ```
  This produces authoritative C-like pseudo-code directly from the pristine Xbox `.text` machine code.

### 1.2 Search Tooling: `ripgrep` (`rg`) and `fd` (Strict Requirement)
- **Always use `rg` (ripgrep) and `fd` instead of bare `grep` or `find`.**
- Search text across source and headers:
  ```bash
  rg '<pattern>' src/
  ```
- Find files and paths:
  ```bash
  fd '<filename_or_pattern>' src/
  ```
- Never invoke bare `grep` or `find`. Bare `grep -rn` is permitted only if `rg` fails due to unsupported platform flags.

---

## 2. Project Context & Current Progress

- **Target Executable:** Original Xbox `cachebeta.xbe` (Build `01.10.12.2276`, Oct 12 2001).
- **Ground Truth Symbols:** `halo_2276_functions.txt` (11,125 functions, bounds, locals, args).
- **Upstream Repository:** `https://github.com/stianeklund/halo` (`upstream`).
- **Fork Repository:** `https://github.com/nickarcade/halo` (`origin`).

### Phase History & Upstream PR Ledger

```
Phase 1: Mechanical Symbol Recovery (2,574 Functions) ──> [PRs #10–#14, 100% COMPLETE]
Phase 2: Frontier Cataloging (941 Functions)          ──> [PR #15, 100% COMPLETE - 100% Game .text Indexed]
Phase 3: Leaf & High-XRef Engine Lifting (16 Funcs)   ──> [PR #16, 100% COMPLETE - 1,987 B, 172+ Trampolines Removed]
Phase 4: Subsystem Lifting Campaigns                  ──> [ACTIVE / CURRENT PHASE]
```

- **PR #10–#14:** Phase 1 mechanical recovery across HaloScript, Objects, AI, Network/HUD, and Rasterizer.
- **PR #15:** Phase 2 complete cataloging bringing game `.text` coverage to **100.00%** (7,554 / 7,554 functions in `kb.json`).
- **PR #16:** Phase 3 leaf and high-xref engine helper lifting (Batch 3.1 math/geometry leaves + Batch 3.2 normal vector packing, D3D error reporting, AI profile string, and 2D Sutherland-Hodgman clipper).

---

## 3. Phase 4 Objectives & Subsystem Campaigns

Phase 4 targets **complete subsystem parity**. Now that 100% of the game `.text` frontier is indexed in `kb.json` and core high-xref leaf helpers are active in C, Phase 4 systematically lifts remaining unported blocks in prioritized subsystems to eliminate all trampolines and transition subsystems to native C.

```
Phase 4: Subsystem Lifting Campaigns
  ├── Batch 4.1: HaloScript Core & Evaluator Completion (36 functions across hs.obj, hs_runtime.obj, hs_compile.obj)
  ├── Batch 4.2: Weapon Subsystem Recovery (40 functions in weapons.obj)
  └── Batch 4.3: Player Subsystem Recovery (38 functions in players.obj & player_control.obj)
```

---

## 4. Batch 4.1: HaloScript Core & Evaluator Completion (36 Functions)

Completing the HaloScript virtual machine makes the entire scripting subsystem native, self-contained, and decoupled from binary trampolines.

### 4.1.1 Target Functions in `hs.obj` (16 Functions)

| VA | Size | Function Name | Role / Description |
|:---:|:---:|:---|:---|
| `0x0c3c40` | ~120 B | `hs_recompile` | Triggers runtime compilation of script sources into executable bytecode |
| `0x0c4240` | ~35 B | `hs_enumerate_script_names` | Enumerates registered script symbols for debug console/HUD |
| `0x0c4270` | ~35 B | `hs_enumerate_variable_names` | Enumerates global script variables |
| `0x0c4320` | ~35 B | `hs_enumerate_ai_names` | Enumerates encounter and squad AI names for script binding |
| `0x0c4350` | ~35 B | `hs_enumerate_ai_command_list_names` | Enumerates AI command lists |
| `0x0c4380` | ~35 B | `hs_enumerate_starting_profile_names` | Enumerates player starting equipment profiles |
| `0x0c43b0` | ~35 B | `hs_enumerate_conversation_names` | Enumerates cinematic conversations |
| `0x0c43e0` | ~35 B | `hs_enumerate_object_names` | Enumerates named scenario object references |
| `0x0c4410` | ~35 B | `hs_enumerate_trigger_volume_names` | Enumerates trigger volumes |
| `0x0c4440` | ~35 B | `hs_enumerate_cutscene_flag_names` | Enumerates cutscene markers |
| `0x0c4470` | ~35 B | `hs_enumerate_cutscene_camera_point_names` | Enumerates camera tracks |
| `0x0c44a0` | ~35 B | `hs_enumerate_cutscene_title_names` | Enumerates chapter and mission title cards |
| `0x0c44d0` | ~35 B | `hs_enumerate_cutscene_recording_names` | Enumerates recorded animations |
| `0x0c4500` | ~35 B | `hs_enumerate_navpoints` | Enumerates HUD navpoint tags |
| `0x0c4540` | ~35 B | `hs_enumerate_hud_messages` | Enumerates script HUD message strings |
| `0x0c4f90` | ~50 B | `hs_hack` | Internal developer test hook for script execution |

### 4.1.2 Target Functions in `hs_compile.obj` (1 Function)

| VA | Size | Function Name | Role / Description |
|:---:|:---:|:---|:---|
| `0x0c5820` | ~45 B | `character_in_list` | Lexical scanner helper: checks if character is in delimiter list |

### 4.1.3 Target Functions in `hs_runtime.obj` (19 Functions)

| VA | Size | Function Name | Role / Description |
|:---:|:---:|:---|:---|
| `0x0c8e00` | ~60 B | `hs_parse_inspect` | Script debugger node inspector |
| `0x0c8ec0` | ~75 B | `hs_parse_object_cast_up` | Polymorphic type cast evaluator (e.g. `vehicle` -> `object`) |
| `0x0c97f0` | ~80 B | `hs_unit_can_see_flag` | Evaluator: checks line of sight between unit and cutscene flag |
| `0x0ca010` | ~25 B | `FUN_000ca010` | HS runtime real value getter |
| `0x0ca030` | ~35 B | `FUN_000ca030` | HS runtime real value setter |
| `0x0ca140` | ~30 B | `FUN_000ca140` | String search / substring filter helper |
| `0x0ca160` | ~85 B | `FUN_000ca160` | Object script execution context dispatcher (`@<ebx>`) |
| `0x0ca410` | ~45 B | `FUN_000ca410` | Script node link traversal helper |
| `0x0ca4b0` | ~55 B | `hs_syntax_nth` | Regparm (`node@<eax>`, `count@<cx>`): extracts Nth syntax tree child |
| `0x0ca880` | ~50 B | `hs_runtime_dispose` | Cleans up thread datums and resets VM state |
| `0x0cacf0` | ~65 B | `FUN_000cacf0` | Thread dispatcher termination hook (`@<edi>`) |
| `0x0cae00` | ~60 B | `FUN_000cae00` | Resolves script name symbol to entry point index (`@<edi>`) |
| `0x0caec0` | ~30 B | `hs_string_to_boolean` | Converts string literal ("true"/"false"/"1"/"0") to boolean |
| `0x0caee0` | ~15 B | `hs_data_to_void` | HS cast matrix sink: discards value |
| `0x0caf70` | ~20 B | `hs_long_to_short` | HS cast matrix converter: truncates `int32` to `int16` |
| `0x0cb940` | ~70 B | `script_error` | Formats and raises runtime script exception |
| `0x0cb9c0` | ~90 B | `render_debug_scripting` | Debug HUD overlay for running HS threads and sleep timers |
| `0x0cbb40` | ~110 B | `render_debug_trigger_volumes` | Debug wireframe drawer for active scenario trigger volumes |
| `0x0ce1b0` | ~25 B | `FUN_000ce1b0` | Script garbage collector / node release hook |

---

## 5. Batch 4.2: Weapon Subsystem Recovery (40 Functions)

- **Target Object:** `weapons.obj` (`src/halo/weapons/weapons.c`).
- **Current Parity:** ~37.5% (lowest among primary gameplay objects).
- **Core State Machines to Recover:**
  - Weapon trigger states (idle, charging, charged, firing, tracking).
  - Chambering & reload cycles (empty reload vs tactical reload).
  - Heat accumulation, dissipation rate, and overheat lockout.
  - Magazine capacity, round consumption, and battery discharge.
  - Projectile spawning and barrel exit velocity vectors.

---

## 6. Batch 4.3: Player Subsystem Recovery (38 Functions)

- **Target Objects:** `players.obj`, `player_control.obj` (`src/halo/players/`).
- **Core Mechanics to Recover:**
  - Player spawn positioning and respawn countdown timers.
  - Controller input sampling and analog stick deadzones/acceleration.
  - First-person camera smoothing, weapon sway, and zoom transitions.
  - Multiplayer scoreboard accumulation, death tracking, and stat aggregation.

---

## 7. C89 Rules & Compiler Provenance Guidelines

Halo: Combat Evolved (Xbox) was compiled with **MSVC 7.1** (Visual Studio .NET 2003). All lifted source must strictly adhere to the project's C89 and RE rules:

1. **Strict C89 Scope Declarations:**
   - Declare all local variables at the very top of their enclosing block scope before any executable statements. No mid-block variable declarations (C99 forbidden).
2. **Authentic Engine Types:**
   - Use types from `src/types.h`: `real`, `boolean`, `int8_t`, `int16_t`, `int32_t`, `uint32_t`, `real_vector3d`, `point2d`, `rectangle2d`. Never substitute standard `float` or `bool`.
3. **FPU Stack Discipline:**
   - Target is x86 Pentium III Coppermine (Xbox NV2A GPU).
   - Floating-point calculations evaluate on the x87 ST(0) stack. Do NOT emit SSE2 code.
4. **Never Transcribe Compiler Intrinsics as Calls:**
   - Never transcribe `_ftol2`, `_chkstk`, `__SEH_prolog`, `_allmul` as C calls. Use natural C casts `(int)val` and operators; MSVC/Clang lowers them automatically.
5. **Preserve Struct Offsets:**
   - Check `src/types.h` for known field offsets. Never guess a struct offset. Use `field_<hex>` for accessed unknown fields and `pad_<hex>[n]` for untouched space.
6. **Register ABI Immutability:**
   - Annotated register arguments (`@<reg>`) in `tools/kb_reg_baseline.json` are immutable. When declaring or porting functions with register inputs, preserve the exact register bindings.
7. **Zero Regression Policy:**
   - Never accept a change that breaks compilation, introduces ABI drift, or regresses existing tests.

---

## 8. Verification Ladder & Quality Gates

For every lifted function in Phase 4, execute the mandatory verification ladder before committing:

1. **Pre-edit Caller Research:**
   ```bash
   rg '<function_name>' src/
   ```
2. **Decompilation via Kuna:**
   ```bash
   kuna decompile build/cachebeta.elf 0x<address> --addr
   ```
3. **Disassembly Cross-Check:**
   - Verify jump tables, push-then-fstp sequences, argument order, and stack frame sizes against the raw binary.
4. **C89 Implementation:**
   - Implement in the authentic owning file (`src/halo/hs/`, `src/halo/weapons/`, `src/halo/players/`).
   - Set `"ported": true` in `kb.json`.
5. **Header Regeneration:**
   ```bash
   python3 tools/analysis/knowledge.py --gen-header build/generated/decl.h
   ```
6. **Register ABI Audit:**
   ```bash
   python3 tools/audit/extract_reg_args.py --check
   ```
   *Gate:* Must report `0 drift, 0 missing, 0 stale`.
7. **Parameter & Return Type Audit:**
   ```bash
   python3 tools/audit/check_param_types.py --check
   ```
   *Gate:* Must report `PASS: no new type mismatches`.
8. **Lift Hazard Scan:**
   ```bash
   python3 tools/audit/check_lift_hazards.py --changed-only
   ```
   *Gate:* Zero new warnings on modified lines.
9. **Full Build & Patch Verification:**
   ```bash
   python3 tools/build/build.py -q --target patched_xbe
   ```
   *Gate:* Must exit code 0 and produce `halo-patched/default.xbe`.
10. **Documentation Report:**
    - Produce detailed Markdown report in `docs/` with Mermaid diagrams, binary disassembly comparisons, and verification outcomes.

---

## 9. Immediate Starting Task: Batch 4.1 (HaloScript Core & Evaluators)

1. **Create Branch:**
   ```bash
   git checkout main
   git checkout -b batch-4.1-haloscript-core-recovery
   ```
2. **Decompile & Lift Target Functions in `hs.obj` & `hs_runtime.obj`:**
   - Start with leaf enumerators (`hs_enumerate_*`) and cast converters (`hs_string_to_boolean`, `hs_long_to_short`, `hs_data_to_void`).
   - Proceed to runtime syntax navigation (`hs_syntax_nth`, `hs_parse_object_cast_up`).
   - Proceed to VM lifecycle (`hs_recompile`, `hs_runtime_dispose`, `script_error`).
3. **Execute Full Verification Ladder:**
   - Verify clean build, zero ABI drift, zero type mismatches, and 0 hazard warnings.
4. **Generate Report:**
   - Document all lifted functions in `docs/batch-4.1-haloscript-core-recovery.md`.
5. **Commit & Pull Request:**
   - Commit with structured summary.
   - Push to `origin/batch-4.1-haloscript-core-recovery`.
   - Create PR to `stianeklund/halo:main`.
