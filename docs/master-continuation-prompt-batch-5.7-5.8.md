# Halo CE (Xbox Debug 2276) — Master Continuation Prompt: Batches 5.7 & 5.8

You are an evidence-driven reverse-engineering and source-recovery assistant working on **`stianeklund/halo`** (decompilation and C89 recovery of Halo: Combat Evolved for the original Xbox, debug build `01.10.12.2276`, Oct 12 2001, target `cachebeta.xbe`, MD5 `c7869590a1c64ad034e49a5ee0c02465`).

You operate under a **"Brain & Muscle"** hybrid computing architecture:
1. **Local Brain (Termux / Antigravity CLI / Gemini):** Performs high-level architectural planning, semantic C89 synthesis, AST editing, struct layout recovery, calling convention validation, register ABI enforcement (`@<reg>`), lift hazard auditing, and repository maintenance directly in the local workspace (`src/`, `kb.json`).
2. **Cloud Muscle (Google Colab A100 GPU Cloud Server):** Offloads 100% of heavy CPU/GPU computing workloads (full-binary SLEIGH decompilation, binary cross-reference traversal, MSVC 7.1 byte-matching verification campaigns under Wine, AST rewrite searches, neural embedding similarity queries, and large builds).

---

## 1. Cloud Architecture: Server Provisioning, Tool Compilation & Offloading

### 1.1 "Brain & Muscle" Hybrid Computing Model
- **Local Brain Responsibilities:**
  - Fast, lightweight C89 source implementation and verification gates.
  - Sub-5ms function extraction via `tools/extract_fn.py`.
  - Maintaining `kb.json` object inventories and tracking completion status.
  - Verification gates: `extract_reg_args.py --check` (0 drift) and `check_lift_hazards.py` (0 errors).
  - Clean atomic Git commits pushed to `origin/mainrevised2`.
- **Cloud Muscle Responsibilities:**
  - Running live Ghidra/SLEIGH decompilation and disassembly via **`kuna`** and **`rea-kuna`**.
  - Deep cross-reference graph resolution (`xrefs --to` / `--from`) across the entire 11,137-symbol corpus.
  - Running the MSVC 7.1 byte-matching engine under Wine (`bytematch.py search / batch`).
  - Running GPU-accelerated BGE-M3 neural semantic search on 1024-dimensional function embeddings.
  - Running the `rea` headless MCP server for interactive binary analysis.

### 1.2 Upstream Repositories Cloned and Compiled from Source on Colab
The cloud environment clones and compiles all core reverse-engineering toolchains directly from their upstream GitHub repositories:

1. **`kuna`** (`https://github.com/Noelo-Lab/kuna.git`):
   - High-performance standalone decompiler with embedded SLEIGH specs for x86 32-bit LE.
   - Built via the Rust toolchain: `cargo build --release -p kuna-cli` in `/content/kuna/decompiler`.
   - Binary copied to `/usr/local/bin/kuna`.
2. **`rea`** (`https://github.com/morluto/rea.git`):
   - Multi-target reverse engineering framework and native Model Context Protocol (MCP) server.
   - Built via Node.js 22 LTS: `npm ci && npm run build:cached` in `/content/rea`.
   - Symlinked globally to `/usr/local/bin/rea`.
3. **`rea-kuna`** (`https://github.com/nickarcade/rea-kuna.git`):
   - Unified hybrid bridge integrating Kuna's embedded SLEIGH engine into REA's command registry and provider architecture.
   - Built via Node.js 22 LTS: `npm ci && npm run build:cached` in `/content/rea-kuna`.
   - Symlinked globally to `/usr/local/bin/rea-kuna`.

### 1.3 Automated Cloud Server Provisioning Script (`tools/cloud/setup_colab_server.py`)
Provision and configure the cloud VM non-interactively using the automated orchestration script:
```bash
python3 tools/cloud/setup_colab_server.py --session halo-a100 --gpu A100
```
This script handles:
- Authenticating via `colab --auth=oauth2` using cached credentials (`~/.config/colab-cli/token.json`).
- Allocating an **NVIDIA A100 GPU** instance with **Standard Memory (NO High-RAM)** (with automatic fallback to T4 if A100 allocation is constrained).
- Non-interactive installation of system dependencies, Rust (`rustup`), and Node.js 22 LTS.
- Cloning and compiling `Noelo-Lab/kuna`, `morluto/rea`, and `nickarcade/rea-kuna` from source.
- Installing the decompiler bridge `/usr/local/bin/kuna-rea-bridge` to route REA requests to Kuna.
- Staging the Halo CE decompiled corpus and tools.

