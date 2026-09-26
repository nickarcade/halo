enum
{
  NUMBER_OF_GAME_TEAMS = 10
};

enum allegiance_incident_type
{
  _allegiance_incident_accident = 0,
  _allegiance_incident_betrayal,
  _allegiance_incident_forgive,
  NUMBER_OF_ALLEGIANCE_INCIDENT_TYPES
};

#define SET_FLAG(f, b, v) ((v) ? ((f) |= (uint32_t)FLAG(b)) : ((f) &= (uint32_t)~FLAG(b)))
#define BIT_VECTOR_SET_FLAG(bit_vector, bit, enable) (SET_FLAG((bit_vector)[(bit) >> 5], ((bit) & 31), enable))
#define TEST_FLAG(f, b) (((f) & FLAG(b)) != 0)
#define BIT_VECTOR_TEST_FLAG(bit_vector, bit) (TEST_FLAG((bit_vector)[(bit) >> 5], ((bit) & 31)))

struct game_allegiance_record
{
  int16_t team1_index;
  int16_t team2_index;
  int16_t incident_threshold;
  int16_t incident_decay_time;
  bool team1_suspicious;
  bool team2_suspicious;
  bool currently_broken;
  bool status_changed;
  bool requires_communication;
  uint8_t reserved0D;
  int16_t current_incidents;
  int16_t current_incident_decay_time;
};
cs(struct game_allegiance_record, 0x12);

struct game_allegiance_globals
{
  int16_t allegiance_count;
  struct game_allegiance_record allegiances[8];
  uint8_t reserved92[2];
  uint32_t ally_bitvector[4];
  uint32_t friendly_bitvector[4];
};
cs(struct game_allegiance_globals, 0xb4);
typedef struct game_allegiance_globals game_allegiance_globals_type;

void game_allegiance_initialize(void)
{
  game_allegiance_globals =
    (char *)game_state_malloc("game allegiance globals", 0, 0xb4);
  csmemset(game_allegiance_globals, 0, 0xb4);
}

void game_allegiance_dispose(void)
{
}

void game_allegiance_initialize_for_new_map(void)
{
  game_allegiance_globals_type *globals;
  int32_t bit_index;
  int16_t team_index;

  globals = (game_allegiance_globals_type *)game_allegiance_globals;
  if (!globals) {
    display_assert(
      "game_allegiance_globals",
      "c:\\halo\\SOURCE\\game\\game_allegiance.c",
      87,
      true);
    system_exit(-1);
    globals = (game_allegiance_globals_type *)game_allegiance_globals;
  }

  globals->allegiance_count = 0;
  csmemset(
    globals->ally_bitvector,
    0,
    sizeof(globals->ally_bitvector));
  csmemset(
    ((game_allegiance_globals_type *)game_allegiance_globals)->friendly_bitvector,
    0,
    sizeof(((game_allegiance_globals_type *)game_allegiance_globals)->friendly_bitvector));

  globals = (game_allegiance_globals_type *)game_allegiance_globals;
  for (team_index = 0;
       team_index < NUMBER_OF_GAME_TEAMS;
       team_index++) {
    bit_index = NUMBER_OF_GAME_TEAMS * team_index + team_index;
    BIT_VECTOR_SET_FLAG(
      globals->friendly_bitvector,
      bit_index,
      true);
  }
}

void game_allegiance_dispose_from_old_map(void)
{
}

/**
 * Returns whether two teams are friendly (not hostile) to each other.
 *
 * Checks a 10x10 bitfield at game_allegiance_globals+0xa4. Each bit represents
 * a team pair (team_a * 10 + team_b). A SET bit means the teams are NOT
 * friendly; a CLEAR bit means they ARE friendly.
 *
 * Out-of-range team indices (negative or >= 10) return true (friendly).
 */
bool game_allegiance_get_team_is_friendly(int16_t team_a, int16_t team_b)
{
  int bit_index;
  bool result;

  result = true;
  if (team_a >= 0 && team_a < 10 && team_b >= 0 && team_b < 10) {
    bit_index = team_a * 10 + team_b;
    result =
      (*(uint32_t *)(game_allegiance_globals + 0xa4 + (bit_index >> 5) * 4) &
       (1 << (bit_index & 0x1f))) == 0;
  }
  return result;
}

