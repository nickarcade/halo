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
as zero).

### AI

| Command | Type | Default |
|---------|------|---------|
| `ai_debug_ballistic_lineoffire_freeze` | boolean | `—` |
| `ai_debug_blind` | boolean | `—` |
| `ai_debug_communication_focus_enable` | boolean | `—` |
| `ai_debug_communication_random_disabled` | boolean | `—` |
| `ai_debug_communication_timeout_disabled` | boolean | `—` |
| `ai_debug_communication_unit_repeat_disabled` | boolean | `—` |
| `ai_debug_deaf` | boolean | `—` |
| `ai_debug_disable_wounded_sounds` | boolean | `—` |
| `ai_debug_evaluate_all_positions` | boolean | `—` |
| `ai_debug_fast_los` | boolean | `—` |
| `ai_debug_flee_always` | boolean | `—` |
| `ai_debug_force_all_active` | boolean | `—` |
| `ai_debug_force_crouch` | boolean | `—` |
| `ai_debug_force_vocalizations` | boolean | `—` |
| `ai_debug_ignore_player` | boolean | `—` |
| `ai_debug_invisible_player` | boolean | `—` |
| `ai_debug_oversteer_disable` | boolean | `—` |
| `ai_debug_path` | boolean | `—` |
| `ai_debug_path_accept_radius` | real | `—` |
| `ai_debug_path_attractor` | boolean | `—` |
| `ai_debug_path_attractor_radius` | real | `—` |
| `ai_debug_path_attractor_weight` | real | `—` |
| `ai_debug_path_disable_obstacle_avoidance` | boolean | `—` |
| `ai_debug_path_disable_smoothing` | boolean | `—` |
| `ai_debug_path_end_freeze` | boolean | `—` |
| `ai_debug_path_flood` | boolean | `—` |
| `ai_debug_path_maximum_radius` | real | `—` |
| `ai_debug_path_start_freeze` | boolean | `—` |
| `ai_fix_actor_variants` | boolean | `—` |
| `ai_fix_defending_guard_firing_positions` | boolean | `—` |
| `ai_print_acknowledgement` | boolean | `—` |
| `ai_print_allegiance` | boolean | `—` |
| `ai_print_automatic_migration` | boolean | `—` |
| `ai_print_bsp_transition` | boolean | `—` |
| `ai_print_command_lists` | boolean | `—` |
| `ai_print_communication` | boolean | `—` |
| `ai_print_communication_player` | boolean | `—` |
| `ai_print_conversations` | boolean | `—` |
| `ai_print_damage_modifiers` | boolean | `—` |
| `ai_print_evaluation_statistics` | boolean | `—` |
| `ai_print_killing_sprees` | boolean | `—` |
| `ai_print_lost_speech` | boolean | `—` |
| `ai_print_major_upgrade` | boolean | `—` |
| `ai_print_migration` | boolean | `—` |
| `ai_print_oversteer` | boolean | `—` |
| `ai_print_placement` | boolean | `—` |
| `ai_print_pursuit_checks` | boolean | `—` |
| `ai_print_respawn` | boolean | `—` |
| `ai_print_rule_values` | boolean | `—` |
| `ai_print_rules` | boolean | `—` |
| `ai_print_scripting` | boolean | `—` |
| `ai_print_secondary_looking` | boolean | `—` |
| `ai_print_speech` | boolean | `—` |
| `ai_print_speech_timers` | boolean | `—` |
| `ai_print_surprise` | boolean | `—` |
| `ai_print_uncovering` | boolean | `—` |
| `ai_print_unfinished_paths` | boolean | `—` |
| `ai_print_vocalizations` | boolean | `—` |
| `ai_profile_disable` | boolean | `—` |
| `ai_profile_random` | boolean | `—` |
| `ai_render` | boolean | `—` |
| `ai_render_activation` | boolean | `—` |
| `ai_render_active_cover_seeking` | boolean | `—` |
| `ai_render_aiming_validity` | boolean | `—` |
| `ai_render_aiming_vectors` | boolean | `—` |
| `ai_render_all_actors` | boolean | `—` |
| `ai_render_audibility` | boolean | `—` |
| `ai_render_ballistic_lineoffire` | boolean | `—` |
| `ai_render_burst_geometry` | boolean | `—` |
| `ai_render_charge_decisions` | boolean | `—` |
| `ai_render_control` | boolean | `—` |
| `ai_render_current_state` | boolean | `—` |
| `ai_render_danger_zones` | boolean | `—` |
| `ai_render_detailed_state` | boolean | `—` |
| `ai_render_dialogue_variants` | boolean | `—` |
| `ai_render_emotions` | boolean | `—` |
| `ai_render_encounter_activeregion` | boolean | `—` |
| `ai_render_evaluations` | boolean | `—` |
| `ai_render_firing_positions` | boolean | `—` |
| `ai_render_grenade_decisions` | boolean | `—` |
| `ai_render_gun_positions` | boolean | `—` |
| `ai_render_idle_look` | boolean | `—` |
| `ai_render_inactive_actors` | boolean | `—` |
| `ai_render_lineoffire` | boolean | `—` |
| `ai_render_lineoffire_crouching` | boolean | `—` |
| `ai_render_lineofsight` | boolean | `—` |
| `ai_render_melee_check` | boolean | `—` |
| `ai_render_paths` | boolean | `—` |
| `ai_render_paths_avoidance_obstacles` | boolean | `—` |
| `ai_render_paths_avoidance_search` | boolean | `—` |
| `ai_render_paths_avoidance_segment` | short | `—` |
| `ai_render_paths_avoided` | boolean | `—` |
| `ai_render_paths_current` | boolean | `—` |
| `ai_render_paths_destination` | boolean | `—` |
| `ai_render_paths_failed` | boolean | `—` |
| `ai_render_paths_nodes` | boolean | `—` |
| `ai_render_paths_nodes_all` | boolean | `—` |
| `ai_render_paths_nodes_closest` | boolean | `—` |
| `ai_render_paths_nodes_costs` | boolean | `—` |
| `ai_render_paths_nodes_polygons` | boolean | `—` |
| `ai_render_paths_raw` | boolean | `—` |
| `ai_render_paths_selected_only` | boolean | `—` |
| `ai_render_paths_smoothed` | boolean | `—` |
| `ai_render_player_aiming_blocked` | boolean | `—` |
| `ai_render_player_ratings` | boolean | `—` |
| `ai_render_postcombat` | boolean | `—` |
| `ai_render_projectile_aiming` | boolean | `—` |
| `ai_render_props` | boolean | `—` |
| `ai_render_props_no_friends` | boolean | `—` |
| `ai_render_props_target_weight` | boolean | `—` |
| `ai_render_props_unopposable` | boolean | `—` |
| `ai_render_props_unreachable` | boolean | `—` |
| `ai_render_props_web` | boolean | `—` |
| `ai_render_pursuit` | boolean | `—` |
| `ai_render_recent_damage` | boolean | `—` |
| `ai_render_secondary_looking` | boolean | `—` |
| `ai_render_shooting` | boolean | `—` |
| `ai_render_spatial_effects` | boolean | `—` |
| `ai_render_speech` | boolean | `—` |
| `ai_render_states` | boolean | `—` |
| `ai_render_support_surfaces` | boolean | `—` |
| `ai_render_targets` | boolean | `—` |
| `ai_render_targets_last_visible` | boolean | `—` |
| `ai_render_teams` | boolean | `—` |
| `ai_render_threats` | boolean | `—` |
| `ai_render_trigger` | boolean | `—` |
| `ai_render_vector_avoidance` | boolean | `—` |
| `ai_render_vector_avoidance_avoid_t` | boolean | `—` |
| `ai_render_vector_avoidance_clear_time` | boolean | `—` |
| `ai_render_vector_avoidance_intermediate` | boolean | `—` |
| `ai_render_vector_avoidance_objects` | boolean | `—` |
| `ai_render_vector_avoidance_rays` | boolean | `—` |
| `ai_render_vector_avoidance_sense_t` | boolean | `—` |
| `ai_render_vector_avoidance_weights` | boolean | `—` |
| `ai_render_vehicle_avoidance` | boolean | `—` |
| `ai_render_vehicles_enterable` | boolean | `—` |
| `ai_render_vision_cones` | boolean | `—` |
| `ai_render_vitality` | boolean | `—` |
| `ai_show` | boolean | `—` |
| `ai_show_actors` | boolean | `—` |
| `ai_show_line_of_sight` | boolean | `—` |
| `ai_show_paths` | boolean | `—` |
| `ai_show_prop_types` | boolean | `—` |
| `ai_show_sound_distance` | boolean | `—` |
| `ai_show_stats` | boolean | `—` |
| `ai_show_swarms` | boolean | `—` |

