#include "x87_math.h"
/* Refresh every local player's HUD weapon state (0xda980).
 * Source: c:\halo\SOURCE\interface\hud_weapon.c line 0xd4.
 * Stack-guard idiom: 0x200-byte 0x62 fill plus a return-address canary
 * (get_return_eip), both asserted after the per-player loop. */
void hud_update_weapon(void)
{
  int guard[128];
  unsigned char weapon_state[32];
  int empty_state[8];
  int return_addr;
  int unit_handle;
  int weapon_handle;
  int update_handle;
  int update_tag_index;
  void *update_state;
  void *player;
  void *unit;
  void *other_unit;
  void *weapon_tag;
  unsigned char *weapon_entry;
  void *hud_weapon_state;
  volatile int *p;
  int n;
  short local_player_index;
  short i;
  short corrupt_index;

  return_addr = get_return_eip();
  csmemset(guard, 0x62, 0x200);

  local_player_index = local_player_get_next(-1);
  while (local_player_index != -1) {
    if (local_player_get_player_index(local_player_index) == -1) {
      goto next_player;
    }
    player = datum_get(*(data_t **)0x5aa6d4,
                       local_player_get_player_index(local_player_index));
    unit_handle = *(int *)((char *)player + 0x34);
    if (unit_handle == -1) {
      goto next_player;
    }

    unit = object_get_and_verify_type(unit_handle, 3);
    weapon_handle = unit_inventory_get_weapon(
      unit_handle, *(unsigned short *)((char *)unit + 0x2a2));
    if (weapon_handle != -1) {
      goto have_weapon;
    }

    /* No weapon in hand: try the unit this one is riding (parent at +0xcc)
     * when its seat (+0x2a0) is flagged 0x8 in the unit tag's seat block. */
    unit = object_get_and_verify_type(unit_handle, 3);
    if (*(int *)((char *)unit + 0xcc) != -1 &&
        *(short *)((char *)unit + 0x2a0) != -1) {
      weapon_entry = (unsigned char *)tag_block_get_element(
        (char *)tag_get(0x756e6974, *(int *)object_get_and_verify_type(
                                      *(int *)((char *)unit + 0xcc), 3)) +
          0x2e4,
        (int)*(short *)((char *)unit + 0x2a0), 0x11c);
      if ((*weapon_entry & 8) == 0) {
        goto store_weapon;
      }
      other_unit = object_get_and_verify_type(*(int *)((char *)unit + 0xcc), 3);
      weapon_handle = unit_inventory_get_weapon(
        *(int *)((char *)unit + 0xcc),
        *(unsigned short *)((char *)other_unit + 0x2a2));
      if (weapon_handle != -1) {
        goto have_weapon;
      }
    }

    if (unit_count_weapons(unit_handle) != 0) {
      goto store_weapon;
    }
    empty_state[0] = 0;
    p = (volatile int *)empty_state + 1;
    for (n = 7; n != 0; n--) {
      *p = 0;
      p++;
    }
    update_handle = -1;
    update_tag_index = *(int *)((char *)*(void **)0x46bd0c + 0x2cc);
    update_state = empty_state;
    goto update_hud;

  have_weapon:
    weapon_tag =
      tag_get(0x77656170, *(int *)object_get_and_verify_type(weapon_handle, 4));
    weapon_build_weapon_interface_state(weapon_handle, (int)weapon_state);
    if (*(int *)((char *)weapon_tag + 0x48c) == -1) {
      goto store_weapon;
    }
    update_handle = weapon_handle;
    update_tag_index = *(int *)((char *)weapon_tag + 0x48c);
    update_state = weapon_state;

  update_hud:
    FUN_000d9960(local_player_index, update_handle, update_tag_index,
                 update_state);

  store_weapon:
    hud_weapon_state = get_hud_state(local_player_index);
    *(int *)((char *)hud_weapon_state + 0x20) = weapon_handle;

  next_player:
    local_player_index = local_player_get_next(local_player_index);
  }

  corrupt_index = -1;
  for (i = 0x7f; i >= 0; i--) {
    if (guard[(int)i] != 0x62626262) {
      corrupt_index = i;
      break;
    }
  }

  if (get_return_eip() != return_addr) {
    display_assert("corrupt return address!",
                   "c:\\halo\\SOURCE\\interface\\hud_weapon.c", 0xd4, 1);
    system_exit(-1);
  }

  if (corrupt_index != -1) {
    display_assert(
      csprintf((char *)0x5ab100, "corrupt stack at %d!", (int)corrupt_index),
      "c:\\halo\\SOURCE\\interface\\hud_weapon.c", 0xd4, 1);
    system_exit(-1);
  }
}