/**
 * Returns whether two teams have had an allegiance incident (natural change).
 *
 * Checks a 10x10 bitfield at game_allegiance_globals+0x94. Each bit represents
 * a team pair (team_a * 10 + team_b). A SET bit means the teams have had an
 * incident; a CLEAR bit means they have not.
 *
 * Out-of-range team indices (negative or >= 10) return false.
 */
bool game_team_is_ally(int16_t team_a, int16_t team_b)
{
  int bit_index;
  bool result;

  result = false;
  if (team_a >= 0 && team_a < 10 && team_b >= 0 && team_b < 10) {
    bit_index = team_a * 10 + team_b;
    result =
      (*(uint32_t *)(game_allegiance_globals + 0x94 + (bit_index >> 5) * 4) &
       (1 << (bit_index & 0x1f))) != 0;
  }
  return result;
}

bool game_team_ally_status_changed(int16_t team_a, int16_t team_b)
{
  struct game_allegiance_record *allegiance;
  int16_t allegiance_count;
  int16_t allegiance_index;
  bool result = false;

  allegiance = ((game_allegiance_globals_type *)game_allegiance_globals)->allegiances;
  allegiance_count = ((game_allegiance_globals_type *)game_allegiance_globals)->allegiance_count;
  for (allegiance_index = 0;
       allegiance_index < allegiance_count;
       allegiance_index++, allegiance++) {
    int16_t team1_index = allegiance->team1_index;

    if ((team1_index == team_a &&
         allegiance->team2_index == team_b) ||
        (allegiance->team2_index == team_a &&
         team1_index == team_b)) {
      result = allegiance->status_changed;
      break;
    }
  }

  return result;
}

int16_t game_allegiance_get_incidents(int16_t team_a, int16_t team_b,
                                      int16_t *out_threshold)
{
  int16_t i;
  int16_t result;
  int16_t threshold;
  int16_t *entry;

  i = 0;
  entry = (int16_t *)game_allegiance_globals + 1;
  result = 0;
  threshold = -1;
  if (*(int16_t *)game_allegiance_globals > 0) {
    do {
      if ((entry[0] == team_a && entry[1] == team_b) ||
          (entry[1] == team_a && entry[0] == team_b)) {
        result = entry[7];
        threshold = entry[2];
        break;
      }
      i++;
      entry += 9;
    } while (i < *(int16_t *)game_allegiance_globals);
  }
  if (out_threshold != NULL) {
    *out_threshold = threshold;
  }
  return result;
}

void game_allegiance_provoke(int16_t team_a, int16_t team_b)
{
  int16_t i;
  int16_t *entry;

  i = 0;
  entry = (int16_t *)game_allegiance_globals + 1;
  if (*(int16_t *)game_allegiance_globals > 0) {
    while (
      (entry[0] != team_a || entry[1] != team_b || *((char *)entry + 9) == 0) &&
      (entry[1] != team_a || entry[0] != team_b || *((char *)entry + 8) == 0)) {
      i++;
      entry += 9;
      /* induction variable on the left: the reference loop tail is
       * `cmp dx, si` + `jl`, i.e. the source wrote `i >= count`. */
      if (i >= *(int16_t *)game_allegiance_globals) {
        return;
      }
    }
    if (entry[7] > 0 && entry[3] != -1) {
      entry[8] = entry[3];
    }
  }
}

void game_allegiance_notify_change(int16_t team_a, int16_t team_b)
{
  int16_t i;
  int16_t *entry;

  entry = (int16_t *)game_allegiance_globals + 1;
  for (i = 0; i < *(int16_t *)game_allegiance_globals; i++, entry += 9) {
    if ((entry[0] == team_a && entry[1] == team_b) ||
        (entry[1] == team_a && entry[0] == team_b)) {
      *((char *)entry + 0xb) = 0;
      break;
    }
  }
}

/**
 * Sets the friendship state between two teams in an allegiance entry.
 *
 * Updates two symmetric 10x10 bitfields in game_allegiance_globals:
 *   +0x94 (incidents): if force==0, sets bits (marks incident); if force!=0,
 *         clears bits (removes incident record).
 *   +0xa4 (hostility): if friendship==0, sets bits (hostile); if friendship!=0,
 *         clears bits (friendly). A set bit means NOT friendly, matching
 *         game_allegiance_get_team_is_friendly which returns (bit == 0).
 *
 * Both bitfields are updated symmetrically for (team_a*10+team_b) and
 * (team_b*10+team_a). After updating, the entry's changed flag is set and
 * game_allegiance_apply_change is called to propagate to AI encounters.
 *
 * Skips the update entirely if force==0 and the friendship value hasn't
 * changed.
 */
