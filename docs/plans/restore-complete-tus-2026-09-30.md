# Restore the 14 TUs that lost 100% completion

Status as of 2026-09-30, main at `cbfb65b2a`.

**Completed 2026-09-30.** All 48 functions are `ported: true` in kb.json after
`8626d9ae0` (dormant lifts) and `71f443a13` (activation after a patched-build
boot).

The standalone nxdk bring-up found functions that were missing from `kb.json`
(c22a24def, re-homed in a1c9e9bde). Nothing was un-ported. The new functions
landed in 14 objects that were fully ported before, so the count of
fully-ported objects dropped from 113 to 99.

This list holds the **48 unported functions** in those 14 objects. When all
48 are ported:

- Fully-ported objects: 99 → 113.
- Game-code progress: 6,416 / 7,011 (91.5%) → 6,464 / 7,011 (92.2%).

The denominator stays at 7,011, so progress will not go back to 93.5%. The
old 93.5% was computed over a function list that was missing 147 real
functions.

## Rules

- Every new body goes through `/lift` (CLAUDE.md rule 12), even an empty
  `RET` stub.
- New and dormant bodies stay `ported: false` (kb_meta `"unknown"`) until the
  patched build boots with them active. Activation needs that proof. It is
  never only a change to the flag.
- Every `RET` stub below is a plain `C3` (cdecl, pops nothing). Callers reach
  most of them through a function-pointer table. A `void f(void) {}` body is
  ABI-correct even when the table caller passes arguments.
- Check the "Watch" column before you lift. Some `void(void)` declarations in
  kb.json are wrong.

## Tier A: activate existing dormant bodies (16)

These functions already have C bodies (`ported: false`). Boot the patched
build with each one active, then set `ported: true`.

| Object | Addr | Function | Source |
|---|---|---|---|
| devices.obj | 0x96060 | devices_initialize | devices/devices.c |
| devices.obj | 0x960b0 | devices_dispose_from_old_map | devices/devices.c |
| devices.obj | 0x969e0 | devices_initialize_for_new_map | devices/devices.c |
| effects.obj | 0x9c990 | effects_disconnect_from_structure_bsp | effects/effects.c |
| game_state.obj | 0x1bfbd0 | game_state_set_revert_time | saved_games/game_state.c |
| glow.obj | 0x132f50 | glow_initialize_for_new_map | objects/widgets/glow.c |
| glow.obj | 0x132f80 | glow_dispose_from_old_map | objects/widgets/glow.c |
| glow.obj | 0x133750 | glow_initialize | objects/widgets/glow.c |
| objects.obj | 0x139740 | lights_disconnect_from_structure_bsp ¹ | objects/objects.c |
| objects.obj | 0x13cf30 | object_types_reconnect_to_structure_bsp ¹ | objects/objects.c |
| projectiles.obj | 0xf7c90 | projectiles_dispose_from_old_map | items/projectiles.c |
| structure_detail_objects.obj | 0x1939e0 | structure_detail_objects_flush | structures/structure_detail_objects.c |
| units.obj | 0x1a7f00 | units_initialize | units/units.c |
| units.obj | 0x1a7f40 | units_initialize_for_new_map | units/units.c |
| units.obj | 0x1a7f60 | units_dispose_from_old_map | units/units.c |
| units.obj | 0x1b4dc0 | unit_damage_aftermath | units/units.c |

¹ The real TUs are `object_lights.c` and `object_types.c`. Moving these
functions there is a separate task. Leave them in objects.obj until then.

Scores measured 2026-09-30 (VC71 from `vc71_scores.json`, raw from
`raw_xbe_structural.py`):

- **14 of 16 are exact:** VC71 100 and raw bytes 100%.
- **unit_damage_aftermath:** VC71 98.1 and raw 88.5%. It meets the VC71
  target of 90 or more. Raw bytes are below 100%.
- **lights_disconnect_from_structure_bsp:** VC71 72 and raw 28%. It is
  **below the target**. The logic is complete. The original build inlines
  `light_disconnect_from_map` (0x1396e0, ported, VC71 97.2). Our build
  emits a `call` to it, so the inlined block shows as missing, and ESI/EDI
  are allocated the other way round. Activating it is safe for behaviour,
  but the score needs the callee inlined first.

## Tier B: empty `RET` stubs (21)

Each function is a 1-byte `RET`, and callers reach each one only through a
function-pointer table.

| Object | Addr | Function | Table ref |
|---|---|---|---|
| game_engine.obj | 0xafeb0 | FUN_000afeb0 | 0x2efe90 |
| game_engine.obj | 0xaff60 | FUN_000aff60 | 0x2efe98 |
| game_engine.obj | 0xaff90 | FUN_000aff90 | 0x2efea0 |
| game_engine.obj | 0xb01f0 | FUN_000b01f0 | 0x2efee4 |
| game_engine.obj | 0xb0420 | FUN_000b0420 | 0x2efef4 |
| game_engine.obj | 0xb1150 | FUN_000b1150 | 0x2eff18 |
| game_engine.obj | 0xb14d0 | FUN_000b14d0 | 0x2eff20 |
| game_engine.obj | 0xb2690 | FUN_000b2690 | 0x2efff0 |
| game_engine.obj | 0xb26d0 | FUN_000b26d0 | 0x2f0000 |
| game_engine.obj | 0xb36e0 | FUN_000b36e0 | 0x2f0078 |
| game_engine.obj | 0xb38f0 | FUN_000b38f0 | 0x2f0080 |
| game_engine.obj | 0xb3930 | FUN_000b3930 | 0x2f0088 |
| game_engine.obj | 0xb3c50 | FUN_000b3c50 | 0x2f00a8 |
| glow.obj | 0x132f40 | glow_dispose | 0x32358c |
| objects.obj | 0x135410 | FUN_00135410 | none found (no xrefs) |
| projectiles.obj | 0xf7c70 | projectiles_initialize | 0x324118 |
| projectiles.obj | 0xf7c80 | projectiles_initialize_for_new_map | 0x324120 |
| projectiles.obj | 0xf7d20 | projectile_delete | 0x324134 |
| units.obj | 0x1a7fe0 | FUN_001a7fe0 | 0x323cd4 |
| weapons.obj | 0xfad20 | weapons_initialize | 0x323f38 |
| weapons.obj | 0xfad30 | weapons_initialize_for_new_map | 0x323f40 |

