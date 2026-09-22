# Batch Raw-Byte Audit Plan

## Objective

Measure strict literal candidate-versus-pristine-XBE byte equality across every
eligible ported function, then surface coverage and per-function results in the
progress dashboard.

This is intentionally separate from VC71 mnemonic matching. Do not call its
results "VC71 accuracy" or silently normalize anything.

## Current Baseline

- `tools/verify/raw_byte_audit.py` performs one fresh VC71 compilation and one
  strict literal audit for a named source function/address.
- It records `raw-byte exact`, `bytes differ`, or `not comparable`.
- It rejects candidate functions containing COFF relocations. There is no
  relocation masking, instruction normalization, padding trimming, or
  disassembly in the comparison.
- Each audit JSON records source, candidate object/span, pristine XBE/span, and
  bounds provenance hashes.
- `tools/report/generate_decomp_report.py` loads fresh audit records and shows a
  per-function `Raw bytes` badge. It discards records whose source/XBE/bounds
  provenance is stale.

## Required Output

Add subcommands to `tools/verify/raw_byte_audit.py`:

```text
single   Existing one-function command (retain compatibility)
populate Freshly audit every eligible ported function
check    Re-audit selected/current inputs and detect a regression from exact
show     Print aggregated coverage and strict audit totals
```

Recommended usage:

```bash
rtk python3 tools/verify/raw_byte_audit.py populate --workers 4
rtk python3 tools/verify/raw_byte_audit.py populate --source src/halo/math/vector_math.c
rtk python3 tools/verify/raw_byte_audit.py check --changed
rtk python3 tools/verify/raw_byte_audit.py show
```

## Evidence Contract

Each per-function JSON must retain this contract:

```json
{
  "function": "distance_squared3d",
  "address": "0x000121a0",
  "generated_at": "RFC3339 UTC timestamp",
  "verdict": "raw-byte exact | bytes differ | not comparable",
  "source": {"path": "...", "sha256": "..."},
  "candidate": {
    "path": "...", "sha256": "...",
    "sha256_span": "...", "length": 49,
    "coff_symbol": "...", "coff_section": ".text"
  },
  "reference": {
    "path": "halo-patched/cachebeta.xbe", "sha256": "...",
    "start": "0x000121a0", "end": "0x000121d1",
    "sha256_span": "...", "length": 49,
    "bound_kind": "auto", "bound_provenance": "table"
  }
}
```

The batch summary must be written to:

```text
artifacts/raw_byte_audit/summary.json
```

and include tool version, invocation, XBE SHA-256, bounds-table SHA-256,
`decl.h` SHA-256, source-TU count, candidate compile failures, and function/
byte totals by verdict.

## Strict Comparison Rules

1. Compile each source TU once, freshly, with the same VC71 compiler options
   used by `vc71_verify.py`.
2. Pin/regenerate `build/generated/decl.h` **once in the parent** before
   workers begin. Workers must not regenerate it concurrently.
3. Resolve every ported function address from its owning kb.json TU/source, not
   from a global name match alone.
4. Extract only an unambiguous COFF function extent.
   - Accept a valid function auxiliary total size.
   - Accept a one-function `/Gy` COMDAT `.text` section at offset zero.
   - Otherwise report `not comparable`.
5. If any COFF relocation lies inside the candidate function span, report
   `not comparable`. Do not zero or mask relocation fields.
6. Read the literal reference span from the pristine `cachebeta.xbe` using the
   committed `tools/verify/function_bounds.json` entry.
7. Do not disassemble, normalize operands, strip padding, elide absolute
   addresses, or apply any other equivalence transformation.
8. A candidate/reference length mismatch is `bytes differ` with
   `first_difference = min(candidate_length, reference_length)`.
9. Persist every completed function result, including `not comparable`, so the
   dashboard can distinguish an audited limitation from no audit.

## Selection and Freshness

### `populate`

- Enumerate only `ported: true` functions with a real C source path.
- Group by source TU.
- Default: all eligible TUs.
- `--source` limits to explicitly named source files.
- `--workers N` parallelizes by TU, never by function.
- Use a fresh output directory or atomically replace per-function JSON only
  after the record is complete.

### Incremental behavior

Do **not** add a hidden stale-cache mode. A prior record may be reused only if
all of these match:

- source SHA-256;
- pristine XBE SHA-256;
- `function_bounds.json` SHA-256 and the individual range;
- compiler invocation/options version;
- generated `decl.h` SHA-256.

`--changed` should select source files changed from the merge base plus TUs
whose declarations/bounds/tool provenance changed. It must print the selection
reason for every TU.

## Summary Metrics

Do not publish one unlabeled "byte accuracy" number. Report both coverage and
strict exactness:

```text
Raw-byte audit coverage
  raw-byte exact:  <functions>, <original bytes>
  bytes differ:    <functions>, <original bytes>
  not comparable:  <functions>, <original bytes>
  not audited:     <functions>, <original bytes>

Exact among comparable: <exact / (exact + differs)>
Exact across all ported functions: <exact / ported>
```

Label the first metric **Exact among comparable**. Label the second **Exact
coverage of ported functions**. Neither is behavioral verification.

## Dashboard Integration

The existing function-detail `Raw bytes` column should remain separate from
`Mnemonic %` and `Verified`:

| Raw bytes badge | Meaning |
|---|---|
| Exact | Fresh literal span equality. Strong structural evidence. |
| Differs | Fresh literal mismatch; not by itself a behavior failure. |
| N/C | Audited but strict comparison is unavailable; show reason. |
| — | No current audit. |

Add a dashboard overview card based exclusively on `summary.json`:

```text
Raw-byte audits
Exact 1,842 · Differs 317 · N/C 3,740
Exact among comparable: 85.3%
Fresh provenance only · Not a behavioral-verification metric
```

Never set the existing `Verified` state from raw-byte exactness alone. A raw
exact result proves emitted bytes for that function/range and build setup; it
cannot validate caller ABI, engine state, or outside-function behavior.

## Tests

Add or extend focused tests for:

1. COFF extraction: auxiliary-size extent, one-function COMDAT extent,
   ambiguous extent rejection, internal relocation rejection, relocation after
   span acceptance.
2. Literal comparison: exact, changed byte, shorter candidate, longer
   candidate, missing bounds, unmapped address.
3. Batch grouping: one compile request per source TU, not per function.
4. Parent-pinned `decl.h`: worker invocations carry `--skip-decl-regen` or use
   a non-regenerating compile helper.
5. Provenance reuse: any changed source/XBE/bounds/compiler/decl hash forces a
   new audit rather than using an old record.
6. Summary aggregation: functions and original-byte totals by all four states.
7. Dashboard loader: accepts valid current records; rejects stale source, XBE,
   and moved-bound records; renders `Exact`, `Differs`, and `N/C` safely.

Run at minimum:

```bash
rtk python3 -m unittest tools/verify/test_raw_byte_audit.py \
  tools/report/test_raw_byte_audit_report.py
rtk python3 -m py_compile tools/verify/raw_byte_audit.py \
  tools/report/generate_decomp_report.py
rtk python3 tools/report/generate_decomp_report.py --no-readme \
  --output /tmp/raw-byte-report.json --html /tmp/raw-byte-report.html
```

## Non-Goals

- No relocation masking, synthesis, or normalization.
- No percentage derived from mnemonic matching.
- No automatic behavioral-verification claim.
- No change to VC71 score floors or `vc71_scores.json`.
- No multi-TU compiler parallelism that lets workers rewrite `decl.h`.
- No commit of generated `artifacts/` output unless repository policy changes.