void game_allegiance_set(
  int16_t *entry,
  bool currently_broken,
  bool permanently_broken)
{
  struct game_allegiance_record *allegiance =
    (struct game_allegiance_record *)entry;

  if (!permanently_broken &&
      allegiance->currently_broken == currently_broken) {
    return;
  }

  allegiance->currently_broken = currently_broken;
  if (allegiance->team1_index < NUMBER_OF_GAME_TEAMS &&
      allegiance->team2_index < NUMBER_OF_GAME_TEAMS) {
    game_allegiance_globals_type *globals =
      (game_allegiance_globals_type *)game_allegiance_globals;

    BIT_VECTOR_SET_FLAG(
      globals->ally_bitvector,
      NUMBER_OF_GAME_TEAMS * allegiance->team1_index +
        allegiance->team2_index,
      !permanently_broken);
    BIT_VECTOR_SET_FLAG(
      globals->ally_bitvector,
      NUMBER_OF_GAME_TEAMS * allegiance->team2_index +
        allegiance->team1_index,
      !permanently_broken);
    BIT_VECTOR_SET_FLAG(
      globals->friendly_bitvector,
      NUMBER_OF_GAME_TEAMS * allegiance->team1_index +
        allegiance->team2_index,
      !currently_broken);
    BIT_VECTOR_SET_FLAG(
      globals->friendly_bitvector,
      NUMBER_OF_GAME_TEAMS * allegiance->team2_index +
        allegiance->team1_index,
      !currently_broken);
  }

  allegiance->status_changed = true;
  game_allegiance_apply_change(
    allegiance->team1_index,
    allegiance->team2_index,
    currently_broken,
    permanently_broken);
}

void game_allegiance_update(void)
{
  struct game_allegiance_record *allegiance;
  int16_t allegiance_index;

  allegiance_index = 0;
  allegiance = ((game_allegiance_globals_type *)game_allegiance_globals)->allegiances;
  if (((game_allegiance_globals_type *)game_allegiance_globals)->allegiance_count > 0) {
    do {
      if (allegiance->current_incident_decay_time > 0) {
        allegiance->current_incident_decay_time--;
        if (allegiance->current_incident_decay_time == 0) {
          assert_halt_at(
            "c:\\halo\\SOURCE\\game\\game_allegiance.c",
            121,
            allegiance->current_incidents > 0);
          allegiance->current_incidents--;
          if (allegiance->current_incidents == 0) {
            game_allegiance_set((int16_t *)allegiance, false, false);
          } else {
            allegiance->current_incident_decay_time =
              allegiance->incident_decay_time;
          }
        }
      }

      allegiance_index++;
      allegiance++;
    } while (allegiance_index < ((game_allegiance_globals_type *)game_allegiance_globals)->allegiance_count);
  }
}

void game_allegiance_create(
  int16_t team_a,
  char is_player,
  int16_t team_b,
  char is_timer,
  int16_t threshold,
  int16_t timer,
  char is_ally)
{
  struct game_allegiance_record *allegiance;
  int16_t allegiance_count;
  int16_t allegiance_index;
  int16_t allegiance_team1_index;

  allegiance = ((game_allegiance_globals_type *)game_allegiance_globals)->allegiances;
  allegiance_count = ((game_allegiance_globals_type *)game_allegiance_globals)->allegiance_count;
  allegiance_index = 0;
  if (allegiance_count > 0) {
    do {
      allegiance_team1_index = allegiance->team1_index;

      if (allegiance_team1_index == team_a &&
          allegiance->team2_index == team_b) {
        break;
      }
      if (allegiance->team2_index == team_a &&
          allegiance_team1_index == team_b) {
        break;
      }

      allegiance_index++;
      allegiance++;
    } while (allegiance_index < ((game_allegiance_globals_type *)game_allegiance_globals)->allegiance_count);
  }

  if (allegiance_index >= ((game_allegiance_globals_type *)game_allegiance_globals)->allegiance_count) {
    if (allegiance_count < 8) {
      allegiance_index = allegiance_count;
      ((game_allegiance_globals_type *)game_allegiance_globals)->allegiance_count = allegiance_count + 1;
    } else {
      error(
        2,
        "game_allegiance_create: too many allegiances (maximum is %d)",
        8);
    }
  }

  if (allegiance_index < ((game_allegiance_globals_type *)game_allegiance_globals)->allegiance_count) {
    struct game_allegiance_record *target =
      &((game_allegiance_globals_type *)game_allegiance_globals)->allegiances[allegiance_index];

    target->team1_index = team_a;
    target->team1_suspicious = is_player;
    target->team2_index = team_b;
    target->team2_suspicious = is_timer;
    target->incident_threshold = threshold;
    target->incident_decay_time = timer;
    target->current_incidents = 0;
    target->current_incident_decay_time = 0;
    target->requires_communication = is_ally;
    target->currently_broken = true;
    game_allegiance_set((int16_t *)target, false, false);
    target->status_changed = false;
  }
}

