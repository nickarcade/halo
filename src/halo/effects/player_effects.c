#include "x87_math.h"

__declspec(noinline) char *player_effect_get(int16_t local_player_index)
{
  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\effects\\player_effects.c", 0x73,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  assert_halt_at("c:\\halo\\SOURCE\\effects\\player_effects.c", 0x74,
                 player_effect_globals);
  return player_effect_globals + local_player_index * 0xec;
}

void player_effect_initialize(void)
{
  player_effect_globals = (char *)game_state_malloc("player effects", 0, 0x3ec);
  assert_halt_at("c:\\halo\\SOURCE\\effects\\player_effects.c", 0x7f,
                 player_effect_globals);
}

void player_effect_dispose(void)
{
}

void player_effect_initialize_for_new_map(void)
{
  csmemset(player_effect_globals, 0, 0x3ec);
  *(_WORD *)(player_effect_globals + 0x3c0) = 0xFFFF;
  *(_DWORD *)(player_effect_globals + 0x3e8) = game_time_get();
}

void player_effect_dispose_from_old_map(void)
{
}

/* player_effect_add_continuous_effect (0xa27a0) */
void player_effect_add_continuous_effect(short local_player_index, uint32_t tag_index, float current_distance)
{
  char *effect;
  float *cdmg_tag;
  real scale;
  real factor;
  real blend;
  real val_a;
  real val_b;

  cdmg_tag = (float *)tag_get(0x63646d67, tag_index); /* 'cdmg' */
  if (current_distance < cdmg_tag[1]) {
    effect = player_effect_get(local_player_index);
    scale = 1.0f - (current_distance - cdmg_tag[0]) / (cdmg_tag[0] - cdmg_tag[1]);
    if (scale < 0.0f) {
      scale = 0.0f;
    } else if (scale > 1.0f) {
      scale = 1.0f;
    }

    factor = (real)FUN_0010a5e0(*(uint16_t *)((char *)cdmg_tag + 0x58), (real)game_time_get() / cdmg_tag[0x17]);
    blend = ((factor * cdmg_tag[0x18]) + (1.0f - cdmg_tag[0x18])) * scale;

    if (*(int16_t *)(effect + 0xdc) >= 1) {
      *(uint16_t *)(effect + 0xdc) = 0;
      csmemset(effect + 0xcc, 0, 0x10);
    }

    val_a = blend * cdmg_tag[0x11];
    if (val_a <= 0.0f) {
      val_a = 0.0f;
    }
    *(float *)(effect + 0xd4) += val_a;

    val_b = blend * cdmg_tag[0x12];
    if (val_b <= 0.0f) {
      val_b = 0.0f;
    }
    *(float *)(effect + 0xd8) += val_b;

    *(float *)(effect + 0xcc) += scale * cdmg_tag[9];
    *(float *)(effect + 0xd0) += scale * cdmg_tag[10];
  }
}


/* scripted_player_effect_set_rotation -- store the three script-supplied
 * rotation components, each scaled by the constant at 0x253d4c
 * (0.017453292f, degrees to radians), into the player-effect globals.
 *
 * Confirmed (0xa28e0..0xa2916): EAX = [0x4557ec] (player_effect_globals);
 * FLD [EBP+8] / FMUL [0x253d4c] / FSTP [EAX+0x3d0], then the same shape for
 * [EBP+0xc] -> +0x3d4 and [EBP+0x10] -> +0x3d8.
 *
 * The first stack slot is loaded with FLD float ptr, so the binary reads it
 * as a float. The kb decl still declares it `int` (inherited from the
 * HaloScript caller at 0xc2fe0, which forwards result[0] verbatim); the decl
 * and caller are left unchanged here and the slot's bits are read as a float,
 * which matches both the caller's verbatim dword forward and this FLD.
 *
 * 0xa28e0 / player_effects.obj */
void scripted_player_effect_set_rotation(int param_1, float param_2,
                                         float param_3)
{
  char *globals;

  globals = player_effect_globals;
  *(float *)(globals + 0x3d0) = *(float *)&param_1 * *(float *)0x253d4c;
  *(float *)(globals + 0x3d4) = param_2 * *(float *)0x253d4c;
  *(float *)(globals + 0x3d8) = param_3 * *(float *)0x253d4c;
}

/* Forward the two script-supplied motor values to the rumble system.
 *
 * Disassembly (0xa2920..0xa2929, 9 bytes):
 *   55            PUSH EBP
 *   8B EC         MOV  EBP,ESP
 *   5D            POP  EBP
 *   E9 57 72 01 00  JMP 0xb9b80   ; rumble_player_set_scripted_values
 *
 * A pure identical-forward tail call: no argument reload, no `add esp`, and
 * — decisively — no FILD/FSTP conversion code.  MSVC only collapses a wrapper
 * to that bare JMP when the parameter list matches the callee's exactly, so
 * both parameters are `float`, like rumble_player_set_scripted_values(float,
 * float) at 0xb9b80.  The kb decl previously read `(int param_1, float
 * param_2)`, inferred from the HaloScript caller at 0xc3030 where argument 1
 * is pushed with `MOV EDX,[EAX]; PUSH EDX` while argument 2 uses the
 * `PUSH ECX; FSTP [ESP]` float idiom.  That mixed shape is MSVC scheduling,
 * not a type signal — the callee proves it: 0xb9b80 stores BOTH parameters
 * into float slots yet marshals the first with `FLD [EBP+8]; FSTP
 * [EAX+0x820]` and the second with a plain dword `MOV ECX,[EBP+0xc];
 * MOV [EAX+0x824],ECX`.  The 0xc3030 call site was corrected to read its
 * first argument as a float lvalue so the dword is still forwarded verbatim
 * instead of being run through an int-to-float conversion.
 *
 * 0xa2920 / player_effects.obj */
void scripted_player_effect_set_rumble(float left_motor, float right_motor)
{
  rumble_player_set_scripted_values(left_motor, right_motor);
}

