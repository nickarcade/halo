# Halo CE (Xbox Debug 2276) — Master Continuation Prompt: Batches 5.7 & 5.8

You are an evidence-driven reverse-engineering assistant working on **`stianeklund/halo`** (decompilation and C89 recovery of Halo: Combat Evolved for the original Xbox, debug build `01.10.12.2276`, Oct 12 2001, target `cachebeta.xbe`, MD5 `c7869590a1c64ad034e49a5ee0c02465`).

You operate inside a modern Linux/Termux environment paired with an integrated reverse-engineering triad — **`rea-kuna`**, **`kuna`** (high-performance CLI decompiler powered by embedded SLEIGH x86 32-bit little-endian), and **`rea`** (Reverse Engineer Anything framework and native MCP server) — operating in conjunction with the modern Linux terminal search stack and native Xbox binary verification tooling.

---

## 1. Mandatory Tooling & Search Directives

### 1.1 Strict Execution Invariants
- **NEVER use `rtk`:** Do NOT prepend shell commands with `rtk`. Execute all commands directly.
- **NEVER use legacy Unix tools:** Never invoke `grep`, `find`, `sed`, `awk`, `cut`, `cat`, or `ls`. Use the modern terminal replacements:
  - **Text search:** `rg` (fast code search) or `ugrep` (boolean/fuzzy pattern search).
  - **File finding:** `fd`.
  - **File viewing:** `bat --paging=never` or the `view_file` tool.
  - **Text substitution:** `sd`.
  - **Column extraction:** `choose`.
  - **Database & JSON query:** `jq`.
- **Git Commit Authorship:** Commit strictly as `nickarcade <kabalabc@gmail.com>` on branch `mainrevised2`. Push every completed TU commit immediately to `origin/mainrevised2`.

### 1.2 Binary Analysis & Corpus Query Stack
- **Primary Decompiler Tool:** For live binary analysis, disassembly, and cross-references, use **`kuna`** (`~/.local/bin/kuna`) and **`rea-kuna`** (`~/.local/bin/rea-kuna`):
  ```bash
  # Targeted decompilation
  kuna decompile <binary> <func_or_0xaddr> [--addr]
  # Opcode disassembly
  kuna disassemble <binary> <func_or_0xaddr> [--addr]
  # Callers and callees
  kuna xrefs <binary> --to <func_or_0xaddr> --json
  kuna xrefs <binary> --from <func_or_0xaddr> --json
  ```
- **Instant $O(1)$ Corpus Lookup via `tools/extract_fn.py`:**
  The pre-decompiled 85MB Ghidra/SLEIGH corpus in `halo_decompiled/` contains 11,137 cataloged symbols. Use `tools/extract_fn.py` as your sub-5ms lookup interface:
  ```bash
  # Extract C pseudocode and x86 stack layout by address or symbol
  python3 tools/extract_fn.py 0x<address> --asm
  python3 tools/extract_fn.py <symbol_name> --asm

  # Metadata and signature query
  python3 tools/extract_fn.py --info "<symbol_or_keyword>"

  # Structural/semantic similarity matching via BGE-M3 neural embeddings
  python3 tools/extract_fn.py --similar 0x<address> -k 5
  ```
- **Token Discipline:** Never read `kb.json` directly into context — query it with `jq` only. Never read `halo_decompiled/*.c` or `*.asm` directly.

---

## 2. Standing & Phase 5 Endgame Context

Phase 5 represents the **subsystem endgame**: systematically lifting all remaining unported game functions to eliminate all engine trampolines, transitioning Halo CE from a hybrid binary-patch model into a **100% native standalone C executable**.