bool game_allegiance_remove(
  int16_t team_a,
  int16_t team_b)
{
  struct game_allegiance_record *allegiance;
  int16_t allegiance_count;
  int16_t allegiance_index;
  bool result = false;
  game_allegiance_globals_type *globals;

  allegiance = ((game_allegiance_globals_type *)game_allegiance_globals)->allegiances;
  allegiance_count = ((game_allegiance_globals_type *)game_allegiance_globals)->allegiance_count;
  for (allegiance_index = 0;
       allegiance_index < allegiance_count;
       allegiance_index++, allegiance++) {
    if ((allegiance->team1_index == team_a &&
         allegiance->team2_index == team_b) ||
        (allegiance->team2_index == team_a &&
         allegiance->team1_index == team_b)) {
      game_allegiance_set((int16_t *)allegiance, true, true);
      globals = (game_allegiance_globals_type *)game_allegiance_globals;
      globals->allegiance_count--;
      if (globals->allegiance_count > allegiance_index) {
        globals->allegiances[allegiance_index] =
          globals->allegiances[globals->allegiance_count];
      }

      result = true;
      break;
    }
  }

  return result;
}

bool game_allegiance_bump(
  int16_t team_a,
  int16_t team_b,
  int16_t action,
  bool *out_changed)
{
  struct game_allegiance_record *allegiance;
  int16_t allegiance_count;
  int16_t allegiance_index;
  int16_t increment;
  bool result = false;

  allegiance = ((game_allegiance_globals_type *)game_allegiance_globals)->allegiances;
  allegiance_count = ((game_allegiance_globals_type *)game_allegiance_globals)->allegiance_count;
  for (allegiance_index = 0;
       allegiance_index < allegiance_count;
       allegiance_index++, allegiance++) {
    if ((allegiance->team1_index == team_a &&
         allegiance->team2_index == team_b &&
         allegiance->team2_suspicious) ||
        (allegiance->team2_index == team_a &&
         allegiance->team1_index == team_b &&
         allegiance->team1_suspicious)) {
      increment = 0;
      switch (action) {
      case _allegiance_incident_accident:
        increment = 1;
        break;
      case _allegiance_incident_betrayal:
        increment = 3;
        break;
      case _allegiance_incident_forgive:
        increment = -1;
        break;
      }

      allegiance->current_incidents += increment;
      if (allegiance->incident_decay_time != NONE) {
        allegiance->current_incident_decay_time =
          allegiance->incident_decay_time;
      }

      if (allegiance->incident_threshold != NONE &&
          allegiance->current_incidents >= allegiance->incident_threshold) {
        game_allegiance_set((int16_t *)allegiance, true, false);
        result = true;
        if (out_changed) {
          *out_changed = !allegiance->requires_communication;
        }
      }

      break;
    }
  }

  return result;
}

int FUN_000a8110(int param_1, int param_2)
{
  char *elem;

  elem =
    (char *)tag_block_get_element((void *)(param_1 + 0x14c), param_2, 0x10);
  return *(int *)(elem + 0xc);
}

int FUN_000a8130(int param_1)
{
  char *item;
  data_iter_t iter;

  data_iterator_new(&iter, *(data_t **)0x5aa6d4);
  item = (char *)data_iterator_next(&iter);
  while (1) {
    if (item == NULL) {
      return 0;
    }
    if (*(int *)(item + 0x20) == param_1)
      break;
    item = (char *)data_iterator_next(&iter);
  }
  return ((int (*)(uint32_t, int))((int *)current_game_engine)[0x48 / 4])(iter.datum_handle, 1);
}