/* player_telefrag_effect_stop -- silence the telefragged player's rumble.
 *
 * Confirmed (0xa2930..0xa2965, 54 bytes):
 *   - MOV EAX,[EBP+8] / MOV ECX,[0x5aa6d4] / PUSH EAX / PUSH ECX /
 *     CALL 0x119320: the function takes ONE stack argument (a player datum
 *     handle) even though the kb decl previously read `(void)`.  Argument
 *     order is datum_get(g_players_data, player_handle), the same shape as
 *     the other player lookups in this TU.
 *   - MOVSX ESI,word ptr [EAX+2]: the local-player index is a signed 16-bit
 *     field at player+2, sign-extended before the CMP ESI,-1 / JZ guard.
 *     The C local must therefore be a 32-bit `int` holding the widened value,
 *     not an `int16_t`; declaring it int16_t makes the compiler emit
 *     `xor esi,esi / mov si,[eax+2]` plus a 16-bit `cmp si,-1` instead of the
 *     single MOVSX and 32-bit CMP.  Both calls receive the full dword in ESI.
 *   - PUSH ESI / CALL 0xa2690 (player_effect_get) with its return value
 *     unused, then PUSH 0 / PUSH 0 / PUSH ESI / CALL 0xb9da0.  The single
 *     ADD ESP,0x10 at 0xa2960 cleans up BOTH calls (1 + 3 dwords) -- this is
 *     why the call-site audit reports cleanup=4 against a 3-parameter decl
 *     for rumble_player_continuous; it is not a fourth argument.
 *
 * The discarded player_effect_get result is preserved because the call is a
 * real side-effecting step in the original instruction stream (it carries the
 * bounds asserts at 0xa2690); its return value is genuinely dead.
 *
 * 0xa2930 / player_effects.obj */
void player_telefrag_effect_stop(int player_handle)
{
  char *player;
  int local_player_index;

  player = (char *)datum_get(*(data_t **)0x5aa6d4, player_handle);
  local_player_index = *(int16_t *)(player + 2);

  if (local_player_index != -1) {
    player_effect_get((int16_t)local_player_index);
    rumble_player_continuous((short)local_player_index, 0, 0);
  }
}

/* player_effect_screen_fade_in -- record a screen fade in the player effect
 * globals and stamp its start time.
 *
 * Confirmed (0xa2970..0xa29b7, 72 bytes, no locals):
 *   - EAX = [0x4557ec] (player_effect_globals), read once for the stores.
 *   - [EBP+8] -> globals+0x3b0 via FLD/FSTP (4 bytes); [EBP+0xc] -> +0x3b4
 *     and [EBP+0x10] -> +0x3b8 via dword MOVs; word [EBP+0x14] -> +0x3c0.
 *     The first slot is x87-copied although the kb decl types it int; the
 *     4-byte copy is bit-identical either way, so the decl is kept unchanged.
 *   - byte +0x3c2 = 0.
 *   - CALL 0xb5aa0 (game_time_get, cdecl, no args); the globals pointer is
 *     re-read after the call and EAX is stored to +0x3bc.
 *
 * 0xa2970 / player_effects.obj */
void player_effect_screen_fade_in(int effect_definition, float scale_a,
                                  float scale_b, uint16_t flags_or_index)
{
  char *globals;

  globals = player_effect_globals;
  *(int *)(globals + 0x3b0) = effect_definition;
  *(float *)(globals + 0x3b4) = scale_a;
  *(float *)(globals + 0x3b8) = scale_b;
  *(uint16_t *)(globals + 0x3c0) = flags_or_index;
  *(char *)(globals + 0x3c2) = 0;
  *(int *)(player_effect_globals + 0x3bc) = game_time_get();
}

/* player_effect_screen_fade_out -- same record as
 * player_effect_screen_fade_in, but marks the fade direction byte as 1.
 *
 * Confirmed (0xa29c0..0xa2a07, 72 bytes, no locals):
 *   - EAX = [0x4557ec] (player_effect_globals), read once for the stores.
 *   - [EBP+8] -> globals+0x3b0 via FLD/FSTP (4 bytes); [EBP+0xc] -> +0x3b4
 *     and [EBP+0x10] -> +0x3b8 via dword MOVs; word [EBP+0x14] -> +0x3c0.
 *     The first slot is typed int per the kb decl (bit-identical 4-byte copy).
 *   - byte +0x3c2 = 1 (fade_in stores 0).
 *   - CALL 0xb5aa0 (game_time_get, cdecl, no args); the globals pointer is
 *     re-read after the call and EAX is stored to +0x3bc.
 *
 * 0xa29c0 / player_effects.obj */
void player_effect_screen_fade_out(int effect_definition, float scale_a,
                                   float scale_b, uint16_t flags_or_index)
{
  char *globals;

  globals = player_effect_globals;
  *(int *)(globals + 0x3b0) = effect_definition;
  *(float *)(globals + 0x3b4) = scale_a;
  *(float *)(globals + 0x3b8) = scale_b;
  *(uint16_t *)(globals + 0x3c0) = flags_or_index;
  *(char *)(globals + 0x3c2) = 1;
  *(int *)(player_effect_globals + 0x3bc) = game_time_get();
}

/* player_effect_get_damage_indicators -- copy the local player's four damage
 * indicator bytes to `out`, then age each live indicator by the elapsed game
 * time, saturating at 0xff.
 *
 * Confirmed (0xa2a10..0xa2a6f):
 *   - PUSH [EBP+8] / CALL 0xa2690: the raw dword is forwarded to
 *     player_effect_get(int16_t).
 *   - LEA ESI,[EAX+0xe4]: the indicator array lives at effect+0xe4 and is
 *     4 bytes wide (PUSH 0x4 / PUSH ESI / PUSH ECX / CALL csmemcpy, ADD
 * ESP,0x10). The same +0xe4/4 window is cleared in player_effect_update.
 *   - The copy happens BEFORE the aging pass, so `out` receives the previous
 *     tick's values.
 *   - Loop is a 4-iteration countdown (MOV EDI,4 / INC ESI / DEC EDI / JNZ)
 *     that skips zero entries (CMP byte ptr [ESI],0x0 / JZ).
 *   - Saturation test is signed on the widened sum: MOVSX EDX,AX (int16_t
 *     game_time_get_elapsed) + MOVZX EAX,byte ptr [ESI], CMP EDX,0xff,
 *     JGE -> 0xff.
 *   - game_time_get_elapsed() is called a SECOND time on the non-saturating
 *     path (CALL 0x000b5ae0 at 0xa2a3d and again at 0xa2a52); the sum is
 *     recomputed rather than reused, so both calls are preserved here.
 *
 * 0xa2a10 / player_effects.obj */
