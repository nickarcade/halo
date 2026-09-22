# Byte-matching verification pilot — 2026-09-22

The first implementation separates encoding evidence from behavioral evidence.
No game implementation or kb.json declaration was changed by this work.

## Reporting changes

- A known different relocation target produces `structural differ`.
- An unknown target cannot produce `structural exact`. If the remaining bytes
  match, the result is `not comparable`, with an aligned-byte uncertainty range.
- A known different relocation operand contributes four fixed mismatch bytes;
  those bytes cannot increase the optimistic upper bound. This is an
  operand-identity penalty, not a literal comparison of an unlinked placeholder.
- Instructions with unknown or different relocation targets do not count as
  normalized-exact instructions.
- Each audit produces JSON and a Markdown report with five separate checks:
  original boundaries, compiler/settings, behavior tested by this audit,
  instruction/operand matching, and literal bytes.
- Static audits leave behavior untested. Existing independent behavioral
  evidence in the dashboard remains separate.
- Schema v1 structural artifacts are obsolete and must be regenerated. The
  dashboard and structural summary commands reject them.
- A failed header generation or compilation replaces a previous output with
  an unverified error report, preventing reuse of a stale success.

## Pilot results

All three functions are in `src/halo/math/real_math.c`. These are fresh VC71
comparison results, not proof that VC71 produced the original game. The original
compiler and flags remain unproven; see [compiler provenance](compiler-provenance.md).

| Target | Original range, end exclusive | Mnemonic match | Aligned bytes | Matching verdict |
|---|---|---:|---:|---|
| `FUN_0001ace0` — squared 2D distance | `0x1ace0..0x1ad01` (33 bytes) | 90.9% | 77.1% | Different |
| `valid_real_sine_cosine` — branching validity check | `0x10fe80..0x10fec5` (69 bytes) | 92.0% | 83.3–88.9% | Different; one unknown constant relocation |
| `component_vectors_from_normal3d` — wrapper with a call | `0x1094d0..0x1094fb` (43 bytes) | 91.4% | 55.8% | Different; call target resolves correctly |

The original instructions were inspected for these small spans. The arithmetic
and wrapper functions end at a single return. Both forward branches in the
validity check lead to included blocks, including its second return. No indirect
jumps or embedded tables occur in these spans. The machine-generated reports
still label bounds as inferred: this inspection does not modify the bounds table
or establish a general boundary-validation mechanism.

Remaining differences:

- Distance: x87 instruction scheduling/stack use differs (`fmul` versus `fmulp`
  placement and an extra `fstp`). This requires operation-level review and tests;
  the mnemonic score does not establish floating-point equivalence.
- Validity check: the candidate uses a popping float store and reload where the
  original retains the x87 value. Precision-sensitive inputs need testing. Its
  `__real@3f800000` relocation has no uniquely mapped original symbol, so four
  bytes remain uncertain rather than being credited as a match.
- Wrapper: register use, address calculation, and stack-cleanup placement differ.
  The candidate's relocation resolves to the original callee at `0x1093b0`.

## Behavioral evidence

The current Clang math object built successfully. A full `halo` build attempt
stopped at an unrelated, already-edited rasterizer function declaration
(`FUN_0015f5e0`, calling-convention conflict).

The first differential attempts did not establish behavioral equivalence:

- Distance and validity: the harness returned `not_applicable` with
  `external_relocations`, despite `--allow-stubs`.
- Wrapper: 100 seeds produced emulation errors at the legitimate callee
  `0x1093b0`, outside the harness's permitted execution ranges; zero passed.

The cause was an environment conflict: the harness prepended a Linux virtualenv
site-packages directory on Windows, whose Capstone native library could not
load. Without Capstone, the raw-XBE classifier could not identify the leaf or
callee interception sites. Preloading the working system Capstone before the
existing verifier resolved that problem without modifying the harness.

| Target | Retry result | Observed coverage | Limitation |
|---|---|---:|---|
| Distance | 100 passed, 0 failures/errors, no write-trace differences | 100% | Tested inputs only |
| Validity check | 100 agreed, but all returned zero: **inconclusive** | 89.9% | The successful validation outcome was not exercised |
| Wrapper | 100 passed, 0 failures/errors, no write-trace differences | 100% | Tested inputs and the harness's modeled callee environment |

