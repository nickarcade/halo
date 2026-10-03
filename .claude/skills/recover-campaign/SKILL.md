---
name: recover-campaign
tier: user
description: Target-only readability campaign for already lifted Halo Xbox source, using 2276 evidence and byte-neutral gates.
triggers: ["recover campaign", "readability campaign", "cleanup campaign"]
---

# Target-only recovery campaign

Improve readability without changing behavior and without consulting external or
cross-build material (other builds, PDB-derived corpora, or their implementation bodies).
`PROVENANCE.md` and `naming-confidence` govern every recovered name and layout.

## Inputs

Use only:

- 2276 target disassembly, embedded strings/asserts/tables, data accesses, and
  complete call-site evidence;
- independently captured runtime behavior, with raw captures kept outside Git;
- existing source whose provenance is already target-verified;
- open specifications or independently licensed source when applicable.

Historical external-source notes may identify a remediation target. They are
not implementation or naming evidence.

## Loop

1. Scope one translation unit or one coherent type cluster.
2. Record a fresh target-byte and build baseline.
3. Inspect callers, target code, ABI, types, and current scoped diff.
4. Apply one evidence class at a time: target-proven names, target-proven
   constants/enums, target-proven field access, then control-flow cleanup.
5. Run the neutral target-byte gate after every category. Revert the category
   if unexplained bytes change.
6. Run non-vacuous equivalence or runtime checks for control-flow changes.
7. Run build, ABI/type gates, and `check_lift_hazards.py --changed-only`.
8. Update the provenance remediation ledger with target addresses, result, and
   remaining uncertainty.

Do not erase comments that disclose external or cross-build influence. Replace them only
when a new comment records both the historical influence and the independent
2276 evidence that now supports the claim.

Raw byte agreement, mnemonic similarity, synthetic equivalence, patched-XBE
activation, emulator behavior, and hardware behavior remain separate results.