### Animation

| Command | Type | Default |
|---------|------|---------|
| `debug_bink` | boolean | `—` |
| `debug_recording` | boolean | `—` |
| `debug_recording_newlines` | short | `10` |
| `model_animation_bullshit0` | long | `—` |
| `model_animation_bullshit1` | long | `—` |
| `model_animation_bullshit2` | long | `—` |
| `model_animation_bullshit3` | long | `—` |
| `model_animation_compression` | boolean | `true` |
| `model_animation_data_compressed_size` | long | `—` |
| `model_animation_data_compression_savings_in_bytes` | long | `—` |
| `model_animation_data_compression_savings_in_bytes_at_import` | long | `—` |
| `model_animation_data_compression_savings_in_percent` | real | `—` |
| `model_animation_data_uncompressed_size` | long | `—` |

### Camera

| Command | Type | Default |
|---------|------|---------|
| `debug_camera` | boolean | `—` |
| `director_camera_switch_fast` | boolean | `—` |
| `force_all_player_views_to_default_player` | boolean | `false` |
| `freeze_flying_camera` | short | `0` |

### Cheats

| Command | Type | Default |
|---------|------|---------|
| `cheat_bottomless_clip` | boolean | `—` |
| `cheat_bump_possession` | boolean | `—` |
| `cheat_controller` | boolean | `—` |
| `cheat_deathless_player` | boolean | `—` |
| `cheat_infinite_ammo` | boolean | `—` |
| `cheat_jetpack` | boolean | `—` |
| `cheat_medusa` | boolean | `—` |
| `cheat_omnipotent` | boolean | `—` |
| `cheat_reflexive_damage_effects` | boolean | `—` |
| `cheat_super_jump` | boolean | `—` |