/* Draw one player's weapon HUD (0xdabf0).  param_1 is the player datum
 * (+0x02 local player index, +0x34 unit handle).
 * Source: c:\halo\SOURCE\interface\hud_weapon.c line 0x1d8 (assert).
 * Shape notes (binary-derived):
 *   - the seat-flag test (seat element byte & 8 clear) latches a local
 *     flag that suppresses the unit_count_weapons fallback;
 *   - the player/render local-player assert fires AFTER the weapon lookup;
 *   - 0x000dad5c pushes the interface-state buffer as an extra stack arg to
 *     play_weapon_hud_sounds, whose code (0x000d8ca0) reads only EAX/ESI;
 *     the kb decl carries no stack param, so the push is not reproduced. */
void FUN_000dabf0(int param_1)
{
  int interface_state[8];
  void *unit;
  void *other_unit;
  void *weapon_tag;
  unsigned char *seat_entry;
  int weapon_handle;
  int whud_index;
  int n;
  int *p;
  short local_player_index;
  char seat_blocks_weapon;

  unit = object_get_and_verify_type(*(int *)(param_1 + 0x34), 3);
  weapon_handle = unit_inventory_get_weapon(
    *(int *)(param_1 + 0x34), *(unsigned short *)((char *)unit + 0x2a2));
  seat_blocks_weapon = 0;
  if (weapon_handle == -1) {
    unit = object_get_and_verify_type(*(int *)(param_1 + 0x34), 3);
    if (*(int *)((char *)unit + 0xcc) != -1 &&
        *(short *)((char *)unit + 0x2a0) != -1) {
      seat_entry = (unsigned char *)tag_block_get_element(
        (char *)tag_get(0x756e6974, *(int *)object_get_and_verify_type(
                                      *(int *)((char *)unit + 0xcc), 3)) +
          0x2e4,
        (int)*(short *)((char *)unit + 0x2a0), 0x11c);
      if ((*seat_entry & 8) == 0) {
        seat_blocks_weapon = 1;
      } else {
        other_unit =
          object_get_and_verify_type(*(int *)((char *)unit + 0xcc), 3);
        weapon_handle = unit_inventory_get_weapon(
          *(int *)((char *)unit + 0xcc),
          *(unsigned short *)((char *)other_unit + 0x2a2));
      }
    }
  }

  if (*(short *)(param_1 + 2) != *(int16_t *)0x506548) {
    display_assert("player->local_player_index==render.local_player_index",
                   "c:\\halo\\SOURCE\\interface\\hud_weapon.c", 0x1d8, 1);
    system_exit(-1);
  }

  if (weapon_handle != -1) {
    weapon_tag =
      tag_get(0x77656170, *(int *)object_get_and_verify_type(weapon_handle, 4));
    weapon_build_weapon_interface_state(weapon_handle, (int)interface_state);
    whud_index = *(int *)((char *)weapon_tag + 0x48c);
    if (whud_index != -1) {
      crosshairs_draw(whud_index, (int *)param_1, weapon_handle,
                      (int)interface_state);
      render_weapon_hud(whud_index, *(unsigned short *)(param_1 + 2),
                        weapon_tag, interface_state, 0, 0, 0);
      play_weapon_hud_sounds(whud_index, *(short *)(param_1 + 2));
    }
  } else if (!seat_blocks_weapon &&
             unit_count_weapons(*(int *)(param_1 + 0x34)) == 0) {
    interface_state[0] = 0;
    p = interface_state + 1;
    for (n = 7; n != 0; n--) {
      *p = 0;
      p++;
    }
    crosshairs_draw(*(int *)((char *)*(void **)0x46bd0c + 0x2cc),
                    (int *)param_1, -1, (int)interface_state);
  }

  render_grenade_hud(*(unsigned short *)(param_1 + 2),
                     *(int *)(param_1 + 0x34));
  local_player_index = *(short *)(param_1 + 2);
  if (local_player_index != -1) {
    *(int *)((char *)get_hud_state(local_player_index) + 0x20) = weapon_handle;
  }
}

/* tiny_point2d_set (0xdade0) -- motion_sensor.c lines 0x69/0x6a.
 * Register-only ABI (binary: ESI and EDI read without being set):
 *   position@<esi>   float[2], asserted against hud_globals+0x2d0
 *                    ("hud_globals->defaults.motion_sensor_range");
 *   tiny_point@<edi> 2 output bytes, each (x / range) * 127.0f via _ftol2.
 * The 0x46bd0c global is re-read before every use, as in the binary. */
void tiny_point2d_set(float *position, char *tiny_point)
{
  if (!(fabs(position[0]) < *(float *)((char *)*(void **)0x46bd0c + 0x2d0))) {
    display_assert(
      "fabs(position->x) < hud_globals->defaults.motion_sensor_range",
      "c:\\halo\\SOURCE\\interface\\motion_sensor.c", 0x69, 1);
    system_exit(-1);
  }
  if (!(fabs(position[1]) < *(float *)((char *)*(void **)0x46bd0c + 0x2d0))) {
    display_assert(
      "fabs(position->y) < hud_globals->defaults.motion_sensor_range",
      "c:\\halo\\SOURCE\\interface\\motion_sensor.c", 0x6a, 1);
    system_exit(-1);
  }
  tiny_point[0] =
    (char)(int)(position[0] / *(float *)((char *)*(void **)0x46bd0c + 0x2d0) *
                127.0f);
  tiny_point[1] =
    (char)(int)(position[1] / *(float *)((char *)*(void **)0x46bd0c + 0x2d0) *
                127.0f);
}

