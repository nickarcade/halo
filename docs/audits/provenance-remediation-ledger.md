# Provenance remediation ledger

Audit: 2026-10-02. Historical influence is retained. A citation in an existing
comment is a lead until freshly checked; compilation does not establish provenance.

Status vocabulary: `HIST/UNVERIFIED`, `TARGET-CFG-REVIEW`, `TARGET-MODEL-CHECK`,
`CANDIDATE-EQUIVALENCE`, `RUNTIME-VERIFIED`. Verification is limited to recorded coverage.
No body below has been newly reconstructed from an external corpus in this audit.

| Priority / item | Historical influence | Independent evidence and current status | Next verification |
|---|---|---|---|
| P0: `actor_combat_update`, 0x22dc0, actor_combat.c | PAL/CEA-assisted actor/debug layout and naming dependencies; body-shape influence still needs historical confirmation | Existing target comments; `ported=false`; HIST/UNVERIFIED for provenance | Rebuild CFG, call ABI, stores and state transitions from 2276; independently review disabled implementation before activation |
| P0: `action_wait_perform`, 0x1beb0, actions.c | Explicit PAL source-level structured shape at line 271 | HIST/UNVERIFIED; related control/setup/update 0x1c030/0x1c0e0/0x1c190 | Target-only branch/order review and independently constructed branch fixtures |
| P0: `object_type_*`, objects.c, lines 4378-4803; region walk near 6596 | Explicit PAL loop form, early-exit and callback shape | Existing comments report target list +0x5c, stride 4, NULL terminator and predicates; fresh review pending | Independently derive each callback loop, termination, ordering and exceptional return; do not infer all callbacks from one verified loop |
| P0: `glow_initialize`, 0x133750, objects/widgets/glow.c | Explicit PAL initialization shape | Target strings cited at 0x29ab58/0x29ab48/0x29ab1c/0x29aaf8 are leads | Verify allocation ordering, capacities, failure handling and initialization stores |
| P1: `actor_compute_prop_unopposable`, 0x2fc20-0x2fd0d, actor_perception.c | PAL state terminology and prop-field names | TARGET-CFG-REVIEW: current C conditions/stores checked against pristine MD5 c7869590a1c64ad034e49a5ee0c02465. TARGET-MODEL-CHECK: 49,152 cases; 71 instruction addresses executed; AL return, two datum_get handles/order, complete synthetic actor/prop regions, store widths/order matched | Compile-candidate comparison and live-runtime coverage remain open; synthetic datum_get stubs do not prove real pool lookup behavior |
| P1: actor_perception.c 0x2fd10/0x30d10/0x30f50/0x31a90/0x32cb0 | PAL filters, grouping and terminology, including single combined filter | HIST/UNVERIFIED; existing target comments are leads | Re-derive predicates, x87 operation order, buffer sizes and early returns |
| P1: ai_communication.c 0x441c0/0x44fd0/0x460e0/0x46530/0x46f10 | PAL/CEA state and structure hypotheses; some target/PAL differences explicitly recorded | HIST/UNVERIFIED; preserve notes about opposite alive predicate and different call arguments | Derive all branches/stores from 2276; verify each immutable register ABI; retain dormant implementations until independently reviewed |
| P1: rasterizer_xbox_environment_fog.c, including 0x166010/0x166210 | PAL structure/field framework | Target-specific short scroll-offset and per-window differences reported; fresh provenance review pending | Verify layouts, random-call counts, allocation/update order, screen lifecycle and floating-point behavior |
| P2: point_physics.c parameter at line 2765; action locals and peripheral glow names | Explicit PAL parameter/local terminology; lower evidence of body copying | HIST/UNVERIFIED name claims | Keep independently descriptive role names only after target behavior/call-site proof; prioritize bodies above cosmetic renames |
| Types: prop_t | Historical PAL grouping and field terminology | Size/offset assertions retained; selected offsets independently accessed at 0x2fc20. Neutral names do not validate other historic widths | Independently inspect prop_add 0x64170, prop_setup_orphan 0x647c0, consumers and each remaining semantic claim |
| Types: actor_debug_info_t / actor clusters | Historical PAL fields | Two unused names neutralized; target word/FST-store comments on other fields are leads | Re-verify 0x22dc0 stores, byte/word widths and semantic roles |
| Types: rasterizer/debug/window/frame/shader/vertex; speech/powerup/object; remaining actor/AI clusters | PAL/CEA/halocea names/layout suggestions in types.h | 137 explicit claim lines inventoried; layout and field-name proof tracked separately | Per-name target strings/behavior proof; neutral field_<offset> when unsupported; do not discard proven layout |

## Neutral names applied without changing layout

All 17 former names below had no named consumers outside types.h. Storage,
packing, types, widths, offsets and size assertions were preserved. Several
fields are accessed by offsets; absence of named consumers is not absence of use.

| Type / offset | Historical name | Public name | Independent evidence |
|---|---|---|---|
| prop_t +4c | ticks_until_orphan | field_4c | Semantics/width pending |
| prop_t +58 | last_idle_look_interest | field_58 | Semantics/width pending |
| prop_t +5c | last_idle_look_time | field_5c | Semantics/width pending |
| prop_t +62 | ally_status_changed | field_62 | Semantics/width pending |
| prop_t +68 | unit_effect_decay_ticks | field_68 | Semantics/width pending |
| prop_t +6c | ticks_since_damage | field_6c | Existing prop_add offset citation; fresh semantic review pending |
| prop_t +70 | damage_inflicted_on_me | field_70 | Existing prop_add offset citation; fresh semantic review pending |
| prop_t +74 | currently_damaging_me | field_74 | Existing prop_add offset citation; fresh semantic review pending |
| prop_t +78 | visible_ticks | field_78 | Semantics/width pending |
| prop_t +9c | unreachable_ticks | field_9c | Word comparison @0x2fc6c; original semantic name not established |
| prop_t +a8 | unopposable_casualty_decay_timer | field_a8 | Semantics/width pending |
| prop_t +aa | unopposable_trigger_hysteresis | field_aa | Word clear @0x2fcba; original semantic name not established |
| prop_t +ac | unopposable_trigger_timer | field_ac | Word clear @0x2fcc8; original semantic name not established |
| prop_t +ae | unopposable_trigger_threshold | field_ae | Word clear @0x2fcc1; original semantic name not established |
| prop_t +102 | body_location_bonus | field_102 | Semantics/width pending |
| actor_debug_info_t +00 | last_render_id | field_00 | Semantics/width pending |
| actor_debug_info_t +04 | last_path_refresh | field_04 | Semantics/width pending |

`type-provenance-claims.tsv` preserves explicit claim text and line context;
its nearest preceding type is navigation context, not a semantic association.
`source-provenance-claims.tsv` inventories 559 explicit claims in 74 source/header
files and marks likely shape/layout cases for priority review. These inventories
are discovery lists, not certificates that all implicit influences are known.

Reproduce the new model check:

```
rtk python3 tools/audit/verify_prop_unopposable_target.py <local-pristine-2276-XBE>
```

No reconstruction commit exists yet: all changes are reviewable working-tree edits.