void player_effect_get_damage_indicators(int player_index, void *out)
{
  unsigned char *indicators;
  int count;
  int aged;

  indicators =
    (unsigned char *)(player_effect_get((int16_t)player_index) + 0xe4);
  csmemcpy(out, indicators, 4);
  count = 4;
  do {
    if (*indicators != 0) {
      if ((int)game_time_get_elapsed() + (int)*indicators < 0xff) {
        aged = game_time_get_elapsed() + *indicators;
      } else {
        aged = 0xff;
      }
      *indicators = (unsigned char)aged;
    }
    indicators++;
    count--;
  } while (count != 0);
}

/* player_effect_clear_damage_indicators -- zero the local player's four damage
 * indicator bytes.
 *
 * Confirmed (0xa2a70..0xa2a8f):
 *   - MOV EAX,[EBP+8] / PUSH EAX / CALL 0xa2690: the raw dword is forwarded to
 *     player_effect_get(int16_t), same shape as 0xa2a10.
 *   - ADD EAX,0xe4: the same 4-byte indicator array at effect+0xe4 that
 *     player_effect_get_damage_indicators copies out of.
 *   - PUSH 0x4 / PUSH 0x0 / PUSH EAX / CALL 0x8db80 -> csmemset(buf, 0, 4).
 *   - The single ADD ESP,0x10 retires the callee argument of
 *     player_effect_get together with csmemset's three; it is not a 4-argument
 *     csmemset call.
 *
 * 0xa2a70 / player_effects.obj */
void player_effect_clear_damage_indicators(int player_index)
{
  char *effect;

  effect = player_effect_get((int16_t)player_index);
  csmemset(effect + 0xe4, 0, 4);
}

/* effect_scale_factor -- returns base + (1.0f - base) * scale.
 *
 * Binary (0xa2a90): FLD [0x2533c8] (1.0f); FSUB [ebp+8]; FMUL [ebp+0xc];
 * FADD [ebp+8]; result left in ST0, plain RET (cdecl, no callees).
 * Parameter meanings unknown; names are positional.
 *
 * 0xa2a90 / player_effects.obj */
float effect_scale_factor(float param_1, float param_2)
{
  return (*(float *)0x002533c8 - param_1) * param_2 + param_1;
}

/* effect_scale_value -- evaluates a transition function at
 * t = 1.0f - param_2 / param_3 and scales the result by param_1.
 *
 * Binary (0xa2c70): FLD [ebp+0xc]; FDIV [ebp+0x10]; FSUBR [0x2533c8] (1.0f);
 * FSTP [esp]; PUSH EAX (caller-supplied, never defined here);
 * CALL transition_function_evaluate; FMUL [ebp+8]; result in ST0.
 * No xrefs in the binary; float parameter meanings unknown (positional).
 *
 * 0xa2c70 / player_effects.obj */
float effect_scale_value(short function_type /* @<eax> */, float param_1,
                         float param_2, float param_3)
{
  return transition_function_evaluate(function_type, *(float *)0x002533c8 -
                                                       param_2 / param_3) *
         param_1;
}

void player_effect_update(void)
{
  int16_t local_player_index;
  int player_index;
  void *player;
  char *effect;

  local_player_index = (int16_t)local_player_get_next(-1);
  while (local_player_index != -1) {
    player_index = local_player_get_player_index(local_player_index);
    if (player_index != -1) {
      player = datum_get(player_data,
                         local_player_get_player_index(local_player_index));
      if (*(int *)((char *)player + 0x34) != -1) {
        local_player_index = (int16_t)local_player_get_next(local_player_index);
        continue;
      }
    }
    effect = player_effect_get(local_player_index);
    csmemset(effect + 0xe4, 0, 4);
    csmemset(player_effect_get(local_player_index), 0, 0xec);
    rumble_player_clear(local_player_index);
    local_player_index = (int16_t)local_player_get_next(local_player_index);
  }
}

/* player_effect_continuous_refresh (0xa2d30) */
void player_effect_continuous_refresh(uint32_t tag_index, void *position)
{
  int16_t i;
  int player_index;
  char *player;
  int unit_index;
  vector3_t victim_pos;
  float *pos;
  float dx, dy, dz, dist;

  pos = (float *)position;
  for (i = 0; i < 4; i++) {
    player_index = local_player_get_player_index(i);
    if (player_index != -1) {
      player = (char *)datum_get(*(data_t **)0x5aa6d4, player_index);
      unit_index = *(int *)(player + 0x34);
      if (unit_index != -1) {
        object_get_world_position(unit_index, &victim_pos);
        dx = pos[0] - victim_pos.x;
        dy = pos[1] - victim_pos.y;
        dz = pos[2] - victim_pos.z;
        dist = x87_sqrt(dx * dx + dy * dy + dz * dz);
        player_effect_add_continuous_effect(i, tag_index, dist);
      }
    }
  }
}

/* scripted_player_effect_set_translation -- store the three script-supplied
 * translation components into the shared player-effect globals.
 *
 * Confirmed (0xa2dc0..0xa2de4, 37 bytes): the globals pointer at 0x4557ec
 * (player_effect_globals, the 0x3ec-byte block allocated at 0xa2700) is loaded
 * once into EAX, then the three incoming stack dwords [EBP+8], [EBP+0xc] and
 * [EBP+0x10] are written verbatim to +0x3c4, +0x3c8 and +0x3cc.  There is no
 * FILD/FSTP conversion and no arithmetic on any of them, so each argument slot
 * is forwarded bit-exact; the dword MOV shape is MSVC scheduling and carries no
 * type signal either way (same caveat as 0xa2920).
 *
 * Unknown: the globals' field types at +0x3c4..+0x3cc.  The kb decl's
 * int/float/float split is inherited from the HaloScript call site at 0xc2f90
 * and is preserved here, so each parameter is stored through a pointer of its
 * own declared type; MSVC copies the float parameters with plain dword MOVs
 * (no FLD/FSTP), reproducing the reference exactly.
 *
 * 0xa2dc0 / player_effects.obj */
void scripted_player_effect_set_translation(int param_1, float param_2,
                                            float param_3)
{
  char *globals;

  globals = player_effect_globals;
  *(int *)(globals + 0x3c4) = param_1;
  *(float *)(globals + 0x3c8) = param_2;
  *(float *)(globals + 0x3cc) = param_3;
}

