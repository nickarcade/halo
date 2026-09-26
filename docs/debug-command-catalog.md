# Halo CE Xbox Debug Command Catalog

Generated from the pristine debug build 2276 XBE by
`tools/docs/debug_command_catalog.py`.

## How console dispatch works

Open the console with <kbd>~</kbd>. `hs_console_evaluate()`
(`src/halo/hs/hs.c`) wraps the line before compiling:

| You type | Becomes | Source table |
|----------|---------|--------------|
| `ai_render true` | `(set ai_render true)` | external globals `0x2f3708` (443) |
| `ai_render` | `(ai_render)` (global read) | external globals |
| `(cheat_all_weapons)` | pass-through | HS function table `0x2f1588` (418) |
| `cheat_all_weapons` | `(cheat_all_weapons)` | HS function table |

A leading word that matches a global name is treated as a `set`.
Anything else is wrapped as a function call. `;` blanks the line
(comment). In-game help: `(help <name>)` prints one entry;
`(script_doc)` writes all 418 function signatures + help to
`hs_doc.txt` — the Xbox build does **not** dump external globals
there, which is why this catalog exists.

## Area index

| Area | Globals | Functions |
|------|--------:|----------:|
| [AI](#ai) | 146 | 100 |
| [Animation](#animation) | 13 | 0 |
| [Camera](#camera) | 4 | 12 |
| [Cheats](#cheats) | 10 | 7 |
| [Cinematics](#cinematics) | 0 | 18 |
| [Collision](#collision) | 42 | 0 |
| [Console](#console) | 2 | 0 |
| [Control](#control) | 0 | 8 |
| [Debug](#debug) | 0 | 19 |
| [Devices](#devices) | 0 | 12 |
| [Effects](#effects) | 8 | 7 |
| [Game](#game) | 4 | 32 |
| [HUD/UI](#hudui) | 1 | 30 |
| [Input](#input) | 8 | 0 |
| [Lighting](#lighting) | 4 | 0 |
| [Logic](#logic) | 0 | 9 |
| [Math](#math) | 0 | 8 |
| [Misc](#misc) | 11 | 13 |
| [Network](#network) | 2 | 2 |
| [Objects](#objects) | 18 | 37 |
| [Pathfinding](#pathfinding) | 8 | 0 |
| [Physics](#physics) | 2 | 0 |
| [Player](#player) | 14 | 27 |
| [Profiling](#profiling) | 5 | 6 |
| [Renderer](#renderer) | 119 | 10 |
| [Screen effects](#screen-effects) | 0 | 1 |
| [Sound](#sound) | 7 | 16 |
| [Structures](#structures) | 8 | 0 |
| [Units](#units) | 7 | 44 |
| **Total** | **443** | **418** |

## External globals (set bare, no parens needed)

Type is the HS descriptor type at `desc+4`. Default is the value
backed by the live C variable in the pristine image (BSS tails read
as zero). Descriptions marked *(Reclaimers)* come from the PC/H1A
extract and are commentary, not Xbox binary evidence.

### AI

| Command | Type | Default | Description |
|---------|------|---------|-------------|
| `ai_debug_ballistic_lineoffire_freeze` | boolean | `—` | *(Reclaimers)* If enabled, the ballistic arc drawn by ai_render_ballistic_lineoffire will not update when new grenades are thrown. |
| `ai_debug_blind` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_debug_communication_focus_enable` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_debug_communication_random_disabled` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_debug_communication_timeout_disabled` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_debug_communication_unit_repeat_disabled` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_debug_deaf` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_debug_disable_wounded_sounds` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_debug_evaluate_all_positions` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_debug_fast_los` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_debug_flee_always` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_debug_force_all_active` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_debug_force_crouch` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_debug_force_vocalizations` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_debug_ignore_player` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_debug_invisible_player` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_debug_oversteer_disable` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_debug_path` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_debug_path_accept_radius` | real | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_debug_path_attractor` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_debug_path_attractor_radius` | real | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_debug_path_attractor_weight` | real | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_debug_path_disable_obstacle_avoidance` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_debug_path_disable_smoothing` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_debug_path_end_freeze` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_debug_path_flood` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_debug_path_maximum_radius` | real | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_debug_path_start_freeze` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_fix_actor_variants` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_fix_defending_guard_firing_positions` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_print_acknowledgement` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_print_allegiance` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_print_automatic_migration` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_print_bsp_transition` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_print_command_lists` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_print_communication` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_print_communication_player` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_print_conversations` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_print_damage_modifiers` | boolean | `—` | *(Reclaimers)* When an encounter is selected, damage modifiers are logged at the bottom of the screen. |
| `ai_print_evaluation_statistics` | boolean | `—` | *(Reclaimers)* Displays red text over AI whenever they vocalize, with the name of the dialogue field played. For example, pain body minor. |
| `ai_print_killing_sprees` | boolean | `—` | *(Reclaimers)* Displays red text over AI whenever they vocalize, with the name of the dialogue field played. For example, pain body minor. |
| `ai_print_lost_speech` | boolean | `—` | *(Reclaimers)* Displays red text over AI whenever they vocalize, with the name of the dialogue field played. For example, pain body minor. |
| `ai_print_major_upgrade` | boolean | `—` | *(Reclaimers)* Displays red text over AI whenever they vocalize, with the name of the dialogue field played. For example, pain body minor. |
| `ai_print_migration` | boolean | `—` | *(Reclaimers)* Displays red text over AI whenever they vocalize, with the name of the dialogue field played. For example, pain body minor. |
| `ai_print_oversteer` | boolean | `—` | *(Reclaimers)* Displays red text over AI whenever they vocalize, with the name of the dialogue field played. For example, pain body minor. |
| `ai_print_placement` | boolean | `—` | *(Reclaimers)* Displays red text over AI whenever they vocalize, with the name of the dialogue field played. For example, pain body minor. |
| `ai_print_pursuit_checks` | boolean | `—` | *(Reclaimers)* Displays red text over AI whenever they vocalize, with the name of the dialogue field played. For example, pain body minor. |
| `ai_print_respawn` | boolean | `—` | *(Reclaimers)* Displays red text over AI whenever they vocalize, with the name of the dialogue field played. For example, pain body minor. |
| `ai_print_rule_values` | boolean | `—` | *(Reclaimers)* Displays red text over AI whenever they vocalize, with the name of the dialogue field played. For example, pain body minor. |
| `ai_print_rules` | boolean | `—` | *(Reclaimers)* Displays red text over AI whenever they vocalize, with the name of the dialogue field played. For example, pain body minor. |
| `ai_print_scripting` | boolean | `—` | *(Reclaimers)* Displays red text over AI whenever they vocalize, with the name of the dialogue field played. For example, pain body minor. |
| `ai_print_secondary_looking` | boolean | `—` | *(Reclaimers)* Displays red text over AI whenever they vocalize, with the name of the dialogue field played. For example, pain body minor. |
| `ai_print_speech` | boolean | `—` | *(Reclaimers)* Displays red text over AI whenever they vocalize, with the name of the dialogue field played. For example, pain body minor. |
| `ai_print_speech_timers` | boolean | `—` | *(Reclaimers)* Prints vocalizations to the console as they happen, including the encounter, squad, actor, dialogue type (e.g. exclaim), and line. For example, beach_lz/camp_center_grunt/grunt: talk flee [72/flee]. |
| `ai_print_surprise` | boolean | `—` | *(Reclaimers)* Prints vocalizations to the console as they happen, including the encounter, squad, actor, dialogue type (e.g. exclaim), and line. For example, beach_lz/camp_center_grunt/grunt: talk flee [72/flee]. |
| `ai_print_uncovering` | boolean | `—` | *(Reclaimers)* Prints vocalizations to the console as they happen, including the encounter, squad, actor, dialogue type (e.g. exclaim), and line. For example, beach_lz/camp_center_grunt/grunt: talk flee [72/flee]. |
| `ai_print_unfinished_paths` | boolean | `—` | *(Reclaimers)* Prints vocalizations to the console as they happen, including the encounter, squad, actor, dialogue type (e.g. exclaim), and line. For example, beach_lz/camp_center_grunt/grunt: talk flee [72/flee]. |
| `ai_print_vocalizations` | boolean | `—` | *(Reclaimers)* Prints vocalizations to the console as they happen, including the encounter, squad, actor, dialogue type (e.g. exclaim), and line. For example, beach_lz/camp_center_grunt/grunt: talk flee [72/flee]. |
| `ai_profile_disable` | boolean | `—` | *(Reclaimers)* Toggles the display of any enabled AI debug overlays. Defaults to true. |
| `ai_profile_random` | boolean | `—` | *(Reclaimers)* Toggles the display of any enabled AI debug overlays. Defaults to true. |
| `ai_render` | boolean | `—` | *(Reclaimers)* Toggles the display of any enabled AI debug overlays. Defaults to true. |
| `ai_render_activation` | boolean | `—` | *(Reclaimers)* Toggles the rendering of ballistic aiming arcs when AI throw grenades only (does not include hunter guns, wraiths, or other ballistic weapons). The arc is shown in green when unobstructed and orange when obstructed by an object or the BSP. Its origin point and vector are shown in yellow. Only a single arc is shown at a time and updates whenever a new grenade is thrown. This can be paused with ai_d |
| `ai_render_active_cover_seeking` | boolean | `—` | *(Reclaimers)* Toggles the rendering of ballistic aiming arcs when AI throw grenades only (does not include hunter guns, wraiths, or other ballistic weapons). The arc is shown in green when unobstructed and orange when obstructed by an object or the BSP. Its origin point and vector are shown in yellow. Only a single arc is shown at a time and updates whenever a new grenade is thrown. This can be paused with ai_d |
| `ai_render_aiming_validity` | boolean | `—` | *(Reclaimers)* Toggles the rendering of ballistic aiming arcs when AI throw grenades only (does not include hunter guns, wraiths, or other ballistic weapons). The arc is shown in green when unobstructed and orange when obstructed by an object or the BSP. Its origin point and vector are shown in yellow. Only a single arc is shown at a time and updates whenever a new grenade is thrown. This can be paused with ai_d |
| `ai_render_aiming_vectors` | boolean | `—` | *(Reclaimers)* Toggles the rendering of ballistic aiming arcs when AI throw grenades only (does not include hunter guns, wraiths, or other ballistic weapons). The arc is shown in green when unobstructed and orange when obstructed by an object or the BSP. Its origin point and vector are shown in yellow. Only a single arc is shown at a time and updates whenever a new grenade is thrown. This can be paused with ai_d |
| `ai_render_all_actors` | boolean | `—` | *(Reclaimers)* Toggles the rendering of ballistic aiming arcs when AI throw grenades only (does not include hunter guns, wraiths, or other ballistic weapons). The arc is shown in green when unobstructed and orange when obstructed by an object or the BSP. Its origin point and vector are shown in yellow. Only a single arc is shown at a time and updates whenever a new grenade is thrown. This can be paused with ai_d |
| `ai_render_audibility` | boolean | `—` | *(Reclaimers)* Toggles the rendering of ballistic aiming arcs when AI throw grenades only (does not include hunter guns, wraiths, or other ballistic weapons). The arc is shown in green when unobstructed and orange when obstructed by an object or the BSP. Its origin point and vector are shown in yellow. Only a single arc is shown at a time and updates whenever a new grenade is thrown. This can be paused with ai_d |
| `ai_render_ballistic_lineoffire` | boolean | `—` | *(Reclaimers)* Toggles the rendering of ballistic aiming arcs when AI throw grenades only (does not include hunter guns, wraiths, or other ballistic weapons). The arc is shown in green when unobstructed and orange when obstructed by an object or the BSP. Its origin point and vector are shown in yellow. Only a single arc is shown at a time and updates whenever a new grenade is thrown. This can be paused with ai_d |
| `ai_render_burst_geometry` | boolean | `—` | *(Reclaimers)* Shows pink text overlays over each unit with dialogue showing which variant they use, like if marines are a "mendoza" or a "bisenti". For example, variant 11 dialogue 0 aussie. |
| `ai_render_charge_decisions` | boolean | `—` | *(Reclaimers)* Shows pink text overlays over each unit with dialogue showing which variant they use, like if marines are a "mendoza" or a "bisenti". For example, variant 11 dialogue 0 aussie. |
| `ai_render_control` | boolean | `—` | *(Reclaimers)* Shows pink text overlays over each unit with dialogue showing which variant they use, like if marines are a "mendoza" or a "bisenti". For example, variant 11 dialogue 0 aussie. |
| `ai_render_current_state` | boolean | `—` | *(Reclaimers)* Shows pink text overlays over each unit with dialogue showing which variant they use, like if marines are a "mendoza" or a "bisenti". For example, variant 11 dialogue 0 aussie. |
| `ai_render_danger_zones` | boolean | `—` | *(Reclaimers)* Shows pink text overlays over each unit with dialogue showing which variant they use, like if marines are a "mendoza" or a "bisenti". For example, variant 11 dialogue 0 aussie. |
| `ai_render_detailed_state` | boolean | `—` | *(Reclaimers)* Shows pink text overlays over each unit with dialogue showing which variant they use, like if marines are a "mendoza" or a "bisenti". For example, variant 11 dialogue 0 aussie. |
| `ai_render_dialogue_variants` | boolean | `—` | *(Reclaimers)* Shows pink text overlays over each unit with dialogue showing which variant they use, like if marines are a "mendoza" or a "bisenti". For example, variant 11 dialogue 0 aussie. |
| `ai_render_emotions` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_encounter_activeregion` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_evaluations` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_firing_positions` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_grenade_decisions` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_gun_positions` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_idle_look` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_inactive_actors` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_lineoffire` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_lineoffire_crouching` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_lineofsight` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_melee_check` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_paths` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_paths_avoidance_obstacles` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_paths_avoidance_search` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_paths_avoidance_segment` | short | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_paths_avoided` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_paths_current` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_paths_destination` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_paths_failed` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_paths_nodes` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_paths_nodes_all` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_paths_nodes_closest` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_paths_nodes_costs` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_paths_nodes_polygons` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_paths_raw` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_paths_selected_only` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_paths_smoothed` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_player_aiming_blocked` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_player_ratings` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_postcombat` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_projectile_aiming` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_props` | boolean | `—` | *(Reclaimers)* Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy. |
| `ai_render_props_no_friends` | boolean | `—` | *(Reclaimers)* Hides prop lines for friends if enabled, which can make seeing enemy props easier. |
| `ai_render_props_target_weight` | boolean | `—` | *(Reclaimers)* Toggles the display of props as a web of lines. |
| `ai_render_props_unopposable` | boolean | `—` | *(Reclaimers)* Toggles the display of props as a web of lines. |
| `ai_render_props_unreachable` | boolean | `—` | *(Reclaimers)* Toggles the display of props as a web of lines. |
| `ai_render_props_web` | boolean | `—` | *(Reclaimers)* Toggles the display of props as a web of lines. |
| `ai_render_pursuit` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_render_recent_damage` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_render_secondary_looking` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_render_shooting` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_render_spatial_effects` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_render_speech` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_render_states` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_render_support_surfaces` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_render_targets` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_render_targets_last_visible` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_render_teams` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_render_threats` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_render_trigger` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_render_vector_avoidance` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_render_vector_avoidance_avoid_t` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_render_vector_avoidance_clear_time` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_render_vector_avoidance_intermediate` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_render_vector_avoidance_objects` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_render_vector_avoidance_rays` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_render_vector_avoidance_sense_t` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_render_vector_avoidance_weights` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_render_vehicle_avoidance` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_render_vehicles_enterable` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_render_vision_cones` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_render_vitality` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_show` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_show_actors` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_show_line_of_sight` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_show_paths` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_show_prop_types` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_show_sound_distance` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_show_stats` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |
| `ai_show_swarms` | boolean | `—` | *(Reclaimers)* Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. |

### Animation

| Command | Type | Default | Description |
|---------|------|---------|-------------|
| `debug_bink` | boolean | `—` | *(Reclaimers)* When set to true, pauses the biped limp body system, which is responsible for moving model nodes towards the ground when an eligible biped has died and has come to rest on the structure BSP. Bipeds will lay roughly at the right angle, but they will not conform to the shape of the surfaces below them. If set to false, the limp body system will take effect again even for existing dead bipeds. |
| `debug_recording` | boolean | `—` | *(Reclaimers)* Freezes the rendering viewport at the current camera location. The camera may still move after frozen, but all portal-based culling, skybox origin, FP models, and some debug overlays will still be based on the previously frozen camera location. This command is useful for inspecting portal behaviour where the camera cannot directly see. |
| `debug_recording_newlines` | short | `10` | *(Reclaimers)* Freezes the rendering viewport at the current camera location. The camera may still move after frozen, but all portal-based culling, skybox origin, FP models, and some debug overlays will still be based on the previously frozen camera location. This command is useful for inspecting portal behaviour where the camera cannot directly see. |
| `model_animation_bullshit0` | long | `—` | *(Reclaimers)* Controls the amount of mouse input acceleration. Set to 0 for none. Defaults to 0.7. |
| `model_animation_bullshit1` | long | `—` | *(Reclaimers)* Controls the amount of mouse input acceleration. Set to 0 for none. Defaults to 0.7. |
| `model_animation_bullshit2` | long | `—` | *(Reclaimers)* Controls the amount of mouse input acceleration. Set to 0 for none. Defaults to 0.7. |
| `model_animation_bullshit3` | long | `—` | *(Reclaimers)* Controls the amount of mouse input acceleration. Set to 0 for none. Defaults to 0.7. |
| `model_animation_compression` | boolean | `true` | *(Reclaimers)* Controls the amount of mouse input acceleration. Set to 0 for none. Defaults to 0.7. |
| `model_animation_data_compressed_size` | long | `—` | *(Reclaimers)* Controls the amount of mouse input acceleration. Set to 0 for none. Defaults to 0.7. |
| `model_animation_data_compression_savings_in_bytes` | long | `—` | *(Reclaimers)* Controls the amount of mouse input acceleration. Set to 0 for none. Defaults to 0.7. |
| `model_animation_data_compression_savings_in_bytes_at_import` | long | `—` | *(Reclaimers)* Controls the amount of mouse input acceleration. Set to 0 for none. Defaults to 0.7. |
| `model_animation_data_compression_savings_in_percent` | real | `—` | *(Reclaimers)* Controls the amount of mouse input acceleration. Set to 0 for none. Defaults to 0.7. |
| `model_animation_data_uncompressed_size` | long | `—` | *(Reclaimers)* Controls the amount of mouse input acceleration. Set to 0 for none. Defaults to 0.7. |

### Camera

| Command | Type | Default | Description |
|---------|------|---------|-------------|
| `debug_camera` | boolean | `—` | *(Reclaimers)* Shows debug information about the camera in the top left corner, including: 3D coordinates in world units BSP leaf indices Cluster indices, e.g. (#2 [2]). If the camera leaves the BSP then the first index will be -1 while the second index tracks the last known valid cluster. This is what allows the game to keep rendering part of the BSP while the camera is outside it. Ground point coordinates and |
| `director_camera_switch_fast` | boolean | `—` | *(Reclaimers)* Force-disconnects from the current multiplayer session and returns to the menu. This can be handy for quickly leaving a server after a game has ended without having to wait for the "Quit" option in the post-game lobby. |
| `force_all_player_views_to_default_player` | boolean | `false` | *(Reclaimers)* When set to true, forces all split-screen rendering windows to display the default (first) player's camera view. By default the game allocates one render window per local player; enabling this global shows every window from player 1's perspective. Useful for debugging co-op splitscreen rendering or scripting cutscenes. |
| `freeze_flying_camera` | short | `0` | *(Reclaimers)* Pauses the game world simulation. Backported from later titles for H1A. |

### Cheats

| Command | Type | Default | Description |
|---------|------|---------|-------------|
| `cheat_bottomless_clip` | boolean | `—` | *(Reclaimers)* Prevents player weapons from generating heat and depleting ammo rounds. Battery will still be depleted. |
| `cheat_bump_possession` | boolean | `—` | *(Reclaimers)* Allows the player to "possess" other characters by walking into them. The player will then be able to control the possessed character and see the world from its view. |
| `cheat_controller` | boolean | `—` | *(Reclaimers)* If enabled, the game loads a cheats.txt file which binds controller buttons to console commands. Known only to work in Halo beta versions. |
| `cheat_deathless_player` | boolean | `—` | *(Reclaimers)* Prevents players from being killed, including all multiplayer clients if enabled on the server. Although the players can still take damage, being reduced to 0 health or falling long distances will not kill them. This also prevents players from being killed with unit_kill. |
| `cheat_infinite_ammo` | boolean | `—` | *(Reclaimers)* Prevents weapons from depleting battery or reserve ammo. Weapons will still generate heat. Magazine-based weapons will still empty their current magazine, but reloading will not use up any reserve ammo. |
| `cheat_jetpack` | boolean | `—` | *(Reclaimers)* If enabled, players take no fall damage. In MCC, holding crouch while mid-air will also cause the player to hover and holding jump will cause you to fly. |
| `cheat_medusa` | boolean | `—` | *(Reclaimers)* Causes enemy AI to be killed instantly if they "look" at or become aware of the player. This does not include allies like Marines, even after allegiance has been broken by killing them. It is unknown if the command is hard-coded to kill certain AI types. |
| `cheat_omnipotent` | boolean | `—` | *(Reclaimers)* The player will instantly kill any unit they damage, including destroying vehicles. Even multiplayer vehicles can be "killed" and rendered inoperable. |
| `cheat_reflexive_damage_effects` | boolean | `—` | *(Reclaimers)* Any damage_effect applied to other characters, even dead ones, will appear on the player's screen regardless of who applied the damage. For example, if an Elite shoots a flood infection form, the player will see their screen flash blue and damage direction indicators appear. The player takes no actual damage. This can be helpful for testing visual effects. |
| `cheat_super_jump` | boolean | `—` | *(Reclaimers)* Players jump to an extreme height, and will die of fall damage if cheat_jetpack or cheat_deathless_player are not used. This can be used to quickly reach areas when testing maps. |

### Collision

| Command | Type | Default | Description |
|---------|------|---------|-------------|
| `collision_debug` | boolean | `—` | *(Reclaimers)* If enabled, a ray is continually shot from the camera (by default) to troubleshoot ray-object and ray-BSP collisions. A red normal-aligned marker will be shown where the ray collides with a surface. The collision surface itself, whether BSP or model, will be outline in red. Information about the collision surface will be shown in the top left corner of the screen, including plane and surface indic |
| `collision_debug_features` | boolean | `—` | *(Reclaimers)* Toggles the display of collision features near the camera, which can be spheres (red), cylinders (blue), or prisms (green). Collision size can be adjusted with collision_debug_width and collision_debug_height. The test point can be frozen in place using collision_debug_repeat. |
| `collision_debug_flag_back_facing_surfaces` | boolean | `—` | *(Reclaimers)* When collision_debug or collision_debug_spray are enabled, causes the test rays to collide with back-facing surfaces (those facing away from the camera). Defaults to false. |
| `collision_debug_flag_front_facing_surfaces` | boolean | `true` | *(Reclaimers)* When collision_debug or collision_debug_spray are enabled, causes the test rays to collide with front-facing surfaces (those facing towards the camera). Defaults to true. Disabling this will have no effect unless collision_debug_flag_back_facing_surfaces is also enabled. |
| `collision_debug_flag_ignore_breakable_surfaces` | boolean | `—` | *(Reclaimers)* Toggles if collision debug rays ignore breakable surfaces. Defaults to false. |
| `collision_debug_flag_ignore_invisible_surfaces` | boolean | `true` | *(Reclaimers)* Toggles if collision debug rays ignore invisible surfaces (e.g. collision-only player clipping). Defaults to true. |
| `collision_debug_flag_ignore_two_sided_surfaces` | boolean | `—` | *(Reclaimers)* Toggles if collision debug rays ignore two-sided surfaces. Defaults to false. |
| `collision_debug_flag_media` | boolean | `true` | *(Reclaimers)* Toggles if collision debug rays collide with water surfaces. |
| `collision_debug_flag_objects` | boolean | `true` | *(Reclaimers)* Toggles if collision debug rays collide with any class of object's collision geometry. This setting will be ignored if any more specific object flag is enabled, such as collision_debug_flag_objects_equipment. |
| `collision_debug_flag_objects_bipeds` | boolean | `—` | *(Reclaimers)* Toggles if collision debug rays collide with bipeds. |
| `collision_debug_flag_objects_controls` | boolean | `—` | *(Reclaimers)* Toggles if collision debug rays collide with device_control. |
| `collision_debug_flag_objects_equipment` | boolean | `—` | *(Reclaimers)* Toggles if collision debug rays collide with equipment. |
| `collision_debug_flag_objects_light_fixtures` | boolean | `—` | *(Reclaimers)* Toggles if collision debug rays collide with device_light_fixture. |
| `collision_debug_flag_objects_machines` | boolean | `—` | *(Reclaimers)* Toggles if collision debug rays collide with device_machine. |
| `collision_debug_flag_objects_placeholders` | boolean | `—` | *(Reclaimers)* Toggles if collision debug rays collide with placeholders. This would require a custom placeholder to have an effect, since the placeholder tags that come with the HEK have no collision model. |
| `collision_debug_flag_objects_projectiles` | boolean | `—` | *(Reclaimers)* Toggles if collision debug rays collide with projectiles. Note that most projectiles do not have a collision model. |
| `collision_debug_flag_objects_scenery` | boolean | `—` | *(Reclaimers)* Toggles if collision debug rays collide with scenery. |
| `collision_debug_flag_objects_vehicles` | boolean | `—` | *(Reclaimers)* Toggles if collision debug rays collide with vehicles. |
| `collision_debug_flag_objects_weapons` | boolean | `—` | *(Reclaimers)* Toggles if collision debug rays collide with weapons. |
| `collision_debug_flag_skip_passthrough_bipeds` | boolean | `—` | *(Reclaimers)* Unknown purpose. Does not seem to affect collision ray tests against bipeds. |
| `collision_debug_flag_structure` | boolean | `true` | *(Reclaimers)* Toggles if collision debug rays collide with the structure BSP. Collisions with model_collision_geometry BSPs are unaffected. |
| `collision_debug_flag_try_to_keep_location_valid` | boolean | `—` | *(Reclaimers)* Unknown purpose. |
| `collision_debug_flag_use_vehicle_physics` | boolean | `—` | *(Reclaimers)* If enabled, collision debug rays will collide with vehicle physics spheres rather than their model_collision_geometry. |
| `collision_debug_height` | real | `—` | *(Reclaimers)* When and collision_debug_features is enabled, controls the height in world units of collision features. Defaults to 0.0. |
| `collision_debug_length` | real | `100.0` | *(Reclaimers)* Sets the maximum test ray length for collision_debug and collision_debug_spray in world units. When the ray reaches this maxmimum, a floating green marker will be shown for collision_debug while spray rays will simply not be shown. Defaults to 100.0. |
| `collision_debug_phantom_bsp` | boolean | `—` | *(Reclaimers)* Causes a floating pink cube and label "phantom bsp" to appear whenever a test ray from the center of the screen intersects with phantom BSP. It can be helpful to pair this with collision_debug_spray. |
| `collision_debug_point_x` | real | `—` | *(Reclaimers)* Represents the current origin of the collision_debug test ray. While collision_debug is active, this value will be continually updated with the camera's location unless collision_debug_repeat is also enabled. In repeat mode, you are able to set this global to move the ray origin to any point you need. See also collision_debug_vector_i. |
| `collision_debug_point_y` | real | `—` | *(Reclaimers)* See collision_debug_point_x. |
| `collision_debug_point_z` | real | `—` | *(Reclaimers)* See collision_debug_point_x. |
| `collision_debug_repeat` | boolean | `—` | *(Reclaimers)* Setting this to true will freeze the test rays and points for collision_debug, collision_debug_phantom_bsp, collision_debug_spray, and collision_debug_features, allowing you to move and view the debug information from another angle or manually set the origin and direction with the collision_debug_point_* and collision_debug_vector_* globals. |
| `collision_debug_spray` | boolean | `—` | *(Reclaimers)* Setting this to true will cause collision ray tests to be performed in a dense grid from the viewport. This operates independently of the collision_debug setting, and only the destination hit markers are shown. Can be affected by collision flags, length, and frozen with collision_debug_repeat. |
| `collision_debug_vector_i` | real | `—` | *(Reclaimers)* Represents the current direction of the collision_debug test ray as ijk vector components. While collision_debug is active, this value will be continually updated with the camera's direction unless collision_debug_repeat is also enabled. In repeat mode, you are able to set this global to orient the ray as needed. |
| `collision_debug_vector_j` | real | `—` | *(Reclaimers)* See collision_debug_vector_i. |
| `collision_debug_vector_k` | real | `—` | *(Reclaimers)* See collision_debug_vector_i. |
| `collision_debug_width` | real | `—` | *(Reclaimers)* When collision_debug and collision_debug_features are enabled, controls the width in world units of collision features. Defaults to 0.0. |
| `collision_log_detailed` | boolean | `—` | *(Reclaimers)* No visible effect. May be related to controller input during development. |
| `collision_log_extended` | boolean | `—` | *(Reclaimers)* No visible effect. May be related to controller input during development. |
| `collision_log_render` | boolean | `—` | *(Reclaimers)* No visible effect. May be related to controller input during development. |
| `collision_log_time` | boolean | `—` | *(Reclaimers)* No visible effect. May be related to controller input during development. |
| `collision_log_totals_only` | boolean | `—` | *(Reclaimers)* No visible effect. May be related to controller input during development. |
| `debug_collision_skip_objects` | boolean | `—` | *(Reclaimers)* Disables collision tests against objects. For example, projectiles will pass through scenery and bipeds through non-moving vehicles. Vehicle-to-vehicle collision still happens, as does moving vehicle against biped. Everything still collides with the BSP. |
| `debug_collision_skip_vectors` | boolean | `—` | *(Reclaimers)* Globally disables vector/ray collision tests, with the following observed effects: Projectiles, particles and moving items will pass through everything, even the BSP. Melees will have no effect. Vehicle suspension, such as the Warthog's wheels, will hang. Object lighting will be unable to determine the ground point below the object when it moves, resulting in incorrect lighting and shadow directio |

### Console

| Command | Type | Default | Description |
|---------|------|---------|-------------|
| `console_dump_to_file` | boolean | `—` | *(Reclaimers)* No visible effect. May be related to controller input during development. |
| `terminal_render` | boolean | `true` | *(Reclaimers)* Toggles the display of console output. Defaults to true. |

### Effects

| Command | Type | Default | Description |
|---------|------|---------|-------------|
| `debug_damage` | boolean | `—` | *(Reclaimers)* When enabled, looking at any collideable object and pressing Space will display the object's body and shield vitalities on the HUD. |
| `debug_damage_taken` | boolean | `—` | *(Reclaimers)* Logs damage to the player as messages at the bottom of the screen. Includes body and shield vitality and the damage source. |
| `debug_effects_nonviolent` | boolean | `—` | *(Reclaimers)* Outlines the edges of fog plane volumes with white lines. |
| `debug_material_effects` | boolean | `—` | *(Reclaimers)* Displays cyan spheres wherever material_effects are being generated, like under the player's feet and where physics spheres intersect with the BSP or model_collision_geometry. |
| `decals` | boolean | `true` |  |
| `decals` | boolean | `true` |  |
| `effects_corpse_nonviolent` | boolean | `true` | *(Reclaimers)* Toggles if shooting bodies produces additional blood effects. |
| `weather` | boolean | `true` | *(Reclaimers)* Toggles the rendering of all weather_particle_system. Defaults to true. |

### Game

| Command | Type | Default | Description |
|---------|------|---------|-------------|
| `debug_game_save` | boolean | `—` | *(Reclaimers)* Displays a series of ticks at the top of the screen which do not seem to be affected by player input. May be a broken feature? |
| `debug_scripting` | boolean | `—` | *(Reclaimers)* Displays a table of active script threads, with their name, sleep time, and currently executing function. |
| `recover_saved_games_hack` | boolean | `—` | *(Reclaimers)* Toggles the display of all contrails. |
| `run_game_scripts` | boolean | `—` | *(Reclaimers)* If enabled, causes level scripts to be run in Sapien. This would allow you to see how the beach battle plays out in b30, for example. |

### HUD/UI

| Command | Type | Default | Description |
|---------|------|---------|-------------|
| `temporary_hud` | boolean | `—` | *(Reclaimers)* Renders a debug HUD with basic weapon information in text and line-drawn circular reticules representing the current error angle (yellow) and autoaim angle (blue, or red if autoaim active). Works in debug builds only. |

### Input

| Command | Type | Default | Description |
|---------|------|---------|-------------|
| `controls_enable_crouch` | boolean | `—` | *(Reclaimers)* No visible effect. May be related to controller input during development. |
| `controls_enable_doubled_spin` | boolean | `—` | *(Reclaimers)* No visible effect. May be related to controller input during development. |
| `controls_swap_doubled_spin_state` | boolean | `—` | *(Reclaimers)* No visible effect. May be related to controller input during development. |
| `controls_swapped` | boolean | `true` | *(Reclaimers)* No visible effect. May be related to controller input during development. |
| `debug_input` | boolean | `—` | *(Reclaimers)* Displays a series of ticks at the top of the screen which do not seem to be affected by player input. May be a broken feature? |
| `debug_input_target` | short | `—` | *(Reclaimers)* Shows orange and white spheres with the radius of each dynamic light. White seems to show when a light is not yet been activated, such as the Warthog's brake lights until their first use. Lens flare only lights are not shown since their radius is 0. |
| `pad3` | short | `0` | *(Reclaimers)* Enables or disables the magnetism aim assist for controllers. |
| `pad3_scale` | real | `1.0` | *(Reclaimers)* Enables or disables the magnetism aim assist for controllers. |

### Lighting

| Command | Type | Default | Description |
|---------|------|---------|-------------|
| `object_light_ambient_base` | real | `0.029999999329447746` | *(Reclaimers)* Sets the amount of ambient light all objects receive. Note that this only affects moving objects when their lighting updates, so you will not see any change on scenery. Setting this to 1 makes objects fullbright. Defaults to 0.03. |
| `object_light_ambient_scale` | real | `0.4000000059604645` | *(Reclaimers)* Scales ambient light from the lightmap. Defaults to 0.4. |
| `object_light_interpolate` | boolean | `true` | *(Reclaimers)* Toggles if object lighting transitions smoothly when the object moves between different ground point surfaces or default lighting. |
| `object_light_secondary_scale` | real | `1.0` | *(Reclaimers)* Scales secondary light on objects, but doesn't apply to default lighting. Defaults to 1. |

### Misc

| Command | Type | Default | Description |
|---------|------|---------|-------------|
| `breakable_surfaces` | boolean | `true` | *(Reclaimers)* Enables or disables the breakable surfaces effect. If disabled, breakable surfaces will simply disappear rather than shatter. |
| `debug_motion_sensor_draw_all_units` | boolean | `—` | *(Reclaimers)* If enabled, completely stops the game from rendering new frames. This includes preventing the rendering of the developer console itself, so disabling this feature can be tricky. If you think the console is still open, press Up to re-enter the previous command, then replace its final argument with true or 1 and press Enter to resume rendering. |
| `f0` | real | `0.0` | *(Reclaimers)* Limits rendering to 30 FPS. |
| `f1` | real | `0.0` | *(Reclaimers)* Limits rendering to 30 FPS. |
| `f2` | real | `0.0` | *(Reclaimers)* Limits rendering to 30 FPS. |
| `f3` | real | `0.0` | *(Reclaimers)* Limits rendering to 30 FPS. |
| `f4` | real | `0.0` | *(Reclaimers)* Limits rendering to 30 FPS. |
| `f5` | real | `0.0` | *(Reclaimers)* Limits rendering to 30 FPS. |
| `find_all_fucked_up_shit` | boolean | `—` | *(Reclaimers)* Limits rendering to 30 FPS. |
| `rider_ejection` | boolean | `true` | *(Reclaimers)* Toggles if bipeds are ejected from overturned vehicles, including players and AI characters. Defaults to true. |
| `stun_enable` | boolean | `—` | *(Reclaimers)* Renders a debug HUD with basic weapon information in text and line-drawn circular reticules representing the current error angle (yellow) and autoaim angle (blue, or red if autoaim active). Works in debug builds only. |

### Network

| Command | Type | Default | Description |
|---------|------|---------|-------------|
| `allow_out_of_sync` | boolean | `—` | *(Reclaimers)* Unknown purpose. Default value is 7. |
| `global_connection_dont_timeout` | boolean | `—` | *(Reclaimers)* Unknown purpose. |

### Objects

| Command | Type | Default | Description |
|---------|------|---------|-------------|
| `debug_inactive_objects` | boolean | `—` | *(Reclaimers)* Displays a series of ticks at the top of the screen which do not seem to be affected by player input. May be a broken feature? |
| `debug_object_garbage_collection` | boolean | `—` | *(Reclaimers)* When garbage_collect_now is run, enabling this causes information to print to the console about the total number of objects, garbage objects, and how many objects were garbage collected. |
| `debug_object_lights` | boolean | `—` | *(Reclaimers)* Shows the incoming light colour and vector for all objects which results from sampling lightmap data at the ground point beneath the object. This data is used to shade the object and cast its shadow. |
| `debug_objects` | boolean | `—` | *(Reclaimers)* When enabled, toggles if debug information is visible on objects (such as bounding sphere and collision models). Individual debug features can be toggled with the debug_objects_* commands. In H1A this now has debug_objects_root_node enabled by default; turn it off if you don't want the orange text. |
| `debug_objects_biped_autoaim_pills` | boolean | `—` | *(Reclaimers)* When debug_objects is enabled, displays biped autoaim pills in red. |
| `debug_objects_biped_physics_pills` | boolean | `—` | *(Reclaimers)* When debug_objects is enabled, displays biped physics pills in white. |
| `debug_objects_bounding_spheres` | boolean | `true` | *(Reclaimers)* When debug_objects is enabled, displays yellow spheres for object bounding radius. The sphere will be black when the object is inactive. This setting defaults to true in HEK Sapien but not H1A Sapien. |
| `debug_objects_collision_models` | boolean | `true` | *(Reclaimers)* When debug_objects is enabled, displays green meshes for object collision models. This setting defaults to true in HEK Sapien but not H1A Sapien. |
| `debug_objects_devices` | boolean | `—` | *(Reclaimers)* Toggles the display of object names, for named objects. The names are shown in purple. |
| `debug_objects_names` | boolean | `—` | *(Reclaimers)* Toggles the display of object names, for named objects. The names are shown in purple. |
| `debug_objects_pathfinding_spheres` | boolean | `—` | *(Reclaimers)* When debug_objects is enabled, displays object pathfinding spheres in blue. |
| `debug_objects_physics` | boolean | `—` | *(Reclaimers)* When debug_objects is enabled, displays physics mass points as white spheres. |
| `debug_objects_position_velocity` | boolean | `—` | *(Reclaimers)* When debug_objects is enabled, displays red, green, and blue object-space reference axes and a yellow velocity vector on each object. |
| `debug_objects_root_node` | boolean | `—` | *(Reclaimers)* When debug_objects is enabled, displays red, green, and blue object-space reference axes and orange text including object ID, class, and tag name on each object. Defaults to true in H1A. |
| `debug_objects_unit_mouth_apeture` | boolean | `—` | *(Reclaimers)* When debug_objects is enabled, displays blue markers at unit seat locations, red markers at their entry points, and a yellow marker at the object origin. |
| `debug_objects_unit_seats` | boolean | `—` | *(Reclaimers)* When debug_objects is enabled, displays blue markers at unit seat locations, red markers at their entry points, and a yellow marker at the object origin. |
| `debug_objects_unit_vectors` | boolean | `—` | *(Reclaimers)* When debug_objects is enabled, displays white and red vectors on objects. Their meaning is unknown. |
| `debug_objects_vehicle_powered_mass_points` | boolean | `—` | *(Reclaimers)* No visible effect, even with debug_objects_physics enabled. |

### Pathfinding

| Command | Type | Default | Description |
|---------|------|---------|-------------|
| `debug_obstacle_path` | boolean | `—` | *(Reclaimers)* Toggles the display of yellow bounding spheres around each permanent decal in the environment. |
| `debug_obstacle_path_goal_point_x` | real | `—` | *(Reclaimers)* Toggles the display of yellow bounding spheres around each permanent decal in the environment. |
| `debug_obstacle_path_goal_point_y` | real | `—` | *(Reclaimers)* Toggles the display of yellow bounding spheres around each permanent decal in the environment. |
| `debug_obstacle_path_goal_surface_index` | long | `—` | *(Reclaimers)* Toggles the display of yellow bounding spheres around each permanent decal in the environment. |
| `debug_obstacle_path_on_failure` | boolean | `—` | *(Reclaimers)* Toggles the display of yellow bounding spheres around each permanent decal in the environment. |
| `debug_obstacle_path_start_point_x` | real | `—` | *(Reclaimers)* Toggles the display of yellow bounding spheres around each permanent decal in the environment. |
| `debug_obstacle_path_start_point_y` | real | `—` | *(Reclaimers)* Toggles the display of yellow bounding spheres around each permanent decal in the environment. |
| `debug_obstacle_path_start_surface_index` | long | `—` | *(Reclaimers)* Toggles the display of yellow bounding spheres around each permanent decal in the environment. |

### Physics

| Command | Type | Default | Description |
|---------|------|---------|-------------|
| `debug_physics_disable_penetration_freeze` | boolean | `—` | *(Reclaimers)* Sets the player's armour color in non-team games. The setting will take effect when the player respawns. The default value -1 causes the player's profile setting to be used. |
| `debug_point_physics` | boolean | `—` | *(Reclaimers)* Renders green or red markers wherever point_physics are being simulated. This includes flags, antenna, contrails, particles, and particle_systems. For weather_particle_system, markers are only shown in their simulation cube and while the weather global not disabled. Red markers indicate point_physics with the collides with structures flag, which are more computationally expensive. It can help to e |

### Player

| Command | Type | Default | Description |
|---------|------|---------|-------------|
| `debug_player` | boolean | `—` | *(Reclaimers)* Sets the player's armour color in non-team games. The setting will take effect when the player respawns. The default value -1 causes the player's profile setting to be used. |
| `debug_player_color` | short | `-1` | *(Reclaimers)* Sets the player's armour color in non-team games. The setting will take effect when the player respawns. The default value -1 causes the player's profile setting to be used. |
| `debug_player_teleport` | boolean | `—` | *(Reclaimers)* Displays a green physics pill where a co-op player would spawn if safe. |
| `player0_look_pitch_rate` | real | `—` |  |
| `player0_look_yaw_rate` | real | `—` |  |
| `player1_look_pitch_rate` | real | `—` |  |
| `player1_look_yaw_rate` | real | `—` |  |
| `player2_look_pitch_rate` | real | `—` |  |
| `player2_look_yaw_rate` | real | `—` |  |
| `player3_look_pitch_rate` | real | `—` |  |
| `player3_look_yaw_rate` | real | `—` |  |
| `player_autoaim` | boolean | `true` | *(Reclaimers)* Enables or disables the magnetism aim assist for controllers. |
| `player_magnetism` | boolean | `true` | *(Reclaimers)* Enables or disables the magnetism aim assist for controllers. |
| `player_spawn_count` | short | `1` | *(Reclaimers)* Displays profiling and budget information in the upper-left of the screen, including object, effects, particles, AI encounters, collision tests, and more. You may find it useful to open the console while using this feature in order to stop the game simulation. |

### Profiling

| Command | Type | Default | Description |
|---------|------|---------|-------------|
| `profile_display` | boolean | `—` | *(Reclaimers)* Displays profiling and budget information in the upper-left of the screen, including object, effects, particles, AI encounters, collision tests, and more. You may find it useful to open the console while using this feature in order to stop the game simulation. |
| `profile_dump_frames` | boolean | `—` | *(Reclaimers)* Broken feature -- causes a crash. |
| `profile_dump_lost_frames` | boolean | `—` | *(Reclaimers)* Broken feature -- causes a crash. |
| `profile_graph` | boolean | `—` | *(Reclaimers)* Broken feature -- causes a crash. |
| `profile_timebase_ticks` | boolean | `—` | *(Reclaimers)* Toggles the active camouflage distortion effect. When disabled, objects with active camouflage are rendered as they normally would without the effect. |

### Renderer

| Command | Type | Default | Description |
|---------|------|---------|-------------|
| `debug_decals` | boolean | `—` | *(Reclaimers)* Displays red numbers over each dynamic and permanent decal in the environment. The mesh of the most recently created decal (initially a permanent decal if one exists, then any new dynamic one) will also be highlighted so you can see how it conforms around the BSP. White points indicate the original 4 corners of the decal, while red ones were added during the conformation to the BSP. The meaning of |
| `debug_detail_objects` | boolean | `—` | *(Reclaimers)* When enabled, active detail object cells will be outlined in blue and individual detail objects are highlighted with red markers. |
| `debug_fog_planes` | boolean | `—` | *(Reclaimers)* Outlines the edges of fog plane volumes with white lines. |
| `debug_framerate` | boolean | `—` | *(Reclaimers)* No visible effect. Replaced with the Ctrl + F12 hotkey? |
| `debug_frustum` | boolean | `—` | *(Reclaimers)* Draws a series of red lines between corners and midpoints of the screen within the view frustrum. These can be seen to intersect with level geometry. |
| `debug_lights` | boolean | `—` | *(Reclaimers)* Shows orange and white spheres with the radius of each dynamic light. White seems to show when a light is not yet been activated, such as the Warthog's brake lights until their first use. Lens flare only lights are not shown since their radius is 0. |
| `debug_no_drawing` | boolean | `—` | *(Reclaimers)* If enabled, completely stops the game from rendering new frames. This includes preventing the rendering of the developer console itself, so disabling this feature can be tricky. If you think the console is still open, press Up to re-enter the previous command, then replace its final argument with true or 1 and press Enter to resume rendering. |
| `debug_no_frustum_clip` | boolean | `—` | *(Reclaimers)* Disables portal-based occlusion culling for objects and BSP faces (use rasterizer_wireframe 1 to see this). In addition to the PVS, which determines the set of clusters which are potentially visible, groups of faces and objects within those clusters can still be culled further using the limited view from the camera's location through the series of portals leading to those clusters (a portal-dimini |
| `debug_permanent_decals` | boolean | `—` | *(Reclaimers)* Toggles the display of yellow bounding spheres around each permanent decal in the environment. |
| `debug_render_freeze` | boolean | `—` | *(Reclaimers)* Freezes the rendering viewport at the current camera location. The camera may still move after frozen, but all portal-based culling, skybox origin, FP models, and some debug overlays will still be based on the previously frozen camera location. This command is useful for inspecting portal behaviour where the camera cannot directly see. |
| `debug_sprites` | boolean | `—` | *(Reclaimers)* Renders 2D sprite effects like particles and weather_particle_system with white triangle outlines. This also displays some sprite statistics at the top of the screen (coverage and big sprites count). |
| `debug_texture_cache` | boolean | `—` | *(Reclaimers)* If enabled, causes red messages to appear in the HUD which are related to texture and/or sound cache events. Exact meaning unknown. |
| `display_framerate` | boolean | `—` | *(Reclaimers)* Displays the current framerate in green in the bottom-right corner of the screen. |
| `display_precache_progress` | boolean | `—` | *(Reclaimers)* Toggles if shooting bodies produces additional blood effects. |
| `display_vblank_deltas` | boolean | `—` |  |
| `framerate_lock` | boolean | `—` | *(Reclaimers)* Limits rendering to 30 FPS. |
| `framerate_throttle` | boolean | `true` | *(Reclaimers)* Limits rendering to 30 FPS. |
| `radiosity_lines` | boolean | `—` |  |
| `radiosity_normals` | boolean | `—` |  |
| `radiosity_quality` | short | `—` |  |
| `radiosity_step_count` | short | `—` |  |
| `rasterizer_DXTC_noise` | boolean | `false` | *(Reclaimers)* Toggles alpha testing for BSP shader_environment. These shaders are rendered opaquely when disabled. |
| `rasterizer_active_camouflage` | boolean | `true` | *(Reclaimers)* Toggles the active camouflage distortion effect. When disabled, objects with active camouflage are rendered as they normally would without the effect. |
| `rasterizer_active_camouflage_multipass` | boolean | `true` | *(Reclaimers)* Toggles whether or not transparent shaders are shown through active camouflage. If disabled, shaders like glass or lights will not be visible through a camouflaged unit. |
| `rasterizer_bump_mapping` | boolean | `true` | *(Reclaimers)* Toggles bump mapping on the BSP, affecting both environmental bump lighting and specular reflections. If you only want to toggle bump lighting, use rasterizer_lightmaps_incident_radiosity. |
| `rasterizer_debug_geometry` | boolean | `true` | *(Reclaimers)* Forces certain object LODs to be used. A value of 4 is the highest quality, and 0 the worst. The default value -1 returns to automatic behaviour. |
| `rasterizer_debug_geometry_multipass` | boolean | `false` | *(Reclaimers)* Forces certain object LODs to be used. A value of 4 is the highest quality, and 0 the worst. The default value -1 returns to automatic behaviour. |
| `rasterizer_debug_meter_shader` | boolean | `false` | *(Reclaimers)* Forces certain object LODs to be used. A value of 4 is the highest quality, and 0 the worst. The default value -1 returns to automatic behaviour. |
| `rasterizer_debug_model_lod` | short | `-1` | *(Reclaimers)* Forces certain object LODs to be used. A value of 4 is the highest quality, and 0 the worst. The default value -1 returns to automatic behaviour. |
| `rasterizer_debug_model_vertices` | boolean | `false` | *(Reclaimers)* Disabling this silences "generic shader has no maps or stages" warnings in the console. Newly added to the MCC tools in the July 2023 CU. |
| `rasterizer_debug_transparents` | boolean | `false` | *(Reclaimers)* Toggles the rendering of detail objects. |
| `rasterizer_detail_objects` | boolean | `true` | *(Reclaimers)* Toggles the rendering of detail objects. |
| `rasterizer_detail_objects_offset_multiplier` | real | `0.4000000059604645` | *(Reclaimers)* Defaults to 0.4. No visible effect on detail objects when changed. |
| `rasterizer_draw_first_person_weapon_first` | boolean | `true` | *(Reclaimers)* Controls whether the first person weapon/arms is rendered before or after the rest of the scene. Defaults to true. If set to false, parts of the FP model can be occluded by nearby objects because they fail the depth test against the depth buffer. This is easily seen in the Warthog passenger seat or with the sniper rifle against scenery. Typically, the FP view draws first and also writes to a stenc |
| `rasterizer_dynamic_lit_geometry` | boolean | `true` | *(Reclaimers)* Toggles alpha testing for BSP shader_environment. These shaders are rendered opaquely when disabled. |
| `rasterizer_dynamic_screen_geometry` | boolean | `true` | *(Reclaimers)* Toggles alpha testing for BSP shader_environment. These shaders are rendered opaquely when disabled. |
| `rasterizer_dynamic_unlit_geometry` | boolean | `true` | *(Reclaimers)* Toggles alpha testing for BSP shader_environment. These shaders are rendered opaquely when disabled. |
| `rasterizer_environment` | boolean | `true` | *(Reclaimers)* Toggles alpha testing for BSP shader_environment. These shaders are rendered opaquely when disabled. |
| `rasterizer_environment_alpha_testing` | boolean | `true` | *(Reclaimers)* Toggles alpha testing for BSP shader_environment. These shaders are rendered opaquely when disabled. |
| `rasterizer_environment_decals` | boolean | `true` | *(Reclaimers)* Toggles the display and creation of both permanent and dynamic decals. While false, effects cannot create new decals but previous ones will reappear when reset to true. |
| `rasterizer_environment_diffuse_lights` | boolean | `true` | *(Reclaimers)* Toggles the rendering of dynamic light diffuse illumination on the BSP. Does not affect specular highlights. |
| `rasterizer_environment_diffuse_textures` | boolean | `true` | *(Reclaimers)* Disables diffuse textures in the BSP, showing just the lightmap shading and specular components. |
| `rasterizer_environment_fog` | boolean | `true` | *(Reclaimers)* Toggles both environmental sky fog and fog plane colors. Does not affect fog screen. Use rasterizer_fog_plane or rasterizer_fog_atmosphere to individually toggle fog types. |
| `rasterizer_environment_fog_screen` | boolean | `true` | *(Reclaimers)* Toggles the cloudy fog screen effect of fog planes, seen in the levels a30, c10, and c40. |
| `rasterizer_environment_lightmaps` | boolean | `true` | *(Reclaimers)* Toggles the rendering of structure BSP lightmaps. When disabled, the level will be completely invisible. |
| `rasterizer_environment_reflection_lightmap_mask` | boolean | `true` | *(Reclaimers)* Toggles the rendering of dynamic mirrors. |
| `rasterizer_environment_reflection_mirrors` | boolean | `true` | *(Reclaimers)* Toggles the rendering of dynamic mirrors. |
| `rasterizer_environment_reflections` | boolean | `true` | *(Reclaimers)* Toggles specular reflections in the BSP. |
| `rasterizer_environment_shadows` | boolean | `true` | *(Reclaimers)* Toggles dynamic shadow mapping for objects. Same effect as render_shadows. |
| `rasterizer_environment_specular_lightmaps` | boolean | `true` | *(Reclaimers)* Sets the far clip distance, which is the maximum draw distance (world units). Defaults to 1024.0. |
| `rasterizer_environment_specular_lights` | boolean | `true` | *(Reclaimers)* Sets the far clip distance, which is the maximum draw distance (world units). Defaults to 1024.0. |
| `rasterizer_environment_specular_mask` | boolean | `true` | *(Reclaimers)* Sets the far clip distance, which is the maximum draw distance (world units). Defaults to 1024.0. |
| `rasterizer_environment_transparents` | boolean | `true` | *(Reclaimers)* Sets the far clip distance, which is the maximum draw distance (world units). Defaults to 1024.0. |
| `rasterizer_far_clip_distance` | real | `1024.0` | *(Reclaimers)* Sets the far clip distance, which is the maximum draw distance (world units). Defaults to 1024.0. |
| `rasterizer_filthy_decal_fog_hack` | boolean | `true` | *(Reclaimers)* Sets the far clip distance, for the first person arms and weapon. Defaults to 1024.0. The world clipping distance can be set with rasterizer_far_clip_distance. |
| `rasterizer_first_person_weapon_far_clip_distance` | real | `1024.0` | *(Reclaimers)* Sets the far clip distance, for the first person arms and weapon. Defaults to 1024.0. The world clipping distance can be set with rasterizer_far_clip_distance. |
| `rasterizer_first_person_weapon_near_clip_distance` | real | `0.01171875` | *(Reclaimers)* Sets the near clip distance of the first person arms and weapon. Defaults to 0.011719. |
| `rasterizer_floating_point_zbuffer` | boolean | `false` | *(Reclaimers)* Toggles atmospheric fog as defined in the active sky tag. |
| `rasterizer_fog_atmosphere` | boolean | `true` | *(Reclaimers)* Toggles atmospheric fog as defined in the active sky tag. |
| `rasterizer_fog_plane` | boolean | `true` | *(Reclaimers)* Toggles the rendering of fog planes. |
| `rasterizer_frame_bounds_bottom` | short | `0` | *(Reclaimers)* Toggles the yellow and red dots seen in the motion sensor. |
| `rasterizer_frame_bounds_left` | short | `0` | *(Reclaimers)* Toggles the yellow and red dots seen in the motion sensor. |
| `rasterizer_frame_bounds_right` | short | `0` | *(Reclaimers)* Toggles the yellow and red dots seen in the motion sensor. |
| `rasterizer_frame_bounds_top` | short | `0` | *(Reclaimers)* Toggles the yellow and red dots seen in the motion sensor. |
| `rasterizer_framerate_stabilization` | boolean | `false` | *(Reclaimers)* Toggles the yellow and red dots seen in the motion sensor. |
| `rasterizer_framerate_throttle` | boolean | `true` | *(Reclaimers)* Toggles the yellow and red dots seen in the motion sensor. |
| `rasterizer_hud_motion_sensor` | boolean | `true` | *(Reclaimers)* Toggles the yellow and red dots seen in the motion sensor. |
| `rasterizer_lens_flares` | boolean | `true` | *(Reclaimers)* Toggles rendering of all lens flares. |
| `rasterizer_lens_flares_occlusion` | boolean | `true` | *(Reclaimers)* Toggles lens flare occlusion. If set to false, lens flares will no longer be occluded and stay visible even through objects in the foreground. |
| `rasterizer_lens_flares_occlusion_debug` | boolean | `false` | *(Reclaimers)* Displays red squares over lens flares in the environment. How much the square is occluded by other geometry or the view frustrum is how much the lens flare fades out. The size of the square relates to the occlusion radius. |
| `rasterizer_lightmap_ambient` | real | `1.0` | *(Reclaimers)* Sets the amount of ambient light when rendering the BSP in fullbright mode (like when radiosity has not yet been baked or rasterizer_lightmap_mode 2) This defaults to 1.0. It has no effect on the normal lightmap rendering mode. |
| `rasterizer_lightmap_mode` | short | `0` | *(Reclaimers)* Changes the rendering mode of lightmaps: Mode Description 0 Normal (default). 1 BSP specular reflections will not be multiplied by the lightmap. 2 Fullbright mode. You can set the ambient light with rasterizer_lightmap_ambient. 3 Colours BSP surfaces by what lightmap bitmap index they use (technically, with a random colour seeded by the lightmap address). It can help to disable rasterizer_environm |
| `rasterizer_lightmaps_filtering` | boolean | `true` | *(Reclaimers)* Enables or disables texture filtering for lightmaps. When disabled, lightmaps will appear blocky and jagged. Has no effect in H1A. |
| `rasterizer_lightmaps_incident_radiosity` | boolean | `true` | *(Reclaimers)* Toggles directional environmental bump mapped lighting. Does not affect the sampling of stored incident radiosity for object shadows. |
| `rasterizer_mode` | short | `0` | *(Reclaimers)* Changes rendering mode of the level and its objects: Mode Description 0 Normal (default). 1 Additive blending. 2 Disables specular on the BSP (like rasterizer_environment_reflections). |
| `rasterizer_model_lighting_ambient` | real | `0.0` | *(Reclaimers)* Toggles the rendering of transparent shaders in models. For example, the Warthog's windshield. |
| `rasterizer_model_transparents` | boolean | `true` | *(Reclaimers)* Toggles the rendering of transparent shaders in models. For example, the Warthog's windshield. |
| `rasterizer_models` | boolean | `true` | *(Reclaimers)* Toggles the rendering of all models. When disabled, all objects like scenery, units, projectiles, and even the skybox and FP arms will become invisible. The BSP and effects like particles and decals are still visible. |
| `rasterizer_near_clip_distance` | real | `0.0625` | *(Reclaimers)* Sets the near clip distance, which is the minimum draw distance (world units). Defaults to 0.0625. This does not appear to work fully, as the near clip distance will only be adjusted for one frame. |
| `rasterizer_plasma_energy` | boolean | `true` | *(Reclaimers)* Toggles the lens flare "god rays" effect, present on sky lights or lens flares explicitly set to sun. |
| `rasterizer_profile_log` | boolean | `false` | *(Reclaimers)* Toggles the lens flare "god rays" effect, present on sky lights or lens flares explicitly set to sun. |
| `rasterizer_profile_objectlock_time` | real | `0.0` | *(Reclaimers)* Toggles the lens flare "god rays" effect, present on sky lights or lens flares explicitly set to sun. |
| `rasterizer_profile_print_locks` | boolean | `false` | *(Reclaimers)* Toggles the lens flare "god rays" effect, present on sky lights or lens flares explicitly set to sun. |
| `rasterizer_pushbuffer_kickoff_size` | short | `0` |  |
| `rasterizer_pushbuffer_size` | short | `768` |  |
| `rasterizer_ray_of_buddha` | boolean | `true` | *(Reclaimers)* Toggles the lens flare "god rays" effect, present on sky lights or lens flares explicitly set to sun. |
| `rasterizer_refresh_rate` | short | `0` | *(Reclaimers)* Toggles the display of screen flashes, such as those from a damage_effect or powerup equipment. |
| `rasterizer_safe_frame_bounds` | boolean | `false` | *(Reclaimers)* Toggles the display of screen flashes, such as those from a damage_effect or powerup equipment. |
| `rasterizer_screen_effects` | boolean | `true` | *(Reclaimers)* Toggles the display of screen flashes, such as those from a damage_effect or powerup equipment. |
| `rasterizer_screen_flashes` | boolean | `true` | *(Reclaimers)* Toggles the display of screen flashes, such as those from a damage_effect or powerup equipment. |
| `rasterizer_secondary_render_target_debug` | boolean | `false` | *(Reclaimers)* Toggles the blurring of dynamic object shadows. Defaults to true. |
| `rasterizer_shadows_convolution` | boolean | `true` | *(Reclaimers)* Toggles the blurring of dynamic object shadows. Defaults to true. |
| `rasterizer_shadows_debug` | boolean | `false` | *(Reclaimers)* When enabled, all dynamic object shadow maps will be rendered with a partially-shadowed background, making their rectangular boundaries visible. The size of this rectangle depends on the object's bounding radius. |
| `rasterizer_smart` | boolean | `true` | *(Reclaimers)* No visible effect. |
| `rasterizer_soft_filter` | boolean | `false` | *(Reclaimers)* No visible effect. |
| `rasterizer_splitscreen_VB_optimization` | boolean | `false` | *(Reclaimers)* Displays renderer statistics on the screen, with several modes. All modes include at least framerate stats. Mode Description 0 Off (default). 1 Some unknown counts which usually read 0, though fast increases when flags are on-screen. 2 Vertices, triangles, and primitives counts for various model and environment features and effects. 3 GPU profiling for each stage of rendering. Uknown if this still |
| `rasterizer_stats` | short | `0` | *(Reclaimers)* Displays renderer statistics on the screen, with several modes. All modes include at least framerate stats. Mode Description 0 Off (default). 1 Some unknown counts which usually read 0, though fast increases when flags are on-screen. 2 Vertices, triangles, and primitives counts for various model and environment features and effects. 3 GPU profiling for each stage of rendering. Uknown if this still |
| `rasterizer_stencil_mask` | boolean | `true` | *(Reclaimers)* Enables or disables the stencil mask used to prevent the background scene from overlapping the first person view. Defaults to true. Disabling this has the same effect as disabling rasterizer_draw_first_person_weapon_first and you will sometimes see nearby objects occluding parts of the FP model. |
| `rasterizer_transparent_pixel_counter` | boolean | `false` | *(Reclaimers)* Toggles the rendering of shader_transparent_water shaders. |
| `rasterizer_water` | boolean | `true` | *(Reclaimers)* Toggles the rendering of shader_transparent_water shaders. |
| `rasterizer_water_mipmapping` | boolean | `false` | *(Reclaimers)* No visible effect. Defaults to false. |
| `rasterizer_wireframe` | boolean | `false` | *(Reclaimers)* Toggles rendering in wireframe mode, which only draws pixels along triangle edges rather than filling trangles. This can be useful for troubleshooting portals. |
| `rasterizer_zbias` | long | `8` | *(Reclaimers)* Controls how far away from surfaces new decals are generated, e.g. for projectile impacts. Defaults to 0.003906. The units are not world units. |
| `rasterizer_zoffset` | real | `0.00390625` | *(Reclaimers)* Controls how far away from surfaces new decals are generated, e.g. for projectile impacts. Defaults to 0.003906. The units are not world units. |
| `rasterizer_zsprites` | boolean | `true` | *(Reclaimers)* Toggles the display of all contrails. |
| `render_contrails` | boolean | `true` | *(Reclaimers)* Toggles the display of all contrails. |
| `render_model_index_counts` | boolean | `—` | *(Reclaimers)* If true, displays a red number above each object with its model index count. If render_model_vertex_counts is also enabled, the vertex count and index count are separated by a slash like "<vertices>/<indices>". |
| `render_model_markers` | boolean | `—` | *(Reclaimers)* If enabled, all model markers will be rendered in 3D with their name and rotation axis. |
| `render_model_no_geometry` | boolean | `—` | *(Reclaimers)* If true, displays a red number above each object with its model vertex count. If render_model_index_counts is also enabled, the vertex count and index count are separated by a slash like "<vertices>/<indices>". |
| `render_model_nodes` | boolean | `—` | *(Reclaimers)* If enabled, all model skeletons will be rendered. Nodes are shown as axis gizmos and connected to their parents by white lines. |
| `render_model_vertex_counts` | boolean | `—` | *(Reclaimers)* If true, displays a red number above each object with its model vertex count. If render_model_index_counts is also enabled, the vertex count and index count are separated by a slash like "<vertices>/<indices>". |
| `render_particles` | boolean | `true` | *(Reclaimers)* Toggles the display of all particles. |
| `render_psystems` | boolean | `true` | *(Reclaimers)* Toggles the rendering of particle_systems and weather_particle_system. |
| `render_shadows` | boolean | `true` | *(Reclaimers)* Toggles the display of dynamic object shadows. Same effect as rasterizer_environment_shadows. |
| `render_wsystems` | boolean | `true` | *(Reclaimers)* No visible effect on weather or particle systems. |
| `screenshot_count` | short | `—` | *(Reclaimers)* When the -screenshot argument is enabled, pressing Prnt Scrn will generate a series of tiled screenhots in the screenshots directory. For example, a value of 2 will generate 4 screenshots meant to tile in a 2x2 arrangement for a high resolution result. You should disable the HUD with show_hud 0 before using this. |
| `screenshot_size` | short | `1` | *(Reclaimers)* Appears to set some kind of crop or scaling factor for generated screenshots, but is probably not working as intended. Setting this to a value other than 1 can crash when using screenshot_size. |
| `texture_cache_graph` | boolean | `—` | *(Reclaimers)* Toggles a live representation of the texture cache in the top left corner of the screen, depicting which entries are being loaded and evicted. Works in debug builds only. |
| `texture_cache_list` | boolean | `—` | *(Reclaimers)* Shows a live list of all bitmap tag paths currently loaded in the texture cache. Works in debug builds only. |

### Sound

| Command | Type | Default | Description |
|---------|------|---------|-------------|
| `debug_looping_sound` | boolean | `—` | *(Reclaimers)* Displays active sound_looping with cyan spheres for their maxmimum distance and blue spheres for their minimum. |
| `debug_sound` | boolean | `—` | *(Reclaimers)* Sound sources will be labeled in 3D with their tag path and a their minimum and maximum distances shown as red and yellow spheres, respectively. |
| `debug_sound_cache` | boolean | `—` | *(Reclaimers)* If enabled, sound cache statistics will be shown in the top left corner of the screen, including how full it is. |
| `debug_sound_channels` | boolean | `—` | *(Reclaimers)* Displays the utilization of sound channel limits in the top left corner of the screen. |
| `debug_sound_environment` | boolean | `—` | *(Reclaimers)* If enabled, shows the tag path of the cluster's current sound_environment. |
| `loud_dialog_hack` | boolean | `—` | *(Reclaimers)* Controls the amount of mouse input acceleration. Set to 0 for none. Defaults to 0.7. |
| `sound_gain_under_dialog` | real | `0.699999988079071` | *(Reclaimers)* Controls how quiet non-dialog sounds are when scripted dialog is playing (sound class must be scripted_dialog_other, scripted_dialog_force_player, or scripted_dialog_force_unspatialized). Does not apply to involuntary AI dialog like death/pain lines. Defaults to 0.7. The effect takes about half a second to fade in/out. |

### Structures

| Command | Type | Default | Description |
|---------|------|---------|-------------|
| `debug_bsp` | boolean | `—` | *(Reclaimers)* Toggles the display of structure BSP node traversal for the camera location. At each level, the node and plane indices are shown as well as a + or - symbol indicating if the camera was on the front or back side of the plane. |
| `debug_leaf_index` | long | `-1` | *(Reclaimers)* Shows orange and white spheres with the radius of each dynamic light. White seems to show when a light is not yet been activated, such as the Warthog's brake lights until their first use. Lens flare only lights are not shown since their radius is 0. |
| `debug_leaf_portal_index` | long | `-1` | *(Reclaimers)* Shows orange and white spheres with the radius of each dynamic light. White seems to show when a light is not yet been activated, such as the Warthog's brake lights until their first use. Lens flare only lights are not shown since their radius is 0. |
| `debug_leaf_portals` | boolean | `—` | *(Reclaimers)* Shows orange and white spheres with the radius of each dynamic light. White seems to show when a light is not yet been activated, such as the Warthog's brake lights until their first use. Lens flare only lights are not shown since their radius is 0. |
| `debug_portals` | boolean | `—` | *(Reclaimers)* Draws BSP portals as red outlines. You may wish to pair this with rasterizer_wireframe 1 to help you understand how portals result in culling parts of the BSP from rendering. The related function debug_pvs will enable/disable this global and structures_use_pvs_for_vs. |
| `debug_structure` | boolean | `—` | *(Reclaimers)* When enabled, all scenario_structure_bsp collision surfaces will be rendered with green outlines. A red bounding box surrounds renderable surfaces. |
| `debug_trigger_volumes` | boolean | `—` | *(Reclaimers)* Renders all scenario trigger volumes and their names. |
| `structures_use_pvs_for_vs` | boolean | `—` | *(Reclaimers)* If enabled, forces the renderer to fully render all clusters and subclusters in the potentially visible set (PVS) without any culling of occluded faces, and even if portals into those clusters are off-screen. Pair with rasterizer_wireframe 1 to see the effects. Use this to debug which clusters are in the PVS of the camera's cluster, which can help you understand why a cluster is considered indoor |

### Units

| Command | Type | Default | Description |
|---------|------|---------|-------------|
| `debug_biped_limp_body_disable` | boolean | `—` | *(Reclaimers)* When set to true, pauses the biped limp body system, which is responsible for moving model nodes towards the ground when an eligible biped has died and has come to rest on the structure BSP. Bipeds will lay roughly at the right angle, but they will not conform to the shape of the surfaces below them. If set to false, the limp body system will take effect again even for existing dead bipeds. |
| `debug_biped_physics` | boolean | `—` | *(Reclaimers)* For this to be visible, collision_debug must also be enabled which has the side-effect of making the collision_debug feature itself unusable until the game is restarted. This displays several markers and vectors within the player's physics pill (debug_objects_biped_physics_pill) which can be more easily observed in third person and with framerate_throttle 1. The number and colours of the debug ove |
| `debug_biped_skip_collision` | boolean | `—` | *(Reclaimers)* If true, disables collision checks for bipeds. They will be able to keep "walking" horizontally, but cannot jump or collide with any objects the BSP, and are unaffected by gravity. They will still be forced to stay crouched if crouching below or within a collideable surface. |
| `debug_biped_skip_update` | boolean | `—` | *(Reclaimers)* Toggles the display of structure BSP node traversal for the camera location. At each level, the node and plane indices are shown as well as a + or - symbol indicating if the camera was on the front or back side of the plane. |
| `debug_unit_all_animations` | boolean | `—` | *(Reclaimers)* Logs lines to the console output as unit animations occur. For example: cyborg_mp: animation stand pistol move-right. |
| `debug_unit_animations` | boolean | `—` | *(Reclaimers)* Shows red console log output whenever a unit animation is missing. For example: MISSING: cyborg 'G-driver unarned aim-still'. |
| `debug_unit_illumination` | boolean | `—` | *(Reclaimers)* Force-disconnects from the current multiplayer session and returns to the menu. This can be handy for quickly leaving a server after a game has ended without having to wait for the "Quit" option in the post-game lobby. |

## HaloScript functions (parenthesized)

Return type is the descriptor flags word interpreted through the
HS type-name table at `0x2f14a8`. Help is the descriptor's own
string (same text `help` / `script_doc` print).

### AI

| Command | Returns | Usage | Help |
|---------|---------|-------|------|
| `(ai_actors <type_17>)` | object_list | `<type_17>` | converts an ai reference to an object list. |
| `(ai_allegiance <type_33> <type_33>)` | void | `<type_33> <type_33>` | creates an allegiance between two teams. |
| `(ai_allegiance_broken <type_33> <type_33>)` | boolean | `<type_33> <type_33>` | returns whether two teams have an allegiance that is currently broken by traitorous behavior |
| `(ai_allegiance_remove <type_33> <type_33>)` | void | `<type_33> <type_33>` | destroys an allegiance between two teams. |
| `(ai_allow_charge <type_17> <boolean>)` | void | `<type_17> <boolean>` | either enables or disables charging behavior for a group of actors |
| `(ai_allow_dormant <type_17> <boolean>)` | void | `<type_17> <boolean>` | either enables or disables automatic dormancy for a group of actors |
| `(ai_attach <unit> <type_17>)` | void | `<unit> <type_17>` | attaches the specified unit to the specified encounter. |
| `(ai_attach_free <unit> <type_29>)` | void | `<unit> <type_29>` | attaches a unit to a newly created free actor of the specified type |
| `(ai_attack <type_17>)` | void | `<type_17>` | makes the specified platoon(s) go into the attacking state. |
| `(ai_automatic_migration_target <type_17> <boolean>)` | void | `<type_17> <boolean>` | enables or disables a squad as being an automatic migration target |
| `(ai_berserk <type_17> <boolean>)` | void | `<type_17> <boolean>` | forces a group of actors to start or stop berserking |
| `(ai_braindead <type_17> <boolean>)` | void | `<type_17> <boolean>` | makes a group of actors braindead, or restores them to life (in their initial state) |
| `(ai_braindead_by_unit <object_list> <boolean>)` | void | `<object_list> <boolean>` | makes a list of objects braindead, or restores them to life. if you pass in a vehicle index, it makes all actors in that vehicle braindead (including any built-in guns) |
| `(ai_command_list <type_17> <type_18>)` | void | `<type_17> <type_18>` | tells a group of actors to begin executing the specified command list |
| `(ai_command_list_advance <type_17>)` | void | `<type_17>` | tells a group of actors that are running a command list that they may advance further along the list (if they are waiting for a stimulus) |
| `(ai_command_list_advance_by_unit <unit>)` | void | `<unit>` | just like ai_command_list_advance but operates upon a unit instead |
| `(ai_command_list_by_unit <unit> <type_18>)` | void | `<unit> <type_18>` | tells a named unit to begin executing the specified command list |
| `(ai_command_list_status <object_list>)` | short | `<object_list>` | gets the status of a number of units running command lists: 0 = none, 1 = finished command list, 2 = waiting for stimulus, 3 = running command list |
| `(ai_conversation <type_20>)` | boolean | `<type_20>` | tries to add an entry to the list of conversations waiting to play. returns FALSE if the required units could not be found to play the conversation, or if the player is too far away and the 'delay' flag is not set. |
| `(ai_conversation_advance <type_20>)` | void | `<type_20>` | tells a conversation that it may advance |
| `(ai_conversation_line <type_20>)` | short | `<type_20>` | returns which line the conversation is currently playing, or 999 if the conversation is not currently playing |
| `(ai_conversation_status <type_20>)` | short | `<type_20>` | returns the status of a conversation (0=none, 1=trying to begin, 2=waiting for guys to get in position, 3=playing, 4=waiting to advance, 5=could not begin, 6=finished successfully, 7=aborted midway |
| `(ai_conversation_stop <type_20>)` | void | `<type_20>` | stops a conversation from playing or trying to play |
| `(ai_debug_communication_focus <string(s)>)` | void | `<string(s)>` | focuses (or stops focusing) a set of unit vocalization types. |
| `(ai_debug_communication_ignore <string(s)>)` | void | `<string(s)>` | ignores (or stops ignoring) a set of AI communication types when printing out communications. |
| `(ai_debug_communication_suppress <string(s)>)` | void | `<string(s)>` | suppresses (or stops suppressing) a set of AI communication types. |
| `(ai_debug_sound_point_set)` | void | `` | drops the AI debugging sound point at the camera location |
| `(ai_debug_speak <type_9>)` | void | `<type_9>` | makes the currently selected AI speak a vocalization (e.g. ai_speak "pain minor") |
| `(ai_debug_speak_list <type_9>)` | void | `<type_9>` | makes the currently selected AI speak a list of vocalizations (e.g. ai_speak_list "involuntary") |
| `(ai_debug_teleport_to <type_17>)` | void | `<type_17>` | teleports all players to the specified encounter |
| `(ai_debug_vocalize <type_9> <type_9>)` | void | `<type_9> <type_9>` | makes the selected AI vocalize |
| `(ai_defend <type_17>)` | void | `<type_17>` | makes the specified platoon(s) go into the defending state. |
| `(ai_deselect)` | void | `` | clears the selected encounter. |
| `(ai_detach <unit>)` | void | `<unit>` | detaches the specified unit from all AI. |
| `(ai_dialogue_triggers <boolean>)` | void | `<boolean>` | turns impromptu dialogue on or off. |
| `(ai_disregard <object_list> <boolean>)` | void | `<object_list> <boolean>` | if TRUE, forces all actors to completely disregard the specified units, otherwise lets them acknowledge the units again |
| `(ai_erase <type_17>)` | void | `<type_17>` | erases the specified encounter and/or squad. |
| `(ai_erase_all)` | void | `` | erases all AI. |
| `(ai_exit_vehicle <type_17>)` | void | `<type_17>` | tells a group of actors to get out of any vehicles that they are in |
| `(ai_follow_distance <type_17> <real>)` | void | `<type_17> <real>` | sets the distance threshold which will cause squads to migrate when following someone |
| `(ai_follow_target_ai <type_17> <type_17>)` | void | `<type_17> <type_17>` | sets the follow target for an encounter to be a group of AI (encounter, squad or platoon) |
| `(ai_follow_target_disable <type_17>)` | void | `<type_17>` | turns off following for an encounter |
| `(ai_follow_target_players <type_17>)` | void | `<type_17>` | sets the follow target for an encounter to be the closest player |
| `(ai_follow_target_unit <type_17> <unit>)` | void | `<type_17> <unit>` | sets the follow target for an encounter to be a specific unit |
| `(ai_force_active <type_17> <boolean>)` | void | `<type_17> <boolean>` | forces an encounter to remain active (i.e. not freeze in place) even if there are no players nearby |
| `(ai_force_active_by_unit <unit> <boolean>)` | void | `<unit> <boolean>` | forces a named actor that is NOT in an encounter to remain active (i.e. not freeze in place) even if there are no players nearby |
| `(ai_free <type_17>)` | void | `<type_17>` | removes a group of actors from their encounter and sets them free |
| `(ai_free_units <object_list>)` | void | `<object_list>` | removes a set of units from their encounter (if any) and sets them free |
| `(ai_go_to_vehicle <type_17> <unit> <type_9>)` | void | `<type_17> <unit> <type_9>` | tells a group of actors to get into a vehicle, in the substring-specified seats (e.g. passenger for pelican)... does not interrupt any actors who are already going to vehicles |
| `(ai_go_to_vehicle_override <type_17> <unit> <type_9>)` | void | `<type_17> <unit> <type_9>` | tells a group of actors to get into a vehicle, in the substring-specified seats (e.g. passenger for pelican)... NB: any actors who are already going to vehicles will stop and go to this one instead! |
| `(ai_going_to_vehicle <unit>)` | short | `<unit>` | return the number of actors that are still trying to get into the specified vehicle |
| `(ai_grenades <boolean>)` | void | `<boolean>` | turns grenade inventory on or off. |
| `(ai_is_attacking <type_17>)` | boolean | `<type_17>` | returns whether a platoon is in the attacking mode (or if an encounter is specified, returns whether any platoon in that encounter is attacking) |
| `(ai_kill <type_17>)` | void | `<type_17>` | instantly kills the specified encounter and/or squad. |
| `(ai_kill_silent <type_17>)` | void | `<type_17>` | instantly and silently (no animation or sound played) kills the specified encounter and/or squad. |
| `(ai_lines)` | void | `` | cycles through AI line-spray modes |
| `(ai_link_activation <type_17> <type_17>)` | void | `<type_17> <type_17>` | links the first encounter so that it will be made active whenever it detects that the second encounter is active |
| `(ai_living_count <type_17>)` | short | `<type_17>` | return the number of living actors in the specified encounter and/or squad. |
| `(ai_living_fraction <type_17>)` | real | `<type_17>` | return the fraction [0-1] of living actors in the specified encounter and/or squad. |
| `(ai_look_at_object <unit> <object>)` | void | `<unit> <object>` | tells an actor to look at an object until further notice |
| `(ai_magically_see_encounter <type_17> <type_17>)` | void | `<type_17> <type_17>` | makes one encounter magically aware of another. |
| `(ai_magically_see_players <type_17>)` | void | `<type_17>` | makes an encounter magically aware of nearby players. |
| `(ai_magically_see_unit <type_17> <unit>)` | void | `<type_17> <unit>` | makes an encounter magically aware of the specified unit. |
| `(ai_maneuver <type_17>)` | void | `<type_17>` | makes all squads in the specified platoon(s) maneuver to their designated maneuver squads. |
| `(ai_maneuver_enable <type_17> <boolean>)` | void | `<type_17> <boolean>` | enables or disables the maneuver/retreat rule for an encounter or platoon. the rule will still trigger, but none of the actors will be given the order to change squads. |
| `(ai_migrate <type_17> <type_17>)` | void | `<type_17> <type_17>` | makes all or part of an encounter move to another encounter. |
| `(ai_migrate_and_speak <type_17> <type_17> <type_9>)` | void | `<type_17> <type_17> <type_9>` | makes all or part of an encounter move to another encounter, and say their 'advance' or 'retreat' speech lines. |
| `(ai_migrate_by_unit <object_list> <type_17>)` | void | `<object_list> <type_17>` | makes a named vehicle or group of units move to another encounter. |
| `(ai_nonswarm_count <type_17>)` | short | `<type_17>` | return the number of non-swarm actors in the specified encounter and/or squad. |
| `(ai_place <type_17>)` | void | `<type_17>` | places the specified encounter on the map. |
| `(ai_playfight <type_17> <boolean>)` | void | `<type_17> <boolean>` | sets an encounter to be playfighting or not |
| `(ai_prefer_target <object_list> <boolean>)` | void | `<object_list> <boolean>` | if TRUE, *ALL* enemies will prefer to attack the specified units. if FALSE, removes the preference. |
| `(ai_reconnect)` | void | `` | reconnects all AI information to the current structure bsp (use this after you create encounters or command lists in sapien, or place new firing points or command list points) |
| `(ai_renew <type_17>)` | void | `<type_17>` | refreshes the health and grenade count of a group of actors, so they are as good as new |
| `(ai_retreat <type_17>)` | void | `<type_17>` | makes all squads in the specified platoon(s) maneuver to their designated maneuver squads. |
| `(ai_select <type_17>)` | void | `<type_17>` | selects the specified encounter. |
| `(ai_set_blind <type_17> <boolean>)` | void | `<type_17> <boolean>` | enables or disables sight for actors in the specified encounter. |
| `(ai_set_current_state <type_17> <type_34>)` | void | `<type_17> <type_34>` | sets the current state of a group of actors. WARNING: may have unpredictable results on actors that are in combat |
| `(ai_set_deaf <type_17> <boolean>)` | void | `<type_17> <boolean>` | enables or disables hearing for actors in the specified encounter. |
| `(ai_set_respawn <type_17> <boolean>)` | void | `<type_17> <boolean>` | enables or disables respawning in the specified encounter. |
| `(ai_set_return_state <type_17> <type_34>)` | void | `<type_17> <type_34>` | sets the state that a group of actors will return to when they have nothing to do |
| `(ai_set_team <type_17> <type_33>)` | void | `<type_17> <type_33>` | makes an encounter change to a new team |
| `(ai_spawn_actor <type_17>)` | void | `<type_17>` | spawns a single actor in the specified encounter and/or squad. |
| `(ai_status <type_17>)` | short | `<type_17>` | returns the most severe combat status of a group of actors (0=inactive, 1=noncombat, 2=guarding, 3=search/suspicious, 4=definite enemy(heard or magic awareness), 5=visible enemy, 6=engaging in combat. |
| `(ai_stop_looking <unit>)` | void | `<unit>` | tells an actor to stop looking at whatever it's looking at |
| `(ai_strength <type_17>)` | real | `<type_17>` | return the current strength (average body vitality from 0-1) of the specified encounter and/or squad. |
| `(ai_swarm_count <type_17>)` | short | `<type_17>` | return the number of swarm actors in the specified encounter and/or squad. |
| `(ai_teleport_to_starting_location <type_17>)` | void | `<type_17>` | teleports a group of actors to the starting locations of their current squad(s) |
| `(ai_teleport_to_starting_location_if_unsupported <type_17>)` | void | `<type_17>` | teleports a group of actors to the starting locations of their current squad(s), only if they are not supported by solid ground (i.e. if they are falling after switching BSPs) |
| `(ai_timer_expire <type_17>)` | void | `<type_17>` | makes a squad's delay timer expire and releases them to enter combat. |
| `(ai_timer_start <type_17>)` | void | `<type_17>` | makes a squad's delay timer start counting. |
| `(ai_try_to_fight <type_17> <type_17>)` | void | `<type_17> <type_17>` | causes a group of actors to preferentially target another group of actors |
| `(ai_try_to_fight_nothing <type_17>)` | void | `<type_17>` | removes the preferential target setting from a group of actors |
| `(ai_try_to_fight_player <type_17>)` | void | `<type_17>` | causes a group of actors to preferentially target the player |
| `(ai_vehicle_encounter <unit> <type_17>)` | void | `<unit> <type_17>` | sets a vehicle to 'belong' to a particular encounter/squad. any actors who get into the vehicle will be placed in this squad. NB: vehicles potentially drivable by multiple teams need their own encounter! |
| `(ai_vehicle_enterable_actor_type <unit> <type_35>)` | void | `<unit> <type_35>` | sets a vehicle as being impulsively enterable for actors of a certain type (grunt, elite, marine etc) |
| `(ai_vehicle_enterable_actors <unit> <type_17>)` | void | `<unit> <type_17>` | sets a vehicle as being impulsively enterable for a certain encounter/squad of actors |
| `(ai_vehicle_enterable_disable <unit>)` | void | `<unit>` | disables actors from impulsively getting into a vehicle (this is the default state for newly placed vehicles) |
| `(ai_vehicle_enterable_distance <unit> <real>)` | void | `<unit> <real>` | sets a vehicle as being impulsively enterable for actors within a certain distance |
| `(ai_vehicle_enterable_team <unit> <type_33>)` | void | `<unit> <type_33>` | sets a vehicle as being impulsively enterable for actors on a certain team |

### Camera

| Command | Returns | Usage | Help |
|---------|---------|-------|------|
| `(camera_control <boolean>)` | void | `<boolean>` | toggles script control of the camera. |
| `(camera_set <type_13> <short>)` | void | `<type_13> <short>` | moves the camera to the specified camera point over the specified number of ticks. |
| `(camera_set_animation <type_28> <type_9>)` | void | `<type_28> <type_9>` | begins a prerecorded camera animation. |
| `(camera_set_dead <unit>)` | void | `<unit>` | makes the scripted camera zoom out around a unit as if it were dead. |
| `(camera_set_first_person <unit>)` | void | `<unit>` | makes the scripted camera follow a unit. |
| `(camera_set_relative <type_13> <short> <object>)` | void | `<type_13> <short> <object>` | moves the camera to the specified camera point over the specified number of ticks (position is relative to the specified object). |
| `(camera_time)` | short | `` | returns the number of ticks remaining in the current camera interpolation. |
| `(recording_kill <unit>)` | void | `<unit>` | kill the specified unit's cutscene recording. |
| `(recording_play <unit> <type_15>)` | boolean | `<unit> <type_15>` | make the specified unit run the specified cutscene recording. |
| `(recording_play_and_delete <unit> <type_15>)` | boolean | `<unit> <type_15>` | make the specified unit run the specified cutscene recording, deletes the unit when the animation finishes. |
| `(recording_play_and_hover <type_39> <type_15>)` | boolean | `<type_39> <type_15>` | make the specified vehicle run the specified cutscene recording, hovers the vehicle when the animation finishes. |
| `(recording_time <unit>)` | short | `<unit>` | return the time remaining in the specified unit's cutscene recording. |

### Cheats

| Command | Returns | Usage | Help |
|---------|---------|-------|------|
| `(cheat_active_camouflage)` | void | `` | gives the player active camouflage |
| `(cheat_active_camouflage_local_player <short>)` | void | `<short>` | gives the player active camouflage |
| `(cheat_all_powerups)` | void | `` | drops all powerups near player |
| `(cheat_all_vehicles)` | void | `` | drops all vehicles on player |
| `(cheat_all_weapons)` | void | `` | drops all weapons near player |
| `(cheat_teleport_to_camera)` | void | `` | teleports player to camera location |
| `(cheats_load)` | void | `` | reloads the cheats.txt file |

### Cinematics

| Command | Returns | Usage | Help |
|---------|---------|-------|------|
| `(attract_mode_start)` | void | `` |  |
| `(cinematic_screen_effect_set_convolution <short> <short> <real> <real> <real>)` | void | `<short> <short> <real> <real> <real>` | sets the convolution effect |
| `(cinematic_screen_effect_set_filter <real> <real> <real> <real> <boolean> <real>)` | void | `<real> <real> <real> <real> <boolean> <real>` | sets the filter effect |
| `(cinematic_screen_effect_set_filter_desaturation_tint <real> <real> <real>)` | void | `<real> <real> <real>` | sets the desaturation filter tint color |
| `(cinematic_screen_effect_set_video <short> <real>)` | void | `<short> <real>` | sets the video effect: <noise intensity[0,1]>, <overbright: 0=none, 1=2x, 2=4x> |
| `(cinematic_screen_effect_start <boolean>)` | void | `<boolean>` | starts screen effect; pass TRUE to clear |
| `(cinematic_screen_effect_stop)` | void | `` | returns control of the screen effects to the rest of the game |
| `(cinematic_set_near_clip_distance <real>)` | void | `<real>` |  |
| `(cinematic_set_title <type_14>)` | void | `<type_14>` | activates the chapter title |
| `(cinematic_set_title_delayed <type_14> <real>)` | void | `<type_14> <real>` | activates the chapter title, delayed by <real> seconds |
| `(cinematic_show_letterbox <boolean>)` | void | `<boolean>` | sets or removes the letterbox bars |
| `(cinematic_skip_start_internal)` | void | `` |  |
| `(cinematic_skip_stop_internal)` | void | `` |  |
| `(cinematic_start)` | void | `` | initializes game to start a cinematic (interruptive) cutscene |
| `(cinematic_stop)` | void | `` | initializes the game to end a cinematic (interruptive) cutscene |
| `(cinematic_suppress_bsp_object_creation <boolean>)` | void | `<boolean>` | suppresses or enables the automatic creation of objects during cutscenes due to a bsp switch |
| `(fade_in <real> <real> <real> <short>)` | void | `<real> <real> <real> <short>` | does a screen fade in from a particular color |
| `(fade_out <real> <real> <real> <short>)` | void | `<real> <real> <real> <short>` | does a screen fade out to a particular color |

### Control

| Command | Returns | Usage | Help |
|---------|---------|-------|------|
| `(begin <expression(s)>)` | passthrough | `<expression(s)>` | returns the last expression in a sequence after evaluating the sequence in order. |
| `(begin_random <expression(s)>)` | passthrough | `<expression(s)>` | evaluates the sequence of expressions in random order and returns the last value evaluated. |
| `(cond (<boolean1> <result1>) [(<boolean2> <result2>) [...]])` | passthrough | `(<boolean1> <result1>) [(<boolean2> <result2>) [...]]` | returns the value associated with the first true condition. |
| `(if <boolean> <then> [<else>])` | passthrough | `<boolean> <then> [<else>]` | returns one of two values based on the value of a condition. |
| `(set <variable name> <expression>)` | passthrough | `<variable name> <expression>` | set the value of a global variable. |
| `(sleep <short> [<script>])` | void | `<short> [<script>]` | pauses execution of this script (or, optionally, another script) for the specified number of ticks. |
| `(sleep_until <boolean> [<short>])` | void | `<boolean> [<short>]` | pauses execution of this script until the specified condition is true, checking once per second unless a different number of ticks is specified. |
| `(wake <script name>)` | void | `<script name>` | wakes a sleeping script in the next update. |

### Debug

| Command | Returns | Usage | Help |
|---------|---------|-------|------|
| `(cls)` | void | `` | clears console text from the screen |
| `(crash <type_9>)` | void | `<type_9>` | crashes (for debugging). |
| `(debug_camera_load)` | void | `` | loads the saved camera position and facing. |
| `(debug_camera_save)` | void | `` | saves the camera position and facing. |
| `(debug_memory)` | void | `` | dumps memory leaks. |
| `(debug_memory_by_file)` | void | `` | dumps memory leaks by source file. |
| `(debug_memory_for_file <type_9>)` | void | `<type_9>` | dumps memory leaks from the specified source file. |
| `(debug_pvs <boolean>)` | void | `<boolean>` | displays the current pvs. |
| `(debug_tags)` | void | `` | writes all memory being used by tag files into tag_dump.txt |
| `(debug_teleport_player <short> <short>)` | void | `<short> <short>` |  |
| `(enumerate_memory_units)` | void | `` | enumerate memory units |
| `(error_overflow_suppression <boolean>)` | void | `<boolean>` | enables or disables the suppression of error spamming |
| `(help <type_9>)` | void | `<type_9>` | prints a description of the named function. |
| `(inspect <expression>)` | void | `<expression>` | prints the value of an expression to the screen for debugging purposes. |
| `(playback)` | void | `` | starts game in film playback mode |
| `(print <type_9>)` | void | `<type_9>` | prints a string to the console. |
| `(script_doc)` | void | `` | saves a file called hs_doc.txt with parameters for all script commands. |
| `(script_recompile)` | void | `` | recompiles scripts. |
| `(version)` | void | `` | prints the build version. |

### Devices

| Command | Returns | Usage | Help |
|---------|---------|-------|------|
| `(device_get_position <type_41>)` | real | `<type_41>` | gets the current position of the given device (used for devices without explicit device groups) |
| `(device_get_power <type_41>)` | real | `<type_41>` | gets the current power of a named device |
| `(device_group_change_only_once_more_set <type_16> <boolean>)` | void | `<type_16> <boolean>` | TRUE allows a device to change states only once |
| `(device_group_get <type_16>)` | real | `<type_16>` | returns the desired value of the specified device group. |
| `(device_group_set <type_16> <real>)` | boolean | `<type_16> <real>` | changes the desired value of the specified device group. |
| `(device_group_set_immediate <type_16> <real>)` | void | `<type_16> <real>` | instantaneously changes the value of the specified device group. |
| `(device_one_sided_set <type_41> <boolean>)` | void | `<type_41> <boolean>` | TRUE makes the given device one-sided (only able to be opened from one direction), FALSE makes it two-sided |
| `(device_operates_automatically_set <type_41> <boolean>)` | void | `<type_41> <boolean>` | TRUE makes the given device open automatically when any biped is nearby, FALSE makes it not |
| `(device_set_never_appears_locked <type_41> <boolean>)` | void | `<type_41> <boolean>` | changes a machine's never_appears_locked flag, but only if paul is a bastard |
| `(device_set_position <type_41> <real>)` | boolean | `<type_41> <real>` | set the desired position of the given device (used for devices without explicit device groups) |
| `(device_set_position_immediate <type_41> <real>)` | void | `<type_41> <real>` | instantaneously changes the position of the given device (used for devices without explicit device groups |
| `(device_set_power <type_41> <real>)` | void | `<type_41> <real>` | immediately sets the power of a named device to the given value |

### Effects

| Command | Returns | Usage | Help |
|---------|---------|-------|------|
| `(damage_new <type_26> <type_12>)` | void | `<type_26> <type_12>` | causes the specified damage at the specified flag. |
| `(damage_object <type_26> <object>)` | void | `<type_26> <object>` | causes the specified damage at the specified object. |
| `(effect_new <type_25> <type_12>)` | void | `<type_25> <type_12>` | starts the specified effect at the specified flag. |
| `(effect_new_on_object_marker <type_25> <object> <type_9>)` | void | `<type_25> <object> <type_9>` | starts the specified effect on the specified object at the specified marker. |
| `(scenery_animation_start <type_42> <type_28> <type_9>)` | void | `<type_42> <type_28> <type_9>` | starts a custom animation playing on a piece of scenery |
| `(scenery_animation_start_at_frame <type_42> <type_28> <type_9> <short>)` | void | `<type_42> <type_28> <type_9> <short>` | starts a custom animation playing on a piece of scenery at a specific frame |
| `(scenery_get_animation_time <type_42>)` | short | `<type_42>` | returns the number of ticks remaining in a custom animation (or zero, if the animation is over). |

### Game

| Command | Returns | Usage | Help |
|---------|---------|-------|------|
| `(core_load)` | void | `` | loads debug game state from core\core.bin |
| `(core_load_at_startup)` | void | `` | loads debug game state from core\core.bin as soon as the map is initialized |
| `(core_load_name <type_9>)` | void | `<type_9>` | loads debug game state from core\<path> |
| `(core_load_name_at_startup <type_9>)` | void | `<type_9>` | loads debug game state from core\<path> as soon as the map is initialized |
| `(core_save)` | void | `` | saves debug game state to core\core.bin |
| `(core_save_name <type_9>)` | void | `<type_9>` | saves debug game state to core\<path> |
| `(delete_save_game_files)` | void | `` | delete all custom profile files |
| `(game_all_quiet)` | boolean | `` | returns FALSE if there are bad guys around, projectiles in the air, etc. |
| `(game_difficulty_get)` | game_difficulty | `` | returns the current difficulty setting, but lies to you and will never return easy, instead returning normal |
| `(game_difficulty_get_real)` | game_difficulty | `` | returns the actual current difficulty setting without lying |
| `(game_difficulty_set <game_difficulty>)` | void | `<game_difficulty>` | changes the difficulty setting for the next map to be loaded. |
| `(game_is_cooperative)` | boolean | `` | returns TRUE if the game is cooperative |
| `(game_lost)` | void | `` | causes the player to revert to his previous saved game |
| `(game_revert)` | void | `` | reverts to last saved game, if any (for testing, the first bastard that does this to me gets it in the head) |
| `(game_reverted)` | boolean | `` | don't use this for anything, you black-hearted bastards. |
| `(game_safe_to_save)` | boolean | `` | returns FALSE if it would be a bad idea to save the player's game right now |
| `(game_safe_to_speak)` | boolean | `` | returns FALSE if it would be a bad idea to save the player's game right now |
| `(game_save)` | void | `` | checks to see if it is safe to save game, then saves (gives up after 8 seconds) |
| `(game_save_cancel)` | void | `` | cancels any pending game_save, timeout or not |
| `(game_save_no_timeout)` | void | `` | checks to see if it is safe to save game, then saves (this version never gives up) |
| `(game_save_totally_unsafe)` | void | `` | disregards player's current situation |
| `(game_saving)` | boolean | `` | checks to see if the game is trying to save the map. |
| `(game_skip_ticks <short>)` | void | `<short>` | skips <short> amount of game ticks. ONLY USE IN CUTSCENES!!! |
| `(game_speed <real>)` | void | `<real>` | changes the game speed. |
| `(game_time)` | long | `` | gets ticks elapsed since the start of the game. |
| `(game_variant <type_9>)` | void | `<type_9>` | set the game engine |
| `(game_won)` | void | `` | causes the player to successfully finish the current level and move to the next |
| `(map_name <type_9>)` | void | `<type_9>` | changes the name of the solo player map. |
| `(map_reset)` | void | `` | starts the map from the beginning. |
| `(multiplayer_map_name <type_9>)` | void | `<type_9>` | changes the name of the multiplayer map |
| `(structure_bsp_index)` | short | `` | returns the current structure bsp index |
| `(switch_bsp <short>)` | void | `<short>` | takes off your condom and changes to a different structure bsp |

### HUD/UI

| Command | Returns | Usage | Help |
|---------|---------|-------|------|
| `(activate_nav_point_flag <type_21> <unit> <type_12> <real>)` | void | `<type_21> <unit> <type_12> <real>` | activates a nav point type <string> attached to (local) player <unit> anchored to a flag with a vertical offset <real>. If the player is not local to the machine, this will fail |
| `(activate_nav_point_object <type_21> <unit> <object> <real>)` | void | `<type_21> <unit> <object> <real>` | activates a nav point type <string> attached to (local) player <unit> anchored to an object with a vertical offset <real>. If the player is not local to the machine, this will fail |
| `(activate_team_nav_point_flag <type_21> <type_33> <type_12> <real>)` | void | `<type_21> <type_33> <type_12> <real>` | activates a nav point type <string> attached to a team anchored to a flag with a vertical offset <real>. If the player is not local to the machine, this will fail |
| `(activate_team_nav_point_object <type_21> <type_33> <object> <real>)` | void | `<type_21> <type_33> <object> <real>` | activates a nav point type <string> attached to a team anchored to an object with a vertical offset <real>. If the player is not local to the machine, this will fail |
| `(deactivate_nav_point_flag <unit> <type_12>)` | void | `<unit> <type_12>` | deactivates a nav point type attached to a player <unit> anchored to a flag |
| `(deactivate_nav_point_object <unit> <object>)` | void | `<unit> <object>` | deactivates a nav point type attached to a player <unit> anchored to an object |
| `(deactivate_team_nav_point_flag <type_33> <type_12>)` | void | `<type_33> <type_12>` | deactivates a nav point type attached to a team anchored to a flag |
| `(deactivate_team_nav_point_object <type_33> <object>)` | void | `<type_33> <object>` | deactivates a nav point type attached to a team anchored to an object |
| `(enable_hud_help_flash <boolean>)` | void | `<boolean>` | starts/stops the help text flashing |
| `(hud_blink_health <boolean>)` | void | `<boolean>` | starts/stops manual blinking of the health panel |
| `(hud_blink_motion_sensor <boolean>)` | void | `<boolean>` | starts/stops manual blinking of the motion sensor panel |
| `(hud_blink_shield <boolean>)` | void | `<boolean>` | starts/stops manual blinking of the shield panel |
| `(hud_clear_messages)` | void | `` | clears all non-state messages on the hud |
| `(hud_get_timer_ticks)` | short | `` | returns the ticks left on the hud timer |
| `(hud_help_flash_restart)` | void | `` | resets the timer for the help text flashing |
| `(hud_set_help_text <type_22>)` | void | `<type_22>` | displays <message> as the help text |
| `(hud_set_objective_text <type_22>)` | void | `<type_22>` | sets <message> as the current objective |
| `(hud_set_timer_position <short> <short> <type_36>)` | void | `<short> <short> <type_36>` | sets the timer upper left position to (x, y)=>(<short>, <short>) |
| `(hud_set_timer_time <short> <short>)` | void | `<short> <short>` | sets the time for the timer to <short> minutes and <short> seconds, and starts and displays timer |
| `(hud_set_timer_warning_time <short> <short>)` | void | `<short> <short>` | sets the warning time for the timer to <short> minutes and <short> seconds |
| `(hud_show_crosshair <boolean>)` | void | `<boolean>` | hides/shows the weapon crosshair |
| `(hud_show_health <boolean>)` | void | `<boolean>` | hides/shows the health panel |
| `(hud_show_motion_sensor <boolean>)` | void | `<boolean>` | hides/shows the motion sensor panel |
| `(hud_show_shield <boolean>)` | void | `<boolean>` | hides/shows the shield panel |
| `(show_hud <boolean>)` | boolean | `<boolean>` | shows or hides the hud |
| `(show_hud_help_text <boolean>)` | boolean | `<boolean>` | shows or hides the hud help text |
| `(show_hud_timer <boolean>)` | void | `<boolean>` | displays the hud timer |
| `(time_code_reset)` | void | `` | resets the time code timer |
| `(time_code_show <boolean>)` | void | `<boolean>` | shows the time code timer |
| `(time_code_start <boolean>)` | void | `<boolean>` | starts/stops the time code timer |

### Logic

| Command | Returns | Usage | Help |
|---------|---------|-------|------|
| `(!= <expression> <expression>)` | boolean | `<expression> <expression>` | returns true if two expressions are not equal |
| `(< <number> <number>)` | boolean | `<number> <number>` | returns true if the first number is smaller than the second. |
| `(<= <number> <number>)` | boolean | `<number> <number>` | returns true if the first number is smaller than or equal to the second. |
| `(= <expression> <expression>)` | boolean | `<expression> <expression>` | returns true if two expressions are equal |
| `(> <number> <number>)` | boolean | `<number> <number>` | returns true if the first number is larger than the second. |
| `(>= <number> <number>)` | boolean | `<number> <number>` | returns true if the first number is larger than or equal to the second. |
| `(and <boolean(s)>)` | boolean | `<boolean(s)>` | returns true if all specified expressions are true. |
| `(not <boolean>)` | boolean | `<boolean>` | returns the opposite of the expression. |
| `(or <boolean(s)>)` | boolean | `<boolean(s)>` | returns true if any specified expressions are true. |

### Math

| Command | Returns | Usage | Help |
|---------|---------|-------|------|
| `(* <number(s)>)` | real | `<number(s)>` | returns the product of all specified expressions. |
| `(+ <number(s)>)` | real | `<number(s)>` | returns the sum of all specified expressions. |
| `(- <number> <number>)` | real | `<number> <number>` | returns the difference of two expressions. |
| `(/ <number> <number>)` | real | `<number> <number>` | returns the quotient of two expressions. |
| `(max <number(s)>)` | real | `<number(s)>` | returns the maximum of all specified expressions. |
| `(min <number(s)>)` | real | `<number(s)>` | returns the minimum of all specified expressions. |
| `(random_range <short> <short>)` | short | `<short> <short>` | returns a random value in the range [lower bound, upper bound) |
| `(real_random_range <real> <real>)` | real | `<real> <real>` | returns a random value in the range [lower bound, upper bound) |

### Misc

| Command | Returns | Usage | Help |
|---------|---------|-------|------|
| `(ai <boolean>)` | void | `<boolean>` | turns all AI on or off. |
| `(display_scenario_help <short>)` | void | `<short>` | display in-game help dialog |
| `(list_count <object_list>)` | short | `<object_list>` | returns the number of objects in a list |
| `(list_get <object_list> <short>)` | object | `<object_list> <short>` | returns an item in an object list. |
| `(numeric_countdown_timer_get <short>)` | short | `<short>` | <digit_index> |
| `(numeric_countdown_timer_restart)` | void | `` |  |
| `(numeric_countdown_timer_set <long> <boolean>)` | void | `<long> <boolean>` | <milliseconds>, <auto_start> |
| `(numeric_countdown_timer_stop)` | void | `` |  |
| `(pause_hud_timer <boolean>)` | void | `<boolean>` | pauses or unpauses the hud timer |
| `(structure_lens_flares_place)` | void | `` | places lens flares in the structure bsp |
| `(ui_widget_show_path <boolean>)` | void | `<boolean>` | blah blah |
| `(unit <object>)` | unit | `<object>` | converts an object to a unit. |
| `(xbox_set_machine_name <type_9>)` | void | `<type_9>` | YAHSFFZ |

### Network

| Command | Returns | Usage | Help |
|---------|---------|-------|------|
| `(fast_setup_network_server)` | void | `` | for zach's multiplayer testing |
| `(network_game_start_now)` | void | `` | another one for zach |

### Objects

| Command | Returns | Usage | Help |
|---------|---------|-------|------|
| `(breakable_surfaces_enable <boolean>)` | void | `<boolean>` | enables or disables breakability of all breakable surfaces on level |
| `(breakable_surfaces_reset)` | void | `` | restores all breakable surfaces |
| `(garbage_collect_now)` | void | `` | causes all garbage objects except those visible to a player to be collected immediately |
| `(object_beautify <object> <boolean>)` | void | `<object> <boolean>` | makes an object pretty for the remainder of the levels' cutscenes. |
| `(object_can_take_damage <object_list>)` | void | `<object_list>` | allows an object to take damage again |
| `(object_cannot_take_damage <object_list>)` | void | `<object_list>` | prevents an object from taking damage |
| `(object_create <type_43>)` | void | `<type_43>` | creates an object from the scenario. |
| `(object_create_anew <type_43>)` | void | `<type_43>` | creates an object, destroying it first if it already exists. |
| `(object_create_anew_containing <type_9>)` | void | `<type_9>` | creates anew all objects from the scenario whose names contain the given substring. |
| `(object_create_containing <type_9>)` | void | `<type_9>` | creates all objects from the scenario whose names contain the given substring. |
| `(object_destroy <object>)` | void | `<object>` | destroys an object. |
| `(object_destroy_all)` | void | `` | destroys all non player objects. |
| `(object_destroy_containing <type_9>)` | void | `<type_9>` | destroys all objects from the scenario whose names contain the given substring. |
| `(object_pvs_activate <object>)` | void | `<object>` | just another (old) name for object_pvs_set_object. |
| `(object_pvs_clear)` | void | `` | removes the special place that activates everything it sees. |
| `(object_pvs_set_camera <type_13>)` | void | `<type_13>` | sets the specified cutscene camera point as the special place that activates everything it sees. |
| `(object_pvs_set_object <object>)` | void | `<object>` | sets the specified object as the special place that activates everything it sees. |
| `(object_set_collideable <object> <boolean>)` | void | `<object> <boolean>` | FALSE prevents any object from colliding with the given object |
| `(object_set_facing <object> <type_12>)` | void | `<object> <type_12>` | turns the specified object in the direction of the specified flag. |
| `(object_set_melee_attack_inhibited <object> <boolean>)` | void | `<object> <boolean>` | FALSE prevents object from using melee attack |
| `(object_set_permutation <object> <type_9> <type_9>)` | void | `<object> <type_9> <type_9>` | sets the desired region (use "" for all regions) to the permutation with the given name, e.g. (object_set_permutation flood "right arm" ~damaged) |
| `(object_set_ranged_attack_inhibited <object> <boolean>)` | void | `<object> <boolean>` | FALSE prevents object from using ranged attack |
| `(object_set_scale <object> <real> <short>)` | void | `<object> <real> <short>` | sets the scale for a given object and interpolates over the given number of frames to achieve that scale |
| `(object_set_shield <object> <real>)` | void | `<object> <real>` | sets the shield vitality of the specified object (between 0 and 1). |
| `(object_teleport <object> <type_12>)` | void | `<object> <type_12>` | moves the specified object to the specified flag. |
| `(object_type_predict <type_31>)` | void | `<type_31>` | loads textures necessary to draw an object that's about to come on-screen. |
| `(objects_attach <object> <type_9> <object> <type_9>)` | void | `<object> <type_9> <object> <type_9>` | attaches the second object to the first; both strings can be empty |
| `(objects_can_see_flag <object_list> <type_12> <real>)` | boolean | `<object_list> <type_12> <real>` | returns true if any of the specified units are looking within the specified number of degrees of the flag. |
| `(objects_can_see_object <object_list> <object> <real>)` | boolean | `<object_list> <object> <real>` | returns true if any of the specified units are looking within the specified number of degrees of the object. |
| `(objects_delete_by_definition <type_31>)` | void | `<type_31>` | deletes all objects of type <definition> |
| `(objects_detach <object> <object>)` | void | `<object> <object>` | detaches from the given parent object the given child object |
| `(objects_dump_memory)` | void | `` | debugs object memory usage |
| `(objects_predict <object_list>)` | void | `<object_list>` | loads textures necessary to draw a objects that are about to come on-screen. |
| `(volume_teleport_players_not_inside <type_11> <type_12>)` | void | `<type_11> <type_12>` | moves all players outside a specified trigger volume to a specified flag. |
| `(volume_test_object <type_11> <object>)` | boolean | `<type_11> <object>` | returns true if the specified object is within the specified volume. |
| `(volume_test_objects <type_11> <object_list>)` | boolean | `<type_11> <object_list>` | returns true if any of the specified objects are within the specified volume. |
| `(volume_test_objects_all <type_11> <object_list>)` | boolean | `<type_11> <object_list>` | returns true if any of the specified objects are within the specified volume. |

### Player

| Command | Returns | Usage | Help |
|---------|---------|-------|------|
| `(player0_joystick_set_is_normal)` | boolean | `` | returns TRUE if player0 is using the normal joystick set |
| `(player0_look_invert_pitch <boolean>)` | void | `<boolean>` | invert player0's look |
| `(player0_look_pitch_is_inverted)` | boolean | `` | returns TRUE if player0's look pitch is inverted |
| `(player_action_test_accept)` | boolean | `` | returns true if any player has hit accept since the last call to (player_action_test_reset). |
| `(player_action_test_action)` | boolean | `` | returns true if any player has hit the action key since the last call to (player_action_test_reset). |
| `(player_action_test_back)` | boolean | `` | returns true if any player has hit the back key since the last call to (player_action_test_reset). |
| `(player_action_test_grenade_trigger)` | boolean | `` | returns true if any player has used grenade trigger since the last call to (player_action_test_reset). |
| `(player_action_test_jump)` | boolean | `` | returns true if any player has jumped since the last call to (player_action_test_reset). |
| `(player_action_test_look_relative_all_directions)` | boolean | `` | returns true if any player has looked up, down, left, and right since the last call to (player_action_test_reset). |
| `(player_action_test_look_relative_down)` | boolean | `` | returns true if any player has looked down since the last call to (player_action_test_reset). |
| `(player_action_test_look_relative_left)` | boolean | `` | returns true if any player has looked left since the last call to (player_action_test_reset). |
| `(player_action_test_look_relative_right)` | boolean | `` | returns true if any player has looked right since the last call to (player_action_test_reset). |
| `(player_action_test_look_relative_up)` | boolean | `` | returns true if any player has looked up since the last call to (player_action_test_reset). |
| `(player_action_test_move_relative_all_directions)` | boolean | `` | returns true if any player has moved forward, backward, left, and right since the last call to (player_action_test_reset). |
| `(player_action_test_primary_trigger)` | boolean | `` | returns true if any player has used primary trigger since the last call to (player_action_test_reset). |
| `(player_action_test_reset)` | void | `` | resets the player action test state so that all tests will return false. |
| `(player_action_test_zoom)` | boolean | `` | returns true if any player has hit the zoom button since the last call to (player_action_test_reset). |
| `(player_add_equipment <unit> <type_19> <boolean>)` | void | `<unit> <type_19> <boolean>` | adds/resets the player's health, shield, and inventory (weapons and grenades) to the named profile. resets if third parameter is true, adds if false. |
| `(player_camera_control <boolean>)` | boolean | `<boolean>` | enables/disables camera control globally |
| `(player_effect_set_max_rotation <real> <real> <real>)` | void | `<real> <real> <real>` | <yaw> <pitch> <roll> |
| `(player_effect_set_max_rumble <real> <real>)` | void | `<real> <real>` | <left> <right> |
| `(player_effect_set_max_translation <real> <real> <real>)` | void | `<real> <real> <real>` | <x> <y> <z> |
| `(player_effect_start <real> <real>)` | void | `<real> <real>` | <max_intensity> <attack time> |
| `(player_effect_stop <real>)` | void | `<real>` | <decay> |
| `(player_enable_input <boolean>)` | void | `<boolean>` | toggle player input. the player can still free-look, but nothing else. |
| `(players)` | object_list | `` | returns a list of the players |
| `(players_unzoom_all)` | void | `` | resets zoom levels on all players |

### Profiling

| Command | Returns | Usage | Help |
|---------|---------|-------|------|
| `(profile_activate <type_9>)` | void | `<type_9>` | activates profile sections based on a substring. |
| `(profile_deactivate <type_9>)` | void | `<type_9>` | deactivates profile sections based on a substring. |
| `(profile_dump <type_9>)` | void | `<type_9>` | dumps profile based on a substring. |
| `(profile_graph_toggle <type_9>)` | void | `<type_9>` | enables or disables profile graph display of a particular value. |
| `(profile_reset)` | void | `` | resets profiling data. |
| `(profile_unlock_solo_levels)` | void | `` | unlocks all the solo player levels for player 1's profile |

### Renderer

| Command | Returns | Usage | Help |
|---------|---------|-------|------|
| `(radiosity_debug_point)` | void | `` | tests sun occlusion at a point. |
| `(radiosity_save)` | void | `` | saves radiosity solution. |
| `(radiosity_start)` | void | `` | starts radiosity computation. |
| `(rasterizer_decals_flush)` | void | `` | flush all decals |
| `(rasterizer_fps_accumulate)` | void | `` | average fps |
| `(rasterizer_lights_reset_for_new_map)` | void | `` |  |
| `(rasterizer_model_ambient_reflection_tint <real> <real> <real> <real>)` | void | `<real> <real> <real> <real>` |  |
| `(render_effects <boolean>)` | void | `<boolean>` |  |
| `(render_lights <boolean>)` | boolean | `<boolean>` | enables/disables dynamic lights |
| `(texture_cache_flush)` | void | `` | don't make me kick your ass |

### Screen effects

| Command | Returns | Usage | Help |
|---------|---------|-------|------|
| `(script_screen_effect_set_value <short> <real>)` | void | `<short> <real>` | sets a screen effect script value |

### Sound

| Command | Returns | Usage | Help |
|---------|---------|-------|------|
| `(debug_sounds_distances <type_9> <real> <real>)` | void | `<type_9> <real> <real>` | changes the minimum and maximum distances for all sound classes matching the substring. |
| `(debug_sounds_enable <type_9> <boolean>)` | void | `<type_9> <boolean>` | enables or disabled all sound classes matching the substring. |
| `(debug_sounds_wet <type_9> <real>)` | void | `<type_9> <real>` | changes the reverb level for all sound classes matching the substring. |
| `(sound_cache_flush)` | void | `` | i'm a rebel! |
| `(sound_class_set_gain <type_9> <real> <short>)` | void | `<type_9> <real> <short>` | changes the gain on the specified sound class(es) to the specified game over the specified number of ticks. |
| `(sound_enable <boolean>)` | void | `<boolean>` | enables or disables all sound. |
| `(sound_get_gain <type_9>)` | real | `<type_9>` | absolutely do not use this either |
| `(sound_impulse_start <type_24> <object> <real>)` | void | `<type_24> <object> <real>` | plays an impulse sound from the specified source object (or "none"), with the specified scale. |
| `(sound_impulse_stop <type_24>)` | void | `<type_24>` | stops the specified impulse sound. |
| `(sound_impulse_time <type_24>)` | long | `<type_24>` | returns the time remaining for the specified impulse sound. |
| `(sound_looping_predict <type_27>)` | void | `<type_27>` | your mom. |
| `(sound_looping_set_alternate <type_27> <boolean>)` | void | `<type_27> <boolean>` | enables or disables the alternate loop/alternate end for a looping sound. |
| `(sound_looping_set_scale <type_27> <real>)` | void | `<type_27> <real>` | changes the scale of the sound (which should affect the volume) within the range 0 to 1. |
| `(sound_looping_start <type_27> <object> <real>)` | void | `<type_27> <object> <real>` | plays a looping sound from the specified source object (or "none"), with the specified scale. |
| `(sound_looping_stop <type_27>)` | void | `<type_27>` | stops the specified looping sound. |
| `(sound_set_gain <type_9> <real>)` | void | `<type_9> <real>` | absolutely do not use this |

### Units

| Command | Returns | Usage | Help |
|---------|---------|-------|------|
| `(custom_animation <unit> <type_28> <type_9> <boolean>)` | boolean | `<unit> <type_28> <type_9> <boolean>` | starts a custom animation playing on a unit (interpolates into animation if last parameter is TRUE) |
| `(custom_animation_list <object_list> <type_28> <type_9> <boolean>)` | boolean | `<object_list> <type_28> <type_9> <boolean>` | starts a custom animation playing on a unit list (interpolates into animation if last parameter is TRUE) |
| `(magic_melee_attack)` | void | `` | causes player's unit to start a melee attack |
| `(magic_seat_name <type_9>)` | void | `<type_9>` | all units controlled by the player will assume the given seat name (valid values are 'asleep', 'alert', 'stand', 'crouch' and 'flee') |
| `(unit_aim_without_turning <unit> <boolean>)` | void | `<unit> <boolean>` | allows a unit to aim in place without turning |
| `(unit_can_blink <unit> <boolean>)` | void | `<unit> <boolean>` | allows a unit to blink or not (units never blink when they are dead) |
| `(unit_close <unit>)` | void | `<unit>` | closes the hatches on a given unit |
| `(unit_custom_animation_at_frame <unit> <type_28> <type_9> <boolean> <short>)` | boolean | `<unit> <type_28> <type_9> <boolean> <short>` | starts a custom animation playing on a unit at a specific frame index(interpolates into animation if next to last parameter is TRUE) |
| `(unit_doesnt_drop_items <object_list>)` | void | `<object_list>` | prevents any of the given units from dropping weapons or grenades when they die |
| `(unit_enter_vehicle <unit> <type_39> <type_9>)` | void | `<unit> <type_39> <type_9>` | puts the specified unit in the specified vehicle (in the named seat) |
| `(unit_exit_vehicle <unit>)` | void | `<unit>` | makes a unit exit its vehicle |
| `(unit_get_current_flashlight_state <unit>)` | boolean | `<unit>` | gets the unit's current flashlight state |
| `(unit_get_custom_animation_time <unit>)` | short | `<unit>` | returns the number of ticks remaining in a unit's custom animation (or zero, if the animation is over). |
| `(unit_get_health <unit>)` | real | `<unit>` | returns the health [0,1] of the unit, returns -1 if the unit does not exists |
| `(unit_get_shield <unit>)` | real | `<unit>` | returns the shield [0,1] of the unit, returns -1 if the unit does not exists |
| `(unit_get_total_grenade_count <unit>)` | short | `<unit>` | returns the total number of grenades for the given unit, 0 if it does not exist |
| `(unit_has_weapon <unit> <type_31>)` | boolean | `<unit> <type_31>` | returns TRUE if the <unit> has <object> as a weapon, FALSE otherwise |
| `(unit_has_weapon_readied <unit> <type_31>)` | boolean | `<unit> <type_31>` | returns TRUE if the <unit> has <object> as the primary weapon, FALSE otherwise |
| `(unit_impervious <object_list> <boolean>)` | void | `<object_list> <boolean>` | prevents any of the given units from being knocked around or playing ping animations |
| `(unit_is_playing_custom_animation <unit>)` | boolean | `<unit>` | returns TRUE if the given unit is still playing a custom animation |
| `(unit_kill <unit>)` | void | `<unit>` | kills a given unit, no saving throw |
| `(unit_kill_silent <unit>)` | void | `<unit>` | kills a given unit silently (doesn't make them play their normal death animation or sound) |
| `(unit_open <unit>)` | void | `<unit>` | opens the hatches on the given unit |
| `(unit_set_current_vitality <unit> <real> <real>)` | void | `<unit> <real> <real>` | sets a unit's current body and shield vitality |
| `(unit_set_desired_flashlight_state <unit> <boolean>)` | void | `<unit> <boolean>` | sets the unit's desired flashlight state |
| `(unit_set_emotion <unit> <short>)` | void | `<unit> <short>` | sets a unit's facial expression (-1 is none, other values depend on unit) |
| `(unit_set_emotion_animation <unit> <type_9>)` | void | `<unit> <type_9>` | sets the emotion animation to be used for the given unit |
| `(unit_set_enterable_by_player <unit> <boolean>)` | void | `<unit> <boolean>` | can be used to prevent the player from entering a vehicle |
| `(unit_set_maximum_vitality <unit> <real> <real>)` | void | `<unit> <real> <real>` | sets a unit's maximum body and shield vitality |
| `(unit_set_seat <unit> <type_9>)` | void | `<unit> <type_9>` | this unit will assume the named seat |
| `(unit_solo_player_integrated_night_vision_is_active)` | boolean | `` | returns whether the night-vision mode could be activated via the flashlight button |
| `(unit_stop_custom_animation <unit>)` | void | `<unit>` | stops the custom animation running on the given unit. |
| `(unit_suspended <unit> <boolean>)` | void | `<unit> <boolean>` | stops gravity from working on the given unit |
| `(units_set_current_vitality <object_list> <real> <real>)` | void | `<object_list> <real> <real>` | sets a group of units' current body and shield vitality |
| `(units_set_desired_flashlight_state <object_list> <boolean>)` | void | `<object_list> <boolean>` | sets the units' desired flashlight state |
| `(units_set_maximum_vitality <object_list> <real> <real>)` | void | `<object_list> <real> <real>` | sets a group of units' maximum body and shield vitality |
| `(vehicle_driver <unit>)` | unit | `<unit>` | returns the driver of a vehicle |
| `(vehicle_gunner <unit>)` | unit | `<unit>` | returns the gunner of a vehicle |
| `(vehicle_hover <type_39> <boolean>)` | void | `<type_39> <boolean>` | stops the vehicle from running real physics and runs fake hovering physics instead. |
| `(vehicle_load_magic <unit> <type_9> <object_list>)` | short | `<unit> <type_9> <object_list>` | makes a list of units (named or by encounter) magically get into a vehicle, in the substring-specified seats (e.g. CD-passenger... empty string matches all seats) |
| `(vehicle_riders <unit>)` | object_list | `<unit>` | returns a list of all riders in a vehicle |
| `(vehicle_test_seat <type_39> <type_9> <unit>)` | boolean | `<type_39> <type_9> <unit>` | tests whether the named seat has a specified unit in it |
| `(vehicle_test_seat_list <type_39> <type_9> <object_list>)` | boolean | `<type_39> <type_9> <object_list>` | tests whether the named seat has an object in the object list |
| `(vehicle_unload <unit> <type_9>)` | short | `<unit> <type_9>` | makes units get out of a vehicle from the substring-specified seats (e.g. CD-passenger... empty string matches all seats) |

## Coverage vs Reclaimers external-globals extract

- Xbox binary: **443** entries (**442** unique names; 1 duplicate)
- Reclaimers extract: **499** entries
- In binary, missing from extract (16): `decals`, `display_vblank_deltas`, `player0_look_pitch_rate`, `player0_look_yaw_rate`, `player1_look_pitch_rate`, `player1_look_yaw_rate`, `player2_look_pitch_rate`, `player2_look_yaw_rate`, `player3_look_pitch_rate`, `player3_look_yaw_rate`, `radiosity_lines`, `radiosity_normals`, `radiosity_quality`, `radiosity_step_count`, `rasterizer_pushbuffer_kickoff_size`, `rasterizer_pushbuffer_size`
- In extract, not in this Xbox build (73) — PC/H1A/server-only names, not usable here: `allow_client_side_weapon_projectiles`, `biped_incremental_rate`, `breadcrumbs_navpoints_enabled_override`, `cl_remote_player_action_queue_limit`, `cl_remote_player_action_queue_tick_limit`, `client_log_destination`, `debug_objects_biped_messages`, `debug_objects_equipment_messages`, `debug_objects_projectile_messages`, `debug_objects_vehicle_messages`, `debug_objects_weapon_messages`, `debug_score`, `debug_sound_cache_graph`, `debug_sound_channels_detail`, `debug_sound_hardware`, `debug_structure_automatic`, `developer_mode`, `director_camera_switching`, `disconnect`, `equipment_incremental_rate`, `error_suppress_all`, `game_paused`, `game_speed_value`, `hud_filter`, `leaf_to_leaf_latency`, `local_player_log_level`, `local_player_update_rate`, `local_player_vehicle_update_rate`, `log_server_player_update_history`, `mouse_acceleration`, `multiplayer_draw_teammates_names`, `multiplayer_hit_sound_volume`, `net_bandwidth`, `net_graph_enabled`, `net_graph_period`, `network_connect_timeout`, `object_prediction`, `oddball_baseline_rate`, `projectile_incremental_rate`, `rasterizer_d3dlight_attenuation0`, `rasterizer_d3dlight_attenuation1`, `rasterizer_d3dlight_attenuation2`, `rasterizer_d3dlight_falloff`, `rasterizer_d3dlight_phi`, `rasterizer_d3dlight_theta`, `rasterizer_debug_shader_transparent_generic`, `rasterizer_effects_level`, `rasterizer_fps`, `rasterizer_frame_drop_ms`, `remote_player_action_baseline_update_rate`, `remote_player_action_update_rate`, `remote_player_log_level`, `remote_player_position_baseline_update_rate`, `remote_player_position_update_rate`, `remote_player_vehicle_baseline_update_rate`, `remote_player_vehicle_update_rate`, `slow_server_startup_safety_zone_in_seconds`, `sound_cache_dump_to_file`, `sound_cache_size`, `sound_obstruction_ratio`, `speed_hack_detection`, `speed_hack_log_level`, `sv_client_action_queue_limit`, `sv_client_action_queue_tick_limit`, `sv_mapcycle_timeout`, `sv_public`, `sv_tk_ban`, `texture_cache_flush`, `transport_dumping`, `use_new_vehicle_update_scheme`, `use_super_remote_players_action_update`, `vehicle_incremental_rate`, `weapon_incremental_rate`

## Related docs

- `docs/debug-commands-keyboard.md` — keyboard shortcuts,
  cheats.txt, console evaluate internals
- `docs/references/h1/scripting-reference.md` — Reclaimers HSC
  reference (PC/H1A; includes names absent from this binary)
- In-game: `(script_doc)` → `hs_doc.txt`, `(help <name>)`

