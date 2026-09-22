# Build and Verification Rules

Load this module for source edits, builds, tests, hazard reviews, VC71 work,
equivalence, runtime checks, or deployment.

## Standard Build and Lift Pipeline

- Toolchain: clang, lld-link, cmake, and python3. Bootstrap details live in skill
  `tool-reference`.
- Configure with
  `rtk cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=toolchains/llvm.cmake`.
- Normal build: `rtk python3 tools/build/build.py -q --target halo`.
- Post-lift pipeline:
  `rtk python3 tools/lift_pipeline.py --target <name_or_addr> --no-metadata-update --verify-policy auto`.
- `/lift` and `/auto-lift` trigger `/build` automatically through
  `.claude/settings.json` hooks.
- xemu must use 128 MiB system memory for debug build 2276.
- Prefer real Xbox XBDM verification over xemu when available.
- Run the narrowest meaningful validation first.

## Mandatory Audits

- After source edits, run
  `rtk python3 tools/audit/check_lift_hazards.py --changed-only` for touched
  files. Intrinsic calls, undersized buffers, duplicate arguments,
  pointer-as-float, and surviving CONCAT constructs block completion. Review all
  warnings in touched files.
- After XCALL macro changes, run
  `rtk python3 tools/audit/check_xcall_types.py`.
- After a new lift or `kb.json` declaration change, run
  `rtk python3 tools/audit/check_param_types.py --check`. New float/integer,
  return-width, or byte-return errors block completion.
- Every new section in `docs/lift-learnings.md` must ship in the same commit
  with a mechanical detector wired into the relevant audit/decompiler tool, or
  state `Automation: not mechanically detectable because ...`.

## Runtime Evidence

- Golden master harness: engine-context original-vs-lift tests in
  `src/halo/test_harness.c`, shell integration in `src/halo/shell_xbox.c`, and
  driver `tools/verify/run_golden_tests.py`.
- For high-value stateful targets, prefer same-process dual-oracle tests on
  cloned inputs over two emulator runs.
- For state replay, use virtual-memory capture via `memsave_snapshot.py` or
  `qmp_capture.py`, then `unicorn_diff.py --state-snapshot <path>` or
  `--from-halorec`. Never use physical `pmemsave` or QEMU savevm/loadvm for
  oracle tests. Verify every capture against a known global first.
- Detailed runtime procedures live in skills `halo-verify-debug` and
  `debug-xemu`.

## Build Failure Triage

For undeclared `FUN_XXXXXXXX` or wrong argument count, do not read source first:

```bash
rtk rg "FUN_XXXXXXXX" build/generated/decl.h
rtk jq '[.. | objects | select(.addr? == "0xXXXXX")] | .[0] | {name, decl}' kb.json
```

If generated declaration is absent, update the renamed call site. Read source
only if the signature itself is wrong.

For `ported=true` symbol absent from EXE exports, first run:

```bash
rtk rg "FUN_XXXXXXXX" src/
```

- Implementation found: fix compile errors or missing `src/CMakeLists.txt`
  registration. Do not set `ported=false`.
- No implementation found: set `ported=false` until a proper lift exists.

## VC71 Verification

- Before VC71 verification, confirm the target address exists in
  `tools/verify/function_bounds.json` with
  `rtk jq '."0x<addr>"' tools/verify/function_bounds.json`. If absent, run
  `rtk python3 tools/verify/function_bounds.py` and commit the regenerated table.
- For FPU-heavy lifts, run
  `rtk python3 tools/verify/vc71_verify.py src/path/to/file.c` and investigate
  every FPU, load-width, immediate, and shape warning.
- A call-count delta in a reference shorter than about 12 instructions usually
  means a dropped call. Decode reference E8/E9 targets first.
- A run scoring zero functions in a translation unit is a compile failure until
  proven otherwise. Read the first `cl.exe` diagnostic; stale `decl.h` is a
  common cause.
- A gate regression without matching source change is a harness bug until
  reproduced. Parallel VC71 workers must share one generated `decl.h` and use
  `--skip-decl-regen`. Run the gate twice and compare failure lists; use
  `VC71_NO_MEASURE_MEMO=1` to distinguish measurement errors from stale memo.

## Score Improvement and Equivalence

- Before score work, record `score_improve.py baseline` for the whole file.
- Test one binary-backed hypothesis at a time with `score_improve.py check`.
  Keep only gains of at least 0.01 percentage points that do not regress another
  function, drop a score, or add warnings.
- Use permuter only for VC71 scores in `[85, 98]%`. Below 85%, investigate
  structure; above 98%, further permutation is not worth the cost. Re-run the
  lift pipeline after accepted permutations.
- Equivalence decision: at least 99% means done; `[85, 98]%` with a delinked
  reference means permute first; pure-leaf FPU-heavy work in that range may use
  equivalence first; below 85% requires lift investigation; structurally capped
  lifts near 55% require equivalence evidence.
- Delinked COFF is required for equivalence and objdiff, not VC71 scoring.
- Detailed flags and confidence tiers live in `halo-verify-debug`; score recipes
  live in `lift-score-improve`.

## Failure Policy

If an edit fails, reread only the affected range before retrying. Never use a
broad reread as verification.
