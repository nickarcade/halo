---
name: recover-campaign
tier: user
description: Fully autonomous readability campaign. Ranks TUs by readability debt, runs parallel one-TU halo-source-recovery workers against a ~1 s byte-identical gate, sends readable rewrites that move bytes to the raw-byte gate, and lands purity-checked per-category commits with no questions asked. The source-recovery counterpart of /byte-campaign.
---

# /recover-campaign — Autonomous Readability Campaign

`/byte-campaign` raises raw byte accuracy. This campaign makes lifted source
read like the original: no raw addresses, no offset arithmetic, no decompiler
names, and no comment noise. It has the same shape: a computed queue, parallel
one-TU workers, a fast deterministic gate, serial landing, and a ledger.

**Doctrine:** the finished-source target is `source-recovery` → *Target style*.
The safe change order is its ladder. This file only adds the loop. Do not restate
or relax either.

Argument: $ARGUMENTS

## Arguments (all optional)

- `N` or `--goal N`: TUs landed before stopping (default: 4).
- `--max-runs N`: batches (default: 3).
- `--workers N`: concurrent TU workers (default: 4, max 6).
- `--tu <src/...c>`: restrict to TU(s); repeatable. `--fn <name>` further
  restricts a TU to named functions (use this for pilots on a large TU).
- `--per-tu N`: functions per worker per run (default: 25). Big TUs take several runs.
- `--no-bytes`: never keep a byte-moving rewrite. Park it instead.
- `--dry-run`: print the queue and per-TU debt only.

**Autonomy:** the campaign never asks. It commits on the current branch, it never
pushes, and it never uses `--no-verify`. Every stop is a reported finding.

## Gates (the loop)

| Gate | Command | Time | Passes when |
|---|---|---|---|
| neutral | `tools/recovery/neutral_gate.py check --source TU --dir D` | ~0.5–2 s | `.obj` byte-identical to capture (names excused only via `--rename-map`); no assert metadata moved |
| bytes | `tools/bytematch/pal_campaign.py gate --source TU --baseline B --target FN` | ~10 s | target gains **raw aligned bytes** against the XBE, and no function in the TU loses any |
| build | `tools/build/build.py -q --target halo` | batch | every includer of a touched header compiles |
| equivalence | `tools/equivalence/unicorn_diff.py FN --allow-stubs -q` | per byte fn | verdict not worse; if vacuous, review the diff by hand |

`neutral_gate.py capture` runs once per TU per run, before the first edit, and
again after each accepted category checkpoint.

### Raw byte accuracy before/after (mandatory, per TU)

**The regression metric is raw aligned-byte accuracy against the 2276 XBE**
(`raw_xbe_structural` via `pal_campaign.py`). VC71 mnemonic % does not decide
anything here. It is too coarse: it misses immediates, registers, and
addressing modes. A byte-identical object cannot regress. The byte route and
header edits can, because a header change reaches every includer.

1. **Before** (the worker, before any edit):
   `pal_campaign.py snapshot --source <TU> --output <run>/<stem>/bytes.before.json`.
2. **After** (the orchestrator, on the final landed state of each TU):
   `pal_campaign.py gate --neutral --source <TU> --baseline <run>/<stem>/bytes.before.json`.
   PASS means no function lost an aligned byte. Run it for every **other TU
   that includes a header the batch touched**, too. Snapshot those TUs before
   landing starts.
3. Any drop reverts the responsible checkpoint (bisect by checkpoint order),
   not the whole TU. Re-run the after check until it passes.
4. The final report carries a per-TU table: aligned bytes and % before → after,
   and functions gained/dropped (dropped must be 0). VC71 mnemonic deltas are
   listed for information only.

**VC71 pre-commit hook conflict.** The hook still enforces VC71 mnemonic
floors. A raw-byte gain that lowers a mnemonic floor gets rejected at commit.
The campaign then reverts that checkpoint and parks the function as
`vc71_floor_conflict`, with both deltas. Never use `--no-verify`, and never
`vc71_regression.py update --force`: it lowers every floor in the TU,
including unrelated stale ones. Before counting a hook failure against the
campaign, check whether the floor already fails at the run's base commit
(stale floors exist).

## Queue (orchestrator, once per run)

```bash
rtk git status --short                                     # target TUs + headers must be clean
rtk python3 tools/recovery/recovery_frontier.py --json artifacts/recover_campaign/<run>/frontier.json
```

For each candidate TU, count debt with `rtk grep -c` on the patterns in
*Debt patterns* below. Rank by debt × functions ported. Drop TUs that have
uncommitted edits. **Header ownership:** in one batch, no two TUs may share a
header they would edit. Give each worker its TU plus the TU's own header (for
example `objects.c` → `objects.h`). Shared preludes (`common.h`, `xdk_common.h`,
`types.h`) are **orchestrator-only**. Workers return proposals for them.

### Debt patterns

| Debt | Pattern |
|---|---|
| raw address | `\*\([^)]*\*\)0x[0-9a-f]{5,}`, `\(void \*\)0x[0-9a-f]{5,}`, `XCALL\(0x` |
| offset arithmetic | `\*\([^)]*\*\)\(\w+ \+ 0x` |
| decompiler names | `\b(iVar\|uVar\|pbVar\|pcVar\|fVar\|param_\|local_)\w*`, `FUN_[0-9a-f]{8}`, `DAT_` |
| comment noise | header block comments containing `0x1[0-9a-f]{5}` addresses, `Confirmed:`, `/ objects.obj`, disassembly mnemonics |

## Step 1: Parallel workers (one per TU)