/* scripted_player_effect_start (0xa2df0) */
void scripted_player_effect_start(uint32_t tag_index, float transition_seconds)
{
  int16_t ticks;

  ticks = (int16_t)(transition_seconds * 30.0f);
  *(uint32_t *)(player_effect_globals + 0x3dc) = tag_index;
  *(int16_t *)(player_effect_globals + 0x3e0) = ticks;
  *(int16_t *)(player_effect_globals + 0x3e2) = ticks;
  *(uint32_t *)(player_effect_globals + 0x3e4) = (*(uint32_t *)(player_effect_globals + 0x3e4) & ~2) | 1;
}

/* scripted_player_effect_stop -- script-driven stop of the scripted player

 * effect: converts the incoming duration to ticks and arms the stop.
 *
 * Confirmed (0xa2e40..0xa2e76): FLD [EBP+8]; FMUL [0x253394]
 * (TICKS_PER_SECOND); FSTP [EBP+8] -- the product is narrowed back into the
 * argument slot -- then FLD; FISTP [EBP-4] (round-to-nearest, no _ftol2).
 * The low word of that integer is stored to both +0x3e0 and +0x3e2 of
 * player_effect_globals (0x4557ec), then bit 1 of the dword at +0x3e4 is set.
 *
 * The kb decl keeps `int param_1` because the only caller (0xc30b0) forwards
 * the HaloScript record's +0 dword bit-exact (MOV EDX,[EAX]; PUSH EDX); the
 * callee reads that slot as a float, so it is reinterpreted, not converted.
 * Unknown: semantics of +0x3e0/+0x3e2 (int16) and of flag bit 1 at +0x3e4.
 *
 * 0xa2e40 / player_effects.obj */
void scripted_player_effect_stop(int param_1)
{
  char *globals;
  int16_t ticks;

  *(float *)&param_1 = *(float *)&param_1 * TICKS_PER_SECOND;
  ticks = (int16_t)x87_round_to_int(*(float *)&param_1);
  globals = player_effect_globals;
  *(int16_t *)(globals + 0x3e0) = ticks;
  *(int16_t *)(globals + 0x3e2) = ticks;
  *(uint32_t *)(globals + 0x3e4) |= 2;
}

/* player_effect_set_from_descriptor -- apply an effect descriptor to a player's
 * effect state. Internal helper at 0xa2ab0.
 *
 * The original binary passes the descriptor in EBX as a register arg;
 * we pass it explicitly since all callers are in this TU.
 *
 * Confirmed: copies 56 bytes (14 dwords) from descriptor to effect+0x18.
 * Confirmed: scales effect+0x28 by intensity_scale.
 * Confirmed: sets effect+0xde to (short)(intensity_scale * effect->field_28).
 * Confirmed: clamps effect+0x3c to [0.0f, max] where max comes from descriptor.
 * Confirmed: sets bit 0 at effect+0xe8.
 */
static void player_effect_set_from_descriptor(int player_index, char *effect,
                                              float intensity,
                                              float intensity_scale,
                                              void *descriptor)
{
  int16_t desc_type = *(int16_t *)descriptor;
  int16_t desc_priority = *((int16_t *)descriptor + 1);
  float desc_duration = *(float *)((char *)descriptor + 0x10);
  float desc_max = *(float *)((char *)descriptor + 0x20);
  float desc_min = *(float *)((char *)descriptor + 0x24);
  int16_t effect_priority = *(int16_t *)(effect + 0x1a);
  int16_t effect_timer = *(int16_t *)(effect + 0xde);
  int16_t *enabled_array = (int16_t *)0x2ef7e0;
  float scaled_duration;
  float clamped_value;

  (void)
    player_index; /* original binary receives this in ESI but never uses it */

  scaled_duration = intensity_scale * desc_duration;

  if (((effect_priority <= desc_priority) ||
       ((float)effect_timer <= scaled_duration)) &&
      (enabled_array[desc_type] != 0)) {
    csmemcpy(effect + 0x18, descriptor, 0x38);

    *(float *)(effect + 0x28) = intensity_scale * *(float *)(effect + 0x28);

    *(int16_t *)(effect + 0xde) = (int16_t)(*(float *)(effect + 0x28));

    clamped_value = 0.0f;
    if (0.0f <= ((1.0f - desc_min) * intensity + desc_min)) {
      if (((1.0f - desc_min) * intensity + desc_min) <= desc_max) {
        clamped_value = (1.0f - desc_min) * intensity + desc_min;
      } else {
        clamped_value = desc_max;
      }
    }
    *(float *)(effect + 0x3c) = clamped_value;
    *(uint8_t *)(effect + 0xe8) |= 1;
  }
}

void player_effect_screen_flash(int player_handle, void *effect_descriptor,
                                float intensity)
{
  int16_t unit_index;
  void *player;
  char *effect;

  if (player_handle == -1)
    return;

  player = datum_get(player_data, player_handle);
  unit_index = *(int16_t *)((char *)player + 2);

  if (unit_index == -1)
    return;

  effect = player_effect_get(unit_index);
  player_effect_set_from_descriptor(unit_index, effect, intensity,
                                    intensity * 30.0f, effect_descriptor);
}

/* player_effect_update_screen_flash (0xa2ab0) */
void player_effect_update_screen_flash(void)
{
}

/* player_effect_update_camera_shake (0xa2ba0) */
void player_effect_update_camera_shake(int unit_index, float damage_amount, float scale, float *effect_data /* @<eax> */, void *effect /* @<ebx> */)
{
  float scaled_intensity;
  float current_val;

  (void)unit_index;
  game_time_get();
  scale *= 30.0f;
  scaled_intensity = (1.0f - effect_data[10]) * damage_amount + effect_data[10];
  current_val = (float)*(int16_t *)((char *)effect + 0xe2);

  if ((scale * effect_data[0] <= current_val) && (scaled_intensity <= *(float *)((char *)effect + 0xac))) {
    if (scaled_intensity < *(float *)((char *)effect + 0xac) || (scale * effect_data[0] <= current_val)) {
      return;
    }
  }

  csmemcpy((char *)effect + 0x84, effect_data, 0x48);
  *(float *)((char *)effect + 0xac) = scaled_intensity;
  *(float *)((char *)effect + 0x84) *= scale;
  *(int16_t *)((char *)effect + 0xe2) = (int16_t)(*(float *)((char *)effect + 0x84));
  *(uint8_t *)((char *)effect + 0xe8) |= 4;
  *(float *)((char *)effect + 0xa4) *= scale;
}