### Collision

| Command | Type | Default |
|---------|------|---------|
| `collision_debug` | boolean | `—` |
| `collision_debug_features` | boolean | `—` |
| `collision_debug_flag_back_facing_surfaces` | boolean | `—` |
| `collision_debug_flag_front_facing_surfaces` | boolean | `true` |
| `collision_debug_flag_ignore_breakable_surfaces` | boolean | `—` |
| `collision_debug_flag_ignore_invisible_surfaces` | boolean | `true` |
| `collision_debug_flag_ignore_two_sided_surfaces` | boolean | `—` |
| `collision_debug_flag_media` | boolean | `true` |
| `collision_debug_flag_objects` | boolean | `true` |
| `collision_debug_flag_objects_bipeds` | boolean | `—` |
| `collision_debug_flag_objects_controls` | boolean | `—` |
| `collision_debug_flag_objects_equipment` | boolean | `—` |
| `collision_debug_flag_objects_light_fixtures` | boolean | `—` |
| `collision_debug_flag_objects_machines` | boolean | `—` |
| `collision_debug_flag_objects_placeholders` | boolean | `—` |
| `collision_debug_flag_objects_projectiles` | boolean | `—` |
| `collision_debug_flag_objects_scenery` | boolean | `—` |
| `collision_debug_flag_objects_vehicles` | boolean | `—` |
| `collision_debug_flag_objects_weapons` | boolean | `—` |
| `collision_debug_flag_skip_passthrough_bipeds` | boolean | `—` |
| `collision_debug_flag_structure` | boolean | `true` |
| `collision_debug_flag_try_to_keep_location_valid` | boolean | `—` |
| `collision_debug_flag_use_vehicle_physics` | boolean | `—` |
| `collision_debug_height` | real | `—` |
| `collision_debug_length` | real | `100.0` |
| `collision_debug_phantom_bsp` | boolean | `—` |
| `collision_debug_point_x` | real | `—` |
| `collision_debug_point_y` | real | `—` |
| `collision_debug_point_z` | real | `—` |
| `collision_debug_repeat` | boolean | `—` |
| `collision_debug_spray` | boolean | `—` |
| `collision_debug_vector_i` | real | `—` |
| `collision_debug_vector_j` | real | `—` |
| `collision_debug_vector_k` | real | `—` |
| `collision_debug_width` | real | `—` |
| `collision_log_detailed` | boolean | `—` |
| `collision_log_extended` | boolean | `—` |
| `collision_log_render` | boolean | `—` |
| `collision_log_time` | boolean | `—` |
| `collision_log_totals_only` | boolean | `—` |
| `debug_collision_skip_objects` | boolean | `—` |
| `debug_collision_skip_vectors` | boolean | `—` |