/* tiny_point2d_get (0xdae90) -- inverse of tiny_point2d_set.
 * Register-only ABI (binary: ECX and EAX read without being set):
 *   tiny_point@<ecx> 2 signed input bytes (MOVSX);
 *   position@<eax>   float[2] output, each byte * range * (1/127) where range
 *                    is hud_globals+0x2d0 and 1/127 is the float at 0x2820c0.
 * The 0x46bd0c global is re-read for each component, as in the binary. */
void tiny_point2d_get(char *tiny_point, float *position)
{
  position[0] = (float)(int)tiny_point[0] *
                *(float *)((char *)*(void **)0x46bd0c + 0x2d0) *
                *(float *)0x2820c0;
  position[1] = (float)(int)tiny_point[1] *
                *(float *)((char *)*(void **)0x46bd0c + 0x2d0) *
                *(float *)0x2820c0;
}

/* Classify a unit relative to a local player, for the motion sensor / event
 * display (0xdaee0).  Takes local_player_index in @<ebx> and the unit object
 * handle in @<esi>; the result byte is returned in AL.
 *
 * Observed result codes (meanings inferred from the branches, names unknown):
 *   5  unit_handle == -1
 *   0  the unit belongs to this same local player
 *   2  the object is not a unit (object_try_and_get_and_verify_type(,3) NULL)
 *   1/2  biped:   game_allegiance_get_team_is_friendly(...) + 1
 *   3/4  vehicle: game_allegiance_get_team_is_friendly(...) + 3
 *   3/4  empty vehicle: 4 when the unit tag's second block element names
 *        "c_dropship", else 3
 * The AL width leaves char vs unsigned char undecidable here; char is used.
 *
 * Shape notes (binary-derived, do not "simplify"):
 *   - the local player's team at player+0x20 is read BEFORE the
 *     unit_handle == -1 early return;
 *   - player_index_from_unit_index is called twice (000daf0b, 000daf1d);
 *   - the occupant branch re-fetches the unit with a fresh
 *     object_get_and_verify_type(occupant_handle, 3) instead of reusing `unit`;
 *   - the biped branch re-derives the local player's team with a second
 *     local_player_get_player_index/datum_get pair rather than reusing the
 *     [EBP-4] copy. */
char FUN_000daee0(int local_player_index, int unit_handle)
{
  int local_team;
  int unit_player_index;
  void *player;
  void *unit;
  void *vehicle;
  int occupant_handle;
  void *occupant;
  void *unit_tag;
  void *seat;

  player =
    datum_get(player_data, local_player_get_player_index(local_player_index));
  local_team = *(int *)((char *)player + 0x20);
  if (unit_handle == -1) {
    return 5;
  }
  if (player_index_from_unit_index(unit_handle) == -1) {
    unit_player_index = -1;
  } else {
    unit_player_index =
      *(int16_t *)((char *)datum_get(
                     player_data, player_index_from_unit_index(unit_handle)) +
                   2);
  }
  if (unit_player_index == local_player_index) {
    return 0;
  }
  if (object_try_and_get_and_verify_type(unit_handle, 3) == 0) {
    return 2;
  }
  unit = object_get_and_verify_type(unit_handle, 3);
  if (object_try_and_get_and_verify_type(unit_handle, 2) != 0) {
    vehicle = object_get_and_verify_type(unit_handle, 2);
    occupant_handle = *(int *)((char *)vehicle + 0x2d8);
    if (occupant_handle != -1) {
      occupant = object_get_and_verify_type(occupant_handle, 3);
      return (char)(game_allegiance_get_team_is_friendly(
                      *(uint16_t *)((char *)occupant + 0x68), local_team) +
                    3);
    }
    {
      occupant_handle = *(int *)((char *)vehicle + 0x2d4);
      if (occupant_handle == -1) {
        unit_tag = tag_get(0x756e6974, *(int *)vehicle);
        if (*(int *)((char *)unit_tag + 0x2e4) > 1) {
          seat = tag_block_get_element((char *)unit_tag + 0x2e4, 0, 0x11c);
          if (csstrncmp((char *)seat + 4, "c_dropship", 10) == 0) {
            return 4;
          }
        }
        return 3;
      }
    }
    occupant = object_get_and_verify_type(occupant_handle, 3);
    return (char)(game_allegiance_get_team_is_friendly(
                    *(uint16_t *)((char *)occupant + 0x68), local_team) +
                  3);
  }
  player =
    datum_get(player_data, local_player_get_player_index(local_player_index));
  local_team = *(int *)((char *)player + 0x20);
  return (char)(game_allegiance_get_team_is_friendly(
                  *(uint16_t *)((char *)unit + 0x68), local_team) +
                1);
}