/* player_telefrag_effect_start -- start the white full-screen flash and
 * full-strength rumble on a player who has just been telefragged.
 *
 * The function builds a synthetic player-effect descriptor on the stack
 * instead of reading one out of a jpt! tag, then runs it through the same two
 * helpers the damage path uses (player_effect_set_from_descriptor at 0xa2ab0
 * and player_effect_update_camera_shake).
 *
 * Confirmed (0xa2ed0..0xa2fbc, 237 bytes):
 *   - The kb decl previously read `(void)`; the body reads [EBP+8] (a player
 *     datum handle) and [EBP+0xc] (a float), so there are two stack params.
 *   - SUB ESP,0x84 covers exactly three memory locals, and 0x4 + 0x38 + 0x48
 *     is exactly 0x84: the effect pointer at EBP-0x4 (spilled across the
 *     0xa2ab0 call because that call site reuses EBX), the 0x38-byte
 *     descriptor at EBP-0x3c, and the 0x48-byte effect-data block at
 *     EBP-0x84.  Both aggregates are zeroed by MSVC's `= {0}` expansion,
 *     which stores the first element explicitly and REP STOSes the rest --
 *     the width of that first store gives the element type:
 *       descriptor : MOV word [EBP-0x3c],0 / ECX=0xd / REP STOSD / STOSW
 *                    = 2 + 52 + 2 bytes  -> int16_t[28]
 *       effect_data: MOV dword [EBP-0x84],0 / ECX=0x11 / REP STOSD
 *                    = 4 + 68 bytes      -> float[18]
 *     The descriptor is zeroed first, so it is declared first.
 *   - datum_get(player_data, player_handle) then MOVSX ESI,word ptr [EAX+2]:
 *     the local-player index is sign-extended to 32 bits before CMP ESI,-1,
 *     so the C local is `int`, exactly as in player_telefrag_effect_stop.
 *   - Descriptor stores (offsets from EBP-0x3c; field meanings come from
 *     player_effect_set_from_descriptor, which csmemcpy's all 0x38 bytes into
 *     effect+0x18):
 *       +0x00 word 1     effect type
 *       +0x02 word 2     priority
 *       +0x10 1.0f       duration
 *       +0x20 intensity  maximum
 *       +0x24 0.0f       minimum
 *       +0x28..+0x37     16 bytes copied through *(float **)0x2ee6c4, the
 *                        pointer to the all-ones colour {1,1,1,1} at
 *                        0x267700 -- the same global ai_debug.c and actors.c
 *                        read as a colour.  Loaded once into EAX
 *                        (MOV EAX,[0x2ee6c4]) and copied as four dwords.
 *   - effect_data stores: [0] = 1.0f and [2] = intensity * 0.01.  The
 *     multiply is FLD float [EBP+0xc] / FMUL *double* ptr [0x26aed0] / FSTP
 *     float [EBP-0x7c], and 0x26aed0 holds the double 0.01, so the literal is
 *     unsuffixed and the product is narrowed on the store.
 *   - PUSH EDI / PUSH EDI with EDI = dword ptr [EBP+0xc]: both rumble motor
 *     values are the raw dword of the float parameter and there is no
 *     FISTP/_ftol anywhere in the function.  rumble_player_continuous is
 *     declared with int motor params because 0xb9da0 stores both through
 *     `*(int *)`, so the dword must be forwarded by value; the punned
 *     `*(int *)&intensity` reproduces the plain MOV/PUSH pair instead of an
 *     int conversion.
 *   - The single ADD ESP,0x2c at 0xa2fb3 cleans up all four cdecl calls
 *     (1 + 3 + 4 + 3 dwords).  That is why the call-site audit reports
 *     cleanup=11 against player_effect_update_camera_shake's three stack
 * params; it is not evidence of extra arguments.
 *   - 0xa2ab0 receives the descriptor in EBX (LEA EBX,[EBP-0x3c] immediately
 *     before the CALL) and 0xa2ba0 receives the effect-data block in EAX and
 *     the effect pointer in EBX.  The descriptor is passed as an ordinary
 *     fifth argument here because 0xa2ab0 is file-local in this TU, matching
 *     the other two call sites.
 *
 * 0xa2ed0 / player_effects.obj */
void player_telefrag_effect_start(int player_handle, float intensity)
{
  char *effect;
  int16_t descriptor[28] = { 0 };
  float effect_data[18] = { 0 };
  char *player;
  int local_player_index;
  float *flash_color;

  player = (char *)datum_get(player_data, player_handle);
  local_player_index = *(int16_t *)(player + 2);

  if (local_player_index != -1) {
    effect = player_effect_get((int16_t)local_player_index);

    effect_data[2] = (float)(intensity * 0.01);

    flash_color = *(float **)0x2ee6c4;
    *(float *)((char *)descriptor + 0x28) = flash_color[0];
    *(float *)((char *)descriptor + 0x2c) = flash_color[1];
    *(float *)((char *)descriptor + 0x30) = flash_color[2];
    *(float *)((char *)descriptor + 0x34) = flash_color[3];

    effect_data[0] = 1.0f;
    descriptor[0] = 1;
    descriptor[1] = 2;
    *(float *)((char *)descriptor + 0x10) = 1.0f;
    *(float *)((char *)descriptor + 0x20) = intensity;
    *(float *)((char *)descriptor + 0x24) = 0.0f;

    rumble_player_continuous((short)local_player_index, *(int *)&intensity,
                             *(int *)&intensity);
    player_effect_set_from_descriptor(local_player_index, effect, intensity,
                                      1.0f, descriptor);
    player_effect_update_camera_shake(local_player_index, intensity, 1.0f,
                                      effect_data /* @<eax> */,
                                      (void *)effect /* @<ebx> */);
  }
}