### Console

| Command | Type | Default |
|---------|------|---------|
| `console_dump_to_file` | boolean | `—` |
| `terminal_render` | boolean | `true` |

### Effects

| Command | Type | Default |
|---------|------|---------|
| `debug_damage` | boolean | `—` |
| `debug_damage_taken` | boolean | `—` |
| `debug_effects_nonviolent` | boolean | `—` |
| `debug_material_effects` | boolean | `—` |
| `decals` | boolean | `true` |
| `decals` | boolean | `true` |
| `effects_corpse_nonviolent` | boolean | `true` |
| `weather` | boolean | `true` |

### Game

| Command | Type | Default |
|---------|------|---------|
| `debug_game_save` | boolean | `—` |
| `debug_scripting` | boolean | `—` |
| `recover_saved_games_hack` | boolean | `—` |
| `run_game_scripts` | boolean | `—` |

### HUD/UI

| Command | Type | Default |
|---------|------|---------|
| `temporary_hud` | boolean | `—` |

### Input

| Command | Type | Default |
|---------|------|---------|
| `controls_enable_crouch` | boolean | `—` |
| `controls_enable_doubled_spin` | boolean | `—` |
| `controls_swap_doubled_spin_state` | boolean | `—` |
| `controls_swapped` | boolean | `true` |
| `debug_input` | boolean | `—` |
| `debug_input_target` | short | `—` |
| `pad3` | short | `0` |
| `pad3_scale` | real | `1.0` |

### Lighting

| Command | Type | Default |
|---------|------|---------|
| `object_light_ambient_base` | real | `0.029999999329447746` |
| `object_light_ambient_scale` | real | `0.4000000059604645` |
| `object_light_interpolate` | boolean | `true` |
| `object_light_secondary_scale` | real | `1.0` |

### Misc

| Command | Type | Default |
|---------|------|---------|
| `breakable_surfaces` | boolean | `true` |
| `debug_motion_sensor_draw_all_units` | boolean | `—` |
| `f0` | real | `0.0` |
| `f1` | real | `0.0` |
| `f2` | real | `0.0` |
| `f3` | real | `0.0` |
| `f4` | real | `0.0` |
| `f5` | real | `0.0` |
| `find_all_fucked_up_shit` | boolean | `—` |
| `rider_ejection` | boolean | `true` |
| `stun_enable` | boolean | `—` |

### Network

| Command | Type | Default |
|---------|------|---------|
| `allow_out_of_sync` | boolean | `—` |
| `global_connection_dont_timeout` | boolean | `—` |

### Objects

