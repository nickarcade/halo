---
name: naming-confidence
tier: agent
description: Evidence rules for naming functions, types, fields, parameters, locals, constants, and enums in the Halo Xbox target reconstruction.
triggers: ["rename", "naming", "field name", "enum name", "struct name", "PDB", "PAL", "CEA"]
---

# Naming confidence

Names are claims. Use the most specific name independently supported by the
2276 target and state the evidence when it is not obvious.

| Tier | Evidence | Allowed result |
|---|---|---|
| T1 | Identifier appears in a 2276 assert, format string, table, export, or other target-resident text and refers unambiguously to the symbol | Exact semantic name with target address citation |
| T2 | 2276 behavior proves the role across relevant reads, writes, call sites, masks, or state transitions | Independently descriptive semantic name with an evidence comment |
| T3 | Width/offset/access is proven but semantics are incomplete | Mechanical or deliberately broad name: `field_<offset>`, `unknown_<role>`, `FUN_<address>` |
| T4 | Guess, single ambiguous use, or external cross-build suggestion without target proof | Keep unknown; record the hypothesis outside authoritative source |

External PAL/CEA/HCEA/PDB-derived projects are `EXT-HYP` under
`PROVENANCE.md`. They may suggest a search target, but they do not raise a name
above T4. A name becomes T1/T2 only when 2276 evidence independently proves
that same fact. Record both the historical suggestion and the independent
target evidence; never hide earlier provenance.

## Fields and layouts

- Verify every offset and width from 2276. A cross-build layout is not proof.
- If behavior proves an offset but not its meaning, use `field_<offset>`.
- Use `pad_<offset>[n]` only for bytes not observed accessed.
- When one member becomes proven, split only the necessary range and preserve
  the surrounding unknown bytes.
- Alias unions are acceptable only when every semantic alias has independent
  target evidence. A neutral alias does not validate a semantic one.
- Add `co()`/`cs()` checks for edited layouts.

## Functions, parameters, and locals

- Keep `FUN_<address>` until a target string, table role, or complete caller/
  callee behavior proves a semantic function name.
- Parameter names require independently proven meaning at the ABI boundary.
  Immutable `@<reg>` annotations are ABI evidence and must not be changed to
  fit a name.
- Local names may describe a value that target data flow proves. Do not copy
  local names merely because another build uses them.

## Enums and constants

- Preserve raw values until 2276 dispatch, strings, masks, or consumers prove
  the member meaning.
- A target-proven meaning may use a fresh descriptive spelling even when an
  external project first suggested where to look.
- Do not infer an enum count from the highest observed value.

## Review checklist

1. Cite the target address/string/call sites or runtime observation.
2. Check every caller or consumer needed to disambiguate the role.
3. Separate proven layout from proposed terminology.
4. Record external historical influence in the provenance ledger.
5. Prefer an explicit unknown whenever the target evidence remains ambiguous.