/* player_effect_get_screen_flash (0xa2fc0) */
void player_effect_get_screen_flash(short local_player_index, void *flash_out)
{
  uint16_t *out;
  char *globals;
  char *effect;
  int elapsed;
  real fade_progress;
  real total_ticks;
  real current_ticks;
  real max_intensity;
  int16_t timer;

  out = (uint16_t *)flash_out;
  globals = player_effect_globals;

  if (!out) {
    assert_halt(0);
  }

  if (!console_is_active()) {
    if (*(int16_t *)(globals + 0x3c0) != -1 &&
        (*(int8_t *)(globals + 0x3c2) || ((int)game_time_get() - *(int *)(globals + 0x3bc) <= *(int16_t *)(globals + 0x3c0)))) {
      *out = 1;
      *(uint32_t *)(out + 3) = *(uint32_t *)(globals + 0x3b0);
      *(uint32_t *)(out + 4) = *(uint32_t *)(globals + 0x3b4);
      *(uint32_t *)(out + 5) = *(uint32_t *)(globals + 0x3b8);
      *(uint32_t *)(out + 2) = 0x3f800000; /* 1.0f */

      if (*(int16_t *)(globals + 0x3c0) >= 1) {
        total_ticks = (real)*(int16_t *)(globals + 0x3c0);
        current_ticks = (real)(*(int16_t *)(globals + 0x3c0) - ((int)game_time_get() - *(int *)(globals + 0x3bc)));
        fade_progress = current_ticks / total_ticks;
        if (fade_progress < 0.0f) {
          fade_progress = 0.0f;
        } else if (fade_progress > 1.0f) {
          fade_progress = 1.0f;
        }

        if (*(int8_t *)(globals + 0x3c2) == 0) {
          fade_progress = 1.0f - fade_progress;
        }
        *(real *)(out + 2) = fade_progress;
      }
      return;
    }

    if (local_player_index != -1) {
      effect = player_effect_get(local_player_index);
      timer = *(int16_t *)(effect + 0xde);
      if (timer > 0 || (*(uint8_t *)(effect + 0xe8) & 1)) {
        *(uint8_t *)(effect + 0xe8) &= ~1;
        *out = *(uint16_t *)(0x2ef7e0 + *(int16_t *)(effect + 0x18) * 2);
        *(uint32_t *)(out + 4) = *(uint32_t *)(effect + 0x40);
        *(uint32_t *)(out + 5) = *(uint32_t *)(effect + 0x44);
        *(uint32_t *)(out + 6) = *(uint32_t *)(effect + 0x48);
        *(uint32_t *)(out + 7) = *(uint32_t *)(effect + 0x4c);

        if (*(float *)(effect + 0x28) <= 0.0f) {
          *(uint32_t *)(out + 2) = *(uint32_t *)(effect + 0x3c);
        } else {
          max_intensity = *(float *)(effect + 0x3c);
          fade_progress = transition_function_evaluate(*(uint16_t *)(effect + 0x2c), ((real)timer / *(float *)(effect + 0x28)) * max_intensity);
          *(real *)(out + 2) = fade_progress;
        }

        elapsed = game_time_get_elapsed();
        *(int16_t *)(effect + 0xde) -= (int16_t)elapsed;

        if (*(float *)(out + 2) < 0.0f || *(float *)(out + 2) > 1.0f) {
          assert_halt(0);
        }
      }
    }
  }
}

/* get_shake_matrix -- apply a random-axis rotation and a random-direction
 * translation to the caller's matrix (ESI).
 *
 * Binary (0xa32e0): if [ebp+0xc] != [0x2533c0]: seed =
 * random_math_get_local_seed_address(); random_seed_get_direction3d(seed,
 * &axis); FUN_001092d0(ESI, &axis, fsin(angle), fcos(angle)) (both floats
 * FSTP'd into the reused arg slots). If [ebp+8] != [0x2533c0]: same random
 * direction, scaled by [ebp+8], stored to matrix +0x28/+0x2c/+0x30.
 * Callers: FUN_000a3370 (x2).
 *
 * 0xa32e0 / player_effects.obj */
void get_shake_matrix(float *matrix /* @<esi> */, float translation_scale,
                      float rotation_angle)
{
  float direction[3];

  if (rotation_angle != *(float *)0x002533c0) {
    random_seed_get_direction3d(random_math_get_local_seed_address(),
                                direction);
    FUN_001092d0(matrix, direction, x87_fsin(rotation_angle),
                 x87_fcos(rotation_angle));
  }
  if (translation_scale != *(float *)0x002533c0) {
    random_seed_get_direction3d(random_math_get_local_seed_address(),
                                direction);
    matrix[10] = direction[0] * translation_scale;
    matrix[11] = direction[1] * translation_scale;
    matrix[12] = direction[2] * translation_scale;
  }
}