Send all workers in **one message**: `subagent_type: halo-source-recovery`,
`model: opus`. Brief (self-contained; name skills `source-recovery`,
`naming-confidence`, `struct-recovery`, `offset-to-struct`, `re-comment-capture`):

> TU `<src/...c>`, owned header `<h or none>`, functions `<list, highest debt
> first>`, run dir `artifacts/recover_campaign/<run>/<stem>/`.
>
> **Edit only the TU and the owned header.** Do not edit kb.json, `types.h`,
> `common.h`, `xdk_common.h`, or any other file. Do not build, and do not
> commit. Record what you need there as a proposal.
>
> 0. `neutral_gate.py capture --source <TU> --dir <run>/<stem>/gate`. If the TU
>    uses `assert_halt(`/`assert_halt_msg(` (implicit `__LINE__`), first pin
>    each one to `assert_halt_at(__FILE__, <its line>, …)`, gate neutral, and
>    checkpoint it as `00-assert-pin`.
> 1. Work the ladder in order, one category at a time, per function:
>    comments → local-renames → global-names → const-enum → struct-define →
>    offset-to-field. Follow *Target style*. After each edit, run the neutral
>    gate. On REJECT, revert that edit, then try step 2.
> 2. **Byte route** (skip under `--no-bytes`): a rewrite that is the target
>    style but moves bytes goes to the bytes gate. On PASS, keep it for the
>    `bytes` checkpoint only (never inside a category checkpoint), and run
>    equivalence before and after. On REJECT, revert and park it with the
>    measured delta. Use at most 2 attempts per function.
> 3. After each category, copy the TU and owned header to
>    `<run>/<stem>/<NN>-<category>/`, then re-capture the gate. Write the byte
>    rewrites last, as `<NN>-bytes/`.
> 4. **Evidence (binding):** names come from 2276 only: assert strings,
>    `data_new`/`game_state_*` name strings, the hs-globals table
>    (`rtk python3 tools/recovery/name_evidence.py <addr>`, which also prints
>    `.rdata` constant values), kb.json, and public tag field names. Other
>    decompilations may orient you. **Never transcribe their identifiers,
>    comments, or layouts.** Offsets and sizes come from 2276 accesses. With no
>    evidence, use a mechanical name, `field_<hex>`, or `pad_<hex>`.
> 5. Return JSON only: `{tu, checkpoints: [{dir, category, functions, lines_removed}],
>    bytes: [{fn, before, after, equiv_before, equiv_after}], parked: [{fn, category,
>    reason}], proposals: [{file, text, reason}], debt_before, debt_after}`.

## Step 2: Serial landing (orchestrator)

1. Apply prelude `proposals` whose evidence holds. Use one commit, and put
   definitions in alphabetical position inside the existing block.
2. For each TU, apply checkpoints in order. For each checkpoint: copy the files
   into the tree, run `rtk python3 tools/recovery/check_category_purity.py
   <category> --staged` (exit 0 or 2 passes; exit 1 means split the commit or
   drop it), then commit `recover(<stem>): <category> — <n> functions`.
   `bytes` commits say `Improve <stem> raw byte accuracy (readable rewrite)`
   and list per-function `before% -> after%` and the equivalence verdict.
3. Run one `build.py -q --target halo` per batch. If it fails, find the TU,
   `git revert` its commits in reverse order, and mark it `reverted`.
   Then run the **after** byte-accuracy check (*Gates → Byte accuracy
   before/after*) for each landed TU and each includer of a touched header.
   Do not start the next run while any function shows a drop.
4. A pre-commit hook rejection (hazard or VC71 floor) reverts that checkpoint
   and every later checkpoint for that TU. Continue with the next TU.
5. The skill-router hook denies the first `git commit` in each command. Re-run
   the identical command.

## Ledger

Append to `artifacts/recover_campaign/campaigns.jsonl`:
`{ts, run, tus, checkpoints, functions_touched, debt_before, debt_after,
bytes_gained, parked, reverted, tokens, wall_s}`. Keep per-function parks in
`artifacts/recover_campaign/parked.json`, keyed by function and TU fingerprint.
A changed TU/header reopens the park. Force-add both files in one chore commit.

## Stop conditions

`--goal` reached; `--max-runs` reached; queue empty; two runs in a row with zero
debt removed; tracked dirt outside the batch's files and known generated noise;
build broken after reverts (a systemic stop).

## Final report

Debt removed per TU (by pattern), checkpoints landed with SHAs, byte gains,
parks with reasons (a park is a finding), proposals not applied, and the
next queue head.

## Known levers and traps (from the objects.c pilot, 2026-09-29)

- `light_mark`/`light_unmarked`: a typed local plus struct fields plus named
  globals was **byte-identical**. Absolute-address struct globals
  (`#define lights_globals (*(lights_globals_t *)0x5a8d60)`) compile the same as
  the raw derefs.
- `render_debug_light` **(`vc71_floor_conflict`)**: a struct copy
  (`color = *global_real_argb_orange`) raised raw accuracy 87.3→88.8%
  (+3 bytes) and lowered VC71 mnemonic 93.8→90.6%. The pre-commit hook
  rejected it. A field-wise copy lost raw bytes (→77.0%) and was rejected by
  the raw gate.
- `objects_get_activating_cluster_index` fails its VC71 floor (100→98.6%) at
  aad9dd3f4, before any campaign edit. That is a stale floor, not a campaign
  regression.
- Permuter artifacts (`iVar1 = 0x14; *(int *)(base + iVar1)`) are fake
  matching under *Target style* rule 10. Park them for the byte lane with a
  note. Do not "clean" them neutrally, because they cannot be.
- The gate compares against the capture, not HEAD. Re-capture after every
  accepted checkpoint, or later edits are judged against a stale object.
