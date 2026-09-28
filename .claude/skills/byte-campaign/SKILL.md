---
name: byte-campaign
tier: user
description: Fast raw-XBE byte-accuracy campaign — rank ported functions by recoverable aligned bytes, use the PAL 2342 decomp (../halo-pal-2342, punpckhdq/halo) as a code-shape hint, run parallel one-TU workers against a ~10s deterministic gate, land one byte commit + one optional byte-neutral readability commit per TU.
---

# /byte-campaign — Raw-Byte Accuracy Campaign (PAL-guided, fast)

`/score-campaign` optimizes the VC71 mnemonic score. This campaign optimizes
**raw-XBE aligned byte accuracy** (`tools/verify/raw_xbe_structural.py`), and
it uses one resource no other lane uses: the PAL build 2342 decompilation at
`../halo-pal-2342`. 304 of its 468 objects are `Matching` — every function
in them compiles byte-exact against 2342 under VC7.1 `/O2 /Oy-`. About 3900
of our ported functions share an exact name with a PAL function.

Everything computable is in `tools/bytematch/pal_campaign.py` (no LLM):
queue, PAL body lookup (4 ms via the PAL codegraph DB), TU snapshot, gate.
LLM effort is spent only on the rewrite itself.

Argument: $ARGUMENTS

## Arguments (all optional)

- `N` or `--goal N` — aligned bytes gained before stopping (default: 2000).
- `--max-runs N` — batches (default: 3).
- `--workers N` — concurrent TU workers (default: 4, max 6).
- `--tu <src/...c>` — restrict to TU(s); repeatable.
- `--min-accuracy X` — floor, 0–1 (default: 0.75). Below this the gap is
  usually a lift bug — list it for `xbox-halo-re-analyst`, do not attempt it.
- `--matching-only` — only targets whose PAL object is `Matching` (fastest
  conversion; recommended for the first run).
- `--no-readability` — skip the byte-neutral readability pass.
- `--dry-run` — print the queue only.

## Evidence policy (binding)

- **2276 is the source of truth.** PAL 2342 is three months newer. Use PAL for
  *shape*: statement order, expression form, temporaries, local types, loop
  form, early returns, argument order at call sites. Never import *behavior*
  that 2276 does not show — an extra call, a changed constant, a new branch.
  When the PAL body and our body disagree on behavior, keep ours and cite the
  2276 disassembly if you are unsure (`get_assembly_context` / the record's
  `differences`). Ghidra is optional; run `tools/audit/check_ghidra_mcp.py`
  first (CLAUDE.md rule 8).
- **Names:** a name in the PAL source is T2 evidence (`naming-confidence`).
  Cite it: `/* name: PAL 2342 <file>:<line> */`.
- **Layouts:** never copy a PAL struct layout. Field offsets come from
  `src/types.h` only. A struct field rewrite is allowed only when the struct
  and field already exist in our tree.
- **Forbidden levers:** CLAUDE.md rule 10 applies — no volatile shaping, dead
  stores, barriers, pragmas, or UB to buy bytes. `@<reg>` annotations are
  immutable. The gate cannot see these; the worker must not do them.

## Preflight (once)

```bash
rtk git status --short                                  # target TUs must be clean
rtk .venv/bin/python -c "import sys; sys.path.insert(0,'tools/verify'); import vc71_verify as v; v.regen_decl_header(quiet=True)"  # fresh decl.h
rtk .venv/bin/python tools/bytematch/pal_campaign.py queue \
  --max-tus $WORKERS --per-tu 8 [--matching-only] [--tu ...] \
  --json artifacts/byte_campaign/<run>/queue.json
```

The queue ranks TUs by `missing_bytes × weight` (Matching 1.0, NonMatching
0.4), from cached records (~20 s warm). If records are stale, refresh only
the TUs you target — **never `populate --source` without a follow-up full
populate**: `populate --source` overwrites the global
`artifacts/raw_xbe_structural/summary.json` with that one TU's totals.

On `main` with no explicit user override, ask once before the first commit.
If a target TU has uncommitted edits, drop it from the batch (never commit
over authored WIP).