Results are in the `*-preloaded-behavior.json` files. This run disabled concolic
search and did not update the equivalence cache. Optional Z3 was unavailable;
these are differential execution results, not formal proofs. The static audit's
own behavior field remains untested because that lane did not execute the code.

A targeted follow-up used the existing snapshot `arg_overrides` mechanism for
the validity check. Four distinct pinned input pairs were tested, each repeated
for three seeds:

| Inputs `(x, y)` | Original and candidate result | Per-case coverage |
|---|---:|---:|
| `(1.0, 0.0)` | 1 | 91.3% |
| `(0.0, 0.0)` | 0 | 87.0% |
| `(0.70710678, 0.70710678)` | 1 | 91.3% |
| `(NaN, 0.0)` | 0 | 68.1% |

Every pinned case agreed with no emulation errors or write-trace differences.
These exercise both return outcomes and the non-finite rejection path. Repeated
seeds with pinned arguments are not additional distinct input coverage. This
does not settle rounding behavior at the acceptance threshold or exhaust the
input domain. Inputs and results are the `valid_real_sine_cosine-*-result.json`
files and corresponding snapshot files in the pilot directory.

## Reproduce

Example per-function audit:

```powershell
rtk python3 tools/verify/raw_xbe_structural.py src/halo/math/real_math.c --function FUN_0001ace0 --address 0x1ace0
```

The default output directory is `artifacts/raw_xbe_structural/`. Repeat with
`valid_real_sine_cosine --address 0x10fe80` and
`component_vectors_from_normal3d --address 0x1094d0`.

This run's reports, behavioral-attempt JSON, and dashboard are under
`artifacts/matching_pilot/2026-09-22/`. Fresh pilot audit records were also copied
to the normal dashboard input directory. The dashboard's other mnemonic scores
come from the committed floor because `vc71_current.json` has no scores; use the
fresh pilot measurements above for these three targets.

## Updating the dashboard

Normal `artifacts/batch_verify/` results are read automatically, and the dashboard
now shows their pass counts and coverage. Custom saved results, such as this
pilot's files under `artifacts/matching_pilot/`, must be imported explicitly so
the dashboard does not discover arbitrary historical JSON files. Import one broad
result with the reusable adapter:

```powershell
rtk python3 tools/report/import_behavior_results.py --main PATH_TO_MAIN_RESULT
```

Add each distinct supplemental case with a repeated `--case LABEL=PATH` option:

```powershell
rtk python3 tools/report/import_behavior_results.py --main PATH_TO_MAIN_RESULT `
  --case valid-10-0=PATH_TO_CASE_RESULT `
  --case invalid-00=PATH_TO_CASE_RESULT
```

Each pinned case is counted once, regardless of how many seeds repeat it. The
main result remains the function's verdict; for example, an inconclusive broad
run remains inconclusive while agreeing targeted cases are shown separately.
The loader selects the latest result for a target and does not sum counts across
older runs. These imported artifacts preserve the recorded evidence and paths;
no historical build hash was recovered. The four validity inputs are targeted
near-unit checks, not proof at the acceptance threshold.

Regenerate the normal report and dashboard without editing the README:

```powershell
rtk python3 tools/report/generate_decomp_report.py `
  --output artifacts/progress/report.json `
  --html artifacts/progress/dashboard.html `
  --no-readme
```

## Validation and next work

Validation: 38 comparator tests, 11 report tests, and 7 dashboard JavaScript tests
passed. A full dashboard was generated and its three pilot rows were checked.
The wrong-call counterexample now reports a fixed 33.3% aligned result with a
target mismatch; the unknown-call example remains unverified.

Next, make the harness interpreter selection reliable and test precision-sensitive
inputs before making evidence-backed source changes. Keep the original compiler
investigation separate from source recovery. The distance and wrapper have
passing behavioral evidence for the tested cases. The validity check's random
run remains inconclusive, supplemented by four agreeing targeted cases.
None of the three is byte-matching complete.
