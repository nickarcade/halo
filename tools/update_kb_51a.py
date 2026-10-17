import json

def main():
    with open("kb.json", "r") as f:
        kb = json.load(f)

    # 1. Update player_effects.obj
    pe_updates = {
        "0xa27a0": {"decl": "void player_effect_add_continuous_effect(short local_player_index, uint32_t tag_index, float current_distance);", "ported": True, "name": "player_effect_add_continuous_effect"},
        "0xa2ab0": {"decl": "void player_effect_update_screen_flash(void);", "ported": True, "name": "player_effect_update_screen_flash"},
        "0xa2ba0": {"decl": "void player_effect_update_camera_shake(int unit_index, float damage_amount, float scale, float *effect_data@<eax>, void *effect@<ebx>);", "ported": True, "name": "player_effect_update_camera_shake"},
        "0xa2d30": {"decl": "void player_effect_continuous_refresh(uint32_t tag_index, void *position);", "ported": True, "name": "player_effect_continuous_refresh"},
        "0xa2df0": {"decl": "void scripted_player_effect_start(uint32_t tag_index, float transition_seconds);", "ported": True, "name": "scripted_player_effect_start"},
        "0xa2fc0": {"decl": "void player_effect_get_screen_flash(short local_player_index, void *flash_out);", "ported": True, "name": "player_effect_get_screen_flash"},
        "0xa32e0": {"decl": "void get_shake_matrix(float *matrix @<esi>, float translation_scale, float rotation_angle);", "ported": True, "name": "get_shake_matrix"},
        "0xa3370": {"decl": "void player_effect_get_camera_effect_matrix(short local_player_index, void *matrix_out);", "ported": True, "name": "player_effect_get_camera_effect_matrix"},
        "0xa3890": {"decl": "void player_effect_update_camera_impulse(int unit_index, float *rumble_def, void *direction, float damage_amount, float scale, float *effect@<eax>);", "ported": True, "name": "player_effect_update_camera_impulse"},
        "0xa3b80": {"decl": "void player_effect_start(int player_index, void *damage_params, void *position, float damage_amount, float scale);", "ported": True, "name": "player_effect_start"}
    }

    # 2. Update player_ui.obj
    pui_updates = {
        "0xe05f0": {"decl": "void overhead_map_initialize(void);", "ported": True, "name": "overhead_map_initialize"},
        "0xe0600": {"decl": "void overhead_map_initialize_for_new_map(void);", "ported": True, "name": "overhead_map_initialize_for_new_map"},
        "0xe0610": {"decl": "void overhead_map_dispose_from_old_map(void);", "ported": True, "name": "overhead_map_dispose_from_old_map"},
        "0xe0620": {"decl": "void overhead_map_post_rasterize(int unknown0, int unknown1, float *position);", "ported": True, "name": "overhead_map_post_rasterize"},
        "0xe0810": {"decl": "int player_ui_get_single_player_local_player_from_controller(short local_player_index);", "ported": True, "name": "player_ui_get_single_player_local_player_from_controller"},
        "0xe0d80": {"decl": "bool player_ui_edit_profile_is_default_profile(void);", "ported": True, "name": "player_ui_edit_profile_is_default_profile"},
        "0xe0ee0": {"decl": "bool player_ui_edit_profile_is_dirty(void);", "ported": True, "name": "player_ui_edit_profile_is_dirty"},
        "0xe1080": {"decl": "void generate_default_player_profile(void *profile);", "ported": True, "name": "generate_default_player_profile"},
        "0xe10c0": {"decl": "void set_local_player_controls_from_player_profile(short local_player_index@<edi>);", "ported": True, "name": "set_local_player_controls_from_player_profile"},
        "0xe12d0": {"decl": "void clear_profile_edit_data(void);", "ported": True, "name": "clear_profile_edit_data"},
        "0xe12e0": {"decl": "void reset_local_player_profile(short local_player_index);", "ported": True, "name": "reset_local_player_profile"},
        "0xe1500": {"decl": "void player_ui_begin_editing_profile(int a1);", "ported": True, "name": "player_ui_begin_editing_profile"},
        "0xe15b0": {"decl": "bool player_ui_save_profile(void);", "ported": True, "name": "player_ui_save_profile"},
        "0xe17b0": {"decl": "void D3DDevice_SetRenderState_17(void);", "ported": True, "name": "D3DDevice_SetRenderState_17"}
    }

    for obj in kb.get("objects", []):
        if obj.get("name") == "player_effects.obj":
            existing = {fn["addr"]: fn for fn in obj.get("functions", [])}
            for addr, data in pe_updates.items():
                if addr in existing:
                    existing[addr].update(data)
                else:
                    obj["functions"].append(dict(addr=addr, **data))
        elif obj.get("name") == "player_ui.obj":
            existing = {fn["addr"]: fn for fn in obj.get("functions", [])}
            for addr, data in pui_updates.items():
                if addr in existing:
                    existing[addr].update(data)
                else:
                    obj["functions"].append(dict(addr=addr, **data))

    # Also update flat map if present
    for addr, data in pe_updates.items():
        if addr in kb:
            kb[addr].update(data)
    for addr, data in pui_updates.items():
        if addr in kb:
            kb[addr].update(data)

    with open("kb.json", "w") as f:
        json.dump(kb, f, indent=2)
    print("kb.json successfully updated for Batch 5.1A.")

if __name__ == "__main__":
    main()