| Command | Type | Default |
|---------|------|---------|
| `debug_inactive_objects` | boolean | `—` |
| `debug_object_garbage_collection` | boolean | `—` |
| `debug_object_lights` | boolean | `—` |
| `debug_objects` | boolean | `—` |
| `debug_objects_biped_autoaim_pills` | boolean | `—` |
| `debug_objects_biped_physics_pills` | boolean | `—` |
| `debug_objects_bounding_spheres` | boolean | `true` |
| `debug_objects_collision_models` | boolean | `true` |
| `debug_objects_devices` | boolean | `—` |
| `debug_objects_names` | boolean | `—` |
| `debug_objects_pathfinding_spheres` | boolean | `—` |
| `debug_objects_physics` | boolean | `—` |
| `debug_objects_position_velocity` | boolean | `—` |
| `debug_objects_root_node` | boolean | `—` |
| `debug_objects_unit_mouth_apeture` | boolean | `—` |
| `debug_objects_unit_seats` | boolean | `—` |
| `debug_objects_unit_vectors` | boolean | `—` |
| `debug_objects_vehicle_powered_mass_points` | boolean | `—` |

### Pathfinding

| Command | Type | Default |
|---------|------|---------|
| `debug_obstacle_path` | boolean | `—` |
| `debug_obstacle_path_goal_point_x` | real | `—` |
| `debug_obstacle_path_goal_point_y` | real | `—` |
| `debug_obstacle_path_goal_surface_index` | long | `—` |
| `debug_obstacle_path_on_failure` | boolean | `—` |
| `debug_obstacle_path_start_point_x` | real | `—` |
| `debug_obstacle_path_start_point_y` | real | `—` |
| `debug_obstacle_path_start_surface_index` | long | `—` |

### Physics

| Command | Type | Default |
|---------|------|---------|
| `debug_physics_disable_penetration_freeze` | boolean | `—` |
| `debug_point_physics` | boolean | `—` |

### Player

| Command | Type | Default |
|---------|------|---------|
| `debug_player` | boolean | `—` |
| `debug_player_color` | short | `-1` |
| `debug_player_teleport` | boolean | `—` |
| `player0_look_pitch_rate` | real | `—` |
| `player0_look_yaw_rate` | real | `—` |
| `player1_look_pitch_rate` | real | `—` |
| `player1_look_yaw_rate` | real | `—` |
| `player2_look_pitch_rate` | real | `—` |
| `player2_look_yaw_rate` | real | `—` |
| `player3_look_pitch_rate` | real | `—` |
| `player3_look_yaw_rate` | real | `—` |
| `player_autoaim` | boolean | `true` |
| `player_magnetism` | boolean | `true` |
| `player_spawn_count` | short | `1` |

### Profiling

| Command | Type | Default |
|---------|------|---------|
| `profile_display` | boolean | `—` |
| `profile_dump_frames` | boolean | `—` |
| `profile_dump_lost_frames` | boolean | `—` |
| `profile_graph` | boolean | `—` |
| `profile_timebase_ticks` | boolean | `—` |

### Renderer

