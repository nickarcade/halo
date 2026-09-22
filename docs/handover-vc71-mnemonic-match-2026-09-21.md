# Handover

## Objective
Finish migration from misleading VC71 "byte accuracy" language to explicit
mnemonic-sequence terminology, while reserving strict raw-byte auditing for a
future separate tool.

## Current State
- Work is partial. Do not claim migration complete yet.
- No OpenCode goal is active.
- User decision: keep one canonical VC71 text contract (`NN.N% mnemonic match`);
  do not preserve parser support for the old `NN.N% match` form.
- User decision: defer real byte measurement. Later tool must be strict literal
  comparison, report `raw-byte exact` / `bytes differ` / `not comparable`, and
  record fresh candidate/reference provenance. Do not add relocation masking now.

## Confirmed
- Before latest edits, `rtk python3 -m py_compile ...` failed because
  `tools/lift_pipeline.py:915` lacked indentation after an `if`; its attribution
  test failed for the same import error.
- Latest patch indented that line and changed the target-specific parser to
  return `(None, False)` when no named VC71 status line exists. It no longer
  substitutes a neighboring function's score.
- `rtk git diff --check -- ...` ran after latest edits with no output.
- Existing dashboard changes already removed `VC71 >=90%` as a `Verified` gate.
  Latest patch keeps VC71 in a separate "Structural score" tooltip line.

## Important Changes
- `tools/verify/vc71_verify.py`: worktree output changed to
  `NN.N% mnemonic match`; not revalidated after the latest patch.
- `tools/lift_pipeline.py`: strict target-line parser; fixed indentation; generic
  `parse_match_percent()` remains for non-VC71 objdiff output.
- `tools/verify/vc71_regression.py`, `tools/verify/score_improve.py`,
  `tools/verify/fast_diff/compile_and_view.py`: strict VC71 parser patterns;
  old optional `(?:mnemonic)?` syntax removed.
- `tools/report/generate_decomp_report.py`: labels VC71 as mnemonic match,
  separates behavioral evidence from high mnemonic score, and labels operand
  score advisory.
- `tools/report/matching.py`, `tools/report/README.md`: clang objdiff result now
  described as a mnemonic-sequence structural diagnostic, not byte accuracy.
- `docs/verification_explained.md`: added claim/evidence table.
- Existing related edits also touch `docs/lift-policy.md`,
  `docs/snapshot-verification.md`, `docs/verification_policy.md`, and
  `docs/z3-equivalence.md`.

## Validation
- Failed before latest patch:
  `rtk python3 -m py_compile tools/lift_pipeline.py tools/report/generate_decomp_report.py tools/verify/fast_diff/compile_and_view.py tools/verify/score_improve.py tools/verify/test_match_attribution.py tools/verify/vc71_regression.py tools/verify/vc71_verify.py`
- Failed before latest patch:
  `rtk python3 tools/verify/test_match_attribution.py`
- Passed after latest patch:
  `rtk git diff --check -- docs/ tools/lift_pipeline.py tools/report/generate_decomp_report.py tools/report/matching.py tools/report/README.md tools/verify/fast_diff/compile_and_view.py tools/verify/score_improve.py tools/verify/test_match_attribution.py tools/verify/vc71_regression.py tools/verify/vc71_verify.py`
- Not yet run after latest patch: Python compile, attribution test, VC71 regression
  tests, and dashboard report generation/inspection.

## Uncertain / Risks
- `tools/verify/vc71_scores.json` has a large unrelated rewrite: latest observed
  diff is 18,422 additions and 33,994 deletions. Do not include it without
  provenance or owner confirmation.
- `README.md` and `tools/verify/compare_obj.py` are modified but were not part of
  this implementation patch; inspect ownership before changing or staging them.
- Strict parser changes may expose callers or tests still producing old status
  text. Treat that as an incomplete migration, not a reason to restore fallback.
- No raw-byte audit exists yet. Current `Raw-byte exact` wording is a reserved
  evidence claim, not a delivered measurement.

## Next Steps
1. Run the Python compile command and `rtk python3 tools/verify/test_match_attribution.py`; fix only migration defects.
2. Run focused VC71 regression tests and inspect active user-facing text with a
   scoped `rtk rg` for byte-accuracy claims.
3. Generate and inspect dashboard/report output. Confirm `Verified` reflects only
   behavioral evidence and mnemonic match is never called byte accurate.
4. Separate unrelated `vc71_scores.json`, `README.md`, and `compare_obj.py`
   changes before staging.
5. Later, separately design and implement strict fresh raw-byte audit with no
   normalization or masking in its first version.

## Resume Prompt
Resume terminology migration from `docs/handover-vc71-mnemonic-match-2026-09-21.md`.
Do not restore old VC71 output parsing. Validate current patches first, complete
remaining active user-facing wording, and keep raw-byte auditing as separate
future work.
