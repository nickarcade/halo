# Byte-match loop

This directory turns the raw-XBE structural audit into a verified source-search
loop. It never treats the mnemonic or masked-operand score as proof.

## Refresh the evidence

```bash
rtk .venv/bin/python tools/verify/raw_xbe_structural.py populate --workers 4
```

Each record in `artifacts/raw_xbe_structural/` includes:

- positional and instruction-aligned byte accuracy;
- relocation targets proven by symbol address or read-only literal content;
- raw operand accuracy for aligned non-relocated instructions;
- per-instruction difference classes;
- per-function optimization flags;
- a register-argument residual classification.

## Rank and search

```bash
rtk .venv/bin/python tools/bytematch/bytematch.py queue --limit 50
rtk .venv/bin/python tools/bytematch/bytematch.py search FUNCTION
rtk .venv/bin/python tools/bytematch/bytematch.py batch --limit 40
rtk .venv/bin/python tools/bytematch/bytematch.py rules
```

`search` locates semantics-preserving transformations through libclang,
compiles every variant with VC7.1, and keeps a rewrite only when aligned bytes
strictly improve. It writes a patch under `artifacts/bytematch/`; it does not
edit source unless `--apply` is passed. The ledger records failed and successful
trials, so later searches try proven rules first.

Current transformations are commutative operand swap, relational flip, and
safe `if`/`else` inversion. They never move assignments, never reorder two
calls, and preserve expression association with parentheses.

## Recover explicit binary values

Rewrite only asserts whose text can be paired with the XBE exactly:

```bash
rtk .venv/bin/python tools/audit/recover_assert_sites.py
rtk .venv/bin/python tools/audit/recover_assert_sites.py --apply
```

Find functions whose original leaves a stack parameter in EAX at every return:

```bash
rtk .venv/bin/python tools/audit/detect_param_return.py \
  --json /tmp/param_returns.json
rtk .venv/bin/python tools/bytematch/bytematch.py verify-returns \
  /tmp/param_returns.json
```

Return-type changes are ABI changes. `verify-returns` creates patches only.
Review the declaration, all callers, and `kb.json` before applying one.

## Required gates after applying a patch

```bash
rtk python3 tools/build/build.py -q --target halo
rtk python3 tools/audit/check_lift_hazards.py --changed-only
rtk .venv/bin/python tools/verify/raw_xbe_structural.py populate --source PATH
```

Also run the function's appropriate equivalence gate. Exact VC7.1 bytes are
comparison evidence, not proof of runtime equivalence or compiler identity.
The original compiler and flags remain unproven.