The game_engine stubs are empty callbacks in the game-engine definition
tables at 0x2efe90–0x2f00a8. The slot position in each table gives the
callback name, which can replace `FUN_` in the same change.

## Tier C: tiny functions and thunks (3)

| Object | Addr | Function | Shape | Watch |
|---|---|---|---|---|
| ui_widget_text_search_and_replace_functions.obj | 0xf52e0 | widget_replace_function_null | `MOV EAX,0x26cdf0; RET` (6 B) | **Returns a value.** The `void(void)` declaration is wrong. Before the lift, find what 0x26cdf0 is (most likely a string constant) and fix the return type in kb.json. This function is entry [0] of the table at 0x31e5a4. |
| rasterizer_sprites.obj | 0x17cdd0 | FUN_0017cdd0 | `JMP 0x160990` (5 B thunk) | The target is a `RET` stub in rasterizer_xbox_environment.obj. Referenced from FUN_00195c40. |
| rasterizer_sprites.obj | 0x17ce20 | FUN_0017ce20 | `JMP 0x160bb0` (5 B thunk) | The target is a `RET` stub in rasterizer_xbox_environment.obj. Referenced from FUN_00195cb0. |

## Tier D: small real functions (8)

| Object | Addr | Function | Size | Callees / notes |
|---|---|---|---|---|
| network_messages.obj | 0x11c1f0 | FUN_0011c1f0 | 28 B | **Register argument:** reads `[ESI+4]` and `[ESI]` without setting ESI. The declaration needs `@<esi>`. No direct xrefs. |
| objects.obj | 0x1399f0 | FUN_001399f0 | 46 B | Tests the byte at 0x5a8d60, then calls FUN_0008d9f0 and thunk_FUN_001029a0. No xrefs. |
| scenario.obj | 0x18e240 | FUN_0018e240 | 27 B | Loop over 10 entries at 0x326a44. Calls FUN_0013cb30 and FUN_00140750. No xrefs. |
| scenario.obj | 0x18e260 | FUN_0018e260 | 27 B | Loop over 13 entries at 0x326a10. Calls FUN_001417c0 and FUN_0013b150. No xrefs. |
| scenario.obj | 0x18e480 | FUN_0018e480 | 73 B | Tests the dword at 0x5064e4. Calls FUN_0008d9f0, tag_block_get_element and thunk_FUN_001029a0. No xrefs. |
| game_engine.obj | 0xb33a0 | FUN_000b33a0 | 204 B | Game-engine table callback (0x2f002c). Calls object_get_and_verify_type, datum_get, FUN_000b5aa0, FUN_000b2610, FUN_000b2e70 and FUN_000a93xx/a94xx/a95xx. |
| game_engine.obj | 0xb2f00 | FUN_000b2f00 | 262 B | Game-engine table callback (0x2efff4). Calls global_scenario_get, csmemset, FUN_000ad270, FUN_0008f390, FUN_000a9350 and FUN_000b2e70. |
| game_engine.obj | 0xb0e50 | FUN_000b0e50 | ? | **Ghidra has no function here yet.** Create it first. The pointer at 0x2efecc references it. It starts after ctf_spawn_equipment's padding. It replaces the bogus 0xb0e77 entry. |

"No xrefs" means Ghidra found no references. These may be dead code, or
callers may reach them in a way Ghidra did not resolve. Search for a
caller before assuming they are unused.

## Order per object

Finishing a whole object restores its 100% status. The cheapest order:

| Object | Needs | Tiers |
|---|---|---|
| weapons.obj | 2 stubs | B |
| ui_widget_text_search_and_replace_functions.obj | 1 tiny | C |
| rasterizer_sprites.obj | 2 thunks | C |
| effects.obj | 1 activation | A |
| game_state.obj | 1 activation | A |
| structure_detail_objects.obj | 1 activation | A |
| devices.obj | 3 activations | A |
| projectiles.obj | 3 stubs + 1 activation | B, A |
| glow.obj | 1 stub + 3 activations | B, A |
| units.obj | 1 stub + 4 activations | B, A |
| network_messages.obj | 1 small (`@<esi>`) | D |
| scenario.obj | 3 small | D |
| objects.obj | 1 stub + 1 small + 2 activations | B, D, A |
| game_engine.obj | 13 stubs + 3 real functions | B, D |

Suggested batching: one `/lift` batch for Tiers B and C (24 functions,
mostly empty bodies). Then one boot-and-activate pass for Tier A. Lift
Tier D functions one at a time.

## Not in scope

The same discovery put 35 named functions in the wrong existing TU:
UI event handlers, `ai_profile_*`, `structure_decals_*`,
`placeholder_*`/`scenery_*` and `device_*` init/dispose. Fixing them means
moving source between `.c` files. None of them affects the 14 objects
above.