## Step 1 — Parallel workers (one per TU)

Spawn up to `--workers` Agents in **one message**, `subagent_type:
vc71-match-optimizer`, `model: opus` (per memory: opus low/medium beats
sonnet on cost and quality; no fable). Each owns exactly one `.c` file. Brief
(self-contained; name loaded skills `lift-score-improve`, `naming-confidence`):

> TU: `<src/...c>`. Targets (from queue.json): `<name, addr, accuracy,
> missing_bytes, pal_status, causes>`. Run dir: `artifacts/byte_campaign/<run>/`.
>
> **Only edit `<TU>`.** Do not edit kb.json, headers, types.h, or any other
> file; do not build; do not commit. If a fix needs kb.json/types.h, record
> it in your report as a proposal and move on.
>
> 1. `rtk .venv/bin/python tools/bytematch/pal_campaign.py snapshot --source <TU> --output <run>/<stem>.base.json`
> 2. Per target, highest `missing_bytes` first:
>    a. `pal_campaign.py pal <fn>` — the PAL body. Read our function with a
>       line-range Read only (`rtk rg -n '^<type>.*\b<fn>\(' <TU>` to locate).
>    b. Diff shape, not behavior (see evidence policy). Make one coherent
>       rewrite toward the PAL shape.
>    c. `pal_campaign.py gate --source <TU> --baseline <run>/<stem>.base.json --target <fn>`
>       (~10 s). PASS → re-snapshot to the same base file. REJECT → revert
>       exactly that edit.
>    d. At most **3 attempts per function**, then move on. Never spend a
>       4th — speed comes from breadth, not depth.
> 3. After all byte work: `cp <TU> <run>/<stem>.bytes.c` (the checkpoint the
>    orchestrator commits first).
> 4. Readability pass (skip if `--no-readability`), only on functions you
>    touched: replace raw `*(T *)(p + 0xNN)` with an **existing** struct field
>    whose offset `src/types.h` proves; rename locals/params using PAL names
>    (cite them). Gate each edit with `gate --neutral` (nothing may drop).
>    Revert on REJECT.
> 5. Equivalence for every function with an accepted byte change:
>    `rtk .venv/bin/python tools/equivalence/unicorn_diff.py <fn> --allow-stubs -q`
>    Run it once **before your first edit** of that function (step 2a) and
>    once after the readability pass. Compare the two runs only with each
>    other, and only when their `build :` lines match (on-demand vs build
>    objects differ). A worse verdict → revert that function's changes and
>    mark `behavior_risk`.
> 6. Return JSON only: `{tu, functions: [{name, before, after, bytes_gained,
>    attempts, result: improved|no_gain|reverted|behavior_risk,
>    equiv_before, equiv_after}], readability_edits: N, proposals: [...],
>    tokens, wall_s}`.

## Step 2 — Serial landing (orchestrator)

When all workers return (or as each returns — land in completion order):

1. `rtk python3 tools/build/build.py -q --target halo` once for the batch.
   On failure, find the TU from the error, restore it with
   `rtk git checkout -- <TU>`, mark it `reverted`, rebuild.
2. Per TU, byte commit first:
   ```bash
   cp <TU> /tmp/<stem>.full.c && cp <run>/<stem>.bytes.c <TU>
   rtk git add <TU> && rtk git commit -F <mktemp msgfile>
   ```
   The pre-commit hook runs the hazard scan (`--staged-only`) and the VC71
   no-regression gate on the staged TU. A hook rejection means the byte
   change lowered a VC71 score or added a hazard: restore the TU, mark
   `reverted`, continue with the next TU. Never `--no-verify`.
   Message: `Improve <tu stem> raw byte accuracy (PAL-guided)` with one line
   per function: `- <fn> @ <addr>: <before>% -> <after>% aligned (+N bytes), equiv <verdict>`.
3. Readability commit (if the full file differs from the bytes checkpoint):
   `cp /tmp/<stem>.full.c <TU>`, commit
   `Readability: <tu stem> struct fields and PAL-cited names (byte-neutral)`.
