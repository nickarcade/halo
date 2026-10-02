# Retired CEA Ghidra workflow

This file is a historical inventory for provenance remediation. The CEA/PDB
workflow is outside the public project's normal process and must not supply
authoritative names, layouts, implementation bodies, or source shape. See
`PROVENANCE.md`. Remaining scripts are retained temporarily so existing
CEA-derived changes can be traced; move them out of the public tree after the
remediation ledger records their dependent claims.

## Scripts

### `CeaTypeExtract.java`

Extracts named and anonymous structs, unions, enums, fields, and type references from a PDB's type information without modifying the open program.

- **Overrides:** `getScriptArgs()[0]` = PDB path; `[1]` = output JSON path. Empty values retain the defaults.
- **Default input:** `C:\path\to\Halo_1_Combat_Evolved_Anniversary_(Jun_24,_2011)\en_us\HCEX.pdb` (placeholder; supply the real PDB path via `getScriptArgs()[0]`)
- **Default output:** `G:\dev\halo\artifacts\ghidra_groom\type_corpus\cea_debug_types_raw.json`
- **Artifact:** JSON array of composite and enum type records, including sizes, fields, offsets, type indices, and enum members.
- **Remaining non-portable paths:** both default Windows paths. This script is
  retained only to trace historical provenance and is not part of normal use.

### `CeaAssertLines.java`

Reads assert/error call sites from the open program and recovers source file and line arguments from nearby pushes. It is read-only.

- **Overrides:** none; this script does **not** call `getScriptArgs()`.
- **Hardcoded output:** `G:\dev\halo\artifacts\ghidra_groom\cea_corpus\our_assert_lines_raw.tsv`
- **Artifact:** tab-separated containing function entry/name, source file, line, callsite, and assert kind.
- **Remaining non-portable paths:** the hardcoded output path. The source-string matcher also expects `c:\...*.c` strings.

### `CeaDumpNames.java`

Dumps every function entry and current name from the open program so corpus matching can avoid collisions. It is read-only.

- **Overrides:** none; this script does **not** call `getScriptArgs()`.
- **Hardcoded output:** `G:\dev\halo\artifacts\ghidra_groom\cea_corpus\ghidra_func_names.tsv`
- **Artifact:** tab-separated `address`/`name` rows and function counts printed in the Ghidra console.
- **Remaining non-portable paths:** the hardcoded output path.

### `CeaApplyRenames.java`

Applies or dry-runs CEA-PDB function renames from a TSV, checking the current function name and rejecting missing, mismatched, or colliding targets.

- **Overrides:** `getScriptArgs()[0]` = input rename TSV; `[1]` = literal `APPLY` for changes. Omit the second argument for the default dry run. The script requires at least the first argument.
- **Hardcoded output:** `G:\dev\halo\artifacts\ghidra_groom\ghidra_rename_result.txt`
- **Artifact:** result log with mode, rename counts, and per-entry mismatch/collision/not-found details. In `APPLY` mode it also adds CEA plate comments.
- **Remaining non-portable paths:** the hardcoded result path; the rename TSV is portable only when supplied explicitly.

### `CeaImportTypes.java`

Imports verified CEA datum/math structures from a flat TSV into the `/halo/cea` Ghidra data-type category with packing disabled.

- **Overrides:** `getScriptArgs()[0]` = input type TSV; omit it to use the default.
- **Default input:** `G:\dev\halo\artifacts\ghidra_groom\type_corpus\import_work.tsv`
- **Hardcoded output:** `G:\dev\halo\artifacts\ghidra_groom\type_corpus\import_result.txt`
- **Artifacts:** imported/replaced data types under `/halo/cea`, plus an import log with type/field counts and failures.
- **Remaining non-portable paths:** both default input and result paths above; the category path `/halo/cea` is also fixed.

### `CeaImportEnums.java`

Imports CEA game enums from a flat TSV into the `/halo/cea` Ghidra data-type category, skipping name collisions.

- **Overrides:** `getScriptArgs()[0]` = input enum TSV; omit it to use the default.
- **Default input:** `G:\dev\halo\artifacts\ghidra_groom\type_corpus\enum_import_work.tsv`
- **Hardcoded output:** `G:\dev\halo\artifacts\ghidra_groom\type_corpus\enum_import_result.txt`
- **Artifacts:** newly created enum data types under `/halo/cea`, plus an import log with enum/member counts and skipped collisions.
- **Remaining non-portable paths:** both default input and result paths above; the category path `/halo/cea` is also fixed.

## Important corpus distinction

`CeaTypeExtract` consumes an HCEX PDB and produces intermediate CEA type
artifacts. Those outputs are distinct from the `halocea` decompiled corpus,
but both remain external hypotheses under `PROVENANCE.md`.