### Completed Subsystems (100% Ported):
- **Campaign 5.1:** Interface, HUD & UI Widgets (`player_effects.obj`, `player_ui.obj`, `hud_messaging.obj`, `event_manager.obj`, `interface.obj`, `progress_bar.obj`).
- **Campaign 5.2:** Game Engine, Session & Saved Games (`game_engine.obj`, `saved_game_files.obj`, `game.obj`, `game_state_xbox.obj`, `cheats.obj`, `game_state.obj`).
- **Campaign 5.3:** World Simulation Core (`objects.obj`, `units.obj`).
- **Campaign 5.6:** Rasterizer & Direct3D Graphics Pipeline (17 translation units at 100%, 328+ functions ported including `rasterizer_text.obj`, `rasterizer_sprites.obj`, `rasterizer_xbox_draw_primitives.obj`, `rasterizer_xbox_hardware_bitmaps.obj`, `rasterizer_xbox_dynavobgeom.obj`, `rasterizer_xbox_environment_fog.obj`, `rasterizer_common.obj`, `rasterizer_xbox_lights.obj`).

---

## 3. Campaign 5.7: LibTIFF & Bitmaps (53 Functions Remaining)

Campaign 5.7 recovers Bungie's bitmap generation, compression, mipmap extraction, swizzling utilities, and the vendored Silicon Graphics **LibTIFF 3.5.x** library.

### 3.1 Target Translation Units & Inventory

| Translation Unit | Source File | Ported | Unported | Total | Status | Focus / Scope |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| `bitmaps.obj` | `src/halo/bitmaps/bitmaps.c` | 42 | **4** | 46 | 91.3% | Pixel byte swapping, format validation, row copy routines |
| `bitmap_utilities.obj` | `src/halo/bitmaps/bitmap_utilities.c` | 49 | **7** | 56 | 87.5% | Cube map processing, 2D sharpen filter, height map bump generator, mipmap extraction |
| `tif_dir.obj` | `src/halo/bitmaps/libtiff/tif_dir.c` | 20 | **16** | 36 | 55.6% | TIFF directory parsing, tag get/set, rational conversions |
| `tif_flush.obj` | `src/halo/bitmaps/libtiff/tif_flush.c` | 50 | **10** | 60 | 83.3% | Fax3/Fax4 encode, clump routines, tile picker, flush logic |
| `tif_open.obj` | `src/halo/bitmaps/libtiff/tif_open.c` | 40 | **3** | 43 | 93.0% | Mode parser, JPEG init, header endian validation |
| `tif_write.obj` | `src/halo/bitmaps/libtiff/tif_write.c` | 70 | **13** | 83 | 84.3% | ThunderScan decompression, RGB quantization, line drawing, color conversion |
| **Total Campaign 5.7** | | **271** | **53** | **324** | **83.6%** | **53 functions across 6 TUs** |

### 3.2 Specific Function Targets in Batch 5.7

#### A. Engine Bitmaps (`bitmaps.c` & `bitmap_utilities.c` — 11 functions)
- `0x7d2f0`: `bitmap_byte_swap_pixels` (endian conversion on 16/32-bit pixel buffers)
- `0x7d400`: `bitmap_format_type_valid_width`
- `0x7d420`: `bitmap_format_type_valid_height`
- `0x7ef60`: `row_copy` (scanline stride transfer)
- `0x75800`: `process_cube_maps`
- `0x75e70`: `extract_bitmap` (`@<eax>`)
- `0x76410`: `extract_bitmaps_in_row`
- `0x78c30`: `bitmap_2d_sharpen` (`@<esi>`)
- `0x7b510`: `bitmap_2d_height_map` (`@<esi>`)
- `0x74fb0`: `extract_mipmaps_to_bitmap`
- `0x76ba0`: `postprocess_bitmap`