/* player_effect_get_camera_effect_matrix (0xa3370) */
void player_effect_get_camera_effect_matrix(short local_player_index, void *matrix_out)
{
  char *globals;
  char *effect;
  real intensity;
  real rot_scale, trans_scale;
  float shake_matrix[13];
  real progress;
  real duration;
  int16_t timer;
  float *mout;
  unsigned int *seed;
  float rand_y, rand_p, rand_r;

  mout = (float *)matrix_out;
  globals = player_effect_globals;

  if (!mout) {
    assert_halt(0);
  }
  if (local_player_index == -1) {
    return;
  }
  game_time_get();

  if (*(uint8_t *)(globals + 0x3e4) & 1) {
    intensity = *(float *)(globals + 0x3dc);
    csmemcpy(mout, (void *)0x31fc60, 0x34);
    timer = *(int16_t *)(globals + 0x3e0);
    if (timer >= 1) {
      if (*(uint8_t *)(globals + 0x3e4) & 2) {
        intensity = ((real)timer / (real)*(int16_t *)(globals + 0x3e2)) * intensity;
        *(int16_t *)(globals + 0x3e0) -= (int16_t)game_time_get_elapsed();
      } else {
        intensity = (1.0f - (real)timer / (real)*(int16_t *)(globals + 0x3e2)) * intensity;
        *(int16_t *)(globals + 0x3e0) -= (int16_t)game_time_get_elapsed();
      }
    } else if (*(uint32_t *)(globals + 0x3e4) & 2) {
      *(uint32_t *)(globals + 0x3e4) &= ~1;
      rumble_player_set_scale(0.0f);
    }

    if (!(*(uint8_t *)(globals + 0x3e4) & 1)) {
      return;
    }
    if (intensity < 0.0f) intensity = 0.0f;
    if (intensity > 1.0f) intensity = 1.0f;

    rumble_player_set_scale(intensity);

    seed = (unsigned int *)random_math_get_local_seed_address();
    rand_y = random_real_range((int *)seed, -1.0f, 1.0f);
    rand_p = random_real_range((int *)seed, -1.0f, 1.0f);
    rand_r = random_real_range((int *)seed, -1.0f, 1.0f);

    FUN_00109e90(mout, rand_r * *(float *)(globals + 0x3d0) * intensity,
                 rand_p * *(float *)(globals + 0x3d4) * intensity,
                 rand_y * *(float *)(globals + 0x3d8) * intensity);

    mout[10] = random_real_range((int *)seed, -1.0f, 1.0f) * *(float *)(globals + 0x3c8) * intensity;
    mout[11] = random_real_range((int *)seed, -1.0f, 1.0f) * *(float *)(globals + 0x3c4) * intensity;
    mout[12] = random_real_range((int *)seed, -1.0f, 1.0f) * *(float *)(globals + 0x3cc) * intensity;
    return;
  }

  effect = player_effect_get(local_player_index);
  timer = *(int16_t *)(effect + 0xe0);
  if (timer >= 1) {
    if (*(uint8_t *)(effect + 0xe8) & 2) {
      progress = 1.0f;
    } else {
      duration = *(float *)(effect + 0x50);
      progress = transition_function_evaluate(*(uint16_t *)(effect + 0x54),
                   1.0f - (duration - (real)timer) / duration);
      progress *= *(float *)(effect + 0x68);
    }
  } else {
    if (!(*(uint8_t *)(effect + 0xe8) & 2)) {
      csmemcpy(mout, (void *)0x31fc60, 0x34);
      goto check_shake;
    }
    progress = 1.0f;
  }

  *(uint8_t *)(effect + 0xe8) &= ~2;
  FUN_001092d0(shake_matrix,
               (float *)effect,
               x87_fsin(progress * *(float *)(effect + 0x58)),
               x87_fcos(progress * *(float *)(effect + 0x58)));
  shake_matrix[10] = progress * *(float *)(effect + 0x5c) + progress * *(float *)effect;
  shake_matrix[11] = progress * *(float *)(effect + 0x60) + progress * *(float *)(effect + 4);
  shake_matrix[12] = progress * *(float *)(effect + 0x64) + progress * *(float *)(effect + 8);

  *(int16_t *)(effect + 0xe0) -= (int16_t)game_time_get_elapsed();
  csmemcpy(mout, shake_matrix, 0x34);

check_shake:
  timer = *(int16_t *)(effect + 0xe2);
  if (timer > 0 || (*(uint8_t *)(effect + 0xe8) & 4)) {
    csmemcpy(shake_matrix, (void *)0x31fc60, 0x34);
    if (*(uint8_t *)(effect + 0xe8) & 4) {
      rot_scale = 1.0f;
    } else {
      duration = *(float *)(effect + 0x84);
      rot_scale = transition_function_evaluate(*(uint16_t *)(effect + 0x88),
                    1.0f - (duration - (real)timer) / duration);
      rot_scale *= *(float *)(effect + 0xac);
    }

    progress = FUN_0010a5e0(*(uint16_t *)(effect + 0xa0),
                 (*(float *)(effect + 0x84) - (real)timer) / *(float *)(effect + 0xa4));
    progress = ((1.0f - *(float *)(effect + 0xa8)) + progress * *(float *)(effect + 0xa8)) * rot_scale;

    rot_scale = progress * *(float *)(effect + 0x8c);
    trans_scale = progress * *(float *)(effect + 0x90);
    if (rot_scale < 0.0f) rot_scale = 0.0f;
    if (trans_scale < 0.0f) trans_scale = 0.0f;

    *(uint8_t *)(effect + 0xe8) &= ~4;
    get_shake_matrix(shake_matrix, trans_scale + *(float *)(effect + 0xd4), rot_scale + *(float *)(effect + 0xd8));
    rumble_player_continuous(local_player_index, *(int *)(effect + 0xcc), *(int *)(effect + 0xd0));

    *(int16_t *)(effect + 0xdc) += (int16_t)game_time_get_elapsed();
    if (*(int16_t *)(effect + 0xdc) >= 1) {
      *(int16_t *)(effect + 0xdc) = 0;
      csmemset(effect + 0xcc, 0, 0x10);
    }

    *(int16_t *)(effect + 0xe2) -= (int16_t)game_time_get_elapsed();
    matrix4x3_multiply((float *)mout, (float *)shake_matrix, (float *)mout);
  }
}

/* player_effect_update_camera_impulse (0xa3890) */
void player_effect_update_camera_impulse(int unit_index, float *rumble_def, void *direction, float damage_amount, float scale, float *effect /* @<eax> */)
{
  vector3_t dir_norm;
  vector3_t facing;
  vector3_t perp;
  float *facing_angles;
  float scale_ticks;
  float scaled_intensity;
  float current_timer;
  float angle;
  float impulse_rot;
  vector3_t impulse_vec;

  game_time_get();
  scale_ticks = scale * 30.0f;
  scaled_intensity = (1.0f - rumble_def[6]) * damage_amount + rumble_def[6];
  current_timer = (float)*(int16_t *)((char *)effect + 0xe0);

  if ((current_timer < effect[0x14]) || (effect[0x1a] < scaled_intensity) ||
      (effect[0x1a] <= scaled_intensity && (current_timer < scale_ticks * rumble_def[0]))) {
    dir_norm = *(vector3_t *)direction;
    dir_norm.z = 0.0f;
    normalize3d((float *)&dir_norm);

    facing_angles = (float *)player_control_get_facing_angles(unit_index);
    angles_to_vector((float *)&facing, facing_angles);
    facing.z = 0.0f;
    normalize3d((float *)&facing);

    angle = ((float (*)(float *, float *))signed_angle_between_vectors2d)((float *)&facing, (float *)&dir_norm);

    csmemcpy((char *)effect + 0x50, rumble_def, 0x34);
    effect[0x14] *= scale_ticks;
    effect[0x1a] = scaled_intensity;
    *(int16_t *)((char *)effect + 0xe0) = (int16_t)effect[0x14];

    effect[2] = 0.0f;
    effect[0] = x87_fcos(angle);
    effect[1] = x87_fsin(angle);

    impulse_rot = random_real_range((int *)random_math_get_local_seed_address(), 0.0f, 6.2831853f);
    cross_product3d((float *)0x31fc44, (float *)effect, (float *)&perp);
    normalize3d((float *)&perp);

    rotate_vector3d_by_sincos((float *)&perp, (float *)effect, x87_fsin(impulse_rot), x87_fcos(impulse_rot));
    perp.x *= rumble_def[7];
    perp.y *= rumble_def[7];
    perp.z *= rumble_def[7];
    *(vector3_t *)((char *)effect + 0x5c) = perp;
    *(uint8_t *)((char *)effect + 0xe8) |= 2;
  }

  scaled_intensity = (1.0f - rumble_def[9]) * damage_amount + rumble_def[9];
  player_control_get_facing_direction(unit_index, (float *)&facing);
  cross_product3d((float *)&facing, (float *)direction, (float *)&impulse_vec);
  impulse_vec.x *= rumble_def[8] * scaled_intensity;
  impulse_vec.y *= rumble_def[8] * scaled_intensity;
  impulse_vec.z *= rumble_def[8] * scaled_intensity;
  player_control_permanent_impulse(unit_index, (float *)&impulse_vec);
}

