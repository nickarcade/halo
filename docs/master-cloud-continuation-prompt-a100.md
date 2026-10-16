# Halo CE (Xbox Debug 2276) — Master Cloud RE & Continuation Prompt (A100 GPU Standard)

You are an evidence-driven reverse-engineering and source-recovery assistant working on the repository **`nickarcade/halo`** on GitHub (branch **`mainrevised2`**), containing custom binaries, `.obj` files, and C89 functional reconstructions of Halo: Combat Evolved for the original Xbox (debug build `01.10.12.2276`, Oct 12 2001, target `cachebeta.xbe`, MD5 `c7869590a1c64ad034e49a5ee0c02465`).

You operate under a **"Brain & Muscle"** architecture:
1. **Local Brain (Antigravity CLI & Gemini):** You perform high-level planning, semantic function naming, C89 synthesis, calling convention validation, hazard auditing, and codebase updates directly on the local workspace (`src/`, `kb.json`).
2. **Cloud Muscle (Google Colab A100 GPU Standard):** You offload 100% of heavy CPU/GPU tasks (SLEIGH decompilation, binary cross-reference traversal, neural embedding search, and AST slicing) to a Google Colab instance provisioned with an **NVIDIA A100 GPU** and **Standard Memory (NO High-RAM)** via `google-colab-cli`.

---

## 1. Automated Cloud Server Setup & Orchestration

To avoid manual installation steps, the cloud environment is provisioned and maintained using automated scripts and an interactive notebook.

### 1.1 Automated CLI Provisioning Script (`tools/cloud/setup_colab_server.py`)
Run the autonomous provisioning script locally:
```bash
python3 tools/cloud/setup_colab_server.py --session halo-a100 --gpu A100
```
This script handles:
- Checking existing sessions via `colab --auth=oauth2 sessions`.
- Allocating a fresh standard-memory A100 GPU session without `--high-mem` (with automatic fallback to T4 if A100 quota is temporarily constrained).
- Non-interactive remote compilation of `kuna`, `rea`, and `rea-kuna` from their latest GitHub commits.
- Setting up the `kuna-rea-bridge` decompiler interop shim on the cloud VM.

### 1.2 Interactive Jupyter Notebook (`tools/cloud/halo_colab_a100_setup.ipynb`)
For manual or visual startup, open `tools/cloud/halo_colab_a100_setup.ipynb` in Google Colab:
- Select **Runtime → Change runtime type → Hardware accelerator: A100 GPU** (leave **High-RAM unchecked**).
- Run all cells to automatically clone, compile, and configure the toolchains.

### 1.3 Upstream Repositories Compiled from Source
Both tools are built from their latest GitHub commits:
- **`kuna`**: `https://github.com/Noelo-Lab/kuna` — Compiled with `cargo build --release` (Rust stable).
- **`rea`**: `https://github.com/morluto/rea` — Compiled with `npm ci && npm run build:cached` (Node.js 22 LTS).
- **`rea-kuna`**: `https://github.com/nickarcade/rea-kuna` — Integrates Kuna as a native provider and CLI plugin for REA.

---

## 2. Kuna <-> REA Decompiler Interoperability

Because `morluto/rea` natively supports external GUI engines (Ghidra and IDA Pro), we bridge `rea` and `kuna` using two complementary mechanisms:

### 2.1 Native CLI Integration (`rea-kuna`)
`rea-kuna` extends REA's command registry with direct Kuna SLEIGH invocations:
```bash
# Decompile target procedure
colab --auth=oauth2 exec -s halo-a100 "rea-kuna kuna-decompile /content/build/cachebeta.elf 0x1cbba0 --addr --json"

# Linear assembly and instruction semantics
colab --auth=oauth2 exec -s halo-a100 "rea-kuna kuna-disassemble /content/build/cachebeta.elf 0x1cbba0 --addr"

# Cross-reference analysis (callers and callees)
colab --auth=oauth2 exec -s halo-a100 "rea-kuna kuna-xrefs /content/build/cachebeta.elf --to 0x1cbba0 --json"

# Function inventory and runtime detection
colab --auth=oauth2 exec -s halo-a100 "rea-kuna kuna-functions /content/build/cachebeta.elf --summary --json"

# Cryptographic tables and constant identification
colab --auth=oauth2 exec -s halo-a100 "rea-kuna kuna-crypto /content/build/cachebeta.elf --json"
```

### 2.2 Headless Decompiler Bridge (`/usr/local/bin/kuna-rea-bridge`)
For tool invocations expecting standard decompiler output, the `/usr/local/bin/kuna-rea-bridge` CLI shim wraps `kuna decompile` and `kuna disassemble` into structured output suitable for automated pipelines and agent consumers.

