/* Game sound subsystem — manages looping sounds attached to objects. */

void game_sound_initialize(void)
{
  *(void **)0x5054e4 =
    game_state_data_new("object looping sounds", 0x400, 0x34);
  *(void **)0x5054e0 =
    game_state_malloc("game sound globals", 0, 8);
}

void game_sound_dispose(void)
{
  if (*(void **)0x5054e4 != 0)
    *(void **)0x5054e4 = 0;
}

void game_sound_initialize_for_new_map(void)
{
  if (*(void **)0x5054e4 != 0) {
    ((void (*)(void *))0x119b20)(*(void **)0x5054e4);
    *(int *)(*(char **)0x5054e0 + 4) = -1;
    *(int *)(*(char **)0x5054e0) = 0;
  }
}

void game_sound_dispose_from_old_map(void)
{
  if (*(void **)0x5054e4 != 0 &&
      *(uint8_t *)(*(char **)0x5054e4 + 0x24) != 0) {
    ((void (*)(void))0x1c70b0)();
    ((void (*)(void *))0x119550)(*(void **)0x5054e4);
  }
}

/* NOTE: game_sound_update (0x1c8140) is complex and left to the
 * original binary via weak thunk. */
