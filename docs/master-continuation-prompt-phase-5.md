# Halo CE (Xbox Debug 2276) — Master Continuation Prompt: Phase 5

You are an evidence-driven reverse-engineering assistant working on the repository **`stianeklund/halo`** (decompilation and C89 recovery of Halo: Combat Evolved for the original Xbox, debug build `01.10.12.2276`, Oct 12 2001, target `cachebeta.xbe`, MD5 `c7869590a1c64ad034e49a5ee0c02465`).

You operate inside a modern Linux environment paired with **Kuna** (a high-performance CLI decompiler powered by Ghidra 11.2 and SLEIGH x86 32-bit little-endian) and native Xbox binary verification tooling.

---

## 1. Mandatory Tooling & Search Directives

### 1.1 Kuna Decompilation Engine & Pre-Decompiled Corpus
- **Pre-Decompiled Binary Corpus:** The entire `.text` segment of the pristine Xbox debug binary (`build/cachebeta.elf`) has **already been decompiled via Kuna/Ghidra SLEIGH and fully indexed** in [`halo_decompiled/`](file:///storage/1F34-EBBE/halo/halo_decompiled/).
- **Primary Access via `tools/extract_fn.py`:** You **MUST** use **`tools/extract_fn.py`** (Section 1.4) as your primary tool to pull up decompiled functions, addresses, symbols, keywords, disassembly, and metadata instantly.
- **Live Kuna CLI (Fallback / Deep Inspection):** When custom AST slicing, type redefinition, or dynamic block inspection is required beyond the pre-indexed corpus:
  - **Binary Path:** `/data/data/com.termux/files/home/kuna/decompiler/target/release/kuna` (accessible as `kuna` on `$PATH`).
  - **Target Image:** ELF wrapper at **`build/cachebeta.elf`**.
  - **Decompilation Command:**
    ```bash
    kuna decompile build/cachebeta.elf 0x<address> --addr
    ```
    This produces live C pseudo-code directly from the pristine Xbox `.text` machine code.

### 1.2 Search Tooling: `ripgrep` (`rg`) and `fd` (Strict Requirement)
- **Always use `rg` (ripgrep) and `fd` instead of bare `grep` or `find`.**
- Search text across source and headers:
  ```bash
  rtk rg '<pattern>' src/
  ```
- Find files and paths:
  ```bash
  rtk fd '<filename_or_pattern>' src/
  ```
- Prefix shell tool calls with `rtk`. Never invoke bare `grep` or `find`. Bare `grep -rn` is permitted only if `rg` fails due to unsupported platform flags.

### 1.3 Neural & Semantic Code Retrieval (BGE-M3 + BGE-Reranker-v2)
- **Purpose:** When lifting complex subsystems in Phase 5, exact symbol names or string references may be stripped, obscured, or not yet ported into `kb.json`. The neural code retrieval pipeline indexes all **11,137 decompiled functions** of the pristine Xbox debug binary (`cachebeta.elf.c`) into high-dimensional semantic vector space, enabling instant behavioral and algorithmic queries across the entire engine.
- **Corpus & Embeddings Location:**
  - **Local Directory:** [`/storage/1F34-EBBE/halo/halo_decompiled/`](file:///storage/1F34-EBBE/halo/halo_decompiled/)
  - **Pre-computed Embeddings:** [`/storage/1F34-EBBE/halo/halo_decompiled/bge_m3_embeddings.npy`](file:///storage/1F34-EBBE/halo/halo_decompiled/bge_m3_embeddings.npy) (Shape: `(11137, 1024)`, float32)
  - **Corpus Metadata:** [`/storage/1F34-EBBE/halo/halo_decompiled/corpus_metadata.json`](file:///storage/1F34-EBBE/halo/halo_decompiled/corpus_metadata.json) (indexed function signatures, offsets, source line numbers from `cachebeta.elf.c` and `index.jsonl`)
  - **Remote GPU Notebook & Mirror:** Stored on Google Notebook Cloud at [`/content/drive/MyDrive/Colab Notebooks/halo-decompiled.ipynb`](file:///content/drive/MyDrive/Colab%20Notebooks/halo-decompiled.ipynb) (synced with Drive mirror `/content/drive/MyDrive/halo_decompiled/`, managed via `google-colab-cli` session `halo-gpu` on Tesla T4 High-RAM, instance ID: `gpu-t4-hm-kkb-use1c0-2tszf4tf1g9kt`).
- **Two-Stage Retrieval Architecture:**
  1. **Stage 1 (Dense Bi-Encoder Retrieval — `BAAI/bge-m3`):** Generates 1024-dim query vectors and performs hardware-accelerated matrix dot-product similarity against all 11,137 function embeddings in <15ms to select top candidate functions (default top-20).
  2. **Stage 2 (Cross-Encoder Reranking — `BAAI/bge-reranker-v2-m3`):** Evaluates joint cross-attention over `(query, decompiled_candidate_source)` pairs (context-window clipped to 2,000 characters) to deliver calibrated relevance scores without GPU VRAM exhaustion.
- **Monochrome Pipeline Flow:**
  ```
  ┌────────────────────────────────────────────────────────┐
  │ Natural Language / Behavior Query                      │
  │ (e.g. "player damage screen flash shield charge tint") │
  └───────────────────────────┬────────────────────────────┘
                              │
                              ▼
  ┌────────────────────────────────────────────────────────┐
  │ Stage 1: BGE-M3 Bi-Encoder (Dense Vector Search)       │
  │ Embed query -> Dot product vs 11,137 embeddings (GPU)  │
  └───────────────────────────┬────────────────────────────┘
                              │ [Top 20 Candidates]
                              ▼
  ┌────────────────────────────────────────────────────────┐
  │ Stage 2: BGE-Reranker-v2-M3 Cross-Encoder              │
  │ Deep semantic cross-attention & score sorting          │
  └───────────────────────────┬────────────────────────────┘
                              │ [Ranked Matches]
                              ▼
  ┌────────────────────────────────────────────────────────┐
  │ Result: Function VA, Name, Source Snippet & File       │
  │ (e.g. 0x0013b820 player_effects_update in C89)        │
  └────────────────────────────────────────────────────────┘
  ```
- **CLI Query Execution via `google-colab-cli`:**
  To query the active GPU backend directly from your local terminal session:
  ```bash
  colab exec -s halo-gpu -- '
  query = "first person shield recharge tint and screen flash"
  results = search_halo(query, top_k=5)
  for r in results:
      print(f"[{r[\"score\"]:.4f}] 0x{r[\"address\"]:08x} {r[\"name\"]} ({r[\"file\"]})")
  '
  ```
- **Colab UI Query Execution:**
  Alternatively, open `halo-decompiled.ipynb` in Google Colab connected to Google Drive, scroll to Cell 4 (Semantic Code Search), edit the `query` string, and run the cell.

### 1.4 Universal Decompiled Binary Interface: `tools/extract_fn.py` (Mandatory)
- **Mandatory Query Command Center:** You **MUST** use **`tools/extract_fn.py`** as your primary tool for pulling up **keywords, functions, addresses, symbols, names, disassembly, and metadata** across the entire Halo CE decompiled codebase.
- **Why `extract_fn.py`:** The decompiled binary corpus in [`halo_decompiled/`](file:///storage/1F34-EBBE/halo/halo_decompiled/) contains over 85MB of pre-indexed reverse-engineering data. Reading these files directly into context or using slow string searches violates token discipline and risks context exhaustion. `extract_fn.py` connects all indexed assets together to provide instant sub-5ms $O(1)$ lookup:
  1. [`corpus_metadata.json`](file:///storage/1F34-EBBE/halo/halo_decompiled/corpus_metadata.json) (942 KB): Catalog of all 11,137 functions with sequential IDs, verified symbol names, absolute VAs, and byte sizes.
  2. [`index.jsonl`](file:///storage/1F34-EBBE/halo/halo_decompiled/index.jsonl) (1.3 MB): Direct byte-offset and length index into `cachebeta.elf.c` for instantaneous $O(1)$ seeks.
  3. [`cachebeta.elf.c`](file:///storage/1F34-EBBE/halo/halo_decompiled/cachebeta.elf.c) (25 MB): Full authoritative C pseudo-code of all 11,137 decompiled functions.
  4. [`cachebeta.elf.asm`](file:///storage/1F34-EBBE/halo/halo_decompiled/cachebeta.elf.asm) (60 MB): Complete x86 disassembly, jump tables, and stack frame layouts.
  5. [`bge_m3_embeddings.npy`](file:///storage/1F34-EBBE/halo/halo_decompiled/bge_m3_embeddings.npy) (44 MB): 1024-dim neural embeddings for instantaneous cosine-similarity search.

- **Authoritative Command Reference:**
  - **Pull up by Memory Address (supports `0x1c4990`, `1c4990`, `0x001c4990`):**
    ```bash
    rtk python3 tools/extract_fn.py 0x1c4990
    ```
  - **Pull up by Exact Symbol or Function Name:**
    ```bash
    rtk python3 tools/extract_fn.py playlist_profile_save
    ```
  - **Keyword & Substring Search across All 11,137 Cataloged Symbols:**
    ```bash
    rtk python3 tools/extract_fn.py "playlist_profile"
    rtk python3 tools/extract_fn.py -s "checkpoint"
    ```
    *(If exactly 1 match, immediately dumps decompiled C source; if multiple matches, outputs an indexed tabular summary with VAs, sizes, and sequence IDs).*
  - **Metadata Table Inspection Only (No Code Dump):**
    ```bash
    rtk python3 tools/extract_fn.py --info "checkpoint"
    rtk python3 tools/extract_fn.py -i "game_engine"
    ```
  - **Include x86 Disassembly & Stack Frame Layout:**
    ```bash
    rtk python3 tools/extract_fn.py 0x1c4990 --asm
    rtk python3 tools/extract_fn.py playlist_profile_save --asm
    ```
  - **Find Semantically & Structurally Similar Functions (via BGE-M3 Embeddings):**
    ```bash
    rtk python3 tools/extract_fn.py --similar playlist_profile_save -k 5
    rtk python3 tools/extract_fn.py --similar 0x1c4990 -k 5
    ```
  - **Extract All Matching Functions when Multiple Match:**
    ```bash
    rtk python3 tools/extract_fn.py --all "screen_flash"
    ```

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
Phase 3: Leaf & High-XRef Engine Lifting (16 Funcs)   ──> [PRs #16 & #17, 100% COMPLETE - 1,987 B, 172+ Trampolines Removed]
Phase 4: Core Subsystem Lifting Campaigns (100 Funcs) ──> [PRs #18–#21, 100% COMPLETE - 20 TUs to 100%]
Phase 5: Subsystem Endgame & Standalone Engine        ──> [ACTIVE / CURRENT PHASE]
```

- **Phase 1 (PRs #10–#14):** Mechanical recovery replacing 2,574 `FUN_` placeholders with authentic symbols.
- **Phase 2 (PR #15):** Frontier cataloging bringing game `.text` coverage to **100.00%** (7,554 / 7,554 functions indexed in `kb.json`).
- **Phase 3 (PRs #16 & #17):** High-xref engine helpers and math/geometry leaves lifted to native C.
- **Phase 4 (PRs #18–#21):** Core subsystem recovery across HaloScript VM, Weapons, Core Players, and 20 single-function near-complete translation units:
  - **PR #18:** Batch 4.1 — HaloScript Core & Evaluators (36 functions across `hs.c`, `hs_runtime.c`, `hs_compile.c`).
  - **PR #19:** Batch 4.2 & 4.x — Weapons subsystem (40 functions in `weapons.c`) + 15 near-complete TUs closed to 100%.
  - **PR #20:** Batch 4.3 — Core Player subsystem (9 functions across `players.c`, `player_control.c`, `player_queues_new.c` — all 3 TUs to 100%).
  - **PR #21:** Fast-Follow — Final 5 single-function near-complete TUs closed to 100% (`marketing_and_strategic_business_development.c`, `rasterizer_xbox_vertex_shaders_initialize.c`, `draw_string.c`, `xcontent.c`, `render_debug.c`).

### Current Codebase Standing
- **Total Functions in `kb.json`**: 10,063 (5,901 ported / 58.64%)
- **Game Logic Functions (`src/halo/`)**: 7,549
  - **Ported into C**: **5,891 functions (78.04%)**
  - **Remaining Unported**: **1,658 functions** across 116 translation units

---

## 3. Phase 5 Objectives & Campaign Structure

Phase 5 represents the **subsystem endgame**: systematically lifting the remaining 1,658 game functions to eliminate all engine trampolines, transitioning Halo CE from a hybrid binary-patch model into a **100% native standalone C executable**.

```
Phase 5: Subsystem Endgame & Standalone Engine Transition
  ├── Campaign 5.1: Interface & UI Widget System (181 functions)
  ├── Campaign 5.2: Game Engine, Session & Saved Game State (205 functions)
  ├── Campaign 5.3: World Simulation — Objects, Units, Vehicles & Camera (158 functions)
  ├── Campaign 5.4: AI Perception, Combat & Encounters (139 functions)
  ├── Campaign 5.5: Cache, Sound & Physics Foundations (147 functions)
  ├── Campaign 5.6: Rasterizer & Direct3D Graphics Pipeline (305 functions)
  ├── Campaign 5.7: LibTIFF & Bitmaps (146 functions)
  └── Campaign 5.8: Standalone Link & Trampoline Retirement
```

> [!TIP]
> **Subsystem Discovery with Neural Search:**
> When identifying target subroutines for any campaign, leverage the dual-stage retrieval engine (Section 1.3). For example:
> - **Campaign 5.1 (UI/HUD):** Query `"player damage screen flash shield charge tint"` or `"hud multiplayer scoreboard overlay"`.
> - **Campaign 5.2 (Game Engine):** Query `"CTF flag state tracking netgame"` or `"saved game memory snapshot decompression"`.
> - **Campaign 5.3 (Simulation):** Query `"vehicle tire ground friction suspension"` or `"biped ragdoll impulse contact"`.
> - **Campaign 5.4 (AI):** Query `"Catmull Rom spline path smoothing"` or `"actor combat burst firing weapon cooldown"`.
> - **Campaign 5.5 (Cache/Sound/Physics):** Query `"virtual texture page fault streaming"` or `"BSP surface pill box intersection"`.
> - **Campaign 5.6 (Rasterizer/NV2A):** Query `"Direct3D push buffer vertex shader setup"` or `"volumetric fog distance vertex calculation"`.

---

## 4. Campaign 5.1: Interface & UI Widget System [Near Complete — 21 Unported Remaining]

Lifts the entire visual presentation layer, player HUD feedback, menus, and UI input dispatch.

### 4.1.1 Batch 5.1A: Player HUD & Effects Transition [100% COMPLETE]
- `player_effects.obj` (`src/halo/effects/player_effects.c`): **29/29 ported (100.0%)** — First-person visual FX, screen flash damage, shield charge tints, binocular zoom overlays.
- `player_ui.obj` (`src/halo/interface/player_ui.c`): **47/47 ported (100.0%)** — Player HUD meters, health/shield drawing, reticle color state, multiplayer scoreboard overlays.

### 4.1.2 Batch 5.1B: UI Widget Hierarchy & Data Binding (Active Frontier: 21 Remaining)

| Object | File | Ported | Unported | Total | Description |
|:---|:---|:---:|:---:|:---:|:---|
| `ui_widget.obj` | `src/halo/interface/ui_widget.c` | 105 | 16 | 121 | UI widget lifecycle, hierarchy traversal, layout bounds, animation controllers |
| `ui_widget_game_data_input_functions.obj` | `src/halo/interface/ui_widget_game_data_input_functions.c` | 73 | 5 | 78 | Data source adapters connecting engine state (network lobby, game settings) to UI widgets |

> [!TIP]
> Use `rtk python3 tools/extract_fn.py "ui_widget"` or `rtk python3 tools/extract_fn.py -i "game_data_input"` to inspect remaining unported function signatures and offsets.

### 4.1.3 Batch 5.1C: Shell & Messaging [100% COMPLETE]
- `hud_messaging.obj` (`src/halo/interface/hud_messaging.c`): **94/94 ported (100.0%)** — HUD text queue, cinematic subtitle timing.
- `event_manager.obj` (`src/halo/interface/event_manager.c`): **38/38 ported (100.0%)** — UI controller event dispatch.
- `interface.obj` (`src/halo/interface/interface.c`): **17/17 ported (100.0%)** — Menu transitions and top-level UI states.
- `progress_bar.obj` (`src/halo/interface/progress_bar.c`): **45/45 ported (100.0%)** — Loading bar rendering and bounds.

---

## 5. Campaign 5.2: Game Engine, Session & Saved Games [100% COMPLETE — 439/439 Functions Ported]

Lifts the core simulation loop, multiplayer match rules, scenario lifecycle, and persistence. **All 6 translation units are 100.0% ported, verified against MSVC 7.1, and passing all quality gates.**

| Object | File | Functions | Ported | Status | Description |
|:---|:---|:---:|:---:|:---:|:---|
| `game_engine.obj` | `src/halo/game/game_engine.c` | 238 | 238 | **100.0%** | Multiplayer game variant rules: CTF flag states, Slayer score tracking, Oddball skull timers |
| `saved_game_files.obj` | `src/halo/saved games/saved_game_files.c` | 62 | 62 | **100.0%** | Checkpoint serialization, FATX hard drive save file headers, profile verification |
| `game.obj` | `src/halo/game/game.c` | 59 | 59 | **100.0%** | Engine main loop, game tick rate stepping, map reload/transition state machine |
| `game_state_xbox.obj` | `src/halo/saved_games/game_state_xbox.c` | 32 | 32 | **100.0%** | Xbox-specific memory layout snapshot persistence |
| `cheats.obj` | `src/halo/game/cheats.c` | 28 | 28 | **100.0%** | Developer console cheats: god mode, infinite ammo, teleportation commands |
| `game_state.obj` | `src/halo/saved_games/game_state.c` | 20 | 20 | **100.0%** | Raw game memory snapshotting and decompression |
| **Total** | | **439** | **439** | **100.0%** | **Full subsystem ported with 0 lift hazards, 0 ABI drift, clean build** |

> [!NOTE]
> Comprehensive verification report recorded at [`docs/batch-5.2-msvc71-verification-report.md`](file:///storage/1F34-EBBE/halo/docs/batch-5.2-msvc71-verification-report.md).

---

## 6. Campaign 5.3: World Simulation — Objects, Units, Vehicles & Camera (64 Unported Remaining)

Completes entity management, damage calculation, vehicle handling, and camera directors. Both core entity managers (`objects.obj` and `units.obj`) are already **100% ported**.

| Object | File | Ported | Unported | Total | Status | Description |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| `objects.obj` | `src/halo/objects/objects.c` | 231 | 0 | 231 | **100.0%** | Object header table management, spatial partition link/unlink, garbage collection |
| `units.obj` | `src/halo/units/units.c` | 208 | 0 | 208 | **100.0%** | Unit inventory slots, weapon switching state, seat entry/exit animations |
| `director.obj` | `src/halo/camera/director.c` | 28 | 19 | 47 | 59.6% | Cinematic camera sequences, perspective transitions, script-controlled camera pans |
| `vehicles.obj` | `src/halo/units/vehicles.c` | 16 | 18 | 34 | 47.1% | Vehicle physics suspension, tire friction, hover physics, vehicle weapon seats |
| `bipeds.obj` | `src/halo/units/bipeds.c` | 34 | 11 | 45 | 75.6% | Character biped skeleton orientation, ragdoll impulse application, ground contact |
| `observer.obj` | `src/halo/camera/observer.c` | 32 | 10 | 42 | 76.2% | First-person / third-person camera observer positioning, collision avoidance |
| `damage.obj` | `src/halo/objects/damage.c` | 28 | 6 | 34 | 82.4% | Damage acceleration, shield absorption, radius falloff calculations |
| **Total** | | **577** | **64** | **641** | **90.0%** | **64 functions remaining across 5 objects** |

> [!TIP]
> Extract camera, vehicle, and damage logic directly with `rtk python3 tools/extract_fn.py <symbol>` or `--asm`.

---

## 7. Campaign 5.4: AI Perception, Combat & Encounters (139 Functions)

Lifts enemy behaviors, perception cones, communication chatter, and squad coordination.

| Object | File | Unported | Description |
|:---|:---|:---:|:---|
| `actor_perception.obj` | `src/halo/ai/actor_perception.c` | 24 | Visual perception cones, sound event detection, target prioritization |
| `props.obj` | `src/halo/ai/props.c` | 19 | AI prop/scenery interaction, cover point search |
| `ai_communication.obj` | `src/halo/ai/ai_communication.c` | 13 | Combat dialogue triggers, squad vocal banter, grunt panic sounds |
| `path_smoothing.obj` | `src/halo/ai/path_smoothing.c` | 6 | Catmull-Rom spline path smoothing around obstacles |
| `encounters.obj` | `src/halo/ai/encounters.c` | 5 | Squad encounter lifecycle, spawn waves, reinforcement triggers |
| `actor_combat.obj` | `src/halo/ai/actor_combat.c` | 6 | Weapon firing patterns, burst timing, grenade evasion decisions |
| `ai.obj` | `src/halo/ai/ai.c` | 6 | Master AI global update tick and actor scheduling |

---

## 8. Campaign 5.5: Cache, Sound & Physics Foundations (147 Functions)

Completes tag/cache file I/O, DirectSound3D mixer, and collision detection primitives.

| Subsystem | Key Files | Unported | Description |
|:---|:---|:---:|:---|
| **Cache & Tags** | `cache_files_windows.c`, `tags.c`, `xbox_texture_cache.c` | 56 | Fast tag header lookup, virtual texture page fault streaming, audio buffer paging |
| **Sound** | `sound_manager.c`, `sound_dsound_xbox.c`, `game_sound.c` | 44 | DirectSound hardware channel allocation, 3D panning, reverb environment DSP |
| **Physics** | `collision_bsp.c`, `collision_features.c`, `point_physics.c` | 47 | BSP surface intersection, pill-box testing, water/plasma splash physics |

---

## 9. Campaign 5.6: Rasterizer & Direct3D Graphics Pipeline (305 Functions)

The largest remaining subsystem: native NV2A command buffer generation, shader setup, and visibility.

| Object | File | Unported | Description |
|:---|:---|:---:|:---|
| `rasterizer.obj` | `src/halo/rasterizer/rasterizer.c` | 75 | Frame preparation, scene clear, viewport setup, render loop dispatch |
| `rasterizer_decals.obj` | `src/halo/rasterizer/xbox/rasterizer_xbox_decals.c` | 40 | Decal quad generation, surface projection, fadeout lifecycle |
| `rasterizer_xbox.obj` | `src/halo/rasterizer/xbox/rasterizer_xbox.c` | 36 | Hardware state manager, blend modes, texture stage configuration |
| `rasterizer_xbox_environment_fog.obj` | `src/halo/rasterizer/xbox/rasterizer_xbox_environment_fog.c` | 27 | Volumetric and distance fog vertex calculation |
| `rasterizer_xbox_hardware_bitmaps.obj` | `src/halo/rasterizer/xbox/rasterizer_xbox_hardware_bitmaps.c` | 26 | NV2A swizzled surface allocation, paletted texture upload |
| `rasterizer_xbox_lights.obj` | `src/halo/rasterizer/xbox/rasterizer_xbox_lights.c` | 22 | Dynamic point light attenuation and specular falloff setup |
| `rasterizer_xbox_models.obj` | `src/halo/rasterizer/xbox/rasterizer_xbox_models.c` | 19 | Model mesh drawing, matrix palette skinning |
| `rasterizer_sprites.obj` | `src/halo/rasterizer/rasterizer_sprites.c` | 18 | Particle sprite billboard orientation and vertex streaming |

---

## 10. Campaign 5.7: LibTIFF & Bitmaps (146 Functions)

- **`bitmaps/libtiff/` (116 functions):** Vendored standard LibTIFF 3.5.x (`tif_write.c` [83], `tif_dir.c` [33], `tif_open.c`, `tif_flush.c`). Can be recovered directly by matching pristine LibTIFF upstream releases or lifting.
- **`bitmaps/` (30 functions):** Engine texture conversion and mipmap generation (`bitmaps.c` [16], `bitmap_utilities.c` [6]).

---

## 11. Campaign 5.8: Standalone Link & Trampoline Retirement

The capstone of Phase 5:
1. **Zero Game Trampolines:** All 7,549 game `.text` functions running natively in C.
2. **CRT & XDK Decoupling:** Replacing binary CRT stubs (`LIBCMT`, `XAPILIB`) with clean, period-accurate C implementations.
3. **Direct Executable Output:** Generating `default.xbe` directly via `lld-link` without `patch.py` injecting detours into `cachebeta.xbe`.

---

## 12. C89 Rules & Compiler Provenance Guidelines

Halo: Combat Evolved (Xbox) was compiled with **MSVC 7.1** (Visual Studio .NET 2003). All lifted source must strictly adhere to project rules:

1. **Strict C89 Scope Declarations:**
   - Declare all local variables at the very top of their enclosing block scope before any executable statements. No mid-block declarations (C99 forbidden).
2. **Authentic Engine Types:**
   - Use types from `src/types.h`: `real`, `boolean`, `int8_t`, `int16_t`, `int32_t`, `uint32_t`, `vector3_t`, `point2d`, `rectangle2d`. Never substitute standard `float` or `bool`.
3. **FPU Stack Discipline:**
   - Target is x86 Pentium III Coppermine (Xbox NV2A GPU).
   - Floating-point calculations evaluate on the x87 ST(0) stack. Do NOT emit SSE2 code.
4. **Never Transcribe Compiler Intrinsics as Calls:**
   - Never transcribe `_ftol2`, `_chkstk`, `__SEH_prolog`, `_allmul` as C calls. Use natural C casts `(int)val` and operators; MSVC/Clang lowers them automatically.
5. **Preserve Struct Offsets:**
   - Check `src/types.h` for known field offsets. Never guess a struct offset. Use `field_<hex>` for accessed unknown fields and `pad_<hex>[n]` for untouched space.
6. **Register ABI Immutability:**
   - Annotated register arguments (`@<reg>`) in `tools/kb_reg_baseline.json` are immutable. When declaring or porting functions with register inputs, preserve the exact register bindings.
7. **Token Discipline & Decompiled Corpus Access (Strict Rule):**
   - **Never read the same file twice in one task.** Track files already read.
   - **For `kb.json`:** `rtk jq` ONLY (0 lines direct view).
   - **For `halo_decompiled/`:** **MANDATORY use of `tools/extract_fn.py`**. Never read `cachebeta.elf.c` (25MB), `cachebeta.elf.asm` (60MB), or `corpus_metadata.json` (1MB) directly into context. Query them via `rtk python3 tools/extract_fn.py <address_or_symbol_or_keyword>`.
   - **For source files >300 lines:** use range reads capped at 100 lines.

---

## 13. Mandatory Verification Ladder & Quality Gates

Before committing any function in Phase 5, execute every step of the verification ladder:

1. **Pre-edit Caller & Behavioral Research:**
   - **Symbol & Text Search:** Locate existing callers, prototypes, and data structures:
     ```bash
     rtk rg '<function_name>' src/
     ```
   - **Decompiled Corpus Lookup via `tools/extract_fn.py` (Mandatory):** Pull up the authoritative decompiled implementation, symbol metadata, keywords, and sister functions:
     ```bash
     rtk python3 tools/extract_fn.py <address_or_symbol>
     rtk python3 tools/extract_fn.py "<keyword>"
     rtk python3 tools/extract_fn.py --info "<keyword>"
     rtk python3 tools/extract_fn.py --similar <address_or_symbol> -k 5
     ```
   - **Neural Behavioral Code Discovery (GPU):** When identifying unknown subroutines by natural language query:
     ```bash
     colab exec -s halo-gpu -- 'results = search_halo("<behavior_query>", top_k=5)'
     ```
2. **Decompilation & Disassembly Cross-Check:**
   - **Primary Extraction via `extract_fn.py`:** Pull up the full decompiled C source and x86 stack layout directly from `halo_decompiled/`:
     ```bash
     rtk python3 tools/extract_fn.py 0x<address> --asm
     ```
   - **Targeted Live Decompilation via Kuna:** When custom AST slicing or instruction options are required:
     ```bash
     kuna decompile build/cachebeta.elf 0x<address> --addr
     ```
3. **Disassembly Cross-Check:**
   - Verify jump tables, push-then-fstp sequences, argument order, and stack frame sizes against the raw binary.
4. **C89 Implementation:**
   - Implement in the authentic owning file (`src/halo/...`).
   - Set `"ported": true` in `kb.json`.
5. **Header Regeneration:**
   ```bash
   python3 tools/analysis/knowledge.py --gen-header build/generated/decl.h
   ```
6. **Register ABI Audit:**
   ```bash
   rtk python3 tools/audit/extract_reg_args.py --check
   ```
   *Gate:* Must report `0 drift, 0 missing, 0 stale`.
7. **Parameter & Return Type Audit:**
   ```bash
   rtk python3 tools/audit/check_param_types.py --check
   ```
   *Gate:* Must report `PASS: no new type mismatches`.
8. **Lift Hazard Scan:**
   ```bash
   rtk python3 tools/audit/check_lift_hazards.py --changed-only
   ```
   *Gate:* Zero new warnings on modified lines.
9. **XCALL Type Audit:**
   ```bash
   rtk python3 tools/audit/check_xcall_types.py
   ```
   *Gate:* Zero errors.
10. **Full Build & Patch Verification:**
    ```bash
    rtk python3 tools/build/build.py -q --target patched_xbe
    ```
    *Gate:* Must exit code 0 and produce valid `halo-patched/default.xbe`.
11. **Documentation Report:**
    - Produce detailed Markdown report in `docs/` summarizing recovered functions, byte verification, and PR links.

---

## 14. Immediate Starting Task: Batch 5.1B (UI Widget Hierarchy & Data Binding)

With Campaign 5.2 (Game Engine, Session & Saved Games) **100% complete** and Batch 5.1A / 5.1C **100% complete**, the immediate active frontier is **Batch 5.1B** to bring the entire Campaign 5.1 (Interface & UI) to 100%:

1. **Target Translation Units (21 Functions Remaining):**
   - **`src/halo/interface/ui_widget.c` (16 unported):** Widget tree traversal, bounds calculation, widget focus management.
   - **`src/halo/interface/ui_widget_game_data_input_functions.c` (5 unported):** UI data binding adapter callbacks.
2. **Workflow via `tools/extract_fn.py`:**
   ```bash
   # Inspect remaining unported symbols and signatures
   rtk python3 tools/extract_fn.py --info "ui_widget"
   rtk python3 tools/extract_fn.py --info "game_data_input"

   # Extract full decompiled C and x86 stack layout for each target
   rtk python3 tools/extract_fn.py <symbol_or_address> --asm
   ```
3. **Execute Full Verification Ladder:**
   - Pass all 10 quality gates cleanly (Register ABI, Type Audit, Lift Hazards, XCALL types, and full XBE build).
4. **Document & Track:**
   - Create verification report in `docs/batch-5.1b-ui-widgets-report.md`.
   - Once Batch 5.1B is closed, Campaign 5.1 is 100% complete, opening Campaign 5.3 (World Simulation).