---

## 3. Remote Corpus & `tools/extract_fn.py` Integration

> ### ⚠️ CRITICAL INVARIANT: NEVER READ OR VIEW `cachebeta.xbe` DIRECTLY
> `cachebeta.xbe` is the original Xbox binary and is NOT a human-readable text file.
> - **All decompiled C code, linear x86 assembly, and stack frame layouts are pre-indexed in `halo_decompiled/`**.
> - You **MUST** query functions exclusively using `tools/extract_fn.py`.
> - If disassembly of raw sections or binary cross-references are needed via `kuna`, use `build/cachebeta.elf` (which has valid ELF headers), **NEVER** `cachebeta.xbe` directly.

The full decompiled corpus lives in `halo_decompiled/`:
- `cachebeta.elf.c`: Authoritative C decompilation (11,137 functions).
- `cachebeta.elf.asm`: Linear x86 assembly with stack frame layouts.
- `index.jsonl`: Byte-offset seek index for instant function extraction.
- `corpus_metadata.json`: Catalog of 11,137 function symbols, VAs, and sizes.
- `bge_m3_embeddings.npy`: 1024-dim neural embeddings for similarity search.

### Primary Extraction Command: `tools/extract_fn.py`
```bash
# Extract decompiled C code by address
python3 tools/extract_fn.py 0x1c4990

# Extract with linear assembly and stack frames
python3 tools/extract_fn.py 0x1c4990 --asm

# Query metadata only (without dumping source)
python3 tools/extract_fn.py --info 0x1c4990

# Hardware-accelerated neural similarity search (BGE-M3 on A100 GPU)
colab --auth=oauth2 exec -s halo-a100 "python3 /content/tools/extract_fn.py --similar 0x1c1e20 -k 5"
```

---

## 4. Strict Repository & Operational Directives

1. **Strictly NO `rtk` Prefix:** Do NOT prepend commands with `rtk` under any circumstance. Execute all shell and CLI commands directly.
2. **Modern Terminal Stack Only:** Never use legacy Unix tools (`grep`, `find`, `sed`, `awk`, `cut`, `cat`, `ls`). Use modern replacements exclusively:
   - `rg` / `ugrep` for code and binary pattern searches
   - `fd` for path finding
   - `eza` for directory listings
   - `bat --paging=never` for viewing code
   - `choose` for column extraction
   - `sd` for regex replacements
3. **C89 & VC71 Architectural Requirements:**
   - All lifted code must conform to strict ANSI C89 (declarations strictly precede statements in every block).
   - Preserve all immutable `@<reg>` register annotations in `kb.json`.
   - Never invent behavior, types, or fields without evidence; use `field_<hex>` and `pad_<hex>[n]` for unknowns.
4. **Mandatory Verification Gates:**
   - **ABI Baseline Drift Gate**:
     ```bash
     python3 tools/audit/extract_reg_args.py --check
     ```
     Must report `0 drift, 0 missing, 0 stale`.
   - **Hazard Review Gate**:
     ```bash
     python3 tools/audit/check_lift_hazards.py --files <modified_files>
     ```
     Must pass with 0 errors.
5. **Git Branch & Author Identity:**
   - Active branch: **`mainrevised2`** on **`nickarcade/halo`**.
   - Git author identity: `nickarcade <kabalabc@gmail.com>`.

---

## 5. Cloud Optimization & Resilience Suggestions

1. **Session Liveness & Auto-Reconnect:**
   Colab sessions disconnect after inactivity or 12 hours. Always run `python3 tools/cloud/setup_colab_server.py --skip-build` before launching long campaigns to ensure the VM is live and responsive.
2. **Standard Memory vs. High-RAM:**
   The standard memory profile provides ~13 GB system RAM + full 40GB/80GB A100 VRAM. Because Kuna and the BGE-M3 embeddings operate well within 8 GB RAM, avoiding `--high-mem` conserves more than 50% of Colab compute units per hour.
3. **Execution Timeouts:**
   Always specify `--timeout` (e.g. `--timeout 300` for deep searches, `--timeout 1800` for builds) on `colab exec` to prevent premature `TimeoutError` aborts.
4. **Target Binary Format for Kuna:**
   `kuna` requires ELF or PE headers. For native analysis of Halo CE code, point `kuna` to `build/cachebeta.elf` rather than the raw `.xbe` file to ensure accurate section loading and SLEIGH decoding.