| Command | Type | Default |
|---------|------|---------|
| `debug_decals` | boolean | `—` |
| `debug_detail_objects` | boolean | `—` |
| `debug_fog_planes` | boolean | `—` |
| `debug_framerate` | boolean | `—` |
| `debug_frustum` | boolean | `—` |
| `debug_lights` | boolean | `—` |
| `debug_no_drawing` | boolean | `—` |
| `debug_no_frustum_clip` | boolean | `—` |
| `debug_permanent_decals` | boolean | `—` |
| `debug_render_freeze` | boolean | `—` |
| `debug_sprites` | boolean | `—` |
| `debug_texture_cache` | boolean | `—` |
| `display_framerate` | boolean | `—` |
| `display_precache_progress` | boolean | `—` |
| `display_vblank_deltas` | boolean | `—` |
| `framerate_lock` | boolean | `—` |
| `framerate_throttle` | boolean | `true` |
| `radiosity_lines` | boolean | `—` |
| `radiosity_normals` | boolean | `—` |
| `radiosity_quality` | short | `—` |
| `radiosity_step_count` | short | `—` |
| `rasterizer_DXTC_noise` | boolean | `false` |
| `rasterizer_active_camouflage` | boolean | `true` |
| `rasterizer_active_camouflage_multipass` | boolean | `true` |
| `rasterizer_bump_mapping` | boolean | `true` |
| `rasterizer_debug_geometry` | boolean | `true` |
| `rasterizer_debug_geometry_multipass` | boolean | `false` |
| `rasterizer_debug_meter_shader` | boolean | `false` |
| `rasterizer_debug_model_lod` | short | `-1` |
| `rasterizer_debug_model_vertices` | boolean | `false` |
| `rasterizer_debug_transparents` | boolean | `false` |
| `rasterizer_detail_objects` | boolean | `true` |
| `rasterizer_detail_objects_offset_multiplier` | real | `0.4000000059604645` |
| `rasterizer_draw_first_person_weapon_first` | boolean | `true` |
| `rasterizer_dynamic_lit_geometry` | boolean | `true` |
| `rasterizer_dynamic_screen_geometry` | boolean | `true` |
| `rasterizer_dynamic_unlit_geometry` | boolean | `true` |
| `rasterizer_environment` | boolean | `true` |
| `rasterizer_environment_alpha_testing` | boolean | `true` |
| `rasterizer_environment_decals` | boolean | `true` |
| `rasterizer_environment_diffuse_lights` | boolean | `true` |
| `rasterizer_environment_diffuse_textures` | boolean | `true` |
| `rasterizer_environment_fog` | boolean | `true` |
| `rasterizer_environment_fog_screen` | boolean | `true` |
| `rasterizer_environment_lightmaps` | boolean | `true` |
| `rasterizer_environment_reflection_lightmap_mask` | boolean | `true` |
| `rasterizer_environment_reflection_mirrors` | boolean | `true` |
| `rasterizer_environment_reflections` | boolean | `true` |
| `rasterizer_environment_shadows` | boolean | `true` |
| `rasterizer_environment_specular_lightmaps` | boolean | `true` |
| `rasterizer_environment_specular_lights` | boolean | `true` |
| `rasterizer_environment_specular_mask` | boolean | `true` |
| `rasterizer_environment_transparents` | boolean | `true` |
| `rasterizer_far_clip_distance` | real | `1024.0` |
| `rasterizer_filthy_decal_fog_hack` | boolean | `true` |
| `rasterizer_first_person_weapon_far_clip_distance` | real | `1024.0` |
| `rasterizer_first_person_weapon_near_clip_distance` | real | `0.01171875` |
| `rasterizer_floating_point_zbuffer` | boolean | `false` |
| `rasterizer_fog_atmosphere` | boolean | `true` |
| `rasterizer_fog_plane` | boolean | `true` |
| `rasterizer_frame_bounds_bottom` | short | `0` |
| `rasterizer_frame_bounds_left` | short | `0` |
| `rasterizer_frame_bounds_right` | short | `0` |
| `rasterizer_frame_bounds_top` | short | `0` |
| `rasterizer_framerate_stabilization` | boolean | `false` |
| `rasterizer_framerate_throttle` | boolean | `true` |
| `rasterizer_hud_motion_sensor` | boolean | `true` |
| `rasterizer_lens_flares` | boolean | `true` |
| `rasterizer_lens_flares_occlusion` | boolean | `true` |
| `rasterizer_lens_flares_occlusion_debug` | boolean | `false` |
| `rasterizer_lightmap_ambient` | real | `1.0` |
| `rasterizer_lightmap_mode` | short | `0` |
| `rasterizer_lightmaps_filtering` | boolean | `true` |
| `rasterizer_lightmaps_incident_radiosity` | boolean | `true` |
| `rasterizer_mode` | short | `0` |
| `rasterizer_model_lighting_ambient` | real | `0.0` |
| `rasterizer_model_transparents` | boolean | `true` |
| `rasterizer_models` | boolean | `true` |
| `rasterizer_near_clip_distance` | real | `0.0625` |
| `rasterizer_plasma_energy` | boolean | `true` |
| `rasterizer_profile_log` | boolean | `false` |
| `rasterizer_profile_objectlock_time` | real | `0.0` |
| `rasterizer_profile_print_locks` | boolean | `false` |
| `rasterizer_pushbuffer_kickoff_size` | short | `0` |
| `rasterizer_pushbuffer_size` | short | `768` |
| `rasterizer_ray_of_buddha` | boolean | `true` |
| `rasterizer_refresh_rate` | short | `0` |
| `rasterizer_safe_frame_bounds` | boolean | `false` |
| `rasterizer_screen_effects` | boolean | `true` |
| `rasterizer_screen_flashes` | boolean | `true` |
| `rasterizer_secondary_render_target_debug` | boolean | `false` |
| `rasterizer_shadows_convolution` | boolean | `true` |
| `rasterizer_shadows_debug` | boolean | `false` |
| `rasterizer_smart` | boolean | `true` |
| `rasterizer_soft_filter` | boolean | `false` |
| `rasterizer_splitscreen_VB_optimization` | boolean | `false` |
| `rasterizer_stats` | short | `0` |
| `rasterizer_stencil_mask` | boolean | `true` |
| `rasterizer_transparent_pixel_counter` | boolean | `false` |
| `rasterizer_water` | boolean | `true` |
| `rasterizer_water_mipmapping` | boolean | `false` |
| `rasterizer_wireframe` | boolean | `false` |
| `rasterizer_zbias` | long | `8` |
| `rasterizer_zoffset` | real | `0.00390625` |
| `rasterizer_zsprites` | boolean | `true` |
| `render_contrails` | boolean | `true` |
| `render_model_index_counts` | boolean | `—` |
| `render_model_markers` | boolean | `—` |
| `render_model_no_geometry` | boolean | `—` |
| `render_model_nodes` | boolean | `—` |
| `render_model_vertex_counts` | boolean | `—` |
| `render_particles` | boolean | `true` |
| `render_psystems` | boolean | `true` |
| `render_shadows` | boolean | `true` |
| `render_wsystems` | boolean | `true` |
| `screenshot_count` | short | `—` |
| `screenshot_size` | short | `1` |
| `texture_cache_graph` | boolean | `—` |
| `texture_cache_list` | boolean | `—` |

