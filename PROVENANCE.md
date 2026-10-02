# Provenance policy

This project reconstructs Halo CE Xbox behavior from executable evidence and
independent observation. Future targets must come from lawfully accessed,
user-owned copies. It does not publish
game executables, game assets, debug-symbol packages, raw memory captures, or
whole-binary decompiler output.

The current technical compatibility target is historical debug build 2276.
Its acquisition provenance has not been independently established. Owning a
retail disc does not establish the origin of that debug executable. A
user-supplied NTSC retail 2276 XBE is now recorded as a migration candidate;
no canonical-target migration has occurred. Technical authority below means
authority about behavior, not a claim about acquisition rights or clean-room
development. Record acquisition evidence separately from behavioral evidence.

## Evidence taxonomy

Every authoritative claim must identify the strongest evidence that supports
it. More than one class may apply.

| Class | Evidence | Permitted authoritative use |
|---|---|---|
| `TGT-BIN` | Target-build instructions, data accesses, embedded strings, asserts, tables, relocations, and call sites | Logic, ABI, offsets, widths, control flow, constants, and names literally present in the target |
| `TGT-RUN` | Reproducible target-runtime observations, with acquisition status recorded separately | Side effects, state transitions, rendering, networking, timing bounds, and failure behavior |
| `TGT-TEST` | Independently constructed tests, synthetic fixtures, and target-versus-reimplementation measurements | Equivalence within the stated inputs and coverage; never broader correctness by itself |
| `OPEN-SRC` | Public specifications and independently licensed source | API contracts and algorithms within the source's license and documented applicability |
| `EXT-HYP` | PAL 2342, CEA/HCEA, PDB-derived projects, other leaked/prototype builds, or other cross-build work | A search hypothesis only; it cannot authorize a name, layout, body, ABI, or control-flow claim |
| `HIST` | Accurate record that earlier work used external or uncertain material | Historical disclosure and remediation tracking only |

`TGT-BIN` and `TGT-RUN` are authoritative for build 2276. `TGT-TEST` is
authoritative only for its documented test domain. An `EXT-HYP` fact becomes
authoritative only after independent target evidence demonstrates that same
fact. The record must cite the target evidence and retain the historical
external-source disclosure.

## Naming and layout

- A semantic name may be used when a target string supplies it or target
  behavior independently proves the role.
- A spelling found only in external cross-build material remains a hypothesis.
  Use `field_<offset>`, `unknown_<role>`, or `FUN_<address>` until target
  evidence supports a semantic name.
- Preserve independently proven offsets, sizes, strides, widths, and ABI while
  neutralizing an unsupported name. Do not discard a valid layout merely
  because an external source once suggested terminology.
- Parameter and local names have the same rule as fields. Convenience does not
  make external source shape authoritative.

## Implementation and verification

- Reconstruct implementation bodies and control flow from target disassembly,
  target call sites, target data, and runtime behavior.
- External builds may suggest what to inspect. They may not supply an
  implementation body, source-level branch/loop shape, struct layout, field
  layout, local name, or parameter name without independent target proof.
- Keep raw-byte agreement, mnemonic similarity, ABI checks, synthetic
  equivalence, patched-XBE activation, emulator behavior, and hardware behavior
  as separate claims.
- Record negative and conflicting evidence. Do not remove old provenance notes
  merely to make current work appear independently derived.

## Repository boundary

The public repository may contain source code, small independently produced
test fixtures, addresses, hashes, disassembly-derived metadata, evidence
summaries, and reproducible analysis tools. The following stay outside Git:

- any original or patched game executable and game asset;
- PDBs and other proprietary debug-symbol packages;
- XBE-derived COFF objects and wholesale disassembly/decompiler exports;
- raw RAM, save-state, or game-state captures, including hex-encoded copies;
- generated corpora that embed substantial target bytes.

Local workflows may consume an ignored, user-supplied target file. They should
emit derived caches under ignored artifact directories and publish only the
minimum non-expressive measurements needed for review.

Live-capture fixtures are local inputs even if trimmed, renamed, or labeled as
regression tests. Constructed fixtures need an explicit generation record;
large memory-shaped fixtures have exact-hash review exceptions in
`tools/audit/reviewed_synthetic_fixtures.json`. An exception does not extend to
new files, modified bytes, executable/object signatures, or compressed copies.

## Review record

Material influenced by `EXT-HYP` must be listed in the provenance remediation
ledger until target-only reconstruction is complete. A completed row records:

1. the historical source and scope of influence;
2. the target addresses, call sites, strings, runtime trace, or tests used for
   independent verification;
3. the result and remaining uncertainty;
4. the commit that performed the target-only reconstruction.
