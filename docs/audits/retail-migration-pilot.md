# Retail migration pilot - 2026-10-02

Keep the debug 2276 target canonical for current verification. This pilot supports
reusing selected implementations in a future retail target; it does not support
rebasing all addresses or declaring the reconstruction independent of its history.

## Inputs and selection

The user supplied a local NTSC retail XBE from their physical-copy collection.
Retail SHA-256: `ed3a8e962351ad6c4b3b620768fb6a0bda658963390252439ea036d5ede3a3ac`.
Debug SHA-256: `a3402b021833dd2a3c368786f239480d68a6394bdf540569fab18340a83ab827`.
No executable or executable bytes are included in the published results.

The sample has 50 functions: AI 15, objects 8, render 6, game 6, network/UI 5,
math/physics 5, platform/library 5. It favors existing unique-window anchors but
also forces twelve substantive, often unanchored controls. This is a feasibility
sample, not an unbiased estimate of migration completion. Existing names are
historical navigation labels; matching does not independently prove those names.

## Results

| Result | Functions | Meaning |
|---|---:|---|
| Complete normalized body with scanned entry reference | 11 | Strong candidates; caller, global and callee roles still need review |
| Complete normalized body without scanned entry reference | 10 | Function boundary additionally unproven |
| Partial window anchor | 19 | Candidate neighborhood, not a mapped function |
| No unique window anchor | 10 | Another mapping method or reconstruction required |

Across the earlier 1,008 candidate anchors, 355 complete normalized bodies match.
That denominator excludes unanchored functions, so this is not a whole-program
reuse percentage. Details: [retail-pilot-results.json](retail-pilot-results.json).

Both images must decode the entire original function span. Comparison preserves
registers, access widths, base/index/displacement layouts, non-address constants,
instruction order and internal branch edges. External calls and absolute addresses
are normalized and recorded separately. Original padding is excluded. Identical
normalized bodies therefore do not by themselves prove corresponding callees,
global pointees or runtime state.

The results record 40 global/address operand comparisons and 13 external call/tail
comparisons. Load-time byte equality is only a diagnostic, not proof of global
semantics. Frame argument reads and return cleanup are partial ABI evidence;
register arguments, floating-point consumers and stack behavior need caller work.
Entry references come from a linear `.text` scan that can decode embedded data;
the scan is a candidate generator, not independent validation of every caller.

One small control was reviewed separately: historical label `fast_ftol_C`, debug
`0xd1c50`, retail `0x11c40`. Its 56-byte body is exactly identical, uses one stack
float slot, returns an integer in EAX, has no global operands or callees, and ends
with `ret`. Retail caller instructions at `0xdb7d8` load a float, reserve one
stack slot, multiply, store the argument to `[esp]`, call at `0xdb7e5`, and clean
four bytes at `0xdb7ea`. This is independently observed binary/ABI evidence for
this helper, not a runtime test or proof of a source implementation.

Several priority cases have no unique anchor, including `actor_combat_update`,
`actor_compute_prop_unopposable`, `actor_emotion_unopposable_retreat`,
`actor_perception_find_sense_position`, `ai_communication_get_player_rating`, `prop_add`,
`prop_setup_orphan`, `glow_initialize`, `object_type_adjust_placement`, and the
fog-wind routine at `0x166210`. Do not transfer their debug layouts/ABIs by analogy.

## Reproduction and next experiment

`tools/audit/retail_migration_pilot.py` consumes locally supplied images, existing
function bounds and the private first-survey anchor JSON. Anchor records contain
`reference`, `name`, `candidate_entry_estimate`, `unique_window_hits` and
`window_count`. Anchor production and all candidate addresses remain hypotheses.
Use Capstone from the local research environment; the ordinary CI needs no XBE.

```text
rtk python3 tools/audit/retail_migration_pilot.py --debug <local-debug-xbe> --retail <local-retail-xbe> --bounds tools/verify/function_bounds.json --anchors <private-anchor-json> --output <local-results-json>
```

Next, establish retail boundaries, caller ABI, callee identity and global roles for
the eleven entry-referenced candidates; independently map one substantive AI
control; then test a few retail patches locally using a separate retail address
namespace. Require per-function retail evidence before activation. Do not change
the current KB or reuse its absolute addresses for retail.

This pilot bounds the first tranche at 21 candidate bodies and leaves 29 sampled
functions requiring stronger evidence. A full migration remains weeks to months
of work, especially for debug assertions, optimized functions, globals and ABI.
The benefit is a reproducible canonical input tied to a retail copy. It does not
erase historical PAL/CEA/PDB assistance or establish a legal conclusion.