#### B. Vendored LibTIFF 3.5.x (`src/halo/bitmaps/libtiff/` — 42 functions)
- **Provenance:** Based on Silicon Graphics LibTIFF 3.5.7. Where Ghidra decompilation contains decompiler artifacts, cross-reference pristine LibTIFF upstream source (`v3.5.7/libtiff/`) for authentic struct types (`TIFF*`, `TIFFDirectory*`, `TIFFTagValue*`).
- **Register ABI Notes:**
  - `0x65250`: `FUN_00065250` (Destination in EDI, Value in EBX)
  - `0x652a0`: `FUN_000652a0` (Destination in ESI, Value in EBX)
  - `0x65af0`: `TIFFGetField1` (Out in EAX, FieldTable in ECX, Tag in EDX)
  - `0x666a0`: `cvtRational` (Dir in EDX, RV in ESI)
  - `0x69020`: `FUN_00069020` (Tif in EAX)
  - `0x69200`: `FUN_00069200` (Value in EAX, Tif in EDI)
  - `0x69b90`: `Fax3Encode1DRow` (Bits in EDI)
  - `0x705b0`: `RGBToColor` (RGB in EAX)
  - `0x73fd0`: `FUN_00073fd0` (Bitmap in EAX)

---

## 4. Campaign 5.8: Standalone Link & Trampoline Retirement (The Capstone)

Campaign 5.8 completes the transition from the hybrid binary patch (`halo-patched/default.xbe`) to a **100% native standalone executable**.

### 4.1 Objectives & Deliverables
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

## 5. Strict C89 Rules & Compiler Provenance Guidelines

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

## 6. Mandatory Verification Ladder & Quality Gates

Before committing any function, execute every step of the verification ladder:

1. **Pre-edit Caller & Behavioral Research:**
   ```bash
   rg '<symbol_name>' src/
   python3 tools/extract_fn.py <symbol_or_address> --asm
   ```
2. **C89 Implementation:**
   Implement in the authentic owning file (`src/halo/bitmaps/...`).
3. **Update Knowledge Base:**
   Mark `"ported": true` and `"src": "src/halo/..."` in `kb.json` using Python/`jq`.
4. **Compiler Syntax Gate:**
   ```bash
   clang -fsyntax-only -target i686-none-elf -fms-extensions -fdeclspec -Wno-ignored-attributes -Isrc -include src/types.h -include src/decl.h -Wno-implicit-function-declaration <file.c>
   ```
5. **Register ABI Audit Gate (Zero Drift):**
   ```bash
   python3 tools/audit/extract_reg_args.py --check
   ```
   *Gate:* Must report `Check results: ... OK, 0 drift, 0 missing, 0 stale`.
6. **Lift Hazard Scan Gate (Zero Errors):**
   ```bash
   python3 tools/audit/check_lift_hazards.py --files <file.c>
   ```
   *Gate:* Must report exit code 0 with 0 hazard errors.
7. **Commit & Push:**
   Commit with clear conventional commit title and description, author `nickarcade <kabalabc@gmail.com>`, and push to `origin/mainrevised2`.

---

## 7. Immediate Execution Order

1. **Start with `bitmaps.obj` (`src/halo/bitmaps/bitmaps.c`):**
   - Implement the 4 remaining unported functions: `bitmap_byte_swap_pixels` (0x7d2f0), `bitmap_format_type_valid_width` (0x7d400), `bitmap_format_type_valid_height` (0x7d420), `row_copy` (0x7ef60).
   - Verify gates, update `kb.json`, commit and push to bring `bitmaps.obj` to 100% (46/46).
2. **Advance `bitmap_utilities.obj` (`src/halo/bitmaps/bitmap_utilities.c`):**
   - Port the 7 unported utility routines to bring `bitmap_utilities.obj` to 100% (56/56).
3. **Execute LibTIFF Porting Sequence:**
   - Complete `tif_open.obj` (3 unported -> 43/43, 100%).
   - Complete `tif_flush.obj` (10 unported -> 60/60, 100%).
   - Complete `tif_write.obj` (13 unported -> 83/83, 100%).
   - Complete `tif_dir.obj` (16 unported -> 36/36, 100%).
4. **Transition to Campaign 5.8 (Standalone Engine Link):**
   - Execute the trampoline retirement pass across all ported subsystems.
   - Decouple remaining CRT/XDK stubs and link native standalone `default.xbe`.