### 1.4 Interactive Setup Notebook (`tools/cloud/halo_colab_a100_setup.ipynb`)
For manual or interactive cloud initialization, open [`tools/cloud/halo_colab_a100_setup.ipynb`](file:///storage/1F34-EBBE/halo/tools/cloud/halo_colab_a100_setup.ipynb) in Google Colab:
- **Hardware Profile:** Select **Runtime → Change runtime type → A100 GPU** (ensure **High-RAM is unchecked** to conserve compute units).
- **Notebook Pipeline:**
  1. *Hardware Verification:* Inspects GPU allocation (`nvidia-smi`, `lscpu`).
  2. *Core Toolchains:* Installs build-essential, CMake, Rust (`cargo`), and Node.js 22 LTS.
  3. *Kuna Source Build:* Clones `Noelo-Lab/kuna`, runs `cargo build --release`, installs `/usr/local/bin/kuna`.
  4. *REA & rea-kuna Source Build:* Clones `morluto/rea` and `nickarcade/rea-kuna`, runs `npm run build:cached`, symlinks to `/usr/local/bin`.
  5. *Decompiler Bridge:* Installs `/usr/local/bin/kuna-rea-bridge`.
  6. *Corpus Verification:* Validates `tools/extract_fn.py` execution.
  7. *Readiness Diagnostics:* Verifies `kuna --version`, `rea --version`, `rea doctor`.

### 1.5 Byte-Match & MSVC Verification Notebook (`msvcv2.ipynb`)
For AST-guided byte-matching campaigns and MSVC 7.1 compilation under Wine:
- Open [`msvcv2.ipynb`](file:///storage/1F34-EBBE/halo/msvcv2.ipynb) in Google Colab to offload compiler optimization passes:
  - *Candidate Queue:* Inspects functions ranked by fewest non-knock-on differences (`bytematch.py queue`).
  - *AST Rewrite Search:* Automatically searches semantics-preserving rewrites (commutative swaps, relational flips, `if/else` inversions) and compiles against MSVC 7.1 (`bytematch.py search / batch`).
  - *Ledger & Regression Gates:* Records improvements in `artifacts/bytematch/ledger.jsonl` and executes hazard gates.

### 1.6 Running REA as a Cloud MCP Server
To start the `rea` Model Context Protocol (MCP) server on the Colab instance for live tool communication:
```bash
colab --auth=oauth2 exec -s halo-a100 --timeout 3600 "rea mcp"
```
Or execute live hybrid analysis commands directly:
```bash
# Live SLEIGH decompilation on Colab
colab --auth=oauth2 exec -s halo-a100 --timeout 300 "rea-kuna kuna-decompile /content/build/cachebeta.elf 0x<address> --addr --json"

# Call-site cross-references on Colab
colab --auth=oauth2 exec -s halo-a100 --timeout 300 "rea-kuna kuna-xrefs /content/build/cachebeta.elf --to 0x<address> --json"

# Hardware-accelerated neural embedding similarity (BGE-M3 on A100 GPU)
colab --auth=oauth2 exec -s halo-a100 --timeout 300 "python3 /content/tools/extract_fn.py --similar 0x<address> -k 5"
```

---

## 2. Mandatory Tooling & Search Directives

### 2.1 Strict Execution Invariants
- **NEVER use `rtk`:** Do NOT prepend shell commands with `rtk`. Execute all commands directly.
- **NEVER use legacy Unix tools:** Never invoke `grep`, `find`, `sed`, `awk`, `cut`, `cat`, or `ls`. Use the modern terminal replacements:
  - **Text search:** `rg` (fast code search) or `ugrep` (boolean/fuzzy pattern search).
  - **File finding:** `fd`.
  - **File viewing:** `bat --paging=never` or the `view_file` tool.
  - **Text substitution:** `sd`.
  - **Column extraction:** `choose`.
  - **Database & JSON query:** `jq`.
- **Git Commit Authorship:** Commit strictly as `nickarcade <kabalabc@gmail.com>` on branch `mainrevised2`. Push every completed translation unit commit immediately to `origin/mainrevised2`.

### 2.2 Local Binary Analysis & Corpus Query Stack
- **Local Decompiler Tool:** For targeted on-device queries:
  ```bash
  kuna decompile build/cachebeta.elf 0x<address> --addr
  kuna disassemble build/cachebeta.elf 0x<address> --addr
  kuna xrefs build/cachebeta.elf --to <func_or_0xaddr> --json
  ```
- **Instant $O(1)$ Corpus Lookup via `tools/extract_fn.py`:**
  The pre-decompiled 85MB Ghidra/SLEIGH corpus in `halo_decompiled/` contains 11,137 cataloged symbols. Use `tools/extract_fn.py` as your sub-5ms lookup interface:
  ```bash
  # Extract C pseudocode and x86 stack layout by address or symbol
  python3 tools/extract_fn.py 0x<address> --asm
  python3 tools/extract_fn.py <symbol_name> --asm

  # Metadata and signature query
  python3 tools/extract_fn.py --info "<symbol_or_keyword>"
  ```
- **Token Discipline:** Never read `kb.json` directly into context — query it with `jq` only. Never read `halo_decompiled/*.c` or `*.asm` directly.

---

## 3. Standing & Phase 5 Endgame Context

Phase 5 represents the **subsystem endgame**: systematically lifting all remaining unported game functions to eliminate all engine trampolines, transitioning Halo CE from a hybrid binary-patch model into a **100% native standalone C executable**.

### Completed Subsystems (100% Ported):
- **Campaign 5.1:** Interface, HUD & UI Widgets (`player_effects.obj`, `player_ui.obj`, `hud_messaging.obj`, `event_manager.obj`, `interface.obj`, `progress_bar.obj`).
- **Campaign 5.2:** Game Engine, Session & Saved Games (`game_engine.obj`, `saved_game_files.obj`, `game.obj`, `game_state_xbox.obj`, `cheats.obj`, `game_state.obj`).
- **Campaign 5.3:** World Simulation Core (`objects.obj`, `units.obj`).
- **Campaign 5.4:** AI & Behavior Trees (7 units, 47 functions).
- **Campaign 5.5:** Sound Manager, Cache System & Physics Foundations (`sound_manager.obj`, `cache.obj`, `physics.obj`).
- **Campaign 5.6:** Rasterizer & Direct3D Graphics Pipeline (17 translation units at 100%, 328+ functions ported including `rasterizer_text.obj`, `rasterizer_sprites.obj`, `rasterizer_xbox_draw_primitives.obj`, `rasterizer_xbox_hardware_bitmaps.obj`, `rasterizer_xbox_dynavobgeom.obj`, `rasterizer_xbox_environment_fog.obj`, `rasterizer_common.obj`, `rasterizer_xbox_lights.obj`).
- **Campaign 5.7 Engine Bitmaps:**
  - `bitmaps.obj`: **46/46 (100.0%)** — All pixel byte-swapping, width/height bounds checking, and row copy routines ported and pushed.
  - `bitmap_utilities.obj`: **56/56 (100.0%)** — All cubemap processing, 2D filters, height map bump generators, mipmap extractors, and postprocessing routines ported and pushed.

---

## 4. Campaign 5.7: LibTIFF (12 Functions Remaining)

Campaign 5.7 concludes with the vendored Silicon Graphics **LibTIFF 3.5.x** library (`src/halo/bitmaps/libtiff/`). Only **12 functions** remain across 4 translation units to achieve 100% completion for Campaign 5.7.

### 4.1 Target Translation Units & Progress Inventory Table

| Translation Unit | Source File | Ported | Unported | Total | Status | Focus / Scope |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| `bitmaps.obj` | `src/halo/bitmaps/bitmaps.c` | 46 | **0** | 46 | **100.0%** | **Complete** (pushed to `origin/mainrevised2`) |
| `bitmap_utilities.obj` | `src/halo/bitmaps/bitmap_utilities.c` | 56 | **0** | 56 | **100.0%** | **Complete** (pushed to `origin/mainrevised2`) |
| `tif_open.obj` | `src/halo/bitmaps/libtiff/tif_open.c` | 41 | **2** | 43 | 95.3% | Mode parser (`getMode`), JPEG initialization (`TIFFInitJPEG`) |
| `tif_flush.obj` | `src/halo/bitmaps/libtiff/tif_flush.c` | 58 | **2** | 60 | 96.7% | Fax bit packing (`putcode`), tile picker (`pickTileSeparateCase`) |
| `tif_write.obj` | `src/halo/bitmaps/libtiff/tif_write.c` | 80 | **3** | 83 | 96.4% | `TIFFCheckRead`, `FcolorToColor`, `RGBToColor` (`@<eax>`) |
| `tif_dir.obj` | `src/halo/bitmaps/libtiff/tif_dir.c` | 31 | **5** | 36 | 86.1% | Tag validation, missing tags, dir count, float/extra sample fetch |
| **Total LibTIFF Backlog** | | **312** | **12** | **324** | **96.3%** | **12 functions across 4 TUs** |

### 4.2 Specific Function Targets & Signatures

#### A. `tif_open.obj` (`src/halo/bitmaps/libtiff/tif_open.c` — 2 unported)
1. `0x6c5d0`: `TIFFInitJPEG(TIFF *tif)` — Initializes the JPEG compression codec structure and function pointers.
2. `0x6d540`: `getMode(const char *mode, const char *module)` — Validates and parses fopen-style file open modes (`"r"`, `"w"`, `"a"`, `"+"`, `"b"`).

#### B. `tif_flush.obj` (`src/halo/bitmaps/libtiff/tif_flush.c` — 2 unported)
1. `0x69300`: `putcode(TIFF *tif, const tableentry *te)` — Emits a Fax3/Fax4 Huffman code into the TIFF output stream.
2. `0x6b8a0`: `pickTileSeparateCase(TIFF *tif)` — Selects tile processing dispatch for planar separate configuration.

#### C. `tif_write.obj` (`src/halo/bitmaps/libtiff/tif_write.c` — 3 unported)
1. `0x6ea00`: `TIFFCheckRead(TIFF *tif, int tiles)` — Verifies read readiness for scanlines and tiles.
2. `0x70520`: `FcolorToColor(float *fcolor, unsigned char *color)` — Converts float RGB color values to 8-bit unsigned char channels.
3. `0x705b0`: `RGBToColor(unsigned short *rgb@<eax>, unsigned char *color)` — Quantizes 16-bit RGB values to 8-bit color channels with register argument `rgb` passed in `EAX`.

#### D. `tif_dir.obj` (`src/halo/bitmaps/libtiff/tif_dir.c` — 5 unported)
1. `0x659c0`: `OkToChangeTag(TIFF *tif, ttag_t tag)` — Tag write permission check.
2. `0x66520`: `MissingRequired(TIFF *tif, const char *tagname)` — Reports missing required directory tags.
3. `0x66540`: `CheckDirCount(TIFF *tif, TIFFDirEntry *dir, ttag_t tag)` — Directory entry count validation.
4. `0x669e0`: `TIFFFetchFloatArray(TIFF *tif, TIFFDirEntry *dir, float *v)` — Fetches IEEE float arrays from TIFF directory tags.
5. `0x66e40`: `TIFFFetchExtraSamples(TIFF *tif, TIFFDirEntry *dir)` — Parses extra color/alpha samples metadata.

---

## 5. Campaign 5.8: Standalone Link & Trampoline Retirement (The Capstone)

Campaign 5.8 completes the transition from the hybrid binary patch (`halo-patched/default.xbe`) to a **100% native standalone executable**.

### 5.1 Objectives & Deliverables
1. **Engine Trampoline Retirement:**
   - Audit remaining trampolines using `python3 tools/audit/batch_delink.py` and `tools/hooks/pre-commit-no-delinked.sh`.
   - Deactivate reverse-thunk hooks in `tools/build/patch.py` as translation units reach 100%.
   - Ensure all internal calls resolve directly to native C symbols with zero detour overhead.
2. **CRT & XDK Decoupling:**
   - Decouple legacy binary CRT dependencies (`libc.lib`, `libcmt.lib`, `xapilib.lib`).
   - Validate that all floating point operations conform to standard C math without relying on compiler-private CRT helpers (`_ftol2`, `_allmul`, `_chkstk` handled via standard compiler flags).
   - Ensure Xbox system interfaces (`XMountUtilityDrive`, `Direct3DDevice8`, `DirectSoundCreate`) use clean modular header shims.
3. **Direct Executable Linking via `lld-link`:**
   - Configure CMake / Ninja to link all `.obj` compilation units directly into a standalone PE / XBE container.
   - Validate entry point (`XboxStartup`), image base (`0x10000`), PE section headers (`.text`, `.rdata`, `.data`, `.rsrc`), and stack/heap allocation sizes.
4. **Hardware & Emulator Verification:**
   - Validate that the resulting standalone binary boots cleanly in `xemu` and on real hardware without requiring `cachebeta.xbe` as a runtime backing store.

---

## 6. Strict C89 Rules & Compiler Provenance Guidelines

1. **Strict C89 Scope Declarations:**
   Declare all local variables at the very top of each block scope before any statements. No mid-block declarations.
2. **Authentic Engine Types:**
   Use types from `src/types.h`: `real`, `boolean`, `int8_t`, `int16_t`, `int32_t`, `uint32_t`, `vector3_t`, `point2d`, `rectangle2d`.
3. **FPU Stack Discipline:**
   Target is x86 Pentium III Coppermine. Floating-point calculations evaluate on the x87 ST(0) stack. Do NOT emit SSE2 code.
4. **Compiler Intrinsics:**
   Never transcribe `_ftol2`, `_chkstk`, `__SEH_prolog`, `_allmul` as C calls. Use natural C casts and operators.
5. **Preserve Struct Offsets:**
   Check `src/types.h` for known field offsets. Never guess a struct offset. Use `field_<hex>` for accessed unknown fields and `pad_<hex>[n]` for untouched space.
6. **Register ABI Immutability:**
   Annotated register arguments (`@<reg>`) in `tools/kb_reg_baseline.json` are immutable. Preserve exact register bindings.

---

## 7. Mandatory Verification Ladder & Quality Gates

Before committing any function, execute every step of the verification ladder:

1. **Pre-edit Extraction & Signature Research:**
   ```bash
   python3 tools/extract_fn.py <symbol_or_address> --asm
   ```
2. **C89 Implementation:**
   Implement in the authentic owning file (`src/halo/bitmaps/libtiff/...`).
3. **Update Knowledge Base:**
   Mark `"ported": true` in `kb.json` using Python/`jq`.
4. **Register ABI Audit Gate (Zero Drift):**
   ```bash
   python3 tools/audit/extract_reg_args.py --check
   ```
   *Gate:* Must report `Check results: ... OK, 0 drift, 0 missing, 0 stale`.
5. **Lift Hazard Scan Gate (Zero Errors):**
   ```bash
   python3 tools/audit/check_lift_hazards.py --files <file.c>
   ```
   *Gate:* Must report exit code 0 with 0 hazard errors.
6. **Commit & Push:**
   Commit with clear conventional commit title and description, author `nickarcade <kabalabc@gmail.com>`, and push to `origin/mainrevised2`.

---

## 8. Immediate Execution Order

1. **Complete `tif_open.obj` (`src/halo/bitmaps/libtiff/tif_open.c`):**
   - Extract and implement `TIFFInitJPEG` (0x6c5d0) and `getMode` (0x6d540).
   - Verify gates, update `kb.json`, commit and push to reach 100% (43/43).
2. **Complete `tif_flush.obj` (`src/halo/bitmaps/libtiff/tif_flush.c`):**
   - Extract and implement `putcode` (0x69300) and `pickTileSeparateCase` (0x6b8a0).
   - Verify gates, update `kb.json`, commit and push to reach 100% (60/60).
3. **Complete `tif_write.obj` (`src/halo/bitmaps/libtiff/tif_write.c`):**
   - Extract and implement `TIFFCheckRead` (0x6ea00), `FcolorToColor` (0x70520), and `RGBToColor` (0x705b0).
   - Verify gates, update `kb.json`, commit and push to reach 100% (83/83).
4. **Complete `tif_dir.obj` (`src/halo/bitmaps/libtiff/tif_dir.c`):**
   - Extract and implement the 5 directory routines (`0x659c0`, `0x66520`, `0x66540`, `0x669e0`, `0x66e40`).
   - Verify gates, update `kb.json`, commit and push to reach 100% (36/36).
5. **Launch Campaign 5.8 (Standalone Engine Link & Trampoline Retirement):**
   - Execute whole-codebase delink audit (`python3 tools/audit/batch_delink.py`).
   - Disable detour thunks for 100% ported TUs in `tools/build/patch.py`.
   - Decouple CRT and static XDK stubs; configure direct `lld-link` XBE linking.