/* player_effect_apply_damage (0xa3b80) — Apply damage-related effects to a
 * player.
 *
 * Uses the damage effect tag (jpt!) to set screen shake, vibration, and
 * directional damage indicators based on the angle of incoming damage
 * relative to the player's camera orientation.
 *
 * Confirmed: datum_get(*(data_t**)0x5aa6d4, player_handle) for player data.
 * Confirmed: assert_halt on direction != NULL.
 * Confirmed: lock_random_seed / unlock_random_seed bracket the entire function.
 * Confirmed: tag_get('jpt!', *damage_params) for tag lookup.
 * Confirmed: player_effect_set_from_descriptor(sVar1, effect, param_4, 1.0f,
 * jpt+0x24). Confirmed: *(unsigned int*)(player+0x1c8) & 0x100 checks vehicle
 * driver flag. Confirmed: Global floats: 0x2533c0=0.0f, 0x2533c8=1.0f,
 * 0x25fea8=~0.0, 0x254a58=~0.7854 (PI/4), 0x26af48=~2.3562 (3*PI/4),
 * 0x2568bc=~1.5708 (PI/2). Confirmed: local_player_get_player_index called
 * twice (original binary artifact). Confirmed: camera+0x20 is forward vector,
 * +0x2c is up vector. Confirmed: effect flags at +0xe4 (right), +0xe5
 * (forward), +0xe6 (down), +0xe7 (side).
 */
void player_effect_start(int player_handle, void *damage_params,
                         void *direction, float damage_amount, float scale)
{
  char *player;
  int16_t unit_index;
  char *jpt_tag;
  char *effect;
  int driver_handle;
  int driver_type_valid;
  int damage_type_valid;
  void *camera;
  float attacker_pos[3];
  vector3_t victim_pos;
  float delta[3];
  float rotated_delta[3];
  float length;
  float angle;

  player = (char *)datum_get(*(data_t **)0x5aa6d4, player_handle);
  unit_index = *(int16_t *)(player + 2);

  if ((int)direction == 0) {
    assert_halt(0);
  }

  lock_global_random_seed();

  if (unit_index != -1) {
    jpt_tag = (char *)tag_get(0x6a707421, *(int *)damage_params);
    effect = player_effect_get(unit_index);

    player_effect_set_from_descriptor(unit_index, effect, damage_amount, 1.0f,
                                      (void *)(jpt_tag + 0x24));
    player_effect_update_camera_impulse(unit_index, (float *)(jpt_tag + 0x98),
                                        direction, damage_amount, 1.0f,
                                        (float *)effect /* @<eax> */);
    player_effect_update_camera_shake(unit_index, damage_amount, 1.0f,
                                      (float *)(jpt_tag + 0xcc) /* @<eax> */,
                                      (void *)effect /* @<ebx> */);
    rumble_player_impulse((short)unit_index, (float *)(jpt_tag + 0x5c),
                          damage_amount, 1.0f);

    if (*(int *)(jpt_tag + 0x120) != -1) {
      sound_impulse_start(*(int *)(jpt_tag + 0x120), 1.0f);
    }

    if ((*(float *)0x2533c0 < scale) &&
        (*(int *)((char *)damage_params + 0xc) != -1)) {
      if ((*(unsigned int *)(jpt_tag + 0x1c8) & 0x100) != 0) {
        *(unsigned char *)(effect + 0xe6) = 1;
        unlock_global_random_seed();
        return;
      }

      driver_handle = local_player_get_player_index(unit_index);
      if (driver_handle == -1) {
        driver_handle = -1;
      } else {
        driver_handle = local_player_get_player_index(unit_index);
        player = (char *)datum_get(*(data_t **)0x5aa6d4, driver_handle);
        driver_handle = *(int *)(player + 0x34);
      }

      driver_type_valid =
        (int)object_try_and_get_and_verify_type(driver_handle, 3) != 0;
      damage_type_valid = (int)object_try_and_get_and_verify_type(
                            *(int *)((char *)damage_params + 0xc), -1) != 0;

      if (driver_type_valid && damage_type_valid) {
        camera = observer_get_camera(unit_index);
        if (camera != (void *)0) {
          unit_get_head_position(driver_handle, attacker_pos);
          object_get_world_position(*(int *)((char *)damage_params + 0xc),
                                    &victim_pos);

          delta[0] = victim_pos.x - attacker_pos[0];
          delta[1] = victim_pos.y - attacker_pos[1];
          delta[2] = victim_pos.z - attacker_pos[2];

          cross_product3d((float *)((char *)camera + 0x20),
                          (float *)((char *)camera + 0x2c), attacker_pos);

          rotated_delta[0] = attacker_pos[0] * delta[0] +
                             attacker_pos[1] * delta[1] +
                             attacker_pos[2] * delta[2];
          rotated_delta[1] = delta[0] * *(float *)((char *)camera + 0x20) +
                             delta[1] * *(float *)((char *)camera + 0x24) +
                             delta[2] * *(float *)((char *)camera + 0x28);
          rotated_delta[2] = delta[0] * *(float *)((char *)camera + 0x2c) +
                             delta[1] * *(float *)((char *)camera + 0x30) +
                             delta[2] * *(float *)((char *)camera + 0x34);

          length = normalize3d(rotated_delta);
          if (length != 0.0f) {
            if ((0.0f < fabsf(rotated_delta[2]))) {
              if (rotated_delta[2] <= 0.0f) {
                *(unsigned char *)(effect + 0xe6) = 1;
              } else {
                *(unsigned char *)(effect + 0xe4) = 1;
              }
            }

            angle = (float)atan2(rotated_delta[1], rotated_delta[0]);
            if ((angle < *(float *)0x254a58) || (*(float *)0x26af48 < angle)) {
              if ((*(float *)0x2568bc < fabsf(angle))) {
                *(unsigned char *)(effect + 0xe5) = 1;
                unlock_global_random_seed();
                return;
              }
              *(unsigned char *)(effect + 0xe7) = 1;
            }
          }
        }
      }
    }
  }

  unlock_global_random_seed();
}