### Sound

| Command | Type | Default |
|---------|------|---------|
| `debug_looping_sound` | boolean | `—` |
| `debug_sound` | boolean | `—` |
| `debug_sound_cache` | boolean | `—` |
| `debug_sound_channels` | boolean | `—` |
| `debug_sound_environment` | boolean | `—` |
| `loud_dialog_hack` | boolean | `—` |
| `sound_gain_under_dialog` | real | `0.699999988079071` |

### Structures

| Command | Type | Default |
|---------|------|---------|
| `debug_bsp` | boolean | `—` |
| `debug_leaf_index` | long | `-1` |
| `debug_leaf_portal_index` | long | `-1` |
| `debug_leaf_portals` | boolean | `—` |
| `debug_portals` | boolean | `—` |
| `debug_structure` | boolean | `—` |
| `debug_trigger_volumes` | boolean | `—` |
| `structures_use_pvs_for_vs` | boolean | `—` |

### Units

| Command | Type | Default |
|---------|------|---------|
| `debug_biped_limp_body_disable` | boolean | `—` |
| `debug_biped_physics` | boolean | `—` |
| `debug_biped_skip_collision` | boolean | `—` |
| `debug_biped_skip_update` | boolean | `—` |
| `debug_unit_all_animations` | boolean | `—` |
| `debug_unit_animations` | boolean | `—` |
| `debug_unit_illumination` | boolean | `—` |

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

## Related docs

- `docs/debug-commands-keyboard.md` — keyboard shortcuts,
  cheats.txt, console evaluate internals
- In-game: `(script_doc)` → `hs_doc.txt`, `(help <name>)`