4. Refresh records for landed TUs so the next queue sees them:
   `rtk .venv/bin/python tools/verify/raw_xbe_structural.py populate --source <TU>` for
   each, then **one** full `populate --workers 6` at campaign end (restores
   the global summary).

## Ledger

Append per run to `artifacts/byte_campaign/campaigns.jsonl`:
`{ts, run, tus, targets, functions_improved, bytes_gained, reverted,
behavior_risk, tokens, wall_s, bytes_per_ktok}`. Per function in
`artifacts/byte_campaign/attempted.json`: `{name: {ts, run, result, before,
after, dependency_fingerprint}}`; obtain the fingerprint with
`pal_campaign.py fingerprint --source <TU>`. The next queue run skips
`no_gain`/`reverted` only when they occurred in either of the last two completed
runs. A changed TU, `types.h`, or generated `decl.h` fingerprint reopens every
result, including an ABI ceiling. Legacy rows without `run` or fingerprint age
out instead of becoming permanent. Force-add both ledgers (`rtk git add -f`) in
a chore commit.

## Stop conditions

`--goal` bytes reached; `--max-runs` reached; queue empty after skips; two
consecutive runs with zero bytes gained; tracked dirt outside the TUs being
landed and generated files (`tools/verify/vc71_scores.json`, `README.md`
stats, `tools/equivalence/leaf_cache.json`, `artifacts/**`).

## Final report

Bytes gained vs goal; functions improved; per-TU table; reverted and
`behavior_risk` rows with reason; kb.json/types.h proposals (these are the
next unlock — an ABI or struct fix often gains more than shape work);
sub-floor functions listed for re-lift review; `bytes_per_ktok` next to the
previous ledger row.

## Measured levers (run 2026-09-26: 12/16 improved, +382 aligned bytes, 1.42 bytes/ktok)

Put these in every worker brief. They are in rough order of yield:
- Remove temps that the decompiler introduced, and re-read fields at the point of use as PAL does. Two functions reached 100% this way.
- Write 12-byte copies as struct assignment: `*(real_vector3d *)d = *(real_vector3d *)s;`.
- Pass a nested call directly as an argument (`display_assert(csprintf(...), ...)`), not through a temp. This gives the reference's push order.
- Use a `switch` over group tags instead of an if/else tree.
- Use a single-exit `result` variable with `break` arms instead of early returns.
- Match local width to the reference. A `short` local compared with NONE compiles to `cmp ax,0xffff`.
- Follow PAL's field-assignment order for adjacent stores.
- Use float literals (`0.0f`, `1.0f`) instead of `*(float *)0x...` constant globals. First verify the value in the pristine XBE (`tools/equivalence/xbe_image.py`).
- **Skip early:** `mov reg, eax` at function entry means an argument arrives in a register. That is an ABI ceiling that no shape change can recover. Record it as a kb.json proposal and mark the function `regarg_ceiling`.
- TUs that use `assert_halt` with `__LINE__` (for example objects.c): keep the line count of each edit unchanged. Pad with a comment if you must. Otherwise the gate reports false 1-byte drops in later functions.
- Scope every text replacement to the function's line range. A global `sed` changed an unrelated function in the first run.

## Landing notes

- The skill-router hook denies the first `git commit` in each command (deny-once). Re-run the identical command.
- A background refresh started by the previous commit's hook can briefly hold `.git/index.lock`. Retry once.
- Check each worker's diff before you commit it: (a) scope, meaning only target functions changed; (b) behavior of any restructured control flow. A vacuous equivalence result is no evidence. Read the control flow yourself. (c) Any value that was replaced with a literal, checked against the XBE.
- Judge a run by the **aligned** bytes of each target in the refreshed records. The global positional `byte_accuracy` in summary.json can go down when a function's length changes, so do not use it for a verdict.

## Speed notes

- The gate is the loop: ~10 s per 80-function TU. Do not substitute
  `vc71_verify`, a full build, or equivalence inside the per-attempt loop.
- Breadth beats depth: 3 attempts per function, `Matching` targets first.
- Workers never read whole files, never read PAL files directly (use
  `pal_campaign.py pal`), and never re-derive the queue.
- `raw_xbe_structural.py check` is a no-op; do not use it as a gate.