/* Per-local-player motion sensor state accessor (0xdb0b0).
 * Takes local_player_index in @<si>; the state block allocated by
 * motion_sensor_initialize holds 4 records of 0x568 bytes (0x15a8 total).
 * Source: c:\halo\SOURCE\interface\motion_sensor.c line 0x11f. */
void *FUN_000db0b0(short local_player_index)
{
  if (local_player_index < 0 || local_player_index >= 4) {
    display_assert("local_player_index>=0 && "
                   "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                   "c:\\halo\\SOURCE\\interface\\motion_sensor.c", 0x11f, 1);
    system_exit(-1);
  }
  return (void *)((char *)*(void **)0x46bd2c + local_player_index * 0x568);
}

/* Allocate the motion sensor (radar) game state block (0xdb0f0). */
void motion_sensor_initialize(void)
{
  *(void **)0x46bd2c =
    game_state_malloc("motion sensor (radar)", "sensor data", 0x15a8);
  if (*(void **)0x46bd2c == 0) {
    display_assert("motion_sensor_globals",
                   "c:\\halo\\SOURCE\\interface\\motion_sensor.c", 0x12a, 1);
    system_exit(-1);
  }
}

/* (0xdb140) */
void FUN_000db140(void)
{
}

/* Clear the motion sensor state and pre-initialize slot tables (0xdb150). */
void FUN_000db150(void)
{
  unsigned char *p;
  unsigned char *row;
  unsigned char *cell;
  int i;
  int j;
  int k;

  csmemset(*(void **)0x46bd2c, 0, 0x15a8);
  p = (unsigned char *)*(void **)0x46bd2c + 2;
  i = 4;
  do {
    j = 10;
    row = p;
    do {
      k = 0x10;
      cell = row;
      do {
        *cell = 6;
        cell += 4;
        k--;
      } while (k != 0);
      row += 0x84;
      j--;
    } while (j != 0);
    p += 0x568;
    i--;
  } while (i != 0);
}

/* (0xdb1b0) */
void FUN_000db1b0(void)
{
}

/* Install a motion sensor reference point and overlay scale, then rebuild the
 * radar overlay via the thunk at 0x17d050 (0xdb1e0).
 * The assert text names the @<esi> parameter "reference"; the meaning of the
 * remaining three stack parameters is unproven (param_2 is never read here,
 * it exists only because the word at [EBP+0x10] is).
 * Source: c:\halo\SOURCE\interface\motion_sensor.c line 0x349. */
void FUN_000db1e0(int *reference, int param_2, bool param_3, short param_4)
{
  if (reference == NULL) {
    display_assert("reference", "c:\\halo\\SOURCE\\interface\\motion_sensor.c",
                   0x349, 1);
    system_exit(-1);
  }
  *(short *)0x5aa676 = param_4;
  *(float *)0x2f66f4 = 0.75f;
  if (!param_3)
    *(float *)0x2f66f4 = 1.0f;
  *(int *)0x5aa680 = reference[0];
  *(int *)0x5aa684 = reference[1];
  FUN_0017d050();
}

/* Per-tick motion sensor (radar) blip collection (0xdb4c0).
 * Advances the 10-entry history ring index at globals+0x15a4 and stores the
 * game time at +0x15a0.  Except on every 15th tick (or tick 0) each local
 * player's previous history record (0x84 bytes) is copied into the current
 * one; on a refresh tick every candidate object from the type-3 iterator that
 * passes FUN_000db250 is binned into up to 0x10 slots per local player.
 * The z of the object's bounding-sphere center is replaced by the player's z
 * before the range test, as in the binary (FLD [pos+8] / FSTP [center+8]).
 * The byte at slot*4+3 is the unit tag's int16 at +0x298 when in [0,3),
 * else 0; its meaning is unproven.
 * Stack-guard idiom: 0x200-byte 0x62 fill plus a return-address canary.
 * Source: c:\halo\SOURCE\interface\motion_sensor.c line 0x282. */
void motion_sensor_update(void)
{
  int guard[128];
  float positions[4][3];
  short players[4];
  float center[3];
  int return_addr;
  int iter[4];
  short counts[4];
  float radius;
  char *globals;
  char *player_record;
  char *record;
  void *object;
  int game_time;
  int current;
  int previous;
  int unit_handle;
  int skipped;
  float dx;
  float dy;
  float dz;
  float range;
  short count;
  short local_player_index;
  short slot;
  short tag_value;
  short i;
  short corrupt_index;
  unsigned char value;
  char done;

  return_addr = get_return_eip();
  csmemset(guard, 0x62, 0x200);
  game_engine_running();
  game_time = game_time_get();
  globals = *(char **)0x46bd2c;
  globals[0x15a6] = 1;
  done = 0;
  *(short *)(globals + 0x15a4) = (short)(*(short *)(globals + 0x15a4) + 1) % 10;
  *(int *)(globals + 0x15a0) = game_time;

  if (game_time % 15 != 0 && game_time != 0) {
    current = *(short *)(globals + 0x15a4);
    previous = (current + 9) % 10;
    count = local_player_count();
    local_player_index = local_player_get_next(-1);
    for (i = 0; i < count; i++) {
      globals = *(char **)0x46bd2c;
      csmemcpy(globals + local_player_index * 0x568 + current * 0x84,
               globals + local_player_index * 0x568 + (short)previous * 0x84,
               0x84);
      local_player_index = local_player_get_next(local_player_index);
    }
  } else {
    count = local_player_count();
    counts[0] = 0;
    counts[1] = 0;
    counts[2] = 0;
    counts[3] = 0;
    local_player_index = local_player_get_next(-1);
    for (i = 0; i < count; i++) {
      globals = *(char **)0x46bd2c;
      record = globals + local_player_index * 0x568 +
               *(short *)(globals + 0x15a4) * 0x84;
      unit_handle =
        local_player_get_player_index(local_player_index) == -1 ?
          -1 :
          *(int *)((char *)datum_get(player_data, local_player_get_player_index(
                                                    local_player_index)) +
                   0x34);
      players[i] = local_player_index;
      if (unit_handle != -1) {
        unit_set_seat_state(unit_handle, positions[local_player_index]);
      }
      *(int *)(record + 0x78) = 0;
      for (slot = 0; slot < 0x10; slot++) {
        record[2 + slot * 4] = 6;
      }
      local_player_index = local_player_get_next(local_player_index);
    }

    object_iterator_new(iter, 3, 1);
    while (object_iterator_next(iter) != 0 && !done) {
      object = object_try_and_get_and_verify_type(iter[2], 3);
      if (object == 0 || (*((unsigned char *)object + 0xb6) & 4) != 0 ||
          !FUN_000db250(iter[2])) {
        continue;
      }
      skipped = 0;
      object_get_bounding_sphere(iter[2], center, &radius);
      for (i = 0; i < count; i++) {
        local_player_index = players[i];
        slot = counts[local_player_index];
        if (slot >= 0x10) {
          skipped++;
          continue;
        }
        center[2] = positions[local_player_index][2];
        if (!game_engine_running()) {
          dx = center[0] - positions[local_player_index][0];
          dy = center[1] - positions[local_player_index][1];
          dz = center[2] - positions[local_player_index][2];
          range = *(float *)(*(char **)0x46bd0c + 0x2d0);
          if (range * range < dz * dz + dy * dy + dx * dx) {
            continue;
          }
        }
        globals = *(char **)0x46bd2c;
        player_record = globals + local_player_index * 0x568;
        record = player_record + *(short *)(globals + 0x15a4) * 0x84;
        unit_handle = iter[2];
        record[slot * 4 + 2] = FUN_000daee0(local_player_index, unit_handle);
        value = 0;
        if (unit_handle != -1 &&
            object_try_and_get_and_verify_type(unit_handle, 3) != 0) {
          tag_value =
            *(short *)((char *)tag_get(
                         0x756e6974,
                         *(int *)object_get_and_verify_type(unit_handle, 3)) +
                       0x298);
          if (tag_value >= 0 && tag_value < 3) {
            value = (unsigned char)tag_value;
          }
        }
        record[slot * 4 + 3] = value;
        *(int *)(player_record + slot * 4 + 0x528) = iter[2];
        counts[local_player_index]++;
        (*(int *)(record + 0x78))++;
      }
      if (skipped == count) {
        done = 1;
      }
    }
  }

  corrupt_index = -1;
  for (i = 0x7f; i >= 0; i--) {
    if (guard[(int)i] != 0x62626262) {
      corrupt_index = i;
      break;
    }
  }

  if (get_return_eip() != return_addr) {
    display_assert("corrupt return address!",
                   "c:\\halo\\SOURCE\\interface\\motion_sensor.c", 0x282, 1);
    system_exit(-1);
  }

  if (corrupt_index != -1) {
    display_assert(
      csprintf((char *)0x5ab100, "corrupt stack at %d!", (int)corrupt_index),
      "c:\\halo\\SOURCE\\interface\\motion_sensor.c", 0x282, 1);
    system_exit(-1);
  }
}

/* Update and draw one local player's motion sensor (radar) for a screen point
 * (0xdbfb0).
 * param_1 is compared as an int16 against NONE (-1) but forwarded as the full
 * dword: MOV ESI,[EBP+8] / CMP SI,-1 / PUSH ESI / MOV ECX,ESI.
 * param_3 is the "pt" the assert names -- FUN_000dbcb0 reads it through
 * @<eax> as two int16s, so it is a point2d; the meaning of param_2 is
 * unproven, it is only forwarded on the stack and FUN_000dbcb0 never reads
 * that slot.
 * Source: c:\halo\SOURCE\interface\motion_sensor.c line 0x1dc. */
void FUN_000dbfb0(int param_1, int param_2, int param_3)
{
  if (param_3 == 0) {
    display_assert("pt", "c:\\halo\\SOURCE\\interface\\motion_sensor.c", 0x1dc,
                   1);
    system_exit(-1);
  }
  if ((short)param_1 != -1) {
    update_motion_sensor(param_1);
    FUN_000dbcb0((short *)param_3, param_1, param_2);
  }
}

/* (0xdc000) Recompute the motion sensor sweep scalar at 0x46bd30 from the
 * game time, then run motion_sensor_update.
 * Binary: FILD game_time_get() / FMUL [0x2546a4] / FLD qword [0x282180] /
 * CALL 0x1daf7e (_CIfmod, FPREM truncated remainder).  If the remainder is
 * (ordered) below [0x28217c] the scalar is [0x2533c8] / ((r + [0x255d90]) *
 * [0x2f6708]) (FDIVR), otherwise it is 0.4f (0x3ecccccd).  The meaning of the
 * constants and of 0x46bd30 is unproven; 0x46bd30 is later pushed as the
 * float slot 2 of FUN_0017d070 (see rasterizer_sprites.c). */
void FUN_000dc000(void)
{
#if defined(_MSC_VER) && !defined(__clang__)
  double __cdecl fmod(double, double);
#endif
  float remainder;

#if defined(_MSC_VER) && !defined(__clang__)
  remainder = fmod(game_time_get() * *(float *)0x2546a4, *(double *)0x282180);
#else
  remainder =
    x87_fmod(game_time_get() * *(float *)0x2546a4, *(double *)0x282180);
#endif
  if (remainder < *(float *)0x28217c) {
    *(float *)0x46bd30 =
      (float)(*(float *)0x2533c8 /
              ((remainder + *(float *)0x255d90) * *(float *)0x2f6708));
  } else {
    *(float *)0x46bd30 = 0.4f;
  }
  motion_sensor_update();
}

/**
 * Check whether it is time to start the attract-mode tab sequence.
 *
 * Returns true when the main menu has been idle long enough (>0x124f8 ms
 * ~= 75 seconds) with no input events.  As a side-effect, starts or stops
 * title music depending on whether the idle threshold (0x11f1c ms ~= 73 s)
 * has been crossed.
 */
bool event_manager_tab_check(void)
{
  unsigned int now;
  unsigned int last_event;
  bool attract_flag;

  if (cache_files_precache_in_progress()) {
    float progress;
    if (cache_files_precache_map_status(&progress) == 1)
      cache_files_precache_map_end();
  }

  if (main_menu_screen_is_active() && !cache_files_precache_in_progress() &&
      !network_game_in_progress() && !bink_playback_active()) {
    now = system_milliseconds();
    last_event = event_manager_get_last_event_time();
    if (*(unsigned int *)0x46bd38 > last_event)
      last_event = *(unsigned int *)0x46bd38;
    attract_flag = ui_main_menu_music_active();
    if (now - last_event >= 0x11f1c) {
      if (attract_flag)
        ui_stop_main_menu_music();
    } else {
      if (!attract_flag)
        ui_start_main_menu_music();
    }
    if (now - last_event >= 0x124f8)
      return true;
  }
  return false;
}

/**
 * Stop attract mode and all sounds, then play the credits Bink video.
 */
void FUN_000dc110(void)
{
  ui_stop_main_menu_music();
  sound_stop_all();
  bink_playback_start("d:\\bink\\credits.bik", 0x2e);
}

/**
 * Record the current time as the "mark" timestamp, used by
 * event_manager_tab_check to measure idle duration.
 */
void event_manager_mark_time(void)
{
  *(unsigned int *)0x46bd38 = system_milliseconds();
}

/**
 * Pick and play a random attract-mode Bink video, ensuring it differs
 * from the previously played one.  Resets the mark-time afterward so
 * the idle clock restarts when the video finishes.
 */
void event_manager_tab_process(void)
{
  const char *attract_files[3];
  int16_t idx;

  attract_files[0] = "d:\\bink\\attract1.bik";
  attract_files[1] = "d:\\bink\\attract2.bik";
  attract_files[2] = "d:\\bink\\attract3.bik";

  do {
    idx = seed_random_range(random_math_get_local_seed_address(), 0, 3);
    if (idx < 0)
      idx = 0;
    else if (idx > 2)
      idx = 2;
  } while (idx == *(int16_t *)0x2f670c);

  *(int16_t *)0x2f670c = idx;
  ui_stop_main_menu_music();
  bink_playback_start(attract_files[idx], 0x2e);

  if (!bink_playback_active())
    *(unsigned int *)0x46bd38 = system_milliseconds();
}

void event_manager_initialize(void)
{
  csmemset(event_manager_globals, 0, 0x108);
  *(_DWORD *)(event_manager_globals + 4) = system_milliseconds();
  event_manager_globals[0] = 1;
}

void event_manager_dispose(void)
{
  csmemset(event_manager_globals, 0, 0x108);
}

/**
 * Zero out the 0x100-byte event ring buffer, discarding all queued events.
 */
void event_manager_flush(void)
{
  csmemset((void *)0x46bd48, 0, 0x100);
}

/**
 * Set or clear the event suppression flag.  While suppressed,
 * event_manager_dispatch ignores all incoming events.
 */
void event_manager_suppress(int suppress)
{
  *(char *)0x46bd41 = (char)suppress;
}

/**
 * Retrieve the next queued event for the given local player (or any
 * player if player_index == NONE / -1).  Scans the per-player event
 * ring from newest to oldest, copies the first non-empty slot into
 * event_data, clears that slot, and returns true.  Returns false when
 * no events remain.
 */
bool event_manager_get_next_event(void *event_data, int16_t player_index)
{
  int i;
  int16_t pi;
  int16_t *slot;

  assert_halt(event_data &&
              ((player_index >= 0 && player_index < MAXIMUM_GAMEPADS) ||
               player_index == NONE));

  if (!event_manager_globals[0])
    return false;

  if (player_index == NONE) {
    for (pi = 0; pi < 4; pi++) {
      if (event_manager_get_next_event(event_data, pi))
        return true;
    }
    return false;
  }

  /* scan from slot 7 (newest) down to slot 0 (oldest) */
  slot = (int16_t *)(0x46bd80 + (int)player_index * 0x40);
  for (i = 7; i >= 0; i--) {
    if (*slot != 0) {
      int idx = i + (int)player_index * 8;
      *(int *)event_data = *(int *)(0x46bd48 + idx * 8);
      *((int *)event_data + 1) = *(int *)(0x46bd4c + idx * 8);
      *(int16_t *)(0x46bd48 + idx * 8) = 0;
      return true;
    }
    slot -= 4;
  }
  return false;
}

/**
 * Return the timestamp of the last non-empty event dispatched.
 */
unsigned int event_manager_get_last_event_time(void)
{
  return *(unsigned int *)0x46bd44;
}

void event_manager_dispatch(int16_t *event, int16_t player_index)
{
  bool dispatch;
  int now;
  int x, y;
  int ax, ay;
  int pi;

  if (*(char *)0x46bd41)
    return;

  now = system_milliseconds();

  if (event[0] == 1) {
    x = (int)event[2];
    y = (int)event[3];

    ax = x < 0 ? -x : x;
    if (ax < 0x7332) {
      ay = y < 0 ? -y : y;
      if (ay < 0x7332) {
        dispatch = false;
        goto store_stick1;
      }
    }

    ax = x < 0 ? -x : x;
    if (ax >= 0x7332) {
      pi = (int)player_index * 4;
      ay = *(int *)(0x46be68 + pi);
      if (ay < 0)
        ay = -ay;
      if (ay < 0x7332)
        goto record_stick1;
    }

    ay = y < 0 ? -y : y;
    if (ay >= 0x7332) {
      pi = (int)player_index * 4;
      ax = *(int *)(0x46be78 + pi);
      if (ax < 0)
        ax = -ax;
      if (ax < 0x7332)
        goto record_stick1;
    }

    pi = (int)player_index * 4;
    if ((unsigned int)(now - *(int *)(0x46be48 + pi)) < 0xfa) {
      dispatch = false;
      goto store_stick1;
    }

  record_stick1:
    *(int *)(0x46be48 + pi) = now;
    dispatch = true;

    ax = x < 0 ? -x : x;
    if (ax >= 0x7332) {
      if (x >= 0) {
        event[2] = 0x7fff;
        x = 0x7fff;
      } else {
        event[2] = (int16_t)0x8000;
        x = (int)(int16_t)0x8000;
      }
    }

    ay = y < 0 ? -y : y;
    if (ay >= 0x7332) {
      if (y >= 0) {
        event[3] = 0x7fff;
        y = 0x7fff;
      } else {
        event[3] = (int16_t)0x8000;
        y = (int)(int16_t)0x8000;
      }
    }

  store_stick1:
    *(int *)(0x46be68 + (int)player_index * 4) = x;
    *(int *)(0x46be78 + (int)player_index * 4) = y;
  } else if (event[0] == 2) {
    x = (int)event[2];
    y = (int)event[3];

    ax = x < 0 ? -x : x;
    if (ax < 0x7332) {
      ay = y < 0 ? -y : y;
      if (ay < 0x7332) {
        dispatch = false;
        goto store_stick2;
      }
    }

    ax = x < 0 ? -x : x;
    if (ax >= 0x7332) {
      pi = (int)player_index * 4;
      ay = *(int *)(0x46be88 + pi);
      if (ay < 0)
        ay = -ay;
      if (ay < 0x7332)
        goto record_stick2;
    }

    ay = y < 0 ? -y : y;
    if (ay >= 0x7332) {
      pi = (int)player_index * 4;
      ax = *(int *)(0x46be98 + pi);
      if (ax < 0)
        ax = -ax;
      if (ax < 0x7332)
        goto record_stick2;
    }

    pi = (int)player_index * 4;
    if ((unsigned int)(now - *(int *)(0x46be58 + pi)) < 0xfa) {
      dispatch = false;
      goto store_stick2;
    }

  record_stick2:
    *(int *)(0x46be58 + pi) = now;
    dispatch = true;

    ax = x < 0 ? -x : x;
    if (ax >= 0x7332) {
      if (x >= 0) {
        event[2] = 0x7fff;
        x = 0x7fff;
      } else {
        event[2] = (int16_t)0x8000;
        x = (int)(int16_t)0x8000;
      }
    }

    ay = y < 0 ? -y : y;
    if (ay >= 0x7332) {
      if (y >= 0) {
        event[3] = 0x7fff;
        y = 0x7fff;
      } else {
        event[3] = (int16_t)0x8000;
        y = (int)(int16_t)0x8000;
      }
    }

  store_stick2:
    *(int *)(0x46be88 + (int)player_index * 4) = x;
    *(int *)(0x46be98 + (int)player_index * 4) = y;
  } else {
    goto record_event;
  }

  if (!dispatch)
    return;

record_event:
  event[1] = player_index;
  pi = (int)player_index * 0x40;
  csmemmove((void *)(0x46bd48 + pi), (void *)(0x46bd50 + pi), 0x38);
  *(int *)(0x46bd48 + pi) = *(int *)event;
  *(int *)(0x46bd4c + pi) = *(int *)&event[2];
  if (event[0] != 0)
    *(int *)0x46bd44 = now;
}

void event_manager_update(void)
{
  int16_t event[4];
  int16_t empty_event[4];
  char *state;
  int i;
  int16_t j;
  bool had_event;

  if (!event_manager_globals[0])
    return;

  for (i = 0; (int16_t)i < 4; i++) {
    had_event = false;
    if (!input_has_gamepad(i) ||
        (state = (char *)input_get_gamepad_state(i)) == NULL)
      goto send_empty;

    /* left stick */
    if (*(int16_t *)(state + 0x20) != 0 || *(int16_t *)(state + 0x22) != 0) {
      *(int32_t *)&event[2] = *(int32_t *)(state + 0x20);
      event[0] = 1;
      event_manager_dispatch(event, (int16_t)i);
      had_event = true;
    }

    /* right stick */
    if (*(int16_t *)(state + 0x24) != 0 || *(int16_t *)(state + 0x26) != 0) {
      *(int32_t *)&event[2] = *(int32_t *)(state + 0x24);
      event[0] = 2;
      event_manager_dispatch(event, (int16_t)i);
      had_event = true;
    }

    /* buttons (16 digital buttons) */
    for (j = 0; j < 0x10; j++) {
      if (state[0x10 + j] != 0) {
        event[0] = 3;
        ((uint8_t *)&event[2])[0] = (uint8_t)j;
        ((uint8_t *)&event[2])[1] = (uint8_t)state[0x10 + j];
        event_manager_dispatch(event, (int16_t)i);
        had_event = true;
      }
    }

    if (had_event)
      continue;

  send_empty:
    *(int32_t *)&empty_event[1] = 0;
    empty_event[0] = 0;
    empty_event[3] = 0;
    event_manager_dispatch(empty_event, (int16_t)i);
  }
}

/* Wrapper: forward three args to animation_update_internal with update_kind=0
 * (0xdc730). */
void FUN_000dc730(int param_1, short *param_2, int *param_3)
{
  animation_update_internal(0, param_1, param_2, param_3);
}

/* Map a game-event type to a UI-widget event type. */
int16_t FUN_000dc800(int event)
{
  int result;

  switch ((int16_t)event) {
  case 0:
    result = 6;
    break;
  case 1:
    result = 7;
    break;
  case 2:
    result = 8;
    break;
  case 3:
    result = 9;
    break;
  case 4:
    result = 10;
    break;
  case 5:
    result = 11;
    break;
  case 6:
    result = 12;
    break;
  case 9:
    result = 13;
    break;
  case 10:
    result = 14;
    break;
  case 11:
    result = 18;
    break;
  case 12:
    result = 19;
    break;
  case 14:
    result = 4;
    break;
  case 15:
    result = 1;
    break;
  case 17:
    result = 20;
    break;
  case 16:
    result = 23;
    break;
  default:
    result = -1;
    break;
  }

  return (int16_t)result;
}
