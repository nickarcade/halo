# Halo CE Xbox Decompilation — Compaction & Continuation Prompt

> **Prior Conversation Reference:** [Conversation bcc751a4-88f9-41a7-8b31-8a105a88efbd](conversation://bcc751a4-88f9-41a7-8b31-8a105a88efbd)  
> **Transcript Log:** `/data/data/com.termux/files/home/.gemini/antigravity-cli/brain/bcc751a4-88f9-41a7-8b31-8a105a88efbd/.system_generated/logs/transcript.jsonl`  
> **Repository:** `/storage/1F34-EBBE/halo`  
> **Branch:** `mainrevised2` (tracking `origin/mainrevised2`)  
> **Target Binary:** Halo Xbox debug build 2276 (`halo-patched/cachebeta.xbe`, MD5 `c7869590a1c64ad034e49a5ee0c02465`)  

---

## 1. Executive Summary & Milestones Achieved

1. **Upstream Sync Completed:** All 7 files from `stianeklund/halo` (`upstream/main`) were successfully integrated into branch `mainrevised2`:
   - `src/d3d8_states.h`
   - `src/shader_model.h`
   - `src/halo/math/real_math.h`
   - `src/halo/objects/widgets/light_volumes.c`
   - `src/halo/camera/first_person_camera.c`
   - `src/halo/game/aim_assist.c`
   - `src/halo/tag_files/files_windows.c`
2. **Duplicate Conflicts Resolved:**
   - `src/halo/tag_files/files.c`: Replaced duplicate file I/O functions with upstream delegation to `files_windows.c`.
   - `src/halo/game/cheats.c`: Pruned aim assist functions moved to `aim_assist.c`.
   - `src/halo/camera/director.c` & `src/halo/camera/observer.c`: Pruned camera functions moved to `first_person_camera.c`.
   - `src/types.h`: Backwards-compatible unions added for vector fields in `object_data_t` (`position`/`unk_12`, `forward`/`unk_36`, etc.) and `unit_data_t` (`desired_facing_vector`/`unk_468`, `throttle`/`unk_552`).
   - `src/halo/objects/objects.h`: Pruned duplicate struct definitions of `vehicle_data_t` and `real_rgb_color`.
3. **Knowledge Base (`kb.json`) & Headers Synchronized:**
   - Added `aim_assist.obj` (17 functions) and `first_person_camera.obj` (5 functions).
   - Synchronized `decl.h`, `thunks.c`, `halo.xbe.def`.
   - Updated `tools/kb_reg_baseline.json` via `extract_reg_args.py --apply` to register `0xa5920` (`object_compute_autoaim_target`).
4. **Complete Project Compilation & Linking Succeeded:**
   - All 209 translation units compiled cleanly on Google Colab (`msvc` session) using Pentium III clang/llvm toolchain.
   - Linked executable `/content/halo/build/halo` (2,490,368 bytes) and `/content/halo/build/halo.lib` (1,617,142 bytes) successfully built!
5. **Packaging Pipeline (`patched_xbe`):**
   - Passed all static audit gates:
     - `check_asm_thunk_conflicts.py` (PASS)
     - `check_raw_casts.py` (PASS)
     - `check_lift_hazards.py` (PASS)
     - `maintain.py --check` (PASS)
     - `kb_meta.py validate` (PASS)
     - `RegAnnotationBaselineTests` (7/7 tests OK)
     - Target binary MD5 verified (`c7869590a1c64ad034e49a5ee0c02465`)
     - EXE imports/exports rebased and patched into original XBE.

---

## 2. Tooling & Reverse Engineering Protocols

### A. Kuna Decompiler Guidelines & Usage
- **Binary Path:** `/data/data/com.termux/files/home/bin/kuna`
- **Skill Reference:** `skills/kuna-decompiler/SKILL.md` (`/data/data/com.termux/files/home/.gemini/config/skills/kuna-decompiler/SKILL.md`)
- **Policy:** Always use `kuna` for binary decompilation, disassembly, or reverse engineering tasks on binaries.
- **Workflow / Best Practices:**
  - When decompiling specific functions from large shared libraries or binaries, extract target function bytes into self-contained objects to allow `kuna` to decompile and lift cleanly at maximum speed.
  - **Decompile specific function by symbol or address:**
    ```bash
    kuna decompile ./binary function_name
    kuna decompile ./binary 0x401230 --addr --json
    ```
  - **Disassemble targeted instructions:**
    ```bash
    kuna disassemble ./binary 0x401230
    ```
  - **Batch decompilation:**
    ```bash
    kuna decompile-all ./binary --json
    ```
  - **Streaming project export:**
    ```bash
    kuna decompile-project ./binary --stream --jobs auto -o ./output.kuna
    ```
  - **Search strings and cross-references:**
    ```bash
    kuna strings ./binary --filter '(?i)pattern' --json
    kuna xrefs ./binary --from main --json
    ```

### B. `extract_fn.py` Corpus Search & Extraction
- **Script Path:** `tools/extract_fn.py` (inside `/storage/1F34-EBBE/halo`)
- **Corpus Data:** Integrates `halo_decompiled/index.jsonl`, `corpus_metadata.json` (11,137 symbols), `cachebeta.elf.c`, and `cachebeta.elf.asm`.
- **Primary Commands:**
  - **Extract function by VA address (supports hex formats):**
    ```bash
    rtk python3 tools/extract_fn.py 0x89240
    rtk python3 tools/extract_fn.py 0xb5210
    ```
  - **Extract with x86 disassembly and stack frames (`--asm`):**
    ```bash
    rtk python3 tools/extract_fn.py 0x89240 --asm
    ```
  - **Extract by exact symbol name:**
    ```bash
    rtk python3 tools/extract_fn.py first_person_camera_fake
    ```
  - **Search corpus by keyword:**
    ```bash
    rtk python3 tools/extract_fn.py -s "camera"
    rtk python3 tools/extract_fn.py "slayer"
    ```
  - **Find structurally/semantically similar functions via BGE-M3 neural embeddings:**
    ```bash
    rtk python3 tools/extract_fn.py --similar 0x89240 -k 5
    ```
  - **View metadata table without full code dump:**
    ```bash
    rtk python3 tools/extract_fn.py --info 0x89240
    ```
  - **Extract all matching functions when multiple exist:**
    ```bash
    rtk python3 tools/extract_fn.py --all "autoaim"
    ```

---

## 3. Current State & Remaining Work

The build stopped at the final export validation step in `tools/build/patch.py` due to exactly two missing export symbols from the binary:

1. **`first_person_camera_fake` (0x89240):**
   - In `src/halo/camera/first_person_camera.c`, this function was omitted during upstream file integration (lines 188-230 are blank).
   - Implementation:
     ```c
     /* Fake first-person camera update from unit facing direction (0x89240).
      * [TU: c:\halo\SOURCE\camera\first_person_camera.c -- __FILE__ assert xref] */
     void first_person_camera_fake(int32_t unit_index, void *result)
     {
       void *unit;

       unit = object_get_and_verify_type(unit_index, 3);
       first_person_camera_for_unit_and_vector(
         (float *)((char *)unit + 0x1ec), unit_index, result);
     }
     ```
2. **`slayer_engine_display_score` (0xb5210):**
   - In `src/halo/game/game.c:1606-1637`, this function is currently implemented under the name `slayer_player_update`.
   - `kb.json` lists address `0xb5210` under `game.obj` as `void slayer_engine_display_score(int player_index);`.
   - Rename `slayer_player_update` to `slayer_engine_display_score` in `src/halo/game/game.c`.

Once these two functions are exported:
1. `cmake --build /content/halo/build --target patched_xbe` will generate `/content/halo/halo-patched/default.xbe`.
2. Copy the resulting `default.xbe` to Google Drive storage as requested.

---

## 4. Standalone Copy-Paste Continuation Prompt

```markdown
Continue the Halo CE Xbox decompilation task on branch `mainrevised2` (following conversation bcc751a4-88f9-41a7-8b31-8a105a88efbd).

### References & Tools:
- Prior Conversation: [Conversation bcc751a4-88f9-41a7-8b31-8a105a88efbd](conversation://bcc751a4-88f9-41a7-8b31-8a105a88efbd)
- Kuna Reverse Engineering CLI: `/data/data/com.termux/files/home/bin/kuna` (Follow skill `kuna-decompiler` at `/data/data/com.termux/files/home/.gemini/config/skills/kuna-decompiler/SKILL.md`). For any binary reversing/decompilation, extract target function bytes into self-contained objects or run `kuna decompile <bin> <addr> --addr --json` / `kuna disassemble`.
- Halo Corpus Search Script: `rtk python3 tools/extract_fn.py <query>` (Use `--asm` for x86 assembly/stack layout, `-s` for keyword search, `--similar <addr> -k 5` for BGE-M3 embeddings).
- Knowledge Base: Never read `kb.json` directly; query it only with `rtk jq`. Prefix all shell commands with `rtk`.

### Current Status:
All 7 upstream files from `stianeklund/halo` have been merged and conflict-resolved. All 209 translation units compiled and linked cleanly into `/content/halo/build/halo` on Google Colab (session `msvc`).
The packaging target `patched_xbe` passed all audit gates and stopped at the final export check due to two missing symbol exports.

### Immediate Action Items:
1. In `src/halo/camera/first_person_camera.c`:
   Insert `first_person_camera_fake` (0x89240) at line 188:
   ```c
   /* Fake first-person camera update from unit facing direction (0x89240).
    * [TU: c:\halo\SOURCE\camera\first_person_camera.c -- __FILE__ assert xref] */
   void first_person_camera_fake(int32_t unit_index, void *result)
   {
     void *unit;

     unit = object_get_and_verify_type(unit_index, 3);
     first_person_camera_for_unit_and_vector(
       (float *)((char *)unit + 0x1ec), unit_index, result);
   }
   ```

2. In `src/halo/game/game.c` (line 1637):
   Rename `slayer_player_update` (0xb5210) to `slayer_engine_display_score` to match `kb.json`:
   ```c
   void slayer_engine_display_score(int player_index)
   ```

3. Commit and push:
   ```bash
   rtk git add src/halo/camera/first_person_camera.c src/halo/game/game.c docs/compaction_continuation_prompt.md
   rtk git commit -m "fix(build): add first_person_camera_fake and rename slayer_engine_display_score"
   rtk git push origin mainrevised2
   ```

4. Pull and run `patched_xbe` on Colab (`msvc` session):
   ```bash
   rtk echo 'import subprocess; print(subprocess.check_output("git pull", cwd="/content/halo", shell=True, text=True))' | rtk colab exec -s msvc
   rtk python3 -c '
   code = """
   import subprocess
   res = subprocess.run(["cmake", "--build", "/content/halo/build", "--target", "patched_xbe"], text=True, capture_output=True)
   print("RET:", res.returncode)
   print("STDOUT:\n", res.stdout[-4000:] if len(res.stdout) > 4000 else res.stdout)
   print("STDERR:\n", res.stderr[-4000:] if len(res.stderr) > 4000 else res.stderr)
   """
   print(code.strip())
   ' | rtk colab exec -s msvc
   ```

5. Deploy build to Google Drive:
   Verify generation of `/content/halo/halo-patched/default.xbe` and copy it to Google Drive storage on Colab.
```
