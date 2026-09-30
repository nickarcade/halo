typedef bool (*ui_widget_event_handler_fn)(void *widget, void *event_data,
                                           bool *widget_deleted);

bool ui_widget_event_handler_function_invoke(void *widget, int unknown,
                                      uint16_t handler_func_index,
                                      bool *widget_deleted)
{
  int16_t index = (int16_t)handler_func_index;
  ui_widget_event_handler_fn handler;
  const char *handler_name;
  bool ok;

  if (widget == NULL || widget_deleted == NULL) {
    display_assert(
      "(widget != NULL) && (widget_deleted != NULL)",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x1de,
      true);
    system_exit(-1);
  }

  if (index >= 0 && index < 0x66) {
    handler = ((ui_widget_event_handler_fn *)0x31e158)[index];
    ok = handler(widget, (void *)unknown, widget_deleted);
    if (!ok) {
      handler_name = ((const char **)0x31e2f0)[index];
      console_warning("event handler '%s' failed", handler_name);
    }
    return ok;
  }

  error(2, "invalid event_handler_function");
  return false;
}
int *widget_instance_find_by_tag_index_recursive(int *root, int tag_handle);
char ui_widget_focusable(void *widget);
void *ui_widget_list_column_get(void *widget, int index);
void ui_widget_set_focus_to_child(int tag_handle, int16_t player_index);
void ui_widget_set_focus_to_child_focused(void *focused);
bool ui_widget_event_handler_function_invoke(void *widget, int unknown,
                                      uint16_t handler_func_index,
                                      bool *widget_deleted);

/* widget_event_function_null (0x0e98a0) — the no-op event handler. Name PAL
 * 2342 ui_widget_event_handler_functions.c event_handler_function_list (T2):
 * PAL slots 0, 3 and 4 are widget_event_function_null, and the 2276 table at
 * 0x31e158 has 0x0e98a0 in exactly those slots (0x31e158/0x31e164/0x31e168).
 * Body is MOV AL,1 / RET: the arguments are never read. */
bool widget_event_function_null(void *widget, void *event_data,
                                bool *widget_deleted)
{
  return true;
}

/**
 * Clears the last error index by resetting it to -1 (no error).
 * The global at 0x31e4c0 tracks which error was most recently displayed
 * by the UI widget error system.
 */
__declspec(noinline) void reset_last_player1_profile_index(void)
{
  *(int *)0x31e4c0 = -1;
}

/* initialize sp level list (0x0e98c0) — rebuilds the 0x50-byte single-player
 * level list scratch block at 0x46cce8 (10 entries x 8 bytes: a level name
 * pointer from the table at 0x31e498 plus four flag bytes at +4..+7).  An
 * entry is unlocked when either local player's profile flag byte (profile
 * offset 0x1c + i) is set, when i is one past either player's stored last
 * level played, or for i == 0 (the first level is always available).  Then
 * asserts the bound 'solo level list' widget is a spinner list ('DeLa' type
 * 2) with 3 list items, publishes the block pointer/count at widget +0x40 /
 * +0x44 and clamps the selected index at +0x3c to [0, 9]. */
bool ui_widget_initialize_single_player_level_list(void *widget,
                                                   void *event_data,
                                                   bool *widget_deleted)
{
  uint8_t profile0[0x30];
  uint8_t profile1[0x30];
  int16_t last_level0;
  int16_t last_level_unused0;
  int16_t last_level1;
  int16_t last_level_unused1;
  int i;
  uint8_t flags0;
  unsigned int flags;
  int16_t *list_tag;
  int16_t selected;

  (void)event_data;
  (void)widget_deleted;

  csmemset((void *)0x46cce8, 0, 0x50);
  player_ui_get_active_player_profile(0, profile0);
  player_profile_save_last_level_played(profile0, &last_level0,
                                        &last_level_unused0);
  player_ui_get_active_player_profile(1, profile1);
  player_profile_save_last_level_played(profile1, &last_level1,
                                        &last_level_unused1);

  /* Spelled do/while: the reference is a bottom-tested loop MSVC left rolled
   * (INC EAX / CMP EAX,0xa / JL). The equivalent `for (i = 0; i < 10; i++)`
   * body is fully unrolled 5x by clang (294 candidate insns vs the
   * reference's 138, 60.6% match). */
  i = 0;
  do {
    *(const char **)(0x46cce8 + i * 8) = ((const char **)0x31e498)[i];
    flags0 = profile0[0x1c + i];
    if ((flags0 != 0) || (i == (int)last_level0 + 1) ||
        (profile1[0x1c + i] != 0) || (i == (int)last_level1 + 1) || (i == 0)) {
      flags = (unsigned int)((int)(signed char)profile1[0x1c + i] |
                             (int)(signed char)flags0);
      *(uint8_t *)(0x46cce8 + i * 8 + 5) = (uint8_t)((flags >> 1) & 1);
      *(uint8_t *)(0x46cce8 + i * 8 + 4) = 1;
      *(uint8_t *)(0x46cce8 + i * 8 + 6) = (uint8_t)((flags >> 2) & 1);
      *(uint8_t *)(0x46cce8 + i * 8 + 7) = (uint8_t)((flags >> 3) & 1);
    }
    i++;
  } while (i < 10);

  list_tag = (int16_t *)tag_get(0x44654c61, *(int *)widget);
  if (*list_tag != 2) {
    display_assert(
      "expected a spinner list widget for 'solo level list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x2b1,
      true);
    system_exit(-1);
  }
  if (*(int *)((char *)list_tag + 0x3e0) != 3) {
    display_assert(
      "expected 3 list items for 'solo level list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x2b2,
      true);
    system_exit(-1);
  }

  *(int *)((char *)widget + 0x40) = 0x46cce8;
  *(int16_t *)((char *)widget + 0x44) = 10;

  if (player_ui_get_last_single_player_level_played(0) < 0) {
    *(int16_t *)((char *)widget + 0x3c) = 0;
    return true;
  }
  if (player_ui_get_last_single_player_level_played(0) > 9) {
    *(int16_t *)((char *)widget + 0x3c) = 9;
    return true;
  }
  selected = player_ui_get_last_single_player_level_played(0);
  *(int16_t *)((char *)widget + 0x3c) = selected;
  return true;
}

/* dispose sp level list (event handler table index 7, 0x0e9a60) — clears the
 * 0x50-byte single-player level list scratch block at 0x46cce8 and drops the
 * widget's cached list pointer/count at +0x40/+0x44. */
bool solo_level_dispose_list(void *widget, void *event_data,
                             bool *widget_deleted)
{
  csmemset((void *)0x46cce8, 0, 0x50);
  *(int *)((char *)widget + 0x40) = 0;
  *(int16_t *)((char *)widget + 0x44) = 0;
  return true;
}

/* solo level set map (event handler table index 8, 0x0e9a90) — the selected
 * item of the solo level list widget (+0x3c) must be a level index 0..9, else
 * it asserts (ui_widget_event_handler_functions.c line 0x2d4). A level is
 * available to a local player when the profile's per-level flag byte at
 * profile+0x1c+level is nonzero, the level directly follows the profile's
 * last level played, or it is level 0. With one player (0x31fa94 == 1) the
 * player 0 result is taken first and player 1's profile is forgotten; the
 * loop over local players 0..1 then runs for either player count. An
 * available level queues its map name from the table at 0x31e498 and a map
 * change; otherwise it reports the level unavailable and plays audio
 * feedback 4. Any other player count is an error. Returns whether the level
 * was available. */
bool solo_level_set_next_map_name(void *widget, void *event_data,
                                  bool *widget_deleted)
{
  widget_instance_t *list;
  char profile[0x30];
  short last_level;
  short difficulty;
  short local_player_index;
  bool available;

  list = (widget_instance_t *)widget;
  available = false;
  if (list->list_selected_index < 0 || list->list_selected_index >= 10) {
    display_assert(
      "I don't think this is the solo level list widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x2d4,
      true);
    system_exit(-1);
  }

  switch (*(int16_t *)0x31fa94) {
  case 1:
    player_ui_get_active_player_profile(0, profile);
    player_profile_save_last_level_played(profile, &last_level, &difficulty);
    if (profile[0x1c + list->list_selected_index] != 0 ||
        list->list_selected_index == last_level + 1 ||
        list->list_selected_index == 0) {
      available = true;
    }
    player_ui_remember_player1_profile(false);
    /* fall through */
  case 2:
    for (local_player_index = 0; local_player_index <= 1;
         local_player_index++) {
      player_ui_get_active_player_profile(local_player_index, profile);
      player_profile_save_last_level_played(profile, &last_level, &difficulty);
      if (profile[0x1c + list->list_selected_index] != 0 ||
          list->list_selected_index == last_level + 1 ||
          list->list_selected_index == 0) {
        available = true;
        break;
      }
    }
    break;
  default:
    error(2, "invalid player count for single player game");
    break;
  }

  if (available == true) {
    main_set_map_name(((const char **)0x31e498)[list->list_selected_index]);
    main_defer_map_map_change();
  } else {
    error(2, "this level is unavailable to you!");
    ui_play_audio_feedback_sound(4);
  }
  return available;
}

/* difficulty_set (event handler, 0x0e9bd0) — reads the widget's selected
 * item index (signed 16-bit) at +0x3c. If it is a valid difficulty
 * (0 <= selected < 4), applies it via main_set_difficulty and plays audio
 * feedback sound 2, then returns true. Otherwise it asserts (message plus
 * this file/line 0x313) and, since display_assert's halt argument is true,
 * falls through to system_exit(-1) — a path the reference marks
 * non-returning. */
bool difficulty_set(void *widget, void *event_data, bool *widget_deleted)
{
  (void)event_data;
  (void)widget_deleted;

  if (*(short *)((char *)widget + 0x3c) < 0 ||
      *(short *)((char *)widget + 0x3c) >= 4) {
    display_assert(
      "I don't think this is the difficulty list widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x313,
      true);
    system_exit(-1);
  }

  main_set_difficulty(*(short *)((char *)widget + 0x3c));
  ui_play_audio_feedback_sound(2);
  return true;
}

/* start new game (event handler table index 10, 0x0e9c30; name from the
 * handler name table at 0x31e2f0) — sets difficulty 1, queues the first
 * entry of the solo level map-name table (0x31e498), switches the game
 * connection to local (0), enters single player and forgets player 1's
 * profile. The arguments are never read; always returns true. */
bool start_new_game(void *widget, void *event_data, bool *widget_deleted)
{
  main_set_difficulty(1);
  main_set_map_name(*(const char **)0x31e498);
  set_game_connection(0);
  main_menu_switch_to_single_player();
  player_ui_remember_player1_profile(false);
  return true;
}

/* pause game restart at checkpoint (event handler table index 11, 0x0e9c60;
 * name from the handler name table at 0x31e2f0) — reverts the map to the last
 * checkpoint. The arguments are never read; always returns true. */
bool pause_game_restart_at_checkpoint(void *widget, void *event_data,
                                      bool *widget_deleted)
{
  main_revert_map();
  return true;
}

/* pause game restart level (event handler table index 12, 0x0e9c70) — resets
 * the map. The arguments are never read; always returns true. */
bool pause_game_restart_level(void *widget, void *event_data,
                              bool *widget_deleted)
{
  main_reset_map();
  return true;
}

/* pause game return to main menu (event handler table index 13, 0x0e9c80) —
 * saves the game state to persistent storage, then goes to the main menu.
 * The arguments are never read; always returns true. */
bool pause_game_return_to_main_menu(void *widget, void *event_data,
                                    bool *widget_deleted)
{
  game_state_save_to_persistent_storage();
  main_goto_main_menu();
  return true;
}

/* clear multiplayer player joins (event handler table index 14, 0x0e9c90;
 * name from the handler name table at 0x31e2f0) — disposes the network game
 * client and server, then clears the multiplayer joins and variant. The
 * arguments are never read; always returns true. */
bool clear_multiplayer_player_joins(void *widget, void *event_data,
                                    bool *widget_deleted)
{
  dispose_global_network_game_client();
  dispose_global_network_game_server();
  player_ui_clear_multiplayer_joins();
  player_ui_clear_multiplayer_variant();
  return true;
}

/* join controller to multiplayer game (event handler table index ??,
 * 0x0e9cb0) — the widget's local_player_index field (+0x8) must already be
 * resolved to a specific gamepad (not NONE/-1); asserts and exits otherwise.
 * Forwards the (zero/sign-extended) index to
 * player_ui_local_player_joined_multiplayer_game and always returns true. */
bool player_wants_to_join_multiplayer_game(void *widget, void *event_data,
                                           bool *widget_deleted)
{
  char *local_player_index_ptr;

  (void)event_data;
  (void)widget_deleted;

  local_player_index_ptr = (char *)widget + 8;
  if (*(int16_t *)local_player_index_ptr == -1) {
    display_assert(
      "need a specific local player index when joining a multiplayer game",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x369,
      true);
    system_exit(-1);
  }

  player_ui_local_player_joined_multiplayer_game(
    *(int16_t *)local_player_index_ptr);
  return true;
}

/* 0x0e9d00 (single data xref at 0x31e198) - starts a game search: disposes
 * the network game client (0x12a2a0) and server (0x12a1e0), clears the
 * multiplayer variant, then creates a network client. On success sets the
 * game connection to 1 and returns true; on failure reports error 2 "failed
 * to create network client to initiate game search" and returns false (XOR
 * AL,AL). No stack frame; the success value is kept in BL. widget/event_data/
 * widget_deleted are unused — the original never reads its incoming
 * event-handler params (only reference is the handler table entry). */
bool network_game_server_list_initialize(void *widget, void *event_data,
                                         bool *widget_deleted)
{
  bool result;

  (void)widget;
  (void)event_data;
  (void)widget_deleted;

  result = true;
  dispose_global_network_game_client();
  dispose_global_network_game_server();
  player_ui_clear_multiplayer_variant();
  if (create_global_network_game_client()) {
    set_game_connection(1);
  } else {
    error(2, "failed to create network client to initiate game search");
    result = false;
  }
  return result;
}

/* start network game server if not already advertised (event handler table
 * index 17, 0x0e9d40) — disposes any existing server, clears the cached
 * multiplayer variant UI text, and re-enables incoming connections. If no
 * server is currently advertised, initializes the game engine playlist and
 * attempts to start hosting (create_global_network_game_server); on success,
 * fetches the fresh server handle, pauses its countdown, begins the playlist,
 * and switches the local game connection state to 2 (host). Once past that gate
 * (or if a server was already up), checks for a local client and, if none,
 * re-derives the result via create_global_network_game_client. On any failure
 * the server and client are torn down, "accept connections" is cleared, the
 * multiplayer variant text is re-cleared, and error 2 "failed to initiate a
 * multiplayer game server" is reported. widget/event_data/widget_deleted
 * are unused — the original never establishes a stack frame and never
 * touches its incoming event-handler params. Called both through the
 * dispatch table above and directly (tail-propagated) by
 * start_network_game_if_no_advertised_servers (0x0f01d0). */
bool network_game_start_new_server(void *widget, void *event_data,
                                   bool *widget_deleted)
{
  bool result;
  void *server;
  void *client;

  (void)widget;
  (void)event_data;
  (void)widget_deleted;

  result = true;
  dispose_global_network_game_client();
  player_ui_clear_multiplayer_variant();
  network_game_set_accept_remote_connections(1);
  server = global_network_game_server_get();
  if (server == NULL) {
    game_engine_playlist_initialize();
    result = create_global_network_game_server();
    if (result) {
      server = global_network_game_server_get();
      network_game_server_pause_countdown(server, 1);
      game_engine_playlist_begin();
      set_game_connection(2);
    }
    if (!result) {
      goto fail;
    }
  }

  client = global_network_game_client_get();
  if (client == NULL) {
    result = create_global_network_game_client();
  }
  if (result) {
    return result;
  }

fail:
  dispose_global_network_game_server();
  dispose_global_network_game_client();
  network_game_set_accept_remote_connections(0);
  player_ui_clear_multiplayer_variant();
  error(2, "failed to initiate a multiplayer game server");
  return result;
}

/* dispose net game server list (event handler table index 18, 0x0e9fd0) —
 * drops the widget's cached list pointer/count at +0x40/+0x44. */
bool network_server_list_dispose(void *widget, void *event_data,
                                 bool *widget_deleted)
{
  *(int *)((char *)widget + 0x40) = 0;
  *(int16_t *)((char *)widget + 0x44) = 0;
  return true;
}

/* 0x0e9ff0 (single data xref at 0x31e1a4) - disposes the network game
 * server (0x12a1e0), then the client (0x12a2a0), then clears the
 * multiplayer variant; returns true (MOV AL,1). */
bool FUN_000e9ff0(void)
{
  dispose_global_network_game_server();
  dispose_global_network_game_client();
  player_ui_clear_multiplayer_variant();
  return true;
}

/* start split-screen game networking (0x0ea010, single data xref at
 * 0x31e1ac) — counterpart to network_game_start_new_server's "failed to
 * initiate a multiplayer game server" path, but for split screen: disallows
 * remote connections, then if no network game server exists yet, spins one up
 * via the game engine playlist and switches the connection to server
 * mode (2); bails out immediately on playlist-begin failure without
 * ever probing the client. If a server already existed (or was just
 * created), then checks for an existing client and creates one if
 * needed. On any failure, tears down both client and server, clears
 * the multiplayer variant, and reports the error. Returns true on
 * success. */
bool split_screen_game_initialize(void)
{
  bool result; /* name: PAL 2342 ui_widget_event_handler_functions.c:2707 */

  result = true;
  network_game_set_accept_remote_connections(0);
  if (global_network_game_server_get() == NULL) {
    game_engine_playlist_initialize();
    result = create_global_network_game_server();
    if (result == true) {
      game_engine_playlist_begin();
      set_game_connection(2);
    }
  }
  if (result && global_network_game_client_get() == NULL) {
    result = create_global_network_game_client();
  }
  if (!result) {
    dispose_global_network_game_server();
    dispose_global_network_game_client();
    player_ui_clear_multiplayer_variant();
    error(2, "failed to initiate split screen game networking");
  }
  return result;
}

/* coop game initialize (event handler table index 22, 0x0ea080; name from
 * the handler name table at 0x31e2f0) — sets player_spawn_count (0x31fa94,
 * 16-bit store) to 2. The arguments are never read; always returns true. */
bool coop_game_initialize(void *widget, void *event_data, bool *widget_deleted)
{
  player_spawn_count = 2;
  return true;
}

/* main_menu_initialize (event handler table index 23, 0x0ea090; data xref
 * 0x31e1b4) — name PAL 2342 ui_widget_event_handler_functions.c:2418 (T2),
 * slot matches PAL "main menu intialize". Clears the multiplayer joins and
 * variant, tears down any network game client/server, stops accepting
 * remote connections, resets player_spawn_count (0x31fa94, 16-bit store) to
 * 1, ends any profile edit, and starts the main menu music if it is not
 * already playing. The arguments are never read; always returns true. */
bool main_menu_initialize(void *widget, void *event_data, bool *widget_deleted)
{
  player_ui_clear_multiplayer_joins();
  player_ui_clear_multiplayer_variant();
  dispose_global_network_game_client();
  dispose_global_network_game_server();
  network_game_set_accept_remote_connections(0);
  player_spawn_count = 1;
  player_ui_end_editing_profile();
  if (!ui_main_menu_music_active()) {
    ui_start_main_menu_music();
  }
  return true;
}

/* mp type menu initialize (event handler table index 24, 0x0ea0d0; name from
 * the handler name table at 0x31e2f0) — resets player_spawn_count (0x31fa94,
 * 16-bit store) to 1. Returns true: the store goes through AX after
 * MOV EAX,1, so AL is 1 at the RET. The arguments are never read. */
bool mp_type_menu_initialize(void *widget, void *event_data,
                             bool *widget_deleted)
{
  player_spawn_count = 1;
  return true;
}

/* pick play stage for quick start (event handler table index 25, 0x0ea0e0;
 * name from the handler name table at 0x31e2f0) — resets the game engine
 * playlist, advances it with game_engine_playlist_next(0, 0, 4) and marks the
 * network game as a local quickstart. The arguments are never read; always
 * returns true. */
bool pick_play_stage_for_quick_start(void *widget, void *event_data,
                                     bool *widget_deleted)
{
  game_engine_playlist_initialize();
  game_engine_playlist_next(0, 0, 4);
  network_game_set_quickstart_local();
  return true;
}

/* mp level list initialize (event handler table entry at data 0x31e1c0,
 * 0x0ea100) — validates that `widget` itself is a spinner-list tag with
 * exactly 3 items ("multiplayer level list", same assert-file/line
 * pattern as the profile-list siblings above), points its list
 * pointer/count at the built-in level_name_table (13 entries) at
 * +0x40/+0x44 — the inverse of multiplayer_level_list_dispose
 * (0x0ea1f0) below, which clears the same two fields — then, if there is
 * a remembered last-used multiplayer map, linearly scans the table for a
 * case-insensitive name match and leaves the matching index selected at
 * +0x3c (reset to 0 if no match is found; left untouched if no map was
 * remembered). Always returns true. */
bool multiplayer_level_list_initialize(void *widget, void *event_data,
                                       bool *widget_deleted)
{
  short *list_tag;
  char saved_map_name[256];
  short level_count;

  (void)event_data;
  (void)widget_deleted;

  list_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */, *(int *)widget);
  level_count = 13;
  if (*list_tag != 2) {
    display_assert(
      "expected a spinner list widget for 'multiplayer level list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x4cc,
      1);
    system_exit(-1);
  }

  if (*(int *)((char *)list_tag + 0x3e0) != 3) {
    display_assert(
      "expected 3 list items for 'multiplayer level list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x4cd,
      1);
    system_exit(-1);
  }

  *(int *)((char *)widget + 0x40) =
    0x31e4c8; /* level_name_table (DAT_0031e4c8), 13 entries */
  *(int16_t *)((char *)widget + 0x44) = level_count;

  if (saved_game_file_retrieve_last_used_multiplayer_map(saved_map_name)) {
    *(int16_t *)((char *)widget + 0x3c) = 0;
    while (*(int16_t *)((char *)widget + 0x3c) < level_count &&
           crt_stricmp(
             saved_map_name,
             ((char **)0x31e4c8)[*(int16_t *)((char *)widget + 0x3c)]) != 0) {
      (*(int16_t *)((char *)widget + 0x3c))++;
    }

    if (*(int16_t *)((char *)widget + 0x3c) == level_count) {
      *(int16_t *)((char *)widget + 0x3c) = 0;
    }
  }

  return true;
}

/* mp level list dispose (event handler table index 27, 0x0ea1f0) — drops the
 * widget's cached list pointer/count at +0x40/+0x44. */
bool multiplayer_level_list_dispose(void *widget, void *event_data,
                                    bool *widget_deleted)
{
  *(int *)((char *)widget + 0x40) = 0;
  *(int16_t *)((char *)widget + 0x44) = 0;
  return true;
}

/* multiplayer level select (event handler, 0x0ea210) — fired when the user
 * accepts a level on the multiplayer level select screen.  Asserts the
 * widget chain: `widget` is a wrapper tag (child count +0x3e0 == 1), its
 * child at +0x34 is a container tag (type 0) with 3 children, and that
 * container's child at +0x34 is a spinner list tag (type 2) with 3 list
 * items.  Reads the list widget's selected index at +0x3c, bounds-checks
 * it against the 13-entry level_name_table at 0x31e4c8, and takes that
 * entry's name.  A debug override file "d:\map_automation.txt", when it
 * opens, replaces the name with its first whitespace-delimited token.  The
 * name is then pushed to main, to the game engine map-name override, and
 * to the network server when one exists; finally the table is scanned
 * case-insensitively and the matching entry is remembered as the last used
 * multiplayer map.  Always returns true. */
bool ui_widget_multiplayer_level_select(void *widget, void *event_data,
                                        bool *widget_deleted)
{
  void *wrapper_tag;
  short *screen_tag;
  void *screen_widget;
  void *list_widget;
  short *list_tag;
  int16_t selected_index;
  char *map_name;
  void *file;
  void *server;
  int index;
  char line[64];

  (void)event_data;
  (void)widget_deleted;

  wrapper_tag = tag_get(0x44654c61 /* 'DeLa' */, *(int *)widget);
  if (*(int *)((char *)wrapper_tag + 0x3e0) != 1) {
    display_assert(
      "expected a wrapper widget around the multiplayer level select screen",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x500,
      1);
    system_exit(-1);
  }

  screen_widget = *(void **)((char *)widget + 0x34);
  screen_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */, *(int *)screen_widget);
  if (*screen_tag != 0 || *(int *)((char *)screen_tag + 0x3e0) != 3) {
    display_assert(
      "expected the multiplayer level select screen to be a container w/ 3 "
      "children",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x505,
      1);
    system_exit(-1);
  }

  list_widget = *(void **)((char *)screen_widget + 0x34);
  list_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */, *(int *)list_widget);
  if (*list_tag != 2) {
    display_assert(
      "expected a spinner list widget for 'multiplayer level list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x508,
      1);
    system_exit(-1);
  }

  if (*(int *)((char *)list_tag + 0x3e0) != 3) {
    display_assert(
      "expected 3 list items for 'multiplayer level list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x509,
      1);
    system_exit(-1);
  }

  selected_index =
    *(int16_t *)((char *)*(void **)((char *)*(void **)((char *)widget + 0x34) +
                                    0x34) +
                 0x3c);
  if (selected_index < 0 || selected_index >= 13) {
    display_assert(
      "invalid multiplayer level specified from 'multiplayer level list' list "
      "widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x512,
      1);
    system_exit(-1);
  }

  map_name = ((char **)0x31e4c8)[selected_index];

  file = crt_fopen("d:\\map_automation.txt", "r");
  if (file != NULL) {
    crt_fgets(line, 0x40, file);
    line[63] = '\0';
    csstrtok(line, "\n\r \t");
    map_name = line;
    crt_fclose(file);
  }

  main_set_multiplayer_map_name(map_name);
  game_engine_override_map_name(map_name);

  server = global_network_game_server_get();
  if (server != NULL) {
    network_game_server_change_map_name((int)server, map_name);
  }

  index = 0;
  do {
    if (crt_stricmp(map_name, ((char **)0x31e4c8)[index]) == 0) {
      saved_game_file_remember_last_used_multiplayer_map(
        ((char **)0x31e4c8)[index]);
      return true;
    }
    index++;
  } while (index < 13);

  return true;
}

/* multiplayer profiles list initialize (event handler, 0x0ea3e0) — builds
 * the "multiplayer settings list" (game-variant profile) list owned by
 * `widget`.  Clears the pending profile handle (DAT_0031e494) and the
 * 0x144-byte profile scratch block at DAT_005aa260 to -1, asserts that
 * `widget` is a spinner-list tag ('DeLa' type 2) with exactly 3 list
 * items, then (re)allocates a 400-byte / 100-entry handle buffer at
 * widget+0x40 through ui_widget_realloc.  FUN_001c26b0 fills the buffer
 * and writes back the number of entries found (capacity passed in as
 * 100); any shortfall below 3 entries is padded with -1 so the list always
 * has at least 3 rows.  The final count lands at widget+0x44.  Finally, if
 * a last-used multiplayer variant directory is remembered and resolves to
 * a profile index, the buffer is scanned linearly and the matching row is
 * left selected at widget+0x3c (left untouched when there is no match).
 * A failed allocation skips everything after the store.  event_data and
 * widget_deleted are unused; always returns true. */
bool ui_widget_multiplayer_profiles_list_initialize(void *widget,
                                                    void *event_data,
                                                    bool *widget_deleted)
{
  short *list_tag;
  int *items;
  int count;
  int profile_index;
  unsigned short i;
  char variant_directory[256];

  (void)event_data;
  (void)widget_deleted;

  *(int *)0x31e494 = -1; /* DAT_0031e494 — pending profile handle */
  csmemset((void *)0x5aa260, -1,
           0x144); /* DAT_005aa260 — profile scratch block */

  list_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */, *(int *)widget);
  if (*list_tag != 2) {
    display_assert(
      "expected a spinner list widget for 'multiplayer settings list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x568,
      1);
    system_exit(-1);
  }

  if (*(int *)((char *)list_tag + 0x3e0) != 3) {
    display_assert(
      "expected 3 list items for 'multiplayer settings list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x569,
      1);
    system_exit(-1);
  }

  items = (int *)ui_widget_realloc(
    *(int *)((char *)widget + 0x40), 400,
    "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x56e);
  *(int **)((char *)widget + 0x40) = items;
  if (items == NULL) {
    return true;
  }

  count = 100;
  FUN_001c26b0(0, &count, items);

  while ((unsigned short)count < 3) {
    items[count] = -1;
    count++;
  }
  *(int16_t *)((char *)widget + 0x44) = (int16_t)count;

  if (!saved_game_file_retrieve_last_used_multiplayer_variant_directory(
        variant_directory)) {
    return true;
  }

  profile_index =
    saved_game_file_find_profile_index_for_directory_path(variant_directory, 1);
  if (profile_index == -1) {
    return true;
  }

  for (i = 0; i < (unsigned short)count; i++) {
    if (items[i] == profile_index) {
      *(int16_t *)((char *)widget + 0x3c) = (int16_t)i;
      break;
    }
  }

  return true;
}

/* dispose owned list (event handler, 0x0ea540; single data xref at
 * 0x31e1d0) — if the widget's cached list pointer at +0x40 is non-NULL,
 * frees it via widget_free and clears the pointer; always clears the
 * 16-bit count at +0x44. Unlike the static-table list-dispose siblings
 * above (mp level list dispose, sp level list dispose, dispose net game
 * server list, ...), this variant owns and frees its buffer. event_data
 * and widget_deleted are unused. Always returns true. */
bool multiplayer_profiles_list_dispose(void *widget, void *event_data,
                                       bool *widget_deleted)
{
  void *list_ptr;

  (void)event_data;
  (void)widget_deleted;

  list_ptr = *(void **)((char *)widget + 0x40);
  if (list_ptr != NULL) {
    widget_free(list_ptr);
    *(void **)((char *)widget + 0x40) = NULL;
  }
  *(int16_t *)((char *)widget + 0x44) = 0;
  return true;
}

/* multiplayer profile set for game (event handler, 0x0ea570) — asserts that
 * `widget` is a wrapper ('DeLa' +0x3e0 == 1) whose child (+0x34) is a
 * container (type 0) with 3 children, whose own child (+0x34) is a spinner
 * list (type 2) with 3 list items.  Reads the selected row (+0x3c, bounded
 * by the count at +0x44) of that list's handle buffer (+0x40).  A handle of
 * -1 plays feedback sound 4; any non-negative handle defers error 0x1f and
 * plays sound 4; both return false.  A negative (non -1) handle is loaded
 * into a local game variant via playlist_profile_delete (logs and returns
 * false on failure), remembers its enclosing directory when resolvable,
 * optionally overrides the variant by name from d:\variant_automation.txt
 * (only when the named lookup is non-zero), then applies it to the player
 * UI and, when a server exists, to the network game server.  Returns true.
 * event_data and widget_deleted are unused. */
bool multiplayer_profile_set_for_game(void *widget, void *event_data,
                                      bool *widget_deleted)
{
  void *tag;
  void *list;
  short selection;
  int profile;
  void *server;
  void *file;
  char directory[256];
  game_variant_t named;
  game_variant_t zero;
  game_variant_t found;
  char line[0x80];
  game_variant_t variant;

  (void)event_data;
  (void)widget_deleted;

  tag = tag_get(0x44654c61 /* 'DeLa' */, *(int *)widget);
  if (*(int *)((char *)tag + 0x3e0) != 1) {
    display_assert(
      "expected a wrapper widget around the multiplayer profile select screen",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x5b9,
      true);
    system_exit(-1);
  }

  list = *(void **)((char *)widget + 0x34);
  tag = tag_get(0x44654c61 /* 'DeLa' */, *(int *)list);
  if (*(short *)tag != 0 || *(int *)((char *)tag + 0x3e0) != 3) {
    display_assert(
      "expected the multiplayer profile select screen to be a container w/ 3 "
      "children",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x5be,
      true);
    system_exit(-1);
  }

  list = *(void **)((char *)list + 0x34);
  tag = tag_get(0x44654c61 /* 'DeLa' */, *(int *)list);
  if (*(short *)tag != 2) {
    display_assert(
      "expected a spinner list widget for 'multiplayer profile list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x5c1,
      true);
    system_exit(-1);
  }
  if (*(int *)((char *)tag + 0x3e0) != 3) {
    display_assert(
      "expected 3 list items for 'multiplayer profile list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x5c2,
      true);
    system_exit(-1);
  }

  list = *(void **)((char *)*(void **)((char *)widget + 0x34) + 0x34);
  selection = *(short *)((char *)list + 0x3c);
  if (selection < 0 ||
      (int)selection >= (int)*(unsigned short *)((char *)list + 0x44)) {
    display_assert(
      "invalid multiplayer profile specified from 'multiplayer profile list' "
      "list widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x5cb,
      true);
    system_exit(-1);
  }

  profile = (*(int **)((char *)list + 0x40))[*(short *)((char *)list + 0x3c)];
  if (profile == -1) {
    ui_play_audio_feedback_sound(4);
    return false;
  }
  if (!(profile & 0x80000000)) {
    display_error_deferred(0x1f, -1, true, false);
    ui_play_audio_feedback_sound(4);
    return false;
  }

  if (playlist_profile_delete(profile, &variant)) {
    server = global_network_game_server_get();
    if (saved_game_file_get_path_to_enclosing_directory(profile, directory)) {
      saved_game_file_remember_last_used_multiplayer_variant_directory(
        directory);
    }

    file = crt_fopen("d:\\variant_automation.txt", "r");
    if (file != NULL) {
      crt_fgets(line, 0x80, file);
      line[0x7f] = 0;
      csstrtok(line, "\n\r \t");
      csmemset(&zero, 0, 0x68);
      found = *game_engine_get_variant_by_name(&named, line);
      if (csmemcmp(&found, &zero, 0x68) != 0) {
        variant = found;
      }
      crt_fclose(file);
    }

    player_ui_set_game_variant(&variant);
    if (server != NULL) {
      network_game_server_change_game_variant(server, &variant);
    }
    return true;
  }
  error(2, "failed to retrieve user selected game variant");
  return false;
}

/* swap teams (event handler, 0x0ea810) — asserts event_data is non-NULL,
 * then, if a network game exists with teams enabled (+0xc0 == 1, same flag
 * multiplayer_game_set_text_box_for_teams_noteams reads) and this client has
 * a valid local machine index (network_game_client_get_local_machine_index),
 * walks the game's 16-slot player table (index_base+0x226, stride 0x20 —
 * same table netgame_join_player and multiplayer_profiles_list_dispose's
 * neighbor walk) for a valid record (network_player_is_valid) whose
 * machine-index byte (record+0x1c) matches the local machine index and
 * whose controller-index byte (record+0x1d) matches the event's
 * controller_index (event_data+2, same field netgame_join_player reads).
 * On the first match it copies the 0x20-byte record to a local buffer,
 * flips the byte at record offset 0x1e to its logical complement (0/1
 * toggle — the team field), and pushes the updated record via
 * network_game_client_update_local_player_data(global_network_game_client_get(),
 * &local_record), logging "failed to update player's team for multiplayer
 * game" via error(2, ...) on failure, then stops walking the table. widget
 * and widget_deleted are unused; always returns true. */
bool multiplayer_game_swap_teams(void *widget, void *event_data,
                                 bool *widget_deleted)
{
  int index_base;
  short local_machine_index;
  int i;
  char *player_slot;
  uint32_t local_record[8];
  bool updated;

  (void)widget;
  (void)widget_deleted;

  if (event_data == NULL) {
    display_assert(
      "event",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x624,
      true);
    system_exit(-1);
  }

  index_base = network_game_get_game();
  if (index_base != 0 && *(char *)(index_base + 0xc0) == 1) {
    local_machine_index = network_game_client_get_local_machine_index();
    if (local_machine_index != -1) {
      player_slot = (char *)index_base + 0x226;
      for (i = 0; i < 0x10; i++, player_slot += 0x20) {
        if (network_player_is_valid(player_slot) &&
            *(player_slot + 0x1c) == local_machine_index &&
            *(player_slot + 0x1d) == *(int16_t *)((char *)event_data + 2)) {
          memcpy(local_record, player_slot, sizeof(local_record));
          ((char *)local_record)[0x1e] = (((char *)local_record)[0x1e] == 0);
          updated = network_game_client_update_local_player_data(
            global_network_game_client_get(), local_record);
          if (!updated) {
            error(2, "failed to update player's team for multiplayer game");
          }
          break;
        }
      }
    }
  }

  return true;
}

/* join network game (event handler, 0x0ea900) — asserts event_data is
 * non-NULL, then, if a network game client exists and its state
 * (network_game_client_get_state) is 2, walks the client's 16-slot player
 * table (index_base from network_game_get_game, records at index_base+0x226,
 * stride 0x20 — same table network_game_client_local_player_quit walks) looking
 * for a valid record whose machine-index byte (+0x242, relative to
 * index_base+i*0x20) matches this client's local machine index
 * (network_game_client_get_local_machine_index) and whose controller-index
 * byte (+0x243) matches the event's controller_index (event_data+2, same
 * field event_controller_index_compatible_with_widget reads above) — if
 * found, the player is already present and the function returns
 * immediately. Otherwise it asks the client to add the player via
 * network_game_client_add_player(client, controller_index), logging
 * "failed to send join request" via network_event on failure. Always
 * returns true; widget and widget_deleted are unused. */
bool netgame_join_player(void *widget, void *event_data, bool *widget_deleted)
{
  void *client;
  int16_t state;
  int state_out;
  int index_base;
  short local_machine_index;
  short i;
  char *player_slot;
  char *record;
  int16_t controller_index;
  bool added;

  (void)widget;
  (void)widget_deleted;

  if (event_data == NULL) {
    display_assert(
      "event",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x652,
      true);
    system_exit(-1);
  }

  client = global_network_game_client_get();
  if (client != NULL) {
    state = network_game_client_get_state(client, &state_out);
    if (state == 2) {
      index_base = network_game_get_game();
      local_machine_index = network_game_client_get_local_machine_index();

      if (index_base == 0) {
        display_assert(
          "game",
          "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
          0x65b, true);
        system_exit(-1);
      }

      controller_index = *(int16_t *)((char *)event_data + 2);

      if (local_machine_index != -1) {
        for (i = 0; i < 0x10; i++) {
          player_slot = (char *)index_base + 0x226 + i * 0x20;
          if (network_player_is_valid(player_slot)) {
            record = (char *)index_base + i * 0x20;
            if (*(record + 0x242) == local_machine_index &&
                *(record + 0x243) == controller_index) {
              return true;
            }
          }
        }
      }

      added =
        network_game_client_add_player(client, (uint16_t)controller_index);
      if (!added) {
        network_event("failed to send join request");
      }
    }
  }

  return true;
}

/* player_profiles_list_initialize (event handler, 0x0eaa10) — builds the
 * "player settings list" spinner list of saved player profiles.  PAL 2342
 * ui_widget_event_handler_functions.c:4366 (names T2; __FILE__/line asserts at
 * 0x696/0x698/0x69d confirm the source file).  Clears the pending profile
 * handle (0x31e494) and the 0x9c-byte cached player-profile block at 0x5aa3c0
 * to -1, asserts a spinner-list definition with 0 or 3 list items, then
 * (re)allocates the 400-byte / 100-entry profile index buffer.
 * FUN_001c0d50 enumerates the profiles available to the widget's local player
 * (capacity 100 in, found count out) and appends the "default" entry unless
 * the list is the 3-wide variant.  A 3-wide list is padded with -1 up to three
 * rows.  Finally the row holding player 1's last-used profile, if any, is
 * selected.  event_data and widget_deleted are unused; always returns true. */
bool player_profiles_list_initialize(void *widget_ptr, void *event_data,
                                     bool *widget_deleted)
{
  widget_instance_t *widget;
  ui_widget_definition_t *definition;
  int *profile_indices;
  int profile_count;
  int last_profile_index;
  int profile_index;
  char include_default;
  short required_profile_count;
  int profile_offset;
  int remaining_profile_count;

  (void)event_data;
  (void)widget_deleted;

  widget = (widget_instance_t *)widget_ptr;
  definition = (ui_widget_definition_t *)tag_get(0x44654c61 /* 'DeLa' */,
                                                 widget->definition_tag_index);
  *(int *)0x31e494 = -1; /* PAL event_handler_functions.profile_index */
  csmemset((void *)0x5aa3c0, -1, 0x9c); /* PAL cached_player_profile */

  if (definition->type != UI_WIDGET_TYPE_SPINNER_LIST) {
    display_assert(
      "expected a spinner list widget for 'player settings list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x696,
      1);
    system_exit(-1);
  }
  if (definition->child_widgets.count != 0 &&
      definition->child_widgets.count != 3) {
    display_assert(
      "expected either 1 or 3 list items for 'player settings list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x698,
      1);
    system_exit(-1);
  }

  profile_indices = (int *)ui_widget_realloc(
    (int)widget->list_items, 400,
    "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x69d);
  widget->list_items = profile_indices;
  if (profile_indices == NULL) {
    return true;
  }

  required_profile_count = 3;
  if (definition->child_widgets.count != required_profile_count) {
    profile_count = 100;
    include_default = 1;
  } else {
    profile_count = 100;
    include_default = 0;
  }
  FUN_001c0d50((uint16_t)widget->local_player_index, &profile_count,
               profile_indices, include_default);

  if (definition->child_widgets.count == required_profile_count &&
      (unsigned short)profile_count < (unsigned short)required_profile_count) {
    profile_offset = (unsigned short)profile_count * 4;
    remaining_profile_count =
      (unsigned short)(required_profile_count - (unsigned short)profile_count);
    do {
      *(int *)((char *)widget->list_items + profile_offset) = -1;
      profile_count++;
      profile_offset += 4;
    } while (--remaining_profile_count);
  }
  widget->list_number_of_items = (uint16_t)profile_count;

  last_profile_index = player_ui_get_player1_last_used_profile_index();
  if (last_profile_index != -1) {
    profile_indices = (int *)widget->list_items;
    for (profile_index = 0; profile_index < widget->list_number_of_items;
         profile_index++) {
      if (profile_indices[profile_index] == last_profile_index) {
        widget->list_selected_index = (int16_t)profile_index;
        break;
      }
    }
  }
  return true;
}

/* dispose owned list, duplicate table entry (event handler, 0x0eab70; data
 * xref at 0x31e1e4, 0x14 bytes after multiplayer_profiles_list_dispose's
 * 0x31e1d0 entry) — byte-identical body to multiplayer_profiles_list_dispose
 * above: if the widget's cached list pointer at +0x40 is non-NULL, frees it via
 * widget_free and clears the pointer; always clears the 16-bit count at +0x44.
 * event_data and widget_deleted are unused. Always returns true. */
bool player_profiles_list_dispose(void *widget, void *event_data,
                                  bool *widget_deleted)
{
  void *list_ptr;

  (void)event_data;
  (void)widget_deleted;

  list_ptr = *(void **)((char *)widget + 0x40);
  if (list_ptr != NULL) {
    widget_free(list_ptr);
    *(void **)((char *)widget + 0x40) = NULL;
  }
  *(int16_t *)((char *)widget + 0x44) = 0;
  return true;
}

/* player_profile_set_for_game_3wide (0xeaba0) — event-handler table entry
 * at 0x31e1e8.  Validates the profile-selection container and its spinner-list
 * child, then applies the selected profile result. */
bool player_profile_set_for_game_3wide(void *widget, void *event_data,
                                       bool *widget_deleted)
{
  wchar_t profile[24];
  int profile_index;
  int local_player_index;
  short *widget_definition;
  void *spinner; /* name: PAL 2342 ui_widget_event_handler_functions.c:4447 */
  short selected_index;
  int *profile_indices;

  if (event_data == NULL || *(int16_t *)((char *)event_data + 2) == -1) {
    display_assert(
      "setting a player profile requires a valid controller index",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x6e3,
      true);
    system_exit(-1);
  }

  widget_definition = (short *)tag_get(0x44654c61, *(int *)widget);
  if (*widget_definition != 0 ||
      *(int *)((char *)widget_definition + 0x3e0) < 3) {
    display_assert(
      "expected the player profile select screen to be a container w/ 3 or "
      "more children",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x6ec,
      true);
    system_exit(-1);
  }

  {
    void *child = *(void **)((char *)widget + 0x34);
    widget_definition = (short *)tag_get(0x44654c61, *(int *)child);
  }
  if (*widget_definition != 2) {
    display_assert(
      "expected a spinner list widget for 'player profile list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x6ef,
      true);
    system_exit(-1);
  }
  if (*(int *)((char *)widget_definition + 0x3e0) != 3) {
    display_assert(
      "expected 3 list items for 'player profile list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x6f0,
      true);
    system_exit(-1);
  }

  spinner = *(void **)((char *)widget + 0x34);
  selected_index = *(int16_t *)((char *)spinner + 0x3c);
  if (selected_index < 0 ||
      (int)selected_index >= (int)*(uint16_t *)((char *)spinner + 0x44)) {
    display_assert(
      "invalid multiplayer profile specified from 'player profile list' list "
      "widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x6f8,
      true);
    system_exit(-1);
  }

  profile_indices = *(int **)((char *)spinner + 0x40);
  profile_index = profile_indices[*(int16_t *)((char *)spinner + 0x3c)];
  if (profile_index != -1) {
    if (!(profile_index & 0x80000000)) {
      display_error_deferred(0x1f, -1, true, false);
      ui_play_audio_feedback_sound(4);
      *widget_deleted = true;
      return false;
    }

    if (player_profile_new(profile_index, profile)) {
      local_player_index =
        player_ui_get_single_player_local_player_from_controller(
          *(int16_t *)((char *)event_data + 2));
      player_ui_set_active_player_profile(
        (short)local_player_index,
        profile_indices[*(int16_t *)((char *)spinner + 0x3c)], profile);
      return true;
    }

    error(2, "failed to retrieve user selected player profile");
    return false;
  }

  error(2, "this is not a selectable player profile");
  ui_play_audio_feedback_sound(4);
  return false;
}

/* player_profile_set_for_game_1wide (0xead60) — event-handler table entry.
 * Same profile-selection idea as player_profile_set_for_game_3wide (0xeaba0)
 * above but for a single spinner list, found by walking the widget's child
 * chain (+0x34, sibling link +0x2c) for the first child of type 2 (spinner
 * list) instead of using a fixed container-of-3 layout, and there is no
 * "container w/ 3 or more children" assert at all (disasm has no such check
 * here). The code-generated-list assert also differs: it requires the tag's
 * +0x3e0 field to be exactly 0, not >= 3.
 *
 * Unlike the 3-wide sibling, this handler never calls
 * player_ui_get_single_player_local_player_from_controller: the raw
 * controller index read once from event_data+2 is reused directly as the
 * local_player_index argument to both display_error_deferred and
 * player_ui_set_active_player_profile (disasm: MOV BX,[ESI+2] once into EBX,
 * then PUSH EBX unmodified at both call sites further down — safe because
 * both callees only read a 16-bit slice of that pushed dword: an int16_t
 * param and int, respectively, at MSVC).
 *
 * Control flow is also flatter than the 3-wide sibling: there is a single
 * `if (profile_index >= 0) {...} else {...}` (JS on the sign bit), not a
 * three-way -1 vs. <-1 vs. >=0 split — so there is no separate "this is not
 * a selectable player profile" branch here, and widget_deleted is never
 * read or written anywhere in this function (unused param). */
bool player_profile_set_for_game_1wide(void *widget, void *event_data,
                                       bool *widget_deleted)
{
  wchar_t profile[24];
  int *available_profiles; /* name: PAL 2342
                              ui_widget_event_handler_functions.c:4044 */
  int16_t controller_index;
  short selected_index;
  short *widget_definition;
  void
    *spinner_list; /* name: PAL 2342 ui_widget_event_handler_functions.c:4041 */

  (void)widget_deleted;

  if (event_data == NULL || *(int16_t *)((char *)event_data + 2) == -1) {
    display_assert(
      "setting a player profile requires a valid controller index",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x72a,
      true);
    system_exit(-1);
  }

  controller_index = *(int16_t *)((char *)event_data + 2);

  spinner_list = *(void **)((char *)widget + 0x34);
  while (spinner_list != NULL &&
         *(int16_t *)((char *)spinner_list + 0xe) != 2) {
    spinner_list = *(void **)((char *)spinner_list + 0x2c);
  }
  if (spinner_list == NULL) {
    display_assert(
      "failed to find the 1-wide spinner list for player profiles (expected "
      "it to be a child of this widget)",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x72e,
      true);
    system_exit(-1);
  }

  widget_definition = (short *)tag_get(0x44654c61, *(int *)spinner_list);
  if (*(int *)((char *)widget_definition + 0x3e0) != 0) {
    display_assert(
      "expected a code-generated 1-wide spinner list for 'mp player profile "
      "list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x735,
      true);
    system_exit(-1);
  }

  selected_index = *(int16_t *)((char *)spinner_list + 0x3c);
  if (selected_index < 0 ||
      (int)selected_index >= (int)*(uint16_t *)((char *)spinner_list + 0x44)) {
    display_assert(
      "invalid multiplayer profile specified from 'mp player profile list' "
      "list widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x73b,
      true);
    system_exit(-1);
  }

  available_profiles = *(int **)((char *)spinner_list + 0x40);
  if (!(available_profiles[*(int16_t *)((char *)spinner_list + 0x3c)] &
        0x80000000)) {
    display_error_deferred(0x1f, controller_index, true, false);
    ui_play_audio_feedback_sound(4);
    return false;
  }

  if (player_profile_new(
        available_profiles[*(int16_t *)((char *)spinner_list + 0x3c)],
        profile)) {
    player_ui_set_active_player_profile(
      (short)controller_index,
      available_profiles[*(int16_t *)((char *)spinner_list + 0x3c)], profile);
    return true;
  }

  error(2, "failed to retrieve user selected player profile");
  return false;
}

/* playlist_profile_begin_editing (0xeaec0) — event-handler table entry
 * (data xref 0x31e1f0). Validates 'widget' itself is a container with 3+
 * children (tag_get on *(int *)widget, same container check shape as
 * delete_player_profile_request), then the child list widget at widget+0x34
 * is a 3-item spinner list, resolves the selected item's profile handle
 * from that list, stores it to DAT_0031e494, and dispatches on it: -1 plays
 * the deny sound and returns false; a negative-but-not-(-1) handle begins
 * editing that profile (player_ui_begin_editing_profile) and returns true;
 * otherwise (>= 0) reports a deferred error and plays the deny sound,
 * returning false. Same three-way -1/<0/>=0 split and callee set as
 * player_profile_begin_editing (0xeed10), but with the container-of-3
 * tag_get check up front (like delete_player_profile_request) instead of
 * that sibling's simple container-flag check, and DAT_0031e494 is cleared
 * to -1 before the container check runs, not after (disasm: the MOV to
 * 0x31e494 is scheduled ahead of the first CALL tag_get). event_data and
 * widget_deleted are unused, same 3-arg handler-table shape as siblings. */
bool playlist_profile_begin_editing(void *widget, void *event_data,
                                    bool *widget_deleted)
{
  short *container_tag;
  int *list_widget;
  short *list_tag;
  short list_index;
  int
    profile_index; /* name: PAL 2342 ui_widget_event_handler_functions.c:3108 */
  int widget_tag_id;
  bool result;

  (void)event_data;
  (void)widget_deleted;

  result = false;

  widget_tag_id = *(int *)widget;
  *(int *)0x31e494 = -1; /* DAT_0031e494 — unknown purpose */

  container_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */, widget_tag_id);
  if (*container_tag != 0 || *(int *)((char *)container_tag + 0x3e0) < 3) {
    display_assert(
      "expected the multiplayer profile select screen to be a container w/ "
      "3+ children",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x768,
      1);
    system_exit(-1);
  }

  list_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */,
                              **(int **)((char *)widget + 0x34));
  if (*list_tag != 2) {
    display_assert(
      "expected a spinner list widget for 'multiplayer profile list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x76b,
      1);
    system_exit(-1);
  }

  if (*(int *)((char *)list_tag + 0x3e0) != 3) {
    display_assert(
      "expected 3 list items for 'multiplayer profile list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x76c,
      1);
    system_exit(-1);
  }

  widget = *(void **)((char *)widget + 0x34);
  list_widget = (int *)widget;
  list_index = *(short *)((char *)list_widget + 0x3c);
  if (list_index < 0 ||
      (int)list_index >= (int)*(unsigned short *)((char *)list_widget + 0x44)) {
    display_assert(
      "invalid multiplayer profile specified from 'multiplayer profile "
      "list' list widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x775,
      1);
    system_exit(-1);
  }

  profile_index = (*(int **)((char *)list_widget +
                             0x40))[*(short *)((char *)list_widget + 0x3c)];

  if (profile_index != -1) {
    if (profile_index & 0x80000000) {
      player_ui_begin_editing_profile(profile_index);
      result = true;
    } else {
      display_error_deferred(0x1f, -1, true, false);
      ui_play_audio_feedback_sound(4);
    }
  } else {
    ui_play_audio_feedback_sound(4);
  }
  return result;
}

/* 0x0eb000 (single data xref at 0x31e1f4) - clears DAT_0031e494 to -1,
 * ends the current player-profile edit (0x0e1760), returns true
 * (MOV AL,1). */
bool FUN_000eb000(void)
{
  *(int *)0x31e494 = -1; /* DAT_0031e494 — unknown purpose */
  player_ui_end_editing_profile();
  return true;
}

/* apply selected game engine item (event handler, data xref 0x31e1f8,
 * 0x14 bytes after player_profiles_list_dispose's 0x31e1e4 entry, same
 * ui_widget_event_handler_fn pointer array as the select_game_engine_item
 * table entry at 0x31e220) — 0xeb020. Runs the inverse of
 * playlist_profile_initialize_game_engine's (0xecd50) profile-to-widget
 * table: fetches the in-progress playlist-profile edit copy
 * (player_ui_get_edit_playlist_profile, called unconditionally first,
 * before the parent-widget check — order preserved), then asserts the
 * widget's PARENT (+0x30, not widget itself) is a column-list widget
 * (+0xe == 3), same "expected column list" display_assert/system_exit(-1)
 * shape as the sibling handlers.
 *
 * If no playlist profile is being edited, logs error(2, "failed to
 * retrieve editable game variant") and returns false.
 *
 * Otherwise remaps the parent's selected-index field (+0x3c, sign-extended
 * per the original's MOVSX) through the table 0 -> 1, 1 -> 4, 2 -> 2,
 * 3 -> 3, 4 -> 5 (exactly the inverse of select_game_engine_item's
 * default -> 0, 2 -> 2, 3 -> 3, 4 -> 1, 5 -> 4 pairing) into a local. Any
 * other value logs error(2, "unknown game engine option selected") and
 * falls back to the profile's current value at +0x18 (a self-comparison
 * no-op, preserved verbatim from the disassembly's MOV ESI,[EDI+0x18]
 * reload on the default arm). If the remapped value differs from the
 * profile's current dword field at +0x18 (unproven — pointed-to type of
 * the profile is void* upstream, same offset select_game_engine_item
 * reads), clears 0x18 bytes at profile+0x4c (csmemset) before storing the
 * new value into profile+0x18. Always returns true on the profile-found
 * path; event_data and widget_deleted are unused, same "3-arg handler
 * typedef pushed by the dispatcher regardless" shape noted at
 * select_game_engine_item. */
bool playlist_profile_set_game_engine(void *widget, void *event_data,
                                      bool *widget_deleted)
{
  void *profile;
  void *parent;
  bool result;
  int game_engine; /* name: PAL 2342 ui_widget_event_handler_functions.c:2878 */

  (void)event_data;
  (void)widget_deleted;

  profile = player_ui_get_edit_playlist_profile();
  parent = *(void **)((char *)widget + 0x30);
  result = true;

  if (parent == NULL || *(int16_t *)((char *)parent + 0xe) != 3) {
    display_assert(
      "expected column list for game engine type list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x7a6,
      true);
    system_exit(-1);
  }

  if (profile != NULL) {
    switch (*(int16_t *)((char *)parent + 0x3c)) {
    case 0:
      game_engine = 1;
      break;
    case 1:
      game_engine = 4;
      break;
    case 2:
      game_engine = 2;
      break;
    case 3:
      game_engine = 3;
      break;
    case 4:
      game_engine = 5;
      break;
    default:
      error(2, "unknown game engine option selected");
      game_engine = *(int *)((char *)profile + 0x18);
      break;
    }

    if (game_engine != *(int *)((char *)profile + 0x18)) {
      csmemset((char *)profile + 0x4c, 0, 0x18);
    }
    *(int *)((char *)profile + 0x18) = game_engine;
  } else {
    error(2, "failed to retrieve editable game variant");
    result = false;
  }
  return result;
}

/* 0x0eb100 (single data xref at 0x31e1fc, same handler table as 0x0eb000)
 * - fetches the in-progress playlist-profile edit copy; if present,
 * launches the virtual keyboard on it (buffer size 0x18, caption index 9)
 * and logs error(2, 0x2865d0) when the launch fails, returning true either
 * way (BL=1). With no profile, logs error(2, 0x286550) and returns false. */
bool FUN_000eb100(void)
{
  wchar_t *profile;
  bool result;

  profile = (wchar_t *)player_ui_get_edit_playlist_profile();
  result = true;
  if (profile != NULL) {
    if (!virtual_keyboard_launch(profile, 0x18, 9)) {
      error(2, "failed to invoke virtual keyboard on profile name");
    }
  } else {
    error(2, "failed to retrieve editable game variant");
    result = false;
  }
  return result;
}

/* apply capture-the-flag rules (event handler) — 0xeb150. Fetches the
 * in-progress playlist-profile edit copy (player_ui_get_edit_playlist_profile,
 * called first; the NULL test at 0xeb15c precedes any [EBP+8] read), then
 * walks five consecutive list items under the widget's first child (+0x34),
 * each followed via the sibling link (+0x2c). For each item, scans the item's
 * child chain (+0x34, following +0x2c) for the first widget whose type field
 * (+0xe) is 2 (option spinner list) and reads its selected index (+0x3c,
 * MOVSX). Missing items/spinners trip display_assert/system_exit(-1) at
 * source lines 0x7f8/0x7fa, 0x803/0x805, 0x812/0x814, 0x81d/0x81f and
 * 0x828/0x82a.
 *
 * Profile stores (field meanings unproven; profile is void* upstream):
 *   'assault'           index 0 -> byte +0x4c = 1, index 1 -> 0
 *   'single flag'       index 0..5 -> dword +0x50 = 0, 0x708, 0xe10,
 *                       0x1518, 0x2328, 0x4650 (jump table 0xeb4c0)
 *   'flag must reset'   index 0 -> byte +0x4e = 1, index 1 -> 0
 *   'flag at home ...'  index 0 -> byte +0x4f = 1, index 1 -> 0
 *   'captures to win'   index 0..4 -> dword +0x40 = 1, 3, 5, 10, 15
 *                       (jump table 0xeb4d8)
 * Any other index logs error(2, ...) and leaves that field untouched. Then
 * pops the widget stack for the widget's u16 at +0x8 (MOVZX-style XOR/MOV CX
 * at 0xeb492). Returns true on BOTH paths: MOV AL,1 at 0xeb4a1 and again at
 * 0xeb4b7 after error(2, "failed to retrieve editable game variant"). Only
 * [EBP+8] is read, so only the widget parameter is declared. */
bool playlist_profile_change_ctf_rules(void *widget)
{
  void *profile;
  void *item;
  void *spinner;

  profile = player_ui_get_edit_playlist_profile();

  if (profile != NULL) {
    item = *(void **)((char *)widget + 0x34);
    if (item == NULL) {
      display_assert(
        "expected 'assault' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x7f8, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'assault' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x7fa, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *((char *)profile + 0x4c) = 1;
      break;
    case 1:
      *((char *)profile + 0x4c) = 0;
      break;
    default:
      error(2, "unknown option selected in 'assault' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'single flag' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x803, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'single flag' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x805, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *(int *)((char *)profile + 0x50) = 0;
      break;
    case 1:
      *(int *)((char *)profile + 0x50) = 0x708;
      break;
    case 2:
      *(int *)((char *)profile + 0x50) = 0xe10;
      break;
    case 3:
      *(int *)((char *)profile + 0x50) = 0x1518;
      break;
    case 4:
      *(int *)((char *)profile + 0x50) = 0x2328;
      break;
    case 5:
      *(int *)((char *)profile + 0x50) = 0x4650;
      break;
    default:
      error(2, "unknown option selected in 'single flag' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'flag must reset' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x812, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'flag must reset' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x814, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *((char *)profile + 0x4e) = 1;
      break;
    case 1:
      *((char *)profile + 0x4e) = 0;
      break;
    default:
      error(2,
            "unknown option selected in 'flag must reset' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'flag at home to score' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x81d, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'flag at home to score' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x81f, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *((char *)profile + 0x4f) = 1;
      break;
    case 1:
      *((char *)profile + 0x4f) = 0;
      break;
    default:
      error(2, "unknown option selected in 'flag at home to score' option "
               "spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'captures to win' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x828, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'captures to win' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x82a, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *(int *)((char *)profile + 0x40) = 1;
      break;
    case 1:
      *(int *)((char *)profile + 0x40) = 3;
      break;
    case 2:
      *(int *)((char *)profile + 0x40) = 5;
      break;
    case 3:
      *(int *)((char *)profile + 0x40) = 10;
      break;
    case 4:
      *(int *)((char *)profile + 0x40) = 15;
      break;
    default:
      error(2,
            "unknown option selected in 'captures to win' option spinner list");
      break;
    }

    ui_widgets_pop_stack(*(uint16_t *)((char *)widget + 0x8));
    return true;
  }

  error(2, "failed to retrieve editable game variant");
  return true;
}

/* apply koth rules (event handler table index 43) — 0xeb4f0. Same shape as
 * playlist_profile_change_slayer_rules with three list items under the
 * widget's first child (+0x34, sibling link +0x2c); each item's spinner is the
 * first child whose type (+0xe) is 2, read by its selected index (+0x3c).
 * Missing items/spinners trip display_assert/system_exit(-1) at source lines
 * 0x84e/0x850, 0x859/0x85b and 0x867/0x869.
 *   'moving hill'   index 0 -> byte +0x4c = 1, index 1 -> 0
 *   'score to win'  index 0..4 -> dword +0x40 = 1, 2, 5, 10, 15
 *                   (jump table 0xeb6f4)
 *   'teams'         index 0 -> byte +0x1c = 1, index 1 -> 0
 * Any other index logs error(2, ...) and leaves that field untouched. Then
 * pops the widget stack for the widget's u16 at +0x8 and returns true. With no
 * profile being edited it logs error(2, ...) and returns false. */
bool playlist_profile_change_koth_rules(void *widget)
{
  void *profile;
  void *item;
  void *spinner;

  profile = player_ui_get_edit_playlist_profile();

  if (profile != NULL) {
    item = *(void **)((char *)widget + 0x34);
    if (item == NULL) {
      display_assert(
        "expected 'moving hill' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x84e, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'moving hill' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x850, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *((char *)profile + 0x4c) = 1;
      break;
    case 1:
      *((char *)profile + 0x4c) = 0;
      break;
    default:
      error(2, "unknown option selected in 'moving hill' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'score to win' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x859, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'score to win' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x85b, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *(int *)((char *)profile + 0x40) = 1;
      break;
    case 1:
      *(int *)((char *)profile + 0x40) = 2;
      break;
    case 2:
      *(int *)((char *)profile + 0x40) = 5;
      break;
    case 3:
      *(int *)((char *)profile + 0x40) = 10;
      break;
    case 4:
      *(int *)((char *)profile + 0x40) = 15;
      break;
    default:
      error(2, "unknown option selected in 'score to win' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'teams' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x867, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'teams' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x869, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *((char *)profile + 0x1c) = 1;
      break;
    case 1:
      *((char *)profile + 0x1c) = 0;
      break;
    default:
      error(2, "unknown option selected in 'teams' option spinner list");
      break;
    }

    ui_widgets_pop_stack(*(uint16_t *)((char *)widget + 0x8));
    return true;
  }

  error(2, "failed to retrieve editable game variant");
  return false;
}

/* apply slayer rules (event handler) — 0xeb710. Same shape as
 * playlist_profile_change_ctf_rules: fetches the in-progress playlist-profile
 * edit copy first (player_ui_get_edit_playlist_profile; NULL test at 0xeb721
 * precedes the [EBP+8] read), then walks five consecutive list items under
 * the widget's first child (+0x34), following the sibling link (+0x2c). For
 * each item, scans its child chain (+0x34, following +0x2c) for the first
 * widget whose type (+0xe) is 2 and reads its selected index (+0x3c, MOVSX).
 * Missing items/spinners trip display_assert/system_exit(-1) at source lines
 * 0x88b/0x88d, 0x896/0x898, 0x8a1/0x8a3, 0x8ac/0x8ae and 0x8ba/0x8bc.
 *
 * Profile stores (field meanings unproven; profile is void* upstream):
 *   'death bonus'    index 0 -> byte +0x4c = 0, index 1 -> 1
 *   'kill in order'  index 0 -> byte +0x4e = 1, index 1 -> 0
 *   'kill penalty'   index 0 -> byte +0x4d = 0, index 1 -> 1
 *   'kills to win'   index 0..4 -> dword +0x40 = 5, 10, 15, 25, 50
 *                    (jump table 0xeba5c)
 *   'teams'          index 0 -> byte +0x1c = 1, index 1 -> 0
 * Any other index logs error(2, ...) and leaves that field untouched. Then
 * pops the widget stack for the widget's u16 at +0x8 and returns true
 * (MOV AL,1 at 0xeba3c). Unlike the ctf sibling, the no-profile path returns
 * false (XOR AL,AL at 0xeba52) after error(2, ...). Only [EBP+8] is read. */
bool playlist_profile_change_slayer_rules(void *widget)
{
  void *profile;
  void *item;
  void *spinner;

  profile = player_ui_get_edit_playlist_profile();

  if (profile != NULL) {
    item = *(void **)((char *)widget + 0x34);
    if (item == NULL) {
      display_assert(
        "expected 'death bonus' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x88b, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'death bonus' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x88d, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *((char *)profile + 0x4c) = 0;
      break;
    case 1:
      *((char *)profile + 0x4c) = 1;
      break;
    default:
      error(2, "unknown option selected in 'death bonus' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'kill in order' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x896, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'kill in order' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x898, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *((char *)profile + 0x4e) = 1;
      break;
    case 1:
      *((char *)profile + 0x4e) = 0;
      break;
    default:
      error(2,
            "unknown option selected in 'kill in order' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'kill penalty' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x8a1, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'kill penalty' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x8a3, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *((char *)profile + 0x4d) = 0;
      break;
    case 1:
      *((char *)profile + 0x4d) = 1;
      break;
    default:
      error(2, "unknown option selected in 'kill penalty' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'kills to win' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x8ac, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'kills to win' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x8ae, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *(int *)((char *)profile + 0x40) = 5;
      break;
    case 1:
      *(int *)((char *)profile + 0x40) = 10;
      break;
    case 2:
      *(int *)((char *)profile + 0x40) = 15;
      break;
    case 3:
      *(int *)((char *)profile + 0x40) = 25;
      break;
    case 4:
      *(int *)((char *)profile + 0x40) = 50;
      break;
    default:
      error(2, "unknown option selected in 'kills to win' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'teams' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x8ba, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'teams' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x8bc, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *((char *)profile + 0x1c) = 1;
      break;
    case 1:
      *((char *)profile + 0x1c) = 0;
      break;
    default:
      error(2, "unknown option selected in 'teams' option spinner list");
      break;
    }

    ui_widgets_pop_stack(*(uint16_t *)((char *)widget + 0x8));
    return true;
  }

  error(2, "failed to retrieve editable game variant");
  return false;
}

/* apply oddball rules (event handler) — 0xeba70. Same shape as
 * playlist_profile_change_slayer_rules: fetches the in-progress
 * playlist-profile edit copy first (player_ui_get_edit_playlist_profile; NULL
 * test at 0xeba7e precedes the [EBP+8] read at 0xeba84), then walks eight
 * consecutive list items under the widget's first child (+0x34), following
 * the sibling link (+0x2c). For each item, scans its child chain (+0x34,
 * following +0x2c) for the first widget whose type (+0xe) is 2 and reads its
 * selected index (+0x3c, MOVSX). Missing items/spinners trip
 * display_assert/system_exit(-1) at source lines 0x8de/0x8e0, 0x8eb/0x8ed,
 * 0x8f8/0x8fa, 0x904/0x906, 0x910/0x912, 0x91b/0x91d, 0x936/0x938 and
 * 0x944/0x946.
 *
 * Profile stores (field meanings unproven; profile is void* upstream):
 *   'trait with ball'    index 0..3 -> dword +0x54 = 0..3 (jump table 0xebfbc)
 *   'trait without ball' index 0..3 -> dword +0x58 = 0..3 (jump table 0xebfcc)
 *   'speed with ball'    index 0 -> dword +0x50 = 1, 1 -> 0, 2 -> 2
 *   'ball type'          index 0..2 -> dword +0x5c = 0..2
 *   'random start'       index 0 -> byte +0x4c = 1, index 1 -> 0
 *   'ball spawn count'   index 0..15 -> dword +0x60 = index + 1
 *   'score to win'       index 0..4 -> dword +0x40 = 1, 2, 5, 10, 15
 *                        (jump table 0xebfdc)
 *   'teams'              index 0 -> byte +0x1c = 1, index 1 -> 0
 * Any other index logs error(2, ...) and leaves that field untouched. Then
 * pops the widget stack for the widget's u16 at +0x8 and returns true
 * (MOV AL,1 at 0xebf9f). The no-profile path returns false (XOR AL,AL at
 * 0xebfb4) after error(2, ...). Only [EBP+8] is read. */
bool playlist_profile_change_oddball_rules(void *widget)
{
  void *profile;
  void *item;
  void *spinner;
  int index;

  profile = player_ui_get_edit_playlist_profile();

  if (profile != NULL) {
    item = *(void **)((char *)widget + 0x34);
    if (item == NULL) {
      display_assert(
        "expected 'trait with ball' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x8de, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'trait with ball' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x8e0, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *(int *)((char *)profile + 0x54) = 0;
      break;
    case 1:
      *(int *)((char *)profile + 0x54) = 1;
      break;
    case 2:
      *(int *)((char *)profile + 0x54) = 2;
      break;
    case 3:
      *(int *)((char *)profile + 0x54) = 3;
      break;
    default:
      error(2,
            "unknown option selected in 'trait with ball' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'trait without ball' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x8eb, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'trait without ball' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x8ed, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *(int *)((char *)profile + 0x58) = 0;
      break;
    case 1:
      *(int *)((char *)profile + 0x58) = 1;
      break;
    case 2:
      *(int *)((char *)profile + 0x58) = 2;
      break;
    case 3:
      *(int *)((char *)profile + 0x58) = 3;
      break;
    default:
      error(2, "unknown option selected in 'trait without ball' option spinner "
               "list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'speed with ball' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x8f8, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'speed with ball' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x8fa, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *(int *)((char *)profile + 0x50) = 1;
      break;
    case 1:
      *(int *)((char *)profile + 0x50) = 0;
      break;
    case 2:
      *(int *)((char *)profile + 0x50) = 2;
      break;
    default:
      error(2,
            "unknown option selected in 'speed with ball' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'ball type' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x904, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'ball type' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x906, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *(int *)((char *)profile + 0x5c) = 0;
      break;
    case 1:
      *(int *)((char *)profile + 0x5c) = 1;
      break;
    case 2:
      *(int *)((char *)profile + 0x5c) = 2;
      break;
    default:
      error(2, "unknown option selected in 'ball type' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'random start' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x910, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'random start' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x912, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *((char *)profile + 0x4c) = 1;
      break;
    case 1:
      *((char *)profile + 0x4c) = 0;
      break;
    default:
      error(2, "unknown option selected in 'random start' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'ball spawn count' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x91b, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'ball spawn count' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x91d, true);
      system_exit(-1);
    }

    index = *(int16_t *)((char *)spinner + 0x3c);
    if (index >= 0 && index <= 15) {
      *(int *)((char *)profile + 0x60) = index + 1;
    } else {
      error(2, "unknown option selected in 'ball spawn count' option spinner "
               "list");
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'score to win' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x936, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'score to win' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x938, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *(int *)((char *)profile + 0x40) = 1;
      break;
    case 1:
      *(int *)((char *)profile + 0x40) = 2;
      break;
    case 2:
      *(int *)((char *)profile + 0x40) = 5;
      break;
    case 3:
      *(int *)((char *)profile + 0x40) = 10;
      break;
    case 4:
      *(int *)((char *)profile + 0x40) = 15;
      break;
    default:
      error(2, "unknown option selected in 'score to win' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'teams' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x944, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'teams' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x946, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *((char *)profile + 0x1c) = 1;
      break;
    case 1:
      *((char *)profile + 0x1c) = 0;
      break;
    default:
      error(2, "unknown option selected in 'teams' option spinner list");
      break;
    }

    ui_widgets_pop_stack(*(uint16_t *)((char *)widget + 0x8));
    return true;
  }

  error(2, "failed to retrieve editable game variant");
  return false;
}

/* apply racing rules (event handler, data xref 0x31e210) — 0xebff0. Same
 * shape as playlist_profile_change_oddball_rules: fetches the in-progress
 * playlist-profile edit copy first (player_ui_get_edit_playlist_profile; NULL
 * test at 0xebffd precedes the [EBP+8] read at 0xec003), then walks four
 * consecutive list items under the widget's first child (+0x34), following
 * the sibling link (+0x2c). For each item, scans its child chain (+0x34,
 * following +0x2c) for the first widget whose type (+0xe) is 2 and reads its
 * selected index (+0x3c, MOVSX). Missing items/spinners trip
 * display_assert/system_exit(-1) at source lines 0x968/0x96a, 0x974/0x976,
 * 0x980/0x982 and 0x98f/0x991.
 *
 * Profile stores (field meanings unproven; profile is void* upstream):
 *   'team scoring' index 0..2 -> dword +0x50 = 0..2
 *   'race type'    index 0..2 -> dword +0x4c = 0..2
 *   'laps to win'  index 0..5 -> dword +0x40 = 1, 3, 5, 10, 15, 25
 *                  (jump table 0xec2a8)
 *   'teams'        index 0 -> byte +0x1c = 1, index 1 -> 0
 * Any other index logs error(2, ...) and leaves that field untouched. Then
 * pops the widget stack for the widget's u16 at +0x8 and returns true
 * (MOV AL,1 at 0xec28d). The no-profile path returns false (XOR AL,AL at
 * 0xec2a1) after error(2, ...). Only [EBP+8] is read; the kb decl was
 * void(void), corrected to bool(void *widget) from those reads. */
bool playlist_profile_change_racing_rules(void *widget)
{
  void *profile;
  void *item;
  void *spinner;

  profile = player_ui_get_edit_playlist_profile();

  if (profile != NULL) {
    item = *(void **)((char *)widget + 0x34);
    if (item == NULL) {
      display_assert(
        "expected 'team scoring' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x968, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'team scoring' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x96a, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *(int *)((char *)profile + 0x50) = 0;
      break;
    case 1:
      *(int *)((char *)profile + 0x50) = 1;
      break;
    case 2:
      *(int *)((char *)profile + 0x50) = 2;
      break;
    default:
      error(2, "unknown option selected in 'team scoring' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'race type' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x974, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'race type' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x976, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *(int *)((char *)profile + 0x4c) = 0;
      break;
    case 1:
      *(int *)((char *)profile + 0x4c) = 1;
      break;
    case 2:
      *(int *)((char *)profile + 0x4c) = 2;
      break;
    default:
      error(2, "unknown option selected in 'race type' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'laps to win' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x980, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'laps to win' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x982, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *(int *)((char *)profile + 0x40) = 1;
      break;
    case 1:
      *(int *)((char *)profile + 0x40) = 3;
      break;
    case 2:
      *(int *)((char *)profile + 0x40) = 5;
      break;
    case 3:
      *(int *)((char *)profile + 0x40) = 10;
      break;
    case 4:
      *(int *)((char *)profile + 0x40) = 15;
      break;
    case 5:
      *(int *)((char *)profile + 0x40) = 25;
      break;
    default:
      error(2, "unknown option selected in 'laps to win' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'teams' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x98f, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'teams' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x991, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *((char *)profile + 0x1c) = 1;
      break;
    case 1:
      *((char *)profile + 0x1c) = 0;
      break;
    default:
      error(2, "unknown option selected in 'teams' option spinner list");
      break;
    }

    ui_widgets_pop_stack(*(uint16_t *)((char *)widget + 0x8));
    return true;
  }

  error(2, "failed to retrieve editable game variant");
  return false;
}

/* apply player options (event handler) — 0xec2c0. Same shape as
 * playlist_profile_change_racing_rules: fetches the in-progress
 * playlist-profile edit copy first (NULL test at 0xec2cb precedes the
 * [EBP+8] read at 0xec2d3), then walks consecutive list items under the
 * widget's first child (+0x34) via the sibling link (+0x2c). For each item,
 * scans its child chain for the first widget whose type (+0xe) is 2 and
 * reads its selected index (+0x3c, MOVSX).
 *
 * Profile stores (field meanings unproven; profile is void* upstream):
 *   'number of lives'       0..3 -> dword +0x38 = 0, 1, 3, 5
 *   'maximum health'        0..5 -> float +0x3c = 0.5, 1, 1.5, 2, 3, 4
 *   'shields'               0 -> dword +0x20 &= ~0x8, 1 -> |= 0x8
 *   'respawn time'          0..3 -> dword +0x30 = 0, 150, 300, 450
 *   'respawn time growth'   0..3 -> dword +0x2c = 0, 150, 300, 450
 *   'odd man out'           0 -> byte +0x28 = 1, 1 -> 0
 *   'invisible players'     0 -> dword +0x20 |= 0x10, 1 -> &= ~0x10
 *   'suicide penalty'       0..3 -> dword +0x34 = 0, 150, 300, 450
 * Any other index logs error(2, ...) and leaves that field untouched. The
 * 'suicide penalty' item itself is optional: a NULL sibling at 0xec730
 * skips straight to the return with no assert. Unlike the racing handler
 * there is no ui_widgets_pop_stack call; every exit on the profile path
 * returns true (MOV AL,1), the no-profile path returns false (XOR AL,AL at
 * 0xec7da). Only [EBP+8] is read; the kb decl was void(void), corrected to
 * bool(void *widget) from those reads. */
bool playlist_profile_change_player_options(void *widget)
{
  void *profile;
  void *item;
  void *spinner;
  bool result;

  result = true;
  profile = player_ui_get_edit_playlist_profile();

  if (profile != NULL) {
    item = *(void **)((char *)widget + 0x34);
    if (item == NULL) {
      display_assert(
        "expected 'number of lives' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x9b3, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'number of lives' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x9b5, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *(int *)((char *)profile + 0x38) = 0;
      break;
    case 1:
      *(int *)((char *)profile + 0x38) = 1;
      break;
    case 2:
      *(int *)((char *)profile + 0x38) = 3;
      break;
    case 3:
      *(int *)((char *)profile + 0x38) = 5;
      break;
    default:
      error(2,
            "unknown option selected in 'number of lives' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'maximum health' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x9c0, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'maximum health' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x9c2, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *(float *)((char *)profile + 0x3c) = 0.5f;
      break;
    case 1:
      *(float *)((char *)profile + 0x3c) = 1.0f;
      break;
    case 2:
      *(float *)((char *)profile + 0x3c) = 1.5f;
      break;
    case 3:
      *(float *)((char *)profile + 0x3c) = 2.0f;
      break;
    case 4:
      *(float *)((char *)profile + 0x3c) = 3.0f;
      break;
    case 5:
      *(float *)((char *)profile + 0x3c) = 4.0f;
      break;
    default:
      error(2,
            "unknown option selected in 'maximum health' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'shields' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x9cf, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'shields' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x9d1, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *(uint32_t *)((char *)profile + 0x20) &= ~0x8u;
      break;
    case 1:
      *(uint32_t *)((char *)profile + 0x20) |= 0x8u;
      break;
    default:
      error(2, "unknown option selected in 'shields' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'respawn time' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x9da, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'respawn time' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x9dc, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *(int *)((char *)profile + 0x30) = 0;
      break;
    case 1:
      *(int *)((char *)profile + 0x30) = 150;
      break;
    case 2:
      *(int *)((char *)profile + 0x30) = 300;
      break;
    case 3:
      *(int *)((char *)profile + 0x30) = 450;
      break;
    default:
      error(2, "unknown option selected in 'respawn time' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'respawn time growth' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x9e7, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'respawn time growth' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x9e9, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *(int *)((char *)profile + 0x2c) = 0;
      break;
    case 1:
      *(int *)((char *)profile + 0x2c) = 150;
      break;
    case 2:
      *(int *)((char *)profile + 0x2c) = 300;
      break;
    case 3:
      *(int *)((char *)profile + 0x2c) = 450;
      break;
    default:
      error(
        2,
        "unknown option selected in 'respawn time growth' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'odd man out' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x9f4, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'odd man out' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x9f6, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *((char *)profile + 0x28) = 1;
      break;
    case 1:
      *((char *)profile + 0x28) = 0;
      break;
    default:
      error(2, "unknown option selected in 'odd man out' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'invisible players' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x9ff, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'invisible players' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xa01, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *(uint32_t *)((char *)profile + 0x20) |= 0x10u;
      break;
    case 1:
      *(uint32_t *)((char *)profile + 0x20) &= ~0x10u;
      break;
    default:
      error(
        2,
        "unknown option selected in 'invisible players' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item != NULL) {
      for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
           spinner = *(void **)((char *)spinner + 0x2c)) {
        if (*(int16_t *)((char *)spinner + 0xe) == 2) {
          break;
        }
      }
      if (spinner == NULL) {
        display_assert(
          "expected 'suicide penalty' option spinner list",
          "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
          0xa0e, true);
        system_exit(-1);
      }

      switch (*(int16_t *)((char *)spinner + 0x3c)) {
      case 0:
        *(int *)((char *)profile + 0x34) = 0;
        break;
      case 1:
        *(int *)((char *)profile + 0x34) = 150;
        break;
      case 2:
        *(int *)((char *)profile + 0x34) = 300;
        break;
      case 3:
        *(int *)((char *)profile + 0x34) = 450;
        break;
      default:
        error(
          2,
          "unknown option selected in 'suicide penalty' option spinner list");
        break;
      }
    }
  } else {
    error(2, "failed to retrieve editable game variant");
    result = false;
  }
  return result;
}

/* apply item options (event handler) — 0xec840. Same shape as
 * playlist_profile_change_player_options: fetches the in-progress
 * playlist-profile edit copy first (TEST EBX,EBX / JZ at 0xec84b precedes
 * the [EBP+8] read at 0xec853), then walks four consecutive list items under
 * the widget's first child (+0x34) via the sibling link (+0x2c). For each
 * item, scans its child chain for the first widget whose type (+0xe) is 2
 * and reads its selected index (+0x3c, MOVSX).
 *
 * Profile stores (field meanings unproven; profile is void* upstream):
 *   'infinite grenades'   0 -> dword +0x20 |= 0x4, 1 -> &= ~0x4
 *   'vehicle set'         0..4  -> dword +0x48 = index (jump table 0xecb18)
 *   'weapon set'          0..10 -> dword +0x44 = index (jump table 0xecb2c)
 *   'starting equipment'  0 -> dword +0x20 &= ~0x20, 1 -> |= 0x20
 * Any other index logs error(2, ...) and leaves that field untouched. Every
 * item is mandatory (asserts at lines 0xa30/0xa32, 0xa3b/0xa3d, 0xa49/0xa4b,
 * 0xa5e/0xa60). Every exit on the profile path returns true (MOV AL,1); the
 * no-profile path returns false (XOR AL,AL at 0xecb13). Only [EBP+8] is
 * read; the kb decl was void(void), corrected to bool(void *widget) from
 * those reads, matching the sibling handlers. */
bool playlist_profile_change_item_options(void *widget)
{
  void *profile;
  void *item;
  void *spinner;

  profile = player_ui_get_edit_playlist_profile();

  if (profile != NULL) {
    item = *(void **)((char *)widget + 0x34);
    if (item == NULL) {
      display_assert(
        "expected 'infinite grenades' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xa30, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'infinite grenades' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xa32, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *(uint32_t *)((char *)profile + 0x20) |= 0x4u;
      break;
    case 1:
      *(uint32_t *)((char *)profile + 0x20) &= ~0x4u;
      break;
    default:
      error(
        2,
        "unknown option selected in 'infinite grenades' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'vehicle set' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xa3b, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'vehicle set' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xa3d, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *(int *)((char *)profile + 0x48) = 0;
      break;
    case 1:
      *(int *)((char *)profile + 0x48) = 1;
      break;
    case 2:
      *(int *)((char *)profile + 0x48) = 2;
      break;
    case 3:
      *(int *)((char *)profile + 0x48) = 3;
      break;
    case 4:
      *(int *)((char *)profile + 0x48) = 4;
      break;
    default:
      error(2, "unknown option selected in 'vehicle set' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'weapon set' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xa49, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'weapon set' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xa4b, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *(int *)((char *)profile + 0x44) = 0;
      break;
    case 1:
      *(int *)((char *)profile + 0x44) = 1;
      break;
    case 2:
      *(int *)((char *)profile + 0x44) = 2;
      break;
    case 3:
      *(int *)((char *)profile + 0x44) = 3;
      break;
    case 4:
      *(int *)((char *)profile + 0x44) = 4;
      break;
    case 5:
      *(int *)((char *)profile + 0x44) = 5;
      break;
    case 6:
      *(int *)((char *)profile + 0x44) = 6;
      break;
    case 7:
      *(int *)((char *)profile + 0x44) = 7;
      break;
    case 8:
      *(int *)((char *)profile + 0x44) = 8;
      break;
    case 9:
      *(int *)((char *)profile + 0x44) = 9;
      break;
    case 10:
      *(int *)((char *)profile + 0x44) = 10;
      break;
    default:
      error(2, "unknown option selected in 'weapon set' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'starting equipment' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xa5e, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'starting equipment' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xa60, true);
      system_exit(-1);
    }

    switch (*(int16_t *)((char *)spinner + 0x3c)) {
    case 0:
      *(uint32_t *)((char *)profile + 0x20) &= ~0x20u;
      break;
    case 1:
      *(uint32_t *)((char *)profile + 0x20) |= 0x20u;
      break;
    default:
      error(
        2,
        "unknown option selected in 'starting equipment' option spinner list");
      break;
    }

    return true;
  }

  error(2, "failed to retrieve editable game variant");
  return false;
}

/* apply multiplayer radar/friends display options (event handler, data
 * xref 0x31e21c in the same ui_widget_event_handler_fn pointer array as
 * ui_widget_game_data_select_game_engine_item at 0x31e220) — 0xecb60.
 * Runs the widget-to-profile direction: fetches the in-progress
 * playlist-profile edit copy (player_ui_get_edit_playlist_profile,
 * called unconditionally first, before the widget is even loaded —
 * order preserved, and here the NULL test precedes the asserts, per the
 * TEST EBX,EBX / JZ at 0xecb6b before [EBP+8] is read at 0xecb73), then
 * walks three consecutive list items under the widget's first child
 * (+0x34), each followed via the sibling link (+0x2c).
 *
 * For each list item the handler scans that item's own child chain
 * (+0x34, following +0x2c) for the first widget whose type field
 * (+0xe) is 2 — the option-spinner list — and reads its selected-index
 * field (+0x3c) sign-extended (MOVSX, matching the sibling handlers'
 * selected-item slot). Every missing item or missing spinner trips the
 * same display_assert/system_exit(-1) shape as the sibling handlers,
 * at source lines 0xa80/0xa82, 0xa8c/0xa8e and 0xa97/0xa99.
 *
 * Item 1 ('radar display') writes the selected index straight through
 * to the profile's dword field at +0x24 (0, 1 or 2). Item 2 ('other
 * players on radar') sets (index 0) or clears (index 1) bit 0 of the
 * profile's flag dword at +0x20. Item 3 ('friends on screen') sets
 * (index 0) or clears (index 1) bit 1 of that same flag dword. Both
 * profile offsets are unproven field meanings — the profile is void*
 * at player_ui_get_edit_playlist_profile's kb decl, the same upstream
 * untyped producer the sibling handlers read +0x18 through.
 *
 * An out-of-range index on any of the three logs error(2, ...) with
 * that item's own message and leaves the corresponding profile field
 * untouched (the second block's common store at 0xecc98 is skipped
 * entirely on the default arm). Returns true whenever a profile was
 * retrieved, false after error(2, "failed to retrieve editable game
 * variant") when none is being edited. event_data/widget_deleted are
 * not read here; only [EBP+8] is touched, so only the widget parameter
 * is declared, same as the single-param sibling
 * ui_widget_game_data_select_game_engine_item. */
bool FUN_000ecb60(void *widget)
{
  void *profile;
  void *item;
  void *spinner;
  int16_t selected;

  profile = player_ui_get_edit_playlist_profile();

  if (profile != NULL) {
    item = *(void **)((char *)widget + 0x34);
    if (item == NULL) {
      display_assert(
        "expected 'radar display' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xa80, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'radar display' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xa82, true);
      system_exit(-1);
    }

    selected = *(int16_t *)((char *)spinner + 0x3c);
    switch (selected) {
    case 0:
      *(int *)((char *)profile + 0x24) = 0;
      break;
    case 1:
      *(int *)((char *)profile + 0x24) = 1;
      break;
    case 2:
      *(int *)((char *)profile + 0x24) = 2;
      break;
    default:
      error(2,
            "unknown option selected in 'radar display' option spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'other players on radar' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xa8c, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'other players on radar' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xa8e, true);
      system_exit(-1);
    }

    selected = *(int16_t *)((char *)spinner + 0x3c);
    switch (selected) {
    case 0:
      *(uint32_t *)((char *)profile + 0x20) =
        *(uint32_t *)((char *)profile + 0x20) | 1;
      break;
    case 1:
      *(uint32_t *)((char *)profile + 0x20) =
        *(uint32_t *)((char *)profile + 0x20) & 0xfffffffe;
      break;
    default:
      error(2, "unknown option selected in 'other players on radar' option "
               "spinner list");
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'friends on screen' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xa97, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'friends on screen' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xa99, true);
      system_exit(-1);
    }

    selected = *(int16_t *)((char *)spinner + 0x3c);
    switch (selected) {
    case 0:
      *(uint32_t *)((char *)profile + 0x20) =
        *(uint32_t *)((char *)profile + 0x20) | 2;
      break;
    case 1:
      *(uint32_t *)((char *)profile + 0x20) =
        *(uint32_t *)((char *)profile + 0x20) & 0xfffffffd;
      break;
    default:
      error(
        2,
        "unknown option selected in 'friends on screen' option spinner list");
      break;
    }

    return true;
  }

  error(2, "failed to retrieve editable game variant");
  return false;
}

/* select game engine item (event handler table index 50, data xref
 * 0x31e220 in the same ui_widget_event_handler_fn pointer array
 * ui_widget_event_handler_function_invoke indexes at 0x31e158; single-param
 * shape matches difficulty_menu_initialize (0xf0640,
 * table index 99) — the 3-arg handler typedef is pushed by the
 * dispatcher regardless, event_data/widget_deleted are simply not read
 * here) — 0xecd50. Fetches the in-progress playlist-profile edit copy
 * (player_ui_get_edit_playlist_profile, called unconditionally first,
 * before widget is even loaded — order preserved) then asserts widget
 * is a column-list widget (+0xe == 3, same "expected a column list"
 * display_assert/system_exit(-1) shape as the difficulty-item sibling).
 *
 * If no playlist profile is being edited, logs error(2, "failed to
 * retrieve editable game variant") and returns false.
 *
 * Otherwise remaps the profile's dword field at +0x18 (unproven —
 * pointed-to type of the profile is void* upstream, offset falls in
 * game_variant_t's un-split unk_2[] padding) through a fixed table
 * into the widget's selected-index field (+0x3c, the same "selected
 * list item" slot difficulty_menu_initialize uses):
 * profile field 1 (and anything outside [1,5], unsigned) -> 0, 2 -> 2,
 * 3 -> 3, 4 -> 1, 5 -> 4. This exact case/value pairing is Ghidra's
 * resolved jump-table decode (0xecd95 JMP [EAX*4+0xecdf0]) and is not
 * re-derivable from the visible disassembly text alone, so the mapping
 * is taken verbatim from the decompiler's switch rather than assumed
 * sequential.
 *
 * Reloads the just-stored selected index from +0x3c (sign-extended,
 * matching the original's MOVSX reload instead of reusing a cached
 * value) to call widget_instance_get_nth_child(widget, index), and
 * stores the resulting child pointer at the selected-child field
 * (+0x38, paired with +0x3c the same way across this widget family).
 * Always returns true on the profile-found path. */
bool playlist_profile_initialize_game_engine(void *widget)
{
  void *profile;
  void *
    focused_child; /* name: PAL 2342 ui_widget_event_handler_functions.c:3353 */

  profile = player_ui_get_edit_playlist_profile();

  if (*(int16_t *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected a column list for the list of available game engines",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xab2,
      true);
    system_exit(-1);
  }

  if (profile != NULL) {
    switch (*(int *)((char *)profile + 0x18)) {
    case 4:
      *(int16_t *)((char *)widget + 0x3c) = 1;
      break;
    case 2:
      *(int16_t *)((char *)widget + 0x3c) = 2;
      break;
    case 3:
      *(int16_t *)((char *)widget + 0x3c) = 3;
      break;
    case 5:
      *(int16_t *)((char *)widget + 0x3c) = 4;
      break;
    case 1:
      goto default_game_engine;
    default:
    default_game_engine:
      *(int16_t *)((char *)widget + 0x3c) = 0;
      break;
    }

    focused_child = widget_instance_get_nth_child(
      widget, *(int16_t *)((char *)widget + 0x3c));
    *(void **)((char *)widget + 0x38) = focused_child;

    return true;
  }

  error(2, "failed to retrieve editable game variant");
  return false;
}

/* multiplayer profile init name (event handler, data xref 0x31e224,
 * same ui_widget_event_handler_fn pointer array as the game-engine-item
 * handlers above) — 0xece10. Fetches the in-progress playlist-profile
 * edit copy (player_ui_get_edit_playlist_profile, called unconditionally
 * first, before the widget-type check — order preserved per
 * disassembly), then asserts widget itself (not a parent) is a text box
 * widget (+0xe == 1), same "expected text box widget for profile name"
 * display_assert/system_exit(-1) shape as the sibling handlers, and the
 * same +0xe==1 text-box check widget_instance_render_text_box's siblings use.
 *
 * If a profile is being edited, (re)allocates a 0x100-byte name buffer
 * through ui_widget_realloc (same stack_memory_pool_realloc wrapper and
 * +0x3c buffer-pointer slot widget_instance_render_text_box above uses),
 * passing the widget's existing +0x3c buffer pointer as the realloc input. The
 * result is stored back to +0x3c unconditionally right after the call
 * (MOV before the NULL-test JZ in the disassembly, order preserved).
 * On successful allocation, copies up to 0x7f wide characters from the
 * profile pointer itself (not an offset field — the profile struct's
 * name is its first member, per the disassembly passing EDI, the raw
 * profile pointer, as ustrncpy's source with no added offset) into the
 * new buffer via ustrncpy, then null-terminates at wchar_t index 0x7f
 * (byte offset 0xfe) by reloading the buffer pointer from +0x3c rather
 * than reusing the local (matches the disassembly's MOV ECX,[ESI+0x3c]
 * reload). Always returns true on the profile-found path.
 *
 * If no playlist profile is being edited, logs error(2, "failed to
 * retrieve editable game variant") (same message/severity as the
 * sibling handlers) and returns false; event_data and widget_deleted are
 * unused, same 3-arg handler typedef shape as the sibling handlers
 * above. */
bool playlist_profile_initialize_name(void *widget, void *event_data,
                                      bool *widget_deleted)
{
  void *profile;
  void *name_buffer;
  bool result;

  (void)event_data;
  (void)widget_deleted;

  profile = player_ui_get_edit_playlist_profile();
  result = true;

  if (*(int16_t *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected text box widget for profile name",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xad2,
      true);
    system_exit(-1);
  }

  if (profile != NULL) {
    name_buffer = ui_widget_realloc(
      *(int *)((char *)widget + 0x3c), 0x100,
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0xad6);
    *(void **)((char *)widget + 0x3c) = name_buffer;

    if (name_buffer != NULL) {
      ustrncpy((wchar_t *)name_buffer, (wchar_t *)profile, 0x7f);
      *(int16_t *)((char *)*(void **)((char *)widget + 0x3c) + 0xfe) = 0;
    }
  } else {
    error(2, "failed to retrieve editable game variant");
    result = false;
  }

  return result;
}

/* multiplayer profile init CTF rules (event handler) — 0xeceb0. Inverse of
 * playlist_profile_change_ctf_rules: reads the in-progress playlist-profile
 * edit copy and writes each option spinner's selected index (+0x3c).
 * player_ui_get_edit_playlist_profile is called first, then the widget
 * itself must be a column list (+0xe == 3, assert line 0xaed). Each list
 * item is reached through +0x34 (first child) / +0x2c (next sibling), and
 * each item's spinner is the first child with +0xe == 2.
 *   'assault'          byte +0x4c == 0 -> 1, else 0 (MOVZX/SUB EBX/JZ
 *                      switch shape at 0xecf5d)
 *   'single flag'      dword +0x50: 0x708->1, 0xe10->2, 0x1518->3,
 *                      0x2328->4, 0x4650->5, 0 and anything else -> 0
 *   'flag must reset'  byte +0x4e != 0 -> 0, else 1
 *   'flag at home ...' byte +0x4f != 0 -> 0, else 1
 *   'captures to win'  dword +0x40: 3->1, 5->2, 10->3, 15->4, else 0
 *                      (DEC EAX / CMP 0xe jump table at 0xed19e: the case
 *                      range starts at 1, so case 1 shares the 0 body)
 * The explicit CMP EAX,EBX against 0 at 0xecfe5 shows case 0 exists and
 * shares the default body. Match-sensitive: the explicit case 0/1 labels
 * keep their own duplicate bodies. Stacked labels let cl.exe fold the byte
 * switches to SETE and lose the XOR EBX,EBX zero register (68.6% VC71 vs
 * 100%). Returns true on the profile path (MOV AL,1),
 * false after error(2, ...) when no profile is being edited. */
bool playlist_profile_initialize_ctf_rules(void *widget)
{
  void *profile;
  void *item;
  void *spinner;

  profile = player_ui_get_edit_playlist_profile();

  if (*(int16_t *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for multiplayer game settings widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xaed,
      true);
    system_exit(-1);
  }

  if (profile != NULL) {
    item = *(void **)((char *)widget + 0x34);
    if (item == NULL) {
      display_assert(
        "expected 'assault' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xaf5, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'assault' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xaf7, true);
      system_exit(-1);
    }

    switch (*((unsigned char *)profile + 0x4c)) {
    case 0:
      *(int16_t *)((char *)spinner + 0x3c) = 1;
      break;
    case 1:
      *(int16_t *)((char *)spinner + 0x3c) = 0;
      break;
    default:
      *(int16_t *)((char *)spinner + 0x3c) = 0;
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'single flag' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xb00, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'single flag' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xb02, true);
      system_exit(-1);
    }

    switch (*(int *)((char *)profile + 0x50)) {
    case 0x708:
      *(int16_t *)((char *)spinner + 0x3c) = 1;
      break;
    case 0xe10:
      *(int16_t *)((char *)spinner + 0x3c) = 2;
      break;
    case 0x1518:
      *(int16_t *)((char *)spinner + 0x3c) = 3;
      break;
    case 0x2328:
      *(int16_t *)((char *)spinner + 0x3c) = 4;
      break;
    case 0x4650:
      *(int16_t *)((char *)spinner + 0x3c) = 5;
      break;
    case 0:
      *(int16_t *)((char *)spinner + 0x3c) = 0;
      break;
    default:
      *(int16_t *)((char *)spinner + 0x3c) = 0;
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'flag must reset' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xb0f, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'flag must reset' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xb11, true);
      system_exit(-1);
    }

    switch (*((unsigned char *)profile + 0x4e)) {
    case 0:
      *(int16_t *)((char *)spinner + 0x3c) = 1;
      break;
    case 1:
      *(int16_t *)((char *)spinner + 0x3c) = 0;
      break;
    default:
      *(int16_t *)((char *)spinner + 0x3c) = 0;
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'flag at home to score' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xb1a, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'flag at home to score' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xb1c, true);
      system_exit(-1);
    }

    switch (*((unsigned char *)profile + 0x4f)) {
    case 0:
      *(int16_t *)((char *)spinner + 0x3c) = 1;
      break;
    case 1:
      *(int16_t *)((char *)spinner + 0x3c) = 0;
      break;
    default:
      *(int16_t *)((char *)spinner + 0x3c) = 0;
      break;
    }

    item = *(void **)((char *)item + 0x2c);
    if (item == NULL) {
      display_assert(
        "expected 'captures to win' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xb25, true);
      system_exit(-1);
    }

    for (spinner = *(void **)((char *)item + 0x34); spinner != NULL;
         spinner = *(void **)((char *)spinner + 0x2c)) {
      if (*(int16_t *)((char *)spinner + 0xe) == 2) {
        break;
      }
    }
    if (spinner == NULL) {
      display_assert(
        "expected 'captures to win' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xb27, true);
      system_exit(-1);
    }

    switch (*(int *)((char *)profile + 0x40)) {
    case 1:
      *(int16_t *)((char *)spinner + 0x3c) = 0;
      return true;
    default:
      *(int16_t *)((char *)spinner + 0x3c) = 0;
      return true;
    case 3:
      *(int16_t *)((char *)spinner + 0x3c) = 1;
      return true;
    case 5:
      *(int16_t *)((char *)spinner + 0x3c) = 2;
      return true;
    case 10:
      *(int16_t *)((char *)spinner + 0x3c) = 3;
      return true;
    case 15:
      *(int16_t *)((char *)spinner + 0x3c) = 4;
      return true;
    }
  }

  error(2, "failed to retrieve editable game variant");
  return false;
}

/* network game server defer game start (event handler table index 84,
 * 0x0f0030; data xref 0x31e2a8) — pauses the network game server's countdown
 * if a server exists. The arguments are never read; always returns true. */
bool network_game_server_defer_game_start(void *widget, void *event_data,
                                          bool *widget_deleted)
{
  void *server;

  server = global_network_game_server_get();
  if (server != NULL) {
    network_game_server_pause_countdown(server, 1);
  }
  return true;
}

/* koth rules initializer (event handler table index 53, 0x0ed240) — fetches
 * the in-progress playlist profile, asserts the widget is a column list
 * (+0xe == 3, line 0xb43), then walks the list items (first child +0x34, next
 * sibling +0x2c); each item's spinner is the first child with +0xe == 2.
 * Missing items/spinners assert at 0xb4b/0xb4d, 0xb56/0xb58 and 0xb64/0xb66.
 *   'moving hill'   byte +0x4c: 0 -> 1, else 0
 *   'score to win'  dword +0x40: 2->1, 5->2, 10->3, 15->4, else 0
 *                   (byte map 0xed45c over 1..15 into jump table 0xed444)
 *   'teams'         byte +0x1c: 0 -> 1, else 0
 * Returns true on the profile path (MOV AL,1), false after error(2, ...) when
 * no profile is being edited. */
bool playlist_profile_initialize_koth_rules(void *widget)
{
  void *profile;
  void *list_item;
  void *option_spinner;

  profile = player_ui_get_edit_playlist_profile();

  if (*(int16_t *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for multiplayer game settings widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xb43,
      true);
    system_exit(-1);
  }

  if (profile != NULL) {
    list_item = *(void **)((char *)widget + 0x34);
    if (list_item == NULL) {
      display_assert(
        "expected 'moving hill' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xb4b, true);
      system_exit(-1);
    }

    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2) {
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'moving hill' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xb4d, true);
      system_exit(-1);
    }

    switch (*(uint8_t *)((char *)profile + 0x4c)) {
    case 0:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      break;
    default:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    }

    list_item = *(void **)((char *)list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'score to win' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xb56, true);
      system_exit(-1);
    }

    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2) {
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'score to win' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xb58, true);
      system_exit(-1);
    }

    switch (*(int *)((char *)profile + 0x40)) {
    case 2:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      break;
    case 5:
      *(int16_t *)((char *)option_spinner + 0x3c) = 2;
      break;
    case 10:
      *(int16_t *)((char *)option_spinner + 0x3c) = 3;
      break;
    case 15:
      *(int16_t *)((char *)option_spinner + 0x3c) = 4;
      break;
    default:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    }

    list_item = *(void **)((char *)list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'teams' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xb64, true);
      system_exit(-1);
    }

    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2) {
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'teams' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xb66, true);
      system_exit(-1);
    }

    switch (*(uint8_t *)((char *)profile + 0x1c)) {
    case 0:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      break;
    case 1:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    default:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    }
    return true;
  }

  error(2, "failed to retrieve editable game variant");
  return false;
}

/* playlist profile slayer rules initializer (0x0ed470) -- fetches the
 * in-progress playlist profile, asserts the widget is a column list
 * (+0xe == 3), then walks the list items (first child +0x34, next sibling
 * +0x2c); each item's spinner is the first child with +0xe == 2.
 *   'death bonus'    byte +0x4c: 1 -> 1, 0 and else -> 0
 *   'kill in order'  byte +0x4e: 0 -> 1, 1 -> 0, else unchanged
 *   'kill penalty'   byte +0x4d: 1 -> 1, 0 and else -> 0
 *   'kills to win'   dword +0x40: 10->1, 15->2, 25->3, 50->4, else 0
 *                    (jump table spans 5..50)
 *   'teams'          byte +0x1c: 0 -> 1, else 0
 * Returns true on the profile path (MOV AL,1), false after
 * error(2, ...) when no profile is being edited. */
bool playlist_profile_initialize_slayer_rules(void *widget)
{
  void *profile;
  void *list_item;
  void *option_spinner;

  profile = player_ui_get_edit_playlist_profile();

  if (*(int16_t *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for multiplayer game settings widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xb7f,
      true);
    system_exit(-1);
  }

  if (profile != NULL) {
    list_item = *(void **)((char *)widget + 0x34);
    if (list_item == NULL) {
      display_assert(
        "expected 'death bonus' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xb87, true);
      system_exit(-1);
    }

    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2) {
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'death bonus' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xb89, true);
      system_exit(-1);
    }

    switch (*(uint8_t *)((char *)profile + 0x4c)) {
    case 0:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    case 1:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      break;
    default:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    }

    list_item = *(void **)((char *)list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'kill in order' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xb92, true);
      system_exit(-1);
    }

    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2) {
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'kill in order' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xb94, true);
      system_exit(-1);
    }

    switch (*(uint8_t *)((char *)profile + 0x4e)) {
    case 0:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      break;
    case 1:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    }

    list_item = *(void **)((char *)list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'kill penalty' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xb9d, true);
      system_exit(-1);
    }

    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2) {
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'kill penalty' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xb9f, true);
      system_exit(-1);
    }

    switch (*(uint8_t *)((char *)profile + 0x4d)) {
    case 0:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    case 1:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      break;
    default:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    }

    list_item = *(void **)((char *)list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'kills to win' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xba8, true);
      system_exit(-1);
    }

    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2) {
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'kills to win' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xbaa, true);
      system_exit(-1);
    }

    switch (*(int *)((char *)profile + 0x40)) {
    case 5:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    case 10:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      break;
    case 15:
      *(int16_t *)((char *)option_spinner + 0x3c) = 2;
      break;
    case 25:
      *(int16_t *)((char *)option_spinner + 0x3c) = 3;
      break;
    case 50:
      *(int16_t *)((char *)option_spinner + 0x3c) = 4;
      break;
    default:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    }

    list_item = *(void **)((char *)list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'teams' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xbb6, true);
      system_exit(-1);
    }

    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2) {
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'teams' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xbb8, true);
      system_exit(-1);
    }

    switch (*(uint8_t *)((char *)profile + 0x1c)) {
    case 0:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      return true;
    case 1:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    default:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    }

    return true;
  }

  error(2, "failed to retrieve editable game variant");
  return false;
}

/* playlist profile oddball rules initializer (0x0ed7c0). */
bool playlist_profile_initialize_oddball_rules(void *widget)
{
  void *profile;
  void *list_item;
  void *option_spinner;
  int value;

  profile = player_ui_get_edit_playlist_profile();
  if (*(int16_t *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for multiplayer game settings widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xbd1,
      true);
    system_exit(-1);
  }
  if (profile == NULL) {
    error(2, "failed to retrieve editable game variant");
    return false;
  }

  list_item = *(void **)((char *)widget + 0x34);
  if (list_item == NULL) {
    display_assert(
      "expected 'trait with ball' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xbd9,
      true);
    system_exit(-1);
  }
  option_spinner = *(void **)((char *)list_item + 0x34);
  while (option_spinner != NULL &&
         *(int16_t *)((char *)option_spinner + 0xe) != 2)
    option_spinner = *(void **)((char *)option_spinner + 0x2c);
  if (option_spinner == NULL) {
    display_assert(
      "expected 'trait with ball' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xbdb,
      true);
    system_exit(-1);
  }
  switch (*(int *)((char *)profile + 0x54)) {
  case 1:
    *(int16_t *)((char *)option_spinner + 0x3c) = 1;
    break;
  case 2:
    *(int16_t *)((char *)option_spinner + 0x3c) = 2;
    break;
  case 3:
    *(int16_t *)((char *)option_spinner + 0x3c) = 3;
    break;
  default:
    *(int16_t *)((char *)option_spinner + 0x3c) = 0;
    break;
  }

  list_item = *(void **)((char *)list_item + 0x2c);
  if (list_item == NULL) {
    display_assert(
      "expected 'trait without ball' list item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xbe6,
      true);
    system_exit(-1);
  }
  option_spinner = *(void **)((char *)list_item + 0x34);
  while (option_spinner != NULL &&
         *(int16_t *)((char *)option_spinner + 0xe) != 2)
    option_spinner = *(void **)((char *)option_spinner + 0x2c);
  if (option_spinner == NULL) {
    display_assert(
      "expected 'trait without ball' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xbe8,
      true);
    system_exit(-1);
  }
  switch (*(int *)((char *)profile + 0x58)) {
  case 1:
    *(int16_t *)((char *)option_spinner + 0x3c) = 1;
    break;
  case 2:
    *(int16_t *)((char *)option_spinner + 0x3c) = 2;
    break;
  case 3:
    *(int16_t *)((char *)option_spinner + 0x3c) = 3;
    break;
  default:
    *(int16_t *)((char *)option_spinner + 0x3c) = 0;
    break;
  }

  list_item = *(void **)((char *)list_item + 0x2c);
  if (list_item == NULL) {
    display_assert(
      "expected 'speed with ball' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xbf3,
      true);
    system_exit(-1);
  }
  option_spinner = *(void **)((char *)list_item + 0x34);
  while (option_spinner != NULL &&
         *(int16_t *)((char *)option_spinner + 0xe) != 2)
    option_spinner = *(void **)((char *)option_spinner + 0x2c);
  if (option_spinner == NULL) {
    display_assert(
      "expected 'speed with ball' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xbf5,
      true);
    system_exit(-1);
  }
  value = *(int *)((char *)profile + 0x50);
  switch (value) {
  case 0:
    *(int16_t *)((char *)option_spinner + 0x3c) = 1;
    break;
  case 2:
    *(int16_t *)((char *)option_spinner + 0x3c) = 2;
    break;
  default:
    *(int16_t *)((char *)option_spinner + 0x3c) = 0;
    break;
  }

  list_item = *(void **)((char *)list_item + 0x2c);
  if (list_item == NULL) {
    display_assert(
      "expected 'ball type' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xbff,
      true);
    system_exit(-1);
  }
  option_spinner = *(void **)((char *)list_item + 0x34);
  while (option_spinner != NULL &&
         *(int16_t *)((char *)option_spinner + 0xe) != 2)
    option_spinner = *(void **)((char *)option_spinner + 0x2c);
  if (option_spinner == NULL) {
    display_assert(
      "expected 'ball type' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc01,
      true);
    system_exit(-1);
  }
  switch (*(int *)((char *)profile + 0x5c)) {
  case 0:
    *(int16_t *)((char *)option_spinner + 0x3c) = 0;
    break;
  case 1:
    *(int16_t *)((char *)option_spinner + 0x3c) = 1;
    break;
  case 2:
    *(int16_t *)((char *)option_spinner + 0x3c) = 2;
    break;
  default:
    *(int16_t *)((char *)option_spinner + 0x3c) = 0;
    break;
  }

  list_item = *(void **)((char *)list_item + 0x2c);
  if (list_item == NULL) {
    display_assert(
      "expected 'random start' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc0b,
      true);
    system_exit(-1);
  }
  option_spinner = *(void **)((char *)list_item + 0x34);
  while (option_spinner != NULL &&
         *(int16_t *)((char *)option_spinner + 0xe) != 2)
    option_spinner = *(void **)((char *)option_spinner + 0x2c);
  if (option_spinner == NULL) {
    display_assert(
      "expected 'random start' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc0d,
      true);
    system_exit(-1);
  }
  if (*(uint8_t *)((char *)profile + 0x4c) == 0)
    *(int16_t *)((char *)option_spinner + 0x3c) = 1;
  else
    *(int16_t *)((char *)option_spinner + 0x3c) = 0;

  list_item = *(void **)((char *)list_item + 0x2c);
  if (list_item == NULL) {
    display_assert(
      "expected 'ball spawn count' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc16,
      true);
    system_exit(-1);
  }
  option_spinner = *(void **)((char *)list_item + 0x34);
  while (option_spinner != NULL &&
         *(int16_t *)((char *)option_spinner + 0xe) != 2)
    option_spinner = *(void **)((char *)option_spinner + 0x2c);
  if (option_spinner == NULL) {
    display_assert(
      "expected 'ball spawn count' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc18,
      true);
    system_exit(-1);
  }
  value = *(int *)((char *)profile + 0x60);
  if (value > 0 && value <= 16)
    *(int16_t *)((char *)option_spinner + 0x3c) = (int16_t)(value - 1);
  else
    *(int16_t *)((char *)option_spinner + 0x3c) = 0;

  list_item = *(void **)((char *)list_item + 0x2c);
  if (list_item == NULL) {
    display_assert(
      "expected 'score to win' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc31,
      true);
    system_exit(-1);
  }
  option_spinner = *(void **)((char *)list_item + 0x34);
  while (option_spinner != NULL &&
         *(int16_t *)((char *)option_spinner + 0xe) != 2)
    option_spinner = *(void **)((char *)option_spinner + 0x2c);
  if (option_spinner == NULL) {
    display_assert(
      "expected 'score to win' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc33,
      true);
    system_exit(-1);
  }
  switch (*(int *)((char *)profile + 0x40)) {
  case 2:
    *(int16_t *)((char *)option_spinner + 0x3c) = 1;
    break;
  case 5:
    *(int16_t *)((char *)option_spinner + 0x3c) = 2;
    break;
  case 10:
    *(int16_t *)((char *)option_spinner + 0x3c) = 3;
    break;
  case 15:
    *(int16_t *)((char *)option_spinner + 0x3c) = 4;
    break;
  default:
    *(int16_t *)((char *)option_spinner + 0x3c) = 0;
    break;
  }

  list_item = *(void **)((char *)list_item + 0x2c);
  if (list_item == NULL) {
    display_assert(
      "expected 'teams' item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc3f,
      true);
    system_exit(-1);
  }
  option_spinner = *(void **)((char *)list_item + 0x34);
  while (option_spinner != NULL &&
         *(int16_t *)((char *)option_spinner + 0xe) != 2)
    option_spinner = *(void **)((char *)option_spinner + 0x2c);
  if (option_spinner == NULL) {
    display_assert(
      "expected 'teams' option spinner list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc41,
      true);
    system_exit(-1);
  }
  switch (*(uint8_t *)((char *)profile + 0x1c)) {
  case 0:
    *(int16_t *)((char *)option_spinner + 0x3c) = 1;
    return true;
  case 1:
    *(int16_t *)((char *)option_spinner + 0x3c) = 0;
    break;
  default:
    *(int16_t *)((char *)option_spinner + 0x3c) = 0;
    break;
  }
  return true;
}

/* playlist profile racing rules initializer (0x0edcd0) — fetches the
 * in-progress playlist profile, asserts the widget is a column list
 * (+0xe == 3), then walks the list items (first child +0x34, next sibling
 * +0x2c); each item's spinner is the first child with +0xe == 2, and the
 * spinner's selection is written to +0x3c.
 *   'team scoring'  dword +0x50: 1->1, 2->2, else 0
 *   'race type'     dword +0x4c: 1->1, 2->2, else 0
 *   'laps to win'   dword +0x40: 3->1, 5->2, 10->3, 15->4, 25->5, else 0
 *   'teams'         byte  +0x1c: 0->1, else 0
 * Signature from binary: reads [EBP+8] (widget) and returns AL (MOV AL,1
 * on the profile path, XOR AL,AL after error(2, ...)). */
bool playlist_profile_initialize_racing_rules(void *widget)
{
  void *profile;
  void *list_item;
  void *option_spinner;

  profile = player_ui_get_edit_playlist_profile();

  if (*(int16_t *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for multiplayer game settings widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xc5a,
      true);
    system_exit(-1);
  }

  if (profile != NULL) {
    list_item = *(void **)((char *)widget + 0x34);
    if (list_item == NULL) {
      display_assert(
        "expected 'team scoring' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xc62, true);
      system_exit(-1);
    }

    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2) {
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'team scoring' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xc64, true);
      system_exit(-1);
    }

    switch (*(int *)((char *)profile + 0x50)) {
    case 0:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    case 1:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      break;
    case 2:
      *(int16_t *)((char *)option_spinner + 0x3c) = 2;
      break;
    default:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    }
    list_item = *(void **)((char *)list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'race type' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xc6e, true);
      system_exit(-1);
    }
    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2) {
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'race type' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xc70, true);
      system_exit(-1);
    }

    switch (*(int *)((char *)profile + 0x4c)) {
    case 0:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    case 1:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      break;
    case 2:
      *(int16_t *)((char *)option_spinner + 0x3c) = 2;
      break;
    default:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    }

    list_item = *(void **)((char *)list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'laps to win' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xc7a, true);
      system_exit(-1);
    }
    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2) {
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'laps to win' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xc7c, true);
      system_exit(-1);
    }

    switch (*(int *)((char *)profile + 0x40)) {
    case 1: /* name: PAL 2342 ui_widget_event_handler_functions.c:4824 */
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    case 3:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      break;
    case 5:
      *(int16_t *)((char *)option_spinner + 0x3c) = 2;
      break;
    case 10:
      *(int16_t *)((char *)option_spinner + 0x3c) = 3;
      break;
    case 15:
      *(int16_t *)((char *)option_spinner + 0x3c) = 4;
      break;
    case 25:
      *(int16_t *)((char *)option_spinner + 0x3c) = 5;
      break;
    default:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    }

    list_item = *(void **)((char *)list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'teams' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xc89, true);
      system_exit(-1);
    }

    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2) {
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'teams' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xc8b, true);
      system_exit(-1);
    }

    switch (*(uint8_t *)((char *)profile + 0x1c)) {
    case 0:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      return true;
    case 1:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    default:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    }

    return true;
  }

  error(2, "failed to retrieve editable game variant");
  return false;
}

/* playlist profile player options initializer (0x0edfb0) — fetches the
 * in-progress playlist profile, asserts the widget is a column list
 * (+0xe == 3), then walks the list items (first child +0x34, next sibling
 * +0x2c); each item's spinner is the first child with +0xe == 2, and the
 * spinner's selection is written to +0x3c.
 *   'number of lives'        dword +0x38: 1->1, 3->2, 5->3, else 0
 *   'maximum health'         float +0x3c: n = -(int)(v * -10.0f);
 *                            10->1, 15->2, 20->3, 30->4, 40->5, else 0
 *   'shields'                dword +0x20 bit 3 set -> 1, else 0
 *   'respawn time'           dword +0x30: 150->1, 300->2, 450->3, else 0
 *   'respawn time growth'    dword +0x2c: 150->1, 300->2, 450->3, else 0
 *   'odd man out'            byte  +0x28: 0->1, else 0
 *   'invisible players'      dword +0x20 bit 4 clear -> 1, else 0
 *   'suicide penalty'        dword +0x34: 150->1, 300->2, 450->3, else 0
 * The 'suicide penalty' spinner walk has no NULL assert in the binary
 * (0x0ee417..0x0ee430 falls straight into the store).
 * Jump tables: 0x0ee4a4 (lives, index = value, 0..5) and 0x0ee4d8/0x0ee4bc
 * (health, index = n - 5, 0..35); constant -10.0f at 0x287b78.
 * Signature from binary: reads [EBP+8] (widget) and returns AL (MOV AL,1
 * on the profile path, XOR AL,AL after error(2, ...)). */
bool playlist_profile_initialize_player_options(void *widget)
{
  void *profile;
  void *list_item;
  void *option_spinner;

  profile = player_ui_get_edit_playlist_profile();

  if (*(int16_t *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for multiplayer game settings widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xca5,
      true);
    system_exit(-1);
  }

  if (profile != NULL) {
    list_item = *(void **)((char *)widget + 0x34);
    if (list_item == NULL) {
      display_assert(
        "expected 'number of lives' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xcad, true);
      system_exit(-1);
    }

    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2) {
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'number of lives' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xcaf, true);
      system_exit(-1);
    }

    switch (*(int *)((char *)profile + 0x38)) {
    case 0:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    case 1:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      break;
    case 3:
      *(int16_t *)((char *)option_spinner + 0x3c) = 2;
      break;
    case 5:
      *(int16_t *)((char *)option_spinner + 0x3c) = 3;
      break;
    default:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    }

    list_item = *(void **)((char *)list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'maximum health' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xcba, true);
      system_exit(-1);
    }

    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2) {
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'maximum health' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xcbc, true);
      system_exit(-1);
    }

    switch (-(int)(*(float *)((char *)profile + 0x3c) * -10.0f)) {
    case 5:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    case 10:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      break;
    case 15:
      *(int16_t *)((char *)option_spinner + 0x3c) = 2;
      break;
    case 20:
      *(int16_t *)((char *)option_spinner + 0x3c) = 3;
      break;
    case 30:
      *(int16_t *)((char *)option_spinner + 0x3c) = 4;
      break;
    case 40:
      *(int16_t *)((char *)option_spinner + 0x3c) = 5;
      break;
    default:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    }

    list_item = *(void **)((char *)list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'shields' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xcc9, true);
      system_exit(-1);
    }

    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2) {
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'shields' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xccb, true);
      system_exit(-1);
    }

    switch ((*(unsigned int *)((char *)profile + 0x20) >> 3) & 1) {
    case 0:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    case 1:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      break;
    default:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    }

    list_item = *(void **)((char *)list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'respawn time' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xcd5, true);
      system_exit(-1);
    }

    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2) {
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'respawn time' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xcd7, true);
      system_exit(-1);
    }

    switch (*(int *)((char *)profile + 0x30)) {
    case 0:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    case 150:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      break;
    case 300:
      *(int16_t *)((char *)option_spinner + 0x3c) = 2;
      break;
    case 450:
      *(int16_t *)((char *)option_spinner + 0x3c) = 3;
      break;
    default:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    }

    list_item = *(void **)((char *)list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'respawn time growth' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xce2, true);
      system_exit(-1);
    }

    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2) {
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'respawn time growth' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xce4, true);
      system_exit(-1);
    }

    switch (*(int *)((char *)profile + 0x2c)) {
    case 0:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    case 150:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      break;
    case 300:
      *(int16_t *)((char *)option_spinner + 0x3c) = 2;
      break;
    case 450:
      *(int16_t *)((char *)option_spinner + 0x3c) = 3;
      break;
    default:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    }

    list_item = *(void **)((char *)list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'odd man out' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xcef, true);
      system_exit(-1);
    }

    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2) {
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'odd man out' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xcf1, true);
      system_exit(-1);
    }

    switch (*(uint8_t *)((char *)profile + 0x28)) {
    case 0:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      break;
    case 1:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    default:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    }

    list_item = *(void **)((char *)list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'invisible players' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xcfa, true);
      system_exit(-1);
    }

    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2) {
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'invisible players' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xcfc, true);
      system_exit(-1);
    }

    switch ((*(unsigned int *)((char *)profile + 0x20) >> 4) & 1) {
    case 0:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      break;
    case 1:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    default:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    }

    list_item = *(void **)((char *)list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'suicide penalty' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xd07, true);
      system_exit(-1);
    }

    /* no spinner NULL assert here in the binary */
    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2) {
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    }

    switch (*(int *)((char *)profile + 0x34)) {
    case 0:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    case 150:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      break;
    case 300:
      *(int16_t *)((char *)option_spinner + 0x3c) = 2;
      break;
    case 450:
      *(int16_t *)((char *)option_spinner + 0x3c) = 3;
      break;
    default:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    }

    return true;
  }

  error(2, "failed to retrieve editable game variant");
  return false;
}

/* playlist profile item options initializer (0x0ee500). */
bool playlist_profile_initialize_item_options(void *widget)
{
  void *profile;
  void *list_item;
  void *option_spinner;

  profile = player_ui_get_edit_playlist_profile();

  if (*(int16_t *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for multiplayer game settings widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xd26,
      true);
    system_exit(-1);
  }

  if (profile != NULL) {
    list_item = *(void **)((char *)widget + 0x34);
    if (list_item == NULL) {
      display_assert(
        "expected 'infinite grenades' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xd2e, true);
      system_exit(-1);
    }
    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2)
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    if (option_spinner == NULL) {
      display_assert(
        "expected 'infinite grenades' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xd30, true);
      system_exit(-1);
    }
    switch ((*(unsigned int *)((char *)profile + 0x20) >> 2) & 1) {
    case 0:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      break;
    case 1:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    default:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    }

    list_item = *(void **)((char *)list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'vehicle set' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xd3a, true);
      system_exit(-1);
    }
    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2)
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    if (option_spinner == NULL) {
      display_assert(
        "expected 'vehicle set' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xd3c, true);
      system_exit(-1);
    }
    switch (*(int *)((char *)profile + 0x48)) {
    case 0:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    case 1:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      break;
    case 2:
      *(int16_t *)((char *)option_spinner + 0x3c) = 2;
      break;
    case 3:
      *(int16_t *)((char *)option_spinner + 0x3c) = 3;
      break;
    case 4:
      *(int16_t *)((char *)option_spinner + 0x3c) = 4;
      break;
    default:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    }

    list_item = *(void **)((char *)list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'weapon set' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xd48, true);
      system_exit(-1);
    }
    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2)
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    if (option_spinner == NULL) {
      display_assert(
        "expected 'weapon set' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xd4a, true);
      system_exit(-1);
    }
    switch (*(int *)((char *)profile + 0x44)) {
    case 0:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    case 1:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      break;
    case 2:
      *(int16_t *)((char *)option_spinner + 0x3c) = 2;
      break;
    case 3:
      *(int16_t *)((char *)option_spinner + 0x3c) = 3;
      break;
    case 4:
      *(int16_t *)((char *)option_spinner + 0x3c) = 4;
      break;
    case 5:
      *(int16_t *)((char *)option_spinner + 0x3c) = 5;
      break;
    case 6:
      *(int16_t *)((char *)option_spinner + 0x3c) = 6;
      break;
    case 7:
      *(int16_t *)((char *)option_spinner + 0x3c) = 7;
      break;
    case 8:
      *(int16_t *)((char *)option_spinner + 0x3c) = 8;
      break;
    case 9:
      *(int16_t *)((char *)option_spinner + 0x3c) = 9;
      break;
    case 10:
      *(int16_t *)((char *)option_spinner + 0x3c) = 10;
      break;
    default:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    }

    list_item = *(void **)((char *)list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'starting equipment' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xd5d, true);
      system_exit(-1);
    }
    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2)
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    if (option_spinner == NULL) {
      display_assert(
        "expected 'starting equpiment' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xd5f, true);
      system_exit(-1);
    }
    switch ((*(unsigned int *)((char *)profile + 0x20) >> 5) & 1) {
    case 0:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      return true;
    case 1:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      return true;
    default:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      return true;
    }
  }

  error(2, "failed to retrieve editable game variant");
  return false;
}

/* playlist profile indicator options initializer (0x0ee810) — fetches the
 * in-progress playlist profile, asserts the widget is a column list
 * (+0xe == 3), then walks the list items (first child +0x34, next sibling
 * +0x2c); each item's spinner is the first child with +0xe == 2.
 *   'radar display'            dword +0x24: 1->1, 2->2, 0 and else -> 0
 *   'other players on radar'   dword +0x20 bit 0 clear -> 1, else 0
 *   'friends on screen'        dword +0x20 bit 1 clear -> 1, else 0
 * Returns true on the profile path (MOV AL,1), false after
 * error(2, ...) when no profile is being edited. */
bool playlist_profile_initialize_indicator_options(void *widget)
{
  void *profile;
  void *list_item; /* name: PAL 2342 ui_widget_event_handler_functions.c:4502 */
  void *option_spinner;

  profile = player_ui_get_edit_playlist_profile();

  if (*(int16_t *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for multiplayer game settings widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xd7a,
      true);
    system_exit(-1);
  }

  if (profile != NULL) {
    list_item = *(void **)((char *)widget + 0x34);
    if (list_item == NULL) {
      display_assert(
        "expected 'radar display' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xd82, true);
      system_exit(-1);
    }

    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2) {
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'radar display' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xd84, true);
      system_exit(-1);
    }

    switch (*(int *)((char *)profile + 0x24)) {
    case 0:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    case 1:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      break;
    case 2:
      *(int16_t *)((char *)option_spinner + 0x3c) = 2;
      break;
    default:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    }

    list_item = *(void **)((char *)list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'other players on radar' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xd8e, true);
      system_exit(-1);
    }

    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2) {
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'other players on radar' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xd90, true);
      system_exit(-1);
    }

    switch (*(int *)((char *)profile + 0x20) & 1) {
    case 0:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      break;
    case 1:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    default:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    }

    list_item = *(void **)((char *)list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'friends on screen' item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xd9a, true);
      system_exit(-1);
    }

    option_spinner = *(void **)((char *)list_item + 0x34);
    while (option_spinner != NULL &&
           *(int16_t *)((char *)option_spinner + 0xe) != 2) {
      option_spinner = *(void **)((char *)option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'friends on screen' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xd9c, true);
      system_exit(-1);
    }

    switch ((*(unsigned int *)((char *)profile + 0x20) >> 1) & 1) {
    case 0:
      *(int16_t *)((char *)option_spinner + 0x3c) = 1;
      return true;
    case 1:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    default:
      *(int16_t *)((char *)option_spinner + 0x3c) = 0;
      break;
    }

    return true;
  }

  error(2, "failed to retrieve editable game variant");
  return false;
}

/* multiplayer playlist profile edit dispose (0x0eea10) — asserts event_data is
 * non-null (halts and exits otherwise), then commits pending edits to the
 * multiplayer playlist profile. If nothing changed it reports the no-op, ends
 * the edit session, closes the widget's last child and marks *widget_deleted.
 * A dirty default profile whose name was never edited is instead routed
 * through the rename prompt. Otherwise the profile is saved and the save
 * result is returned. */
bool playlist_profile_save_changes(void *widget, void *event_data,
                                   bool *widget_deleted)
{
  void *last_child;
  bool result;

  result = false;

  if (event_data == NULL) {
    display_assert(
      "event",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xdb6,
      1);
    system_exit(-1);
  }

  if (player_ui_edit_profile_is_dirty()) {
    if (player_ui_edit_profile_is_default_profile() &&
        !player_ui_edit_profile_name_is_dirty()) {
      if (!player_ui_prompt_user_to_rename_edit_profile()) {
        error(2, "failed to prompt user to rename profile");
      }
      return result;
    }

    result = player_ui_save_profile();
    if (!result) {
      error(2, "failed to save changes to multiplayer playlist profile");
    }
  } else {
    error(2, "no changes to playlist profile detected; not saving to disk");
    player_ui_end_editing_profile();
    last_child = widget_instance_get_topmost_parent(widget);
    ui_widget_delete(last_child);
    *widget_deleted = 1;
  }

  return result;
}

/* player profile color picker menu initialize (0x0eead0) — widget is the
 * 'player color picker list' spinner list itself (definition tag 'DeLa',
 * type 2, 3 list items; asserts otherwise). (Re)allocates its per-index
 * indirection buffer at widget+0x40 sized to the player-profile-color count
 * (FUN_001c0ed0), fills it 0..count-1, and stores the count at widget+0x44.
 * Clamps the currently edited profile's color (profile+0x18) into
 * [0, count-1] and mirrors the clamped value into the widget's selected
 * index at widget+0x3c. Reports a deferred error but still returns true when
 * no profile is currently being edited. */
bool player_profile_color_picker_menu_initialize(void *widget, void *event_data,
                                                 bool *widget_deleted)
{
  unsigned short color_count;
  void *profile;
  void *buffer;
  short *list_tag;
  int i;
  short current_color;

  (void)event_data;
  (void)widget_deleted;

  color_count = FUN_001c0ed0();
  profile = player_ui_get_edit_player_profile();

  list_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */, *(int *)widget);
  if (*list_tag != 2) {
    display_assert(
      "expected a spinner list widget for 'player color picker list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xdf8,
      1);
    system_exit(-1);
  }

  if (*(int *)((char *)list_tag + 0x3e0) != 3) {
    display_assert(
      "expected 3 list items for 'player color picker list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xdf9,
      1);
    system_exit(-1);
  }

  buffer = ui_widget_realloc(
    *(int *)((char *)widget + 0x40), color_count,
    "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xdfd);
  *(void **)((char *)widget + 0x40) = buffer;
  if (buffer != NULL) {
    for (i = 0; i < (int)color_count; i++) {
      ((char *)buffer)[i] = (char)i;
    }
    *(unsigned short *)((char *)widget + 0x44) = color_count;
  }

  if (profile != NULL) {
    current_color = *(short *)((char *)profile + 0x18);
    if (current_color < 0) {
      current_color = 0;
    } else if (current_color >= (short)color_count) {
      current_color = (short)(color_count - 1);
    }
    *(short *)((char *)profile + 0x18) = current_color;
    *(short *)((char *)widget + 0x3c) = current_color;
    return true;
  }

  error(2, "failed to find editing player profile");
  return true;
}

/* color picker menu dispose (event handler table index 62, 0x0eebe0) — frees
 * the child widget cached at +0x40 back to the widget pool, if present. */
bool player_profile_color_picker_menu_dispose(void *widget, void *event_data,
                                              bool *widget_deleted)
{
  void *child;

  child = *(void **)((char *)widget + 0x40);
  if (child != NULL) {
    widget_free(child);
    *(void **)((char *)widget + 0x40) = NULL;
  }
  return true;
}

/* player profile color picker selection handler (event handler table index 63,
 * 0x0eec10) — validates the spinner list widget hanging off widget+0x38 (type
 * 2, definition tag 'DeLa' with 3 list items), bounds-checks the selected color
 * index at list_widget+0x3c against the profile colour count, then writes it
 * into the profile currently being edited at profile+0x18. Reports a deferred
 * error and returns false when no profile is being edited. */
bool player_profile_color_picker_select_color(void *widget, void *event_data,
                                              bool *widget_deleted)
{
  int *color_select_screen; /* name: PAL 2342
                               ui_widget_event_handler_functions.c:2836 */
  void *profile;
  short *list_tag;

  (void)event_data;
  (void)widget_deleted;

  color_select_screen = *(int **)((char *)widget + 0x38);
  profile = player_ui_get_edit_player_profile();

  if (color_select_screen == NULL ||
      *(short *)((char *)color_select_screen + 0xe) != 2) {
    display_assert(
      "expected the color select screen to contain a spinner list for the "
      "color picker",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xe2e,
      1);
    system_exit(-1);
  }

  list_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */, *color_select_screen);
  if (*list_tag != 2) {
    display_assert(
      "expected a spinner list widget for 'player color picker list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xe35,
      1);
    system_exit(-1);
  }

  if (*(int *)((char *)list_tag + 0x3e0) != 3) {
    display_assert(
      "expected 3 list items for 'player color picker list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xe36,
      1);
    system_exit(-1);
  }

  if (*(short *)((char *)color_select_screen + 0x3c) < 0 ||
      (int)*(short *)((char *)color_select_screen + 0x3c) >=
        (int)FUN_001c0ed0()) {
    display_assert(
      "invalid player profile color index specified",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xe3c,
      1);
    system_exit(-1);
  }

  if (profile != NULL) {
    *(short *)((char *)profile + 0x18) =
      *(short *)((char *)color_select_screen + 0x3c);
    return true;
  }
  error(2, "failed to set player profile color because no profile is currently "
           "being edited");
  return false;
}

/* player profile list selection handler (event handler table index 64,
 * 0x0eed10) — validates the 'player profile list' spinner widget (3 items)
 * hanging off widget+0x34, resolves the selected item's profile handle, and
 * either begins editing it, plays a deny sound (no profile / handle == -1),
 * or reports a deferred error (handle >= 0). */
bool player_profile_begin_editing(void *widget, void *event_data,
                                  bool *widget_deleted)
{
  short *list_tag;
  int *list_widget;
  short list_index;
  int profile_handle;

  (void)event_data;
  (void)widget_deleted;

  if (*(short *)((char *)widget + 0xe) != 0) {
    display_assert(
      "expected the player profile select screen to be a container widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xe53,
      1);
    system_exit(-1);
  }

  *(int *)0x31e494 = -1; /* DAT_0031e494 — unknown purpose, cleared here */

  list_widget = *(int **)((char *)widget + 0x34);
  list_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */, *(int *)list_widget);
  if (*list_tag != 2) {
    display_assert(
      "expected a spinner list widget for 'player profile list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xe5d,
      1);
    system_exit(-1);
  }

  if (*(int *)((char *)list_tag + 0x3e0) != 3) {
    display_assert(
      "expected 3 list items for 'player profile list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xe5e,
      1);
    system_exit(-1);
  }

  list_widget = *(int **)((char *)widget + 0x34);
  list_index = *(short *)((char *)list_widget + 0x3c);
  if (list_index < 0 ||
      (int)list_index >= (int)*(unsigned short *)((char *)list_widget + 0x44)) {
    display_assert(
      "invalid player profile specified from 'player profile list' list "
      "widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xe67,
      1);
    system_exit(-1);
  }

  profile_handle = (*(int **)((char *)list_widget + 0x40))[list_index];

  if (profile_handle == -1) {
    ui_play_audio_feedback_sound(4);
    return false;
  }

  if (profile_handle < 0) {
    player_ui_begin_editing_profile(profile_handle);
    return true;
  }

  display_error_deferred(0x1f, -1, true, false);
  ui_play_audio_feedback_sound(4);
  return false;
}

/* event handler (0x0eee40, table xref 0x31e25c) — clears the pending profile
 * handle (DAT_0031e494), ends the current player-profile edit, returns true. */
bool FUN_000eee40(void *widget, void *event_data, bool *widget_deleted)
{
  (void)widget;
  (void)event_data;
  (void)widget_deleted;

  *(int *)0x31e494 = -1;
  player_ui_end_editing_profile();
  return true;
}

/* event handler (0x0eee60, table xref 0x31e260) — fetches the editable player
 * profile and launches the virtual keyboard on its name (profile base passed
 * as the wchar_t buffer, 0x18 chars, caption 8). Reports via error() and
 * returns false if either step fails; returns true otherwise. */
bool FUN_000eee60(void)
{
  void *profile;
  bool result;

  profile = player_ui_get_edit_player_profile();
  result = true;
  if (profile != NULL) {
    if (!virtual_keyboard_launch((wchar_t *)profile, 0x18, 8)) {
      error(2, "failed to invoke virtual keyboard on player profile name");
      result = false;
    }
  } else {
    error(2, "failed to retrieve editable player profile");
    result = false;
  }

  return result;
}

/* player profile edit dispose (0x0eeeb0) — asserts event_data is non-null
 * (halts and exits otherwise), then saves any pending player-profile edits:
 * if nothing changed, no save is attempted; if a save was attempted and
 * succeeded, returns immediately. On no-op or save failure it reports the
 * condition via error(), ends the profile edit session, closes the widget's
 * last child, marks *widget_deleted, and returns false. */
bool player_profile_save_changes(void *widget, void *event_data,
                                 bool *widget_deleted)
{
  void *last_child;
  bool profile_dirty;
  bool result;
  const char *message;

  if (event_data == NULL) {
    display_assert(
      "event",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xeaf,
      1);
    system_exit(-1);
  }

  result = false;
  message = "no changes to player profile detected; not saving to disk";
  profile_dirty = player_ui_edit_profile_is_dirty();
  if (profile_dirty) {
    result = player_ui_save_profile();
    message = "failed to save changes to player profile";
  }

  if (!result) {
    error(2, message);
    player_ui_end_editing_profile();
    last_child = widget_instance_get_topmost_parent(widget);
    ui_widget_delete(last_child);
    *widget_deleted = 1;
  }

  return result;
}

/* controller settings initializer (0x0eef30) — validates the column-list
 * widget, finds the type-2 spinner under the 'joystick config' list item
 * (widget+0x34) and the 'button config' item that follows it (+0x2c), then
 * mirrors the editable profile's joystick (+0x29) and button (+0x28) config
 * bytes into each spinner's selected index (+0x3c). */
bool player_profile_initialize_controller_settings(void *widget)
{
  char *profile;
  char *list_item;
  char *option_spinner;

  profile = (char *)player_ui_get_edit_player_profile();

  if (*(short *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for controller settings widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xed1,
      1);
    system_exit(-1);
  }

  if (profile != NULL) {
    list_item = *(char **)((char *)widget + 0x34);
    if (list_item == NULL) {
      display_assert(
        "expected 'joystick config' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xed9, 1);
      system_exit(-1);
    }

    option_spinner = *(char **)(list_item + 0x34);
    while (option_spinner != NULL && *(short *)(option_spinner + 0xe) != 2) {
      option_spinner = *(char **)(option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'joystick config' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xedb, 1);
      system_exit(-1);
    }

    switch ((unsigned char)profile[0x29]) {
    case 1:
      *(short *)(option_spinner + 0x3c) = 1;
      break;
    case 2:
      *(short *)(option_spinner + 0x3c) = 2;
      break;
    case 3:
      *(short *)(option_spinner + 0x3c) = 3;
      break;
    default:
      *(short *)(option_spinner + 0x3c) = 0;
      break;
    }

    list_item = *(char **)(list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'button config' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xee6, 1);
      system_exit(-1);
    }

    option_spinner = *(char **)(list_item + 0x34);
    while (option_spinner != NULL && *(short *)(option_spinner + 0xe) != 2) {
      option_spinner = *(char **)(option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'button config' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xee8, 1);
      system_exit(-1);
    }

    switch ((unsigned char)profile[0x28]) {
    case 1:
      *(short *)(option_spinner + 0x3c) = 1;
      return true;
    case 2:
      *(short *)(option_spinner + 0x3c) = 2;
      return true;
    case 3:
      *(short *)(option_spinner + 0x3c) = 3;
      return true;
    case 4:
      *(short *)(option_spinner + 0x3c) = 4;
      return true;
    default:
      *(short *)(option_spinner + 0x3c) = 0;
      return true;
    }
  }

  error(2, "failed to retrieve editable player profile");
  return false;
}

/* advanced controller settings initializer (0x0ef110, table xref 0x31e26c) —
 * validates the column-list widget, walks its five list items and each item's
 * type-2 spinner child, then mirrors the editable profile's advanced control
 * bytes (+0x2a..+0x2e) into each spinner's selected index (+0x3c). */
bool player_profile_initialize_advanced_controller_settings(void *widget)
{
  char *profile;
  char *list_item;
  char *option_spinner;
  unsigned char look_sensitivity;

  profile = (char *)player_ui_get_edit_player_profile();

  if (*(short *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for advanced controller settings widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xf04,
      1);
    system_exit(-1);
  }

  if (profile != NULL) {
    list_item = *(char **)((char *)widget + 0x34);
    if (list_item == NULL) {
      display_assert(
        "expected 'invert joystick' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xf0c, 1);
      system_exit(-1);
    }

    option_spinner = *(char **)(list_item + 0x34);
    while (option_spinner != NULL && *(short *)(option_spinner + 0xe) != 2) {
      option_spinner = *(char **)(option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'invert joystick' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xf0e, 1);
      system_exit(-1);
    }

    switch (profile[0x2b]) {
    case 0:
      *(short *)(option_spinner + 0x3c) = 1;
      break;
    default:
      *(short *)(option_spinner + 0x3c) = 0;
      break;
    }

    list_item = *(char **)(list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'look sensitivity' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xf17, 1);
      system_exit(-1);
    }

    option_spinner = *(char **)(list_item + 0x34);
    while (option_spinner != NULL && *(short *)(option_spinner + 0xe) != 2) {
      option_spinner = *(char **)(option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'look sensitivity' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xf19, 1);
      system_exit(-1);
    }

    look_sensitivity = (unsigned char)profile[0x2a];
    if (look_sensitivity > 0 && look_sensitivity <= 10) {
      *(short *)(option_spinner + 0x3c) = (short)(look_sensitivity - 1);
    } else {
      *(short *)(option_spinner + 0x3c) = 0;
    }

    list_item = *(char **)(list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'controller vibration' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xf2c, 1);
      system_exit(-1);
    }

    option_spinner = *(char **)(list_item + 0x34);
    while (option_spinner != NULL && *(short *)(option_spinner + 0xe) != 2) {
      option_spinner = *(char **)(option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'controller vibration' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xf2e, 1);
      system_exit(-1);
    }

    switch (profile[0x2c]) {
    case 0:
      *(short *)(option_spinner + 0x3c) = 0;
      break;
    case 1:
      *(short *)(option_spinner + 0x3c) = 1;
      break;
    default:
      *(short *)(option_spinner + 0x3c) = 0;
      break;
    }

    list_item = *(char **)(list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'flight stick controls' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xf37, 1);
      system_exit(-1);
    }

    option_spinner = *(char **)(list_item + 0x34);
    while (option_spinner != NULL && *(short *)(option_spinner + 0xe) != 2) {
      option_spinner = *(char **)(option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'flight stick controls' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xf39, 1);
      system_exit(-1);
    }

    switch (profile[0x2d]) {
    case 0:
      *(short *)(option_spinner + 0x3c) = 1;
      break;
    default:
      *(short *)(option_spinner + 0x3c) = 0;
      break;
    }

    list_item = *(char **)(list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'autocenter' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xf42, 1);
      system_exit(-1);
    }

    option_spinner = *(char **)(list_item + 0x34);
    while (option_spinner != NULL && *(short *)(option_spinner + 0xe) != 2) {
      option_spinner = *(char **)(option_spinner + 0x2c);
    }
    if (option_spinner == NULL) {
      display_assert(
        "expected 'autocenter' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xf44, 1);
      system_exit(-1);
    }

    switch (profile[0x2e]) {
    case 0:
      *(short *)(option_spinner + 0x3c) = 1;
      return true;
    case 1:
      *(short *)(option_spinner + 0x3c) = 0;
      return true;
    default:
      *(short *)(option_spinner + 0x3c) = 0;
      return true;
    }
  }

  error(2, "failed to retrieve editable player profile");
  return false;
}

/* change controller settings (0x0ef3f0, table xref 0x31e270) — walks the
 * controller settings column list (widget+0xe must be type 3) for its
 * 'joystick config' list item (widget+0x34) and the 'button config' item
 * that follows it (+0x2c), finds the spinner sub-widget (type 2) inside
 * each child chain, and stores the selected option index (+0x3c) into the
 * player profile being edited: joystick config to profile+0x29, button
 * config to profile+0x28. An unavailable editable profile reports an error
 * and returns false; a malformed widget hierarchy halts. Out-of-range
 * option indices are reported and leave the profile field unchanged. */
bool player_profile_change_controller_settings(void *widget, void *event_data,
                                               bool *widget_deleted)
{
  char *profile;
  char *list_item; /* name: PAL 2342 ui_widget_event_handler_functions.c:4730 */
  char *option_spinner;

  (void)event_data;
  (void)widget_deleted;

  profile = (char *)player_ui_get_edit_player_profile();

  if (*(short *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for controller settings widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xf5d,
      1);
    system_exit(-1);
  }

  if (profile != NULL) {
    list_item = *(char **)((char *)widget + 0x34);
    if (list_item == NULL) {
      display_assert(
        "expected 'joystick config' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xf65, 1);
      system_exit(-1);
    }

    option_spinner = *(char **)(list_item + 0x34);
    while (option_spinner != NULL && *(short *)(option_spinner + 0xe) != 2) {
      option_spinner = *(char **)(option_spinner + 0x2c);
    }

    if (option_spinner == NULL) {
      display_assert(
        "expected 'joystick config' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xf67, 1);
      system_exit(-1);
    }

    switch (*(short *)(option_spinner + 0x3c)) {
    case 0:
      profile[0x29] = 0;
      break;
    case 1:
      profile[0x29] = 1;
      break;
    case 2:
      profile[0x29] = 2;
      break;
    case 3:
      profile[0x29] = 3;
      break;
    default:
      error(2, "unknown option selected for joystick config");
      break;
    }

    list_item = *(char **)(list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'button config' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xf72, 1);
      system_exit(-1);
    }

    option_spinner = *(char **)(list_item + 0x34);
    while (option_spinner != NULL && *(short *)(option_spinner + 0xe) != 2) {
      option_spinner = *(char **)(option_spinner + 0x2c);
    }

    if (option_spinner == NULL) {
      display_assert(
        "expected 'button config' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xf74, 1);
      system_exit(-1);
    }

    switch (*(short *)(option_spinner + 0x3c)) {
    case 0:
      profile[0x28] = 0;
      return true;
    case 1:
      profile[0x28] = 1;
      return true;
    case 2:
      profile[0x28] = 2;
      return true;
    case 3:
      profile[0x28] = 3;
      return true;
    case 4:
      profile[0x28] = 4;
      return true;
    default:
      error(2, "unknown button config option selected");
      return true;
    }
  }

  error(2, "failed to retrieve editable player profile");
  return false;
}

/* change advanced controller settings (0x0ef5c0) — walks the advanced
 * controller settings column list (widget+0xe must be type 3) through its
 * 'invert joystick', 'look sensitivity', 'controller vibration', 'flight
 * stick controls' and 'autocenter' list items (+0x34 first, +0x2c next),
 * finds the spinner sub-widget (type 2) inside each child chain, and stores
 * the selected option index (+0x3c) into the edited player profile bytes
 * +0x2b, +0x2a (index+1, index in [0,9]), +0x2c, +0x2d and +0x2e. An
 * unavailable editable profile reports an error and returns false; a
 * malformed widget hierarchy halts. Unknown option indices are reported and
 * leave the profile byte unchanged. */
bool player_profile_change_advanced_controller_settings(void *widget,
                                                        void *event_data,
                                                        bool *widget_deleted)
{
  char *profile;
  char *list_item;
  char *option_spinner;

  (void)event_data;
  (void)widget_deleted;

  profile = (char *)player_ui_get_edit_player_profile();

  if (*(short *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for advanced controller settings widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xf90,
      1);
    system_exit(-1);
  }

  if (profile != NULL) {
    list_item = *(char **)((char *)widget + 0x34);
    if (list_item == NULL) {
      display_assert(
        "expected 'invert joystick' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xf98, 1);
      system_exit(-1);
    }

    option_spinner = *(char **)(list_item + 0x34);
    while (option_spinner != NULL && *(short *)(option_spinner + 0xe) != 2) {
      option_spinner = *(char **)(option_spinner + 0x2c);
    }

    if (option_spinner == NULL) {
      display_assert(
        "expected 'invert joystick' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xf9a, 1);
      system_exit(-1);
    }

    switch (*(short *)(option_spinner + 0x3c)) {
    case 0:
      profile[0x2b] = 1;
      break;
    case 1:
      profile[0x2b] = 0;
      break;
    default:
      error(2, "unknown option selected for invert joystick");
      break;
    }

    list_item = *(char **)(list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'look sensitivity' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xfa3, 1);
      system_exit(-1);
    }

    option_spinner = *(char **)(list_item + 0x34);
    while (option_spinner != NULL && *(short *)(option_spinner + 0xe) != 2) {
      option_spinner = *(char **)(option_spinner + 0x2c);
    }

    if (option_spinner == NULL) {
      display_assert(
        "expected 'look sensitivity' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xfa5, 1);
      system_exit(-1);
    }

    if (*(short *)(option_spinner + 0x3c) >= 0 &&
        *(short *)(option_spinner + 0x3c) <= 9) {
      profile[0x2a] = (char)(*(char *)(option_spinner + 0x3c) + 1);
    } else {
      error(2, "unknown option selected for look sensitivity");
    }

    list_item = *(char **)(list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'controller vibration' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xfb8, 1);
      system_exit(-1);
    }

    option_spinner = *(char **)(list_item + 0x34);
    while (option_spinner != NULL && *(short *)(option_spinner + 0xe) != 2) {
      option_spinner = *(char **)(option_spinner + 0x2c);
    }

    if (option_spinner == NULL) {
      display_assert(
        "expected 'controller vibration' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xfba, 1);
      system_exit(-1);
    }

    switch (*(short *)(option_spinner + 0x3c)) {
    case 0:
      profile[0x2c] = 0;
      break;
    case 1:
      profile[0x2c] = 1;
      break;
    default:
      error(2, "unknown option selected for controller vibration");
      break;
    }

    list_item = *(char **)(list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'flight stick controls' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xfc3, 1);
      system_exit(-1);
    }

    option_spinner = *(char **)(list_item + 0x34);
    while (option_spinner != NULL && *(short *)(option_spinner + 0xe) != 2) {
      option_spinner = *(char **)(option_spinner + 0x2c);
    }

    if (option_spinner == NULL) {
      display_assert(
        "expected 'flight stick controls' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xfc5, 1);
      system_exit(-1);
    }

    switch (*(short *)(option_spinner + 0x3c)) {
    case 0:
      profile[0x2d] = 1;
      break;
    case 1:
      profile[0x2d] = 0;
      break;
    default:
      error(2, "unknown option selected for controller "
               "flight_stick_aircraft_controls");
      break;
    }

    list_item = *(char **)(list_item + 0x2c);
    if (list_item == NULL) {
      display_assert(
        "expected 'autocenter' list item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xfce, 1);
      system_exit(-1);
    }

    option_spinner = *(char **)(list_item + 0x34);
    while (option_spinner != NULL && *(short *)(option_spinner + 0xe) != 2) {
      option_spinner = *(char **)(option_spinner + 0x2c);
    }

    if (option_spinner == NULL) {
      display_assert(
        "expected 'autocenter' option spinner list",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0xfd0, 1);
      system_exit(-1);
    }

    switch (*(short *)(option_spinner + 0x3c)) {
    case 0:
      profile[0x2e] = 1;
      return true;
    case 1:
      profile[0x2e] = 0;
      return true;
    default:
      error(2, "unknown option selected for controller autocenter");
      return true;
    }
  }

  error(2, "failed to retrieve editable player profile");
  return false;
}

/* remove local player from network game (0x0ef900, table xref 0x31e278) —
 * validates that the event's controller index (event_data+0x2) is in [0,4)
 * and, if so, quits that local player from the current network game. A NULL
 * event_data or an out-of-range controller index halts with an assert and
 * exits. */
bool network_game_remove_local_player(void *widget, void *event_data,
                                      bool *widget_deleted)
{
  (void)widget;
  (void)widget_deleted;

  if (event_data == NULL || *(short *)((char *)event_data + 2) < 0 ||
      *(short *)((char *)event_data + 2) >= 4) {
    display_assert(
      "valid controller index required to remove player from network game",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0xfe9,
      1);
    system_exit(-1);
  }

  network_game_client_local_player_quit(*(short *)((char *)event_data + 2));
  return true;
}

/* main menu switch to solo game (event handler table index 73, 0x0ef950; name
 * from the handler name table at 0x31e2f0) — switches the game connection to
 * local (0), enters single player and forgets player 1's profile; the tail of
 * start_new_game without the difficulty/map setup. The arguments are never
 * read; always returns true. */
bool main_menu_switch_to_solo_game(void *widget, void *event_data,
                                   bool *widget_deleted)
{
  set_game_connection(0);
  main_menu_switch_to_single_player();
  player_ui_remember_player1_profile(false);
  return true;
}

/* multiplayer profile list selection handler (0x0ef970) — validates the
 * widget hierarchy (widget itself must be a container w/ 3+ children; the
 * sub-widget at widget+0x34 must be a 3-item spinner list), resolves the
 * selected item's profile handle, stores it to DAT_0031e494, and either
 * plays a deny sound (handle == -1, returns false) or returns true. Sibling
 * of player_profile_begin_editing (player profile list) but with an extra
 * tag_get-based container check instead of a flag check, and no editing-session
 * branch. */
bool delete_player_profile_request(void *widget, void *event_data,
                                   bool *widget_deleted)
{
  short *container_tag;
  short *list_tag;
  int profile_handle;

  (void)event_data;
  (void)widget_deleted;

  container_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */, *(int *)widget);
  if (*container_tag != 0 || *(int *)((char *)container_tag + 0x3e0) < 3) {
    display_assert(
      "expected the multiplayer profile select screen to be a container w/ "
      "3+ children",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x100f, 1);
    system_exit(-1);
  }

  list_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */,
                              **(int **)((char *)widget + 0x34));
  if (*list_tag != 2) {
    display_assert(
      "expected a spinner list widget for 'multiplayer profile list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x1012, 1);
    system_exit(-1);
  }

  if (*(int *)((char *)list_tag + 0x3e0) != 3) {
    display_assert(
      "expected 3 list items for 'multiplayer profile list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x1013, 1);
    system_exit(-1);
  }

  widget = *(void **)((char *)widget + 0x34);
  if (*(short *)((char *)widget + 0x3c) < 0 ||
      *(short *)((char *)widget + 0x3c) >=
        (int)*(unsigned short *)((char *)widget + 0x44)) {
    display_assert(
      "invalid multiplayer profile specified from 'multiplayer profile "
      "list' list widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x101c, 1);
    system_exit(-1);
  }

  profile_handle =
    (*(int **)((char *)widget + 0x40))[*(short *)((char *)widget + 0x3c)];
  *(int *)0x31e494 = profile_handle; /* DAT_0031e494 — unknown purpose */

  if (profile_handle != -1) {
    return true;
  }

  ui_play_audio_feedback_sound(4);
  return false;
}

/* request del playlist profile (event handler table index 75, 0x0efa80) —
 * playlist-profile twin of delete_player_profile_request: validates the
 * container / 3-item spinner list layout, stores the selected profile handle
 * to DAT_0031e494, and refuses (deferred error 26 + deny sound) handles with
 * bit 0x40000000 set.  Result lives in BL (false unless the handle is
 * deletable). */
bool delete_playlist_profile_request(void *widget, void *event_data,
                                     bool *widget_deleted)
{
  bool result;
  short *container_tag;
  short *list_tag;
  int profile_handle;

  (void)event_data;
  (void)widget_deleted;

  result = false;
  container_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */, *(int *)widget);
  if (*container_tag != 0 || *(int *)((char *)container_tag + 0x3e0) < 3) {
    display_assert(
      "expected the playlist profile select screen to be a container w/ 3+ "
      "children",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x103d, 1);
    system_exit(-1);
  }

  list_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */,
                              **(int **)((char *)widget + 0x34));
  if (*list_tag != 2) {
    display_assert(
      "expected a spinner list widget for 'playlist profile list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x1040, 1);
    system_exit(-1);
  }
  if (*(int *)((char *)list_tag + 0x3e0) != 3) {
    display_assert(
      "expected 3 list items for 'playlist profile list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x1041, 1);
    system_exit(-1);
  }

  widget = *(void **)((char *)widget + 0x34);
  if (*(short *)((char *)widget + 0x3c) < 0 ||
      *(short *)((char *)widget + 0x3c) >=
        (int)*(unsigned short *)((char *)widget + 0x44)) {
    display_assert(
      "invalid multiplayer profile specified from 'multiplayer profile "
      "list' list widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x1049, 1);
    system_exit(-1);
  }

  profile_handle =
    (*(int **)((char *)widget + 0x40))[*(short *)((char *)widget + 0x3c)];
  *(int *)0x31e494 = profile_handle; /* pending profile handle */
  if (profile_handle != -1) {
    if (profile_handle & 0x40000000) {
      display_error_deferred(26, -1, true, false);
      ui_play_audio_feedback_sound(4);
    } else {
      result = true;
    }
  } else {
    ui_play_audio_feedback_sound(4);
  }
  return result;
}

/* final del player profile (event handler table index 76, 0x0efbc0) —
 * deletes the pending profile handle (DAT_0031e494) if it names a custom
 * player profile (low nibble 0, bit 0x40000000 clear). */
bool delete_player_profile_final(void *widget, void *event_data,
                                 bool *widget_deleted)
{
  bool result;
  int profile_handle;

  (void)widget;
  (void)event_data;
  (void)widget_deleted;

  result = false;
  profile_handle = *(int *)0x31e494;
  if (!(profile_handle & 0x40000000)) {
    if ((profile_handle & 0xf) == 0) {
      FUN_001c0d70(profile_handle); /* player profile delete */
      result = true;
    } else {
      error(2, "#0x%08lX is not a player profile index", profile_handle);
    }
  } else {
    error(2, "sorry, you are not allowed to delete default player profiles");
    result = false;
  }
  return result;
}

/* final del playlist profile (event handler table index 77, 0x0efc10) —
 * deletes the pending profile handle if its low nibble tags it as a
 * playlist profile (1). */
bool delete_playlist_profile_final(void *widget, void *event_data,
                                   bool *widget_deleted)
{
  bool result;
  int profile_handle;

  (void)widget;
  (void)event_data;
  (void)widget_deleted;

  result = false;
  profile_handle = *(int *)0x31e494;
  if ((profile_handle & 0xf) == 1) {
    FUN_001c1f70(profile_handle); /* playlist profile delete */
    result = true;
  } else {
    error(2, "#0x%08lX is not a playlist profile index", profile_handle);
  }
  return result;
}

/* cancel profile delete (event handler table index 78, 0x0efc50) — clears
 * the pending profile handle. */
bool cancel_profile_delete(void *widget, void *event_data, bool *widget_deleted)
{
  (void)widget;
  (void)event_data;
  (void)widget_deleted;

  *(int *)0x31e494 = -1;
  return true;
}

/* create and begin editing a new multiplayer game-type profile (0x0efc60,
 * table xref 0x31e294) — the game-variant sibling of the player-profile
 * handler below. Asserts the event's local player index (event_data+0x2)
 * is in [0,4), fetches a default "untitled profile" name, creates a new
 * playlist profile for the widget's index field (widget+0x8), begins
 * editing it, seeds the editable profile with the slayer defaults, copies
 * the untitled name in (max 11 chars + NUL) and hands it to the virtual
 * keyboard for validation. On success it also remembers the profile's
 * enclosing directory as the last-used multiplayer variant directory.
 * Any failure reports a deferred error and plays the deny sound. */
bool create_and_begin_editing_new_gametype_profile(void *widget,
                                                   void *event_data,
                                                   bool *widget_deleted)
{
  bool result;
  int profile_index;
  wchar_t untitled_name[128];
  game_variant_t scratch;
  game_variant_t default_variant;
  char directory_path[256];
  wchar_t *profile;

  (void)widget_deleted;
  result = false;

  if (*(short *)((char *)event_data + 2) < 0 ||
      *(short *)((char *)event_data + 2) >= 4) {
    display_assert(
      "creating a new profile requires a valid local player index",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x10b0, 1);
    system_exit(-1);
  }

  saved_game_file_get_useable_untitled_profile_name(untitled_name);
  if (untitled_name[0] != L'\0') {
    /* widget+0x8 — unknown widget field, used as the profile's owning index */
    profile_index = playlist_profile_new(
      *(unsigned short *)((char *)widget + 8), untitled_name);
    if (profile_index != -1) {
      player_ui_begin_editing_profile(profile_index);
      profile = (wchar_t *)player_ui_get_edit_playlist_profile();
      if (profile != NULL) {
        default_variant = *game_engine_slayer_default(&scratch);
        csmemcpy(profile, &default_variant, sizeof(game_variant_t));
        profile[0x32] = L'\0';
        ustrncpy(profile, untitled_name, 0xb);
        profile[0xb] = L'\0';
        result = virtual_keyboard_launch(profile, 0x18, 9);
      } else {
        error(2, "failed to retrieve editable game variant profile!");
        player_ui_end_editing_profile();
      }
    } else {
      error(2, "failed to create a new multiplayer game type profile");
    }
  } else {
    error(2, "unable to create a new untitled profile");
  }

  if (result == true) {
    if (saved_game_file_get_path_to_enclosing_directory(profile_index,
                                                        directory_path)) {
      saved_game_file_remember_last_used_multiplayer_variant_directory(
        directory_path);
    }
  } else if (!result) {
    display_error_deferred(0x26, -1, true, false);
    ui_play_audio_feedback_sound(4);
  }

  return result;
}

/* create and edit a new player profile (0x0efde0, table xref 0x31e298) —
 * fetches a default "untitled profile" name, creates a new saved-game
 * profile for the event's controller index (event_data+0x2; sentinel -1
 * defaults to controller 0) under that name, begins editing it, copies the
 * default name into the now-editable profile's name buffer (max 11 chars +
 * NUL), and hands the buffer to the virtual keyboard for validation. Any
 * failure along the way reports a deferred error and plays the deny sound;
 * a validation failure does the same after also ending the edit session. */
bool create_and_begin_editing_new_player_profile(void *widget, void *event_data,
                                                 bool *widget_deleted)
{
  wchar_t untitled_name[128];
  short controller_index;
  int profile_index;
  void *edit_name;
  bool validated;

  (void)widget;
  (void)widget_deleted;
  validated = false;

  controller_index = *(short *)((char *)event_data + 2);
  if (controller_index == -1) {
    controller_index = 0;
  }

  saved_game_file_get_useable_untitled_profile_name(untitled_name);
  if (untitled_name[0] == L'\0') {
    error(2, "unable to create a new untitled profile");
    goto failure;
  }

  profile_index = FUN_001c1720(controller_index, untitled_name);
  if (profile_index == -1) {
    error(2, "failed to create a new player profile");
    goto failure;
  }

  player_ui_begin_editing_profile(profile_index);
  edit_name = player_ui_get_edit_player_profile();
  if (edit_name == NULL) {
    error(2, "failed to retrieve editable player profile!");
    player_ui_end_editing_profile();
    goto failure;
  }

  ustrncpy((wchar_t *)edit_name, untitled_name, 0xb);
  ((wchar_t *)edit_name)[0xb] = L'\0';
  validated = virtual_keyboard_launch((wchar_t *)edit_name, 0x18, 8);
failure:
  if (!validated) {
    display_error_deferred(0x25, -1, true, false);
    ui_play_audio_feedback_sound(4);
  }

  return validated;
}

/* network start-time-change request handler (0x000efed0) — event handler
 * table entry, same 3-arg bool convention as the siblings above/below in
 * this file (widget/widget_deleted unused here; disasm never touches
 * EBP+8 or EBP+0x10). Always returns true (MOV AL,1 before every RET).
 *
 * Looks up the local network client (global_network_game_client_get), then
 * scans its player table (network_game_client_get_game() + 0x242, 16 entries,
 * stride 0x20; network_player_is_valid() takes the entry base at +0x226) for a
 * valid entry whose machine index (entry+0) matches this client's own machine
 * index (network_game_client_get_machine_index) and whose local-player index
 * (entry+1) matches the field at event_data+2. On a match, requests a game
 * start-time change (request_type=1) and errors if it fails. */
bool network_game_start_faster(void *widget, void *event_data,
                               bool *widget_deleted)
{
  void *client;
  char *entry;
  short local_machine_index;
  int i;

  client = global_network_game_client_get();
  if (client != NULL) {
    entry = (char *)network_game_client_get_game(client);
    local_machine_index = network_game_client_get_machine_index(client);
    i = 0;
    entry += 0x242;
    for (; i < 16; i++, entry += 0x20) {
      if (network_player_is_valid(entry - 0x1c) &&
          (short)*entry == local_machine_index &&
          (short)entry[1] == *(short *)((char *)event_data + 2)) {
        if (!network_game_client_request_start_time_change(client, 1)) {
          error(2, "network_game_client_request_start_time_change() failed");
        }
        return true;
      }
    }
  }
  return true;
}

/* request start-time change (0x0eff70, table xref 0x31e2a0) — scans up to 16
 * player-record slots (client's machine-index base +0x242, stride 0x20) for
 * a valid player whose record bytes at +0x1c/+0x1d match this machine's
 * index and the event's controller index (event_data+0x2); on a match asks
 * the network client to request a start-time change, logging an error if the
 * request is refused. Always returns true regardless of outcome. */
bool network_game_start_slower(void *widget, void *event_data,
                               bool *widget_deleted)
{
  void *client;
  char *player_rec;
  short local_machine_index;
  int i;

  (void)widget;
  (void)widget_deleted;

  client = global_network_game_client_get();
  if (client != NULL) {
    player_rec = (char *)network_game_client_get_game(client);
    local_machine_index = network_game_client_get_machine_index(client);
    i = 0;
    player_rec += 0x242;
    for (; i < 16; i++, player_rec += 0x20) {
      if (network_player_is_valid(player_rec - 0x1c) &&
          (short)*player_rec == local_machine_index &&
          (short)player_rec[1] == *(short *)((char *)event_data + 2)) {
        if (!network_game_client_request_start_time_change(client, 0)) {
          error(2, "network_game_client_request_start_time_change() failed");
        }
        return true;
      }
    }
  }
  return true;
}

/* net server accept conx (event handler table index 83, 0x0f0010) — opens
 * the local network game server to connections, if one exists. */
bool network_game_server_accept_connections(void *widget, void *event_data,
                                            bool *widget_deleted)
{
  void *server;

  (void)widget;
  (void)event_data;
  (void)widget_deleted;

  server = global_network_game_server_get();
  if (server) {
    network_game_server_open_game(server);
  }
  return true;
}

/* net server allow start (event handler table index 85, 0x0f0050) — resumes
 * the local network game server's start countdown, if a server exists. */
bool network_game_server_allow_game_start(void *widget, void *event_data,
                                          bool *widget_deleted)
{
  void *server;

  (void)widget;
  (void)event_data;
  (void)widget_deleted;

  server = global_network_game_server_get();
  if (server) {
    network_game_server_pause_countdown(server, 0);
  }
  return true;
}

/* disable if no xdemos (event handler table index 86, 0x0f0070) — marks the
 * widget disabled (+0x12) and clears its enabled/visible byte (+0x10) when no
 * Xbox demo content is installed. */
bool disable_widget_if_no_xdemos(void *widget, void *event_data,
                                 bool *widget_deleted)
{
  if (!xbox_demos_available()) {
    *(uint8_t *)((char *)widget + 0x12) = 1;
    *(uint8_t *)((char *)widget + 0x10) = 0;
  }
  return true;
}

/* run xdemos (event handler table index 87, 0x0f0090). */
bool run_xdemos(void *widget, void *event_data, bool *widget_deleted)
{
  (void)widget;
  (void)event_data;
  (void)widget_deleted;

  main_run_demos();
  return true;
}

/* single_player_reset_controller_choices (event handler, 0x0f00a0; data xref
 * 0x31e2b8, the table slot just before single_player_set_player1_controller_
 * choice's 0x31e2bc entry) — name PAL 2342 ui_widget_event_handler_functions.c
 * :2164 (T2).  Resets the single-player local player controller assignments
 * and returns true.  The arguments are never read (CALL/MOV AL,1/RET). */
bool single_player_reset_controller_choices(void *widget, void *event_data,
                                            bool *widget_deleted)
{
  (void)widget;
  (void)event_data;
  (void)widget_deleted;

  player_ui_reset_single_player_local_player_controllers();
  return true;
}

/* set single-player controller from event (0x0f00b0, table xref 0x31e2bc) —
 * asserts event_data is non-null (halts and exits otherwise), then sets the
 * single-player local player's controller index to the event's controller
 * index (event_data+0x2). Local player index is always 0. */
bool single_player_set_player1_controller_choice(void *widget, void *event_data,
                                                 bool *widget_deleted)
{
  (void)widget;
  (void)widget_deleted;

  if (event_data == NULL) {
    display_assert(
      "event != NULL",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x11b9, 1);
    system_exit(-1);
  }

  player_ui_set_single_player_local_player_controller(
    0, *(short *)((char *)event_data + 2));

  return true;
}

/* set second local-player controller from event, refusing a controller
 * already claimed by local player 0 (0x0f0100, table xref 0x31e2c0) —
 * asserts event_data is non-null, then compares the event's controller
 * index (event_data+0x2) against player 0's current controller. If they
 * match, shows error 0x12 (modal, no pause) and marks the widget deleted,
 * returning false. Otherwise assigns that controller to local player 1
 * and returns true. */
bool single_player_set_player2_controller_choice(void *widget, void *event_data,
                                                 bool *widget_deleted)
{
  short controller_index;
  short current_controller;

  (void)widget;

  if (event_data == NULL) {
    display_assert(
      "event != NULL",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x11c7, 1);
    system_exit(-1);
  }

  controller_index = *(short *)((char *)event_data + 2);
  current_controller = player_ui_get_single_player_local_player_controller(0);

  if (controller_index == current_controller) {
    ui_widget_display_error(0x12, -1, 1, 0);
    *widget_deleted = 1;
    return false;
  }

  player_ui_set_single_player_local_player_controller(1, controller_index);
  return true;
}

/* check network availability, error if unavailable (0x0f0170, table xref
 * 0x31e2c4) — asserts event_data is non-null (halts and exits otherwise).
 * If transport_network_available() is false, shows error 5 with the
 * event's controller index (event_data+0x2, zero-extended) as the local
 * player index (modal, pauses game). Returns the network-available flag
 * regardless of which branch ran. */
bool display_error_if_no_network_connection(void *widget, void *event_data,
                                            bool *widget_deleted)
{
  bool network_available;

  (void)widget;
  (void)widget_deleted;

  network_available = transport_network_available();

  if (event_data == NULL) {
    display_assert(
      "event != NULL",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x11df, 1);
    system_exit(-1);
  }

  if (!network_available) {
    ui_widget_display_error(5, *(uint16_t *)((char *)event_data + 2), 1, 1);
  }

  return network_available;
}

/* start server if none advertised (0x0f01d0) — asserts widget+0xe is a
 * column-list widget type (3), then, if widget+0x44 (no visible advertised
 * servers) is zero, fetches the network client and, if present and its
 * connection state is 0 ("searching"), forwards this handler's own params to
 * network_game_start_new_server and returns its result directly (the original
 * tail-propagates network_game_start_new_server's EAX into AL without touching
 * it). If widget+0x44 is non-zero, logs that a new server isn't being started
 * because other servers are already available. Falls through to false on:
 * missing client, non-zero client state, or the log branch. */
bool start_network_game_if_no_advertised_servers(void *widget, void *event_data,
                                                 bool *widget_deleted)
{
  bool result;
  int16_t state; /* out-param (discarded) */
  void *client;

  result = false;
  if (*(short *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected a column list for server list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x11f1, 1);
    system_exit(-1);
  }

  if (*(short *)((char *)widget + 0x44) == 0) {
    client = global_network_game_client_get();
    if (client != NULL && network_game_client_get_state(client, &state) == 0) {
      result =
        network_game_start_new_server(widget, event_data, widget_deleted);
    }
  } else {
    error(2, "not attempting to start a new server; there are other servers "
             "available");
  }

  return result;
}

/* unjoin network-game player (0x0f0250) — scans the local client's 16
 * player records at machine-index base +0x226 (stride 0x20) for records on
 * the local machine matching event_data+2. Requests removal of that matching
 * record, clears its local-player autojoin flag, and, when exactly one local
 * record was found, tears down or pauses the server before copying autojoin
 * flags to the next multiplayer game. */
bool netgame_unjoin_player(void *widget, void *event_data, bool *widget_deleted)
{
  void *client;
  char *record;
  char *matched_record;
  short local_machine_index;
  int local_record_count;
  int i;
  void *server;

  (void)widget;
  (void)widget_deleted;

  client = global_network_game_client_get();
  if (client == NULL) {
    return true;
  }

  matched_record = NULL;
  local_machine_index = network_game_client_get_local_machine_index();
  record = (char *)network_game_client_get_game(client) + 0x226;
  local_record_count = 0;
  i = 16;
  do {
    if (network_player_is_valid(record) &&
        *(signed char *)(record + 0x1c) == local_machine_index) {
      local_record_count = local_record_count + 1;
      if (*(signed char *)(record + 0x1d) ==
          *(short *)((char *)event_data + 2)) {
        if (matched_record != NULL) {
          display_assert(
            "duplicate player registered in game",
            "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
            0x1226, 1);
          system_exit(-1);
        }
        matched_record = record;
      }
    }
    record = record + 0x20;
    i = i - 1;
  } while (i != 0);

  if (local_record_count <= 0) {
    return true;
  }

  if (matched_record != NULL) {
    if (!network_game_client_request_remove_player(client, matched_record)) {
      error(2, "failed to request player removal");
    }
    player_ui_clear_multiplayer_autojoin_for_local_player(
      *(signed char *)(matched_record + 0x1d));
  }

  if (local_record_count == 1) {
    server = global_network_game_server_get();
    if (server == NULL || !network_game_should_accept_remote_connections()) {
      dispose_global_network_game_client();
      dispose_global_network_game_server();
    } else {
      server = global_network_game_server_get();
      if (server != NULL) {
        network_game_server_pause_countdown(server, 1);
        player_ui_autojoin_players_to_next_multiplayer_game();
        return true;
      }
    }
    player_ui_autojoin_players_to_next_multiplayer_game();
    return true;
  }

  return false;
}

/* close_calling_widget_if_not_editing_profile (0xf03d0, table xref 0x31e2d0) —
 * closes the widget's last child when neither an in-progress player profile
 * edit nor an in-progress playlist profile edit is active ("no saved game file
 * being edited" cancel path). */
void close_calling_widget_if_not_editing_profile(void *widget)
{
  void *child;

  if (player_ui_get_edit_player_profile() == NULL &&
      player_ui_get_edit_playlist_profile() == NULL) {
    child = widget_instance_get_topmost_parent(widget);
    error(2, "closing widget '%s' because no saved game file is being edited",
          *(const char **)((char *)child + 4));
    *(uint32_t *)((char *)child + 0x1c) = 1;
    *(uint8_t *)((char *)child + 0x10) = 0;
  }
}

/* exit to xbox dashboard (event handler table index 95, 0x0f0420) —
 * launches the dashboard and returns false. */
bool exit_to_xbox_dashboard(void *widget, void *event_data,
                            bool *widget_deleted)
{
  (void)widget;
  (void)event_data;
  (void)widget_deleted;

  FUN_000e0570(); /* dashboard launch */
  return false;
}

/* new campaign chosen (0x0f0430) — asserts + exits if event_data is NULL.
 * Fetches an unused campaign save-profile name into a scratch wide buffer,
 * copies the first 11 chars (+ null terminator) into the global campaign
 * name-entry buffer (DAT_0046ccd0, 12 x wchar_t), stashes event_data+0x2 (a
 * caller-supplied 16-bit value) into DAT_0031e4fc, then opens the virtual
 * keyboard to let the player edit the name. Logs an error (does not fail)
 * if the keyboard couldn't be invoked; always returns true. */
bool new_campaign_chosen(void *widget, void *event_data, bool *widget_deleted)
{
  wchar_t campaign_name[128];
  bool keyboard_ok;

  (void)widget;
  (void)widget_deleted;

  if (event_data == NULL) {
    display_assert(
      "event",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x1285, 1);
    system_exit(-1);
  }

  saved_game_file_get_useable_untitled_profile_name(campaign_name);
  ustrncpy((wchar_t *)0x46ccd0, campaign_name, 0xb);
  *(uint16_t *)0x46cce6 = 0; /* DAT_0046cce6 — null terminator at index 11 */
  *(uint16_t *)0x31e4fc =
    *(uint16_t *)((char *)event_data + 2); /* DAT_0031e4fc */

  keyboard_ok = virtual_keyboard_launch((wchar_t *)0x46ccd0, 0x18, 8);
  if (!keyboard_ok) {
    error(2, "failed to invoke the virtual keyboard for a new campaign profile "
             "name");
  }

  return true;
}

/* virtual-keyboard completion handler for the new campaign name
 * (0xf04c0). The virtual-keyboard done flag gates campaign profile creation;
 * its pending controller index and editable name are held in the global UI
 * state initialized by new_campaign_chosen. */
void new_campaign_decision(void)
{
  wchar_t player_profile[24];
  int profile_index;
  const char *message;
  bool profile_created;

  if (*(short *)0x31e4fc != -1) {
    if (virtual_keyboard_last_exit_saved_text()) {
      if (*(wchar_t *)0x46ccd0 != L'\0') {
        player_ui_set_single_player_local_player_controller(0,
                                                            *(short *)0x31e4fc);
        profile_index = FUN_001c1720(*(short *)0x31e4fc, (wchar_t *)0x46ccd0);
        if (profile_index == -1) {
          saved_game_file_get_useable_untitled_profile_name(player_profile);
          ustrncpy((wchar_t *)0x46ccd0, player_profile, 0xb);
          *(wchar_t *)0x46cce6 = L'\0';
          profile_index = FUN_001c1720(*(short *)0x31e4fc, (wchar_t *)0x46ccd0);
          if (profile_index == -1) {
            message = "failed to create new player profile";
            error(2, message);
            main_goto_main_menu();
            display_error_deferred(0x25, -1, true, false);
            ui_play_audio_feedback_sound(4);
            *(short *)0x31e4fc = -1;
            return;
          }
        }
        profile_created = player_profile_new(profile_index, player_profile);
        if (profile_created) {
          player_ui_set_active_player_profile(0, profile_index, player_profile);
          main_set_map_name(*(const char **)0x31e498);
          main_defer_map_map_change();
          *(short *)0x31e4fc = -1;
          return;
        }
        message = "failed to retrieve newly created player profile";
        error(2, message);
        main_goto_main_menu();
        display_error_deferred(0x25, -1, true, false);
        ui_play_audio_feedback_sound(4);
      } else {
        error(2, "can't create a new profile with an empty name");
        ui_play_audio_feedback_sound(4);
      }
    }
    *(short *)0x31e4fc = -1;
  }
}

/* pop history stack once (event handler table index 98, 0x0f0620) — pops one
 * entry from the widget history stack of the widget's local player (+0x8). */
bool go_back_twice_next_time(void *widget, void *event_data,
                             bool *widget_deleted)
{
  ui_widgets_pop_stack(*(uint16_t *)((char *)widget + 0x8));
  return true;
}

/* difficulty menu item select (0xf0640, ui_widget_game_data_function_table
 * xref 0x31e2e4) — asserts the widget is a column list (widget+0xe == 3).
 * If the "difficulty forced for this map" flag (DAT_0046ce3b) is set and
 * the current map (main_get_map_name()) case-insensitively matches the
 * forced-map name string (DAT_0046cd38), preselects the forced difficulty
 * child index (DAT_0046ce38, a stored int16); otherwise preselects index 1
 * (default difficulty). Stores the resolved child widget pointer at
 * widget+0x38 and the selected index at widget+0x3c — the same "selected
 * list item" field pair player_profile_1wide_list_update uses at +0x3c for its
 * spinner list. */
bool difficulty_menu_initialize(void *widget)
{
  void *difficulty_widget; /* name: PAL 2342
                              ui_widget_event_handler_functions.c:3291 */

  if (*(short *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for difficulty menu widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x12fc, 1);
    system_exit(-1);
  }

  if (*(unsigned char *)0x46ce3b == 1 &&
      crt_stricmp((const char *)0x46cd38, main_get_map_name()) == 0) {
    /* DAT_0046ce38 */
    difficulty_widget =
      widget_instance_get_nth_child(widget, *(short *)0x46ce38);
    if (difficulty_widget == NULL) {
      display_assert(
        "failed to find 'difficulty' menu item",
        "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
        0x1301, 1);
      system_exit(-1);
    }
    *(short *)((char *)widget + 0x3c) = *(short *)0x46ce38;
    *(void **)((char *)widget + 0x38) = difficulty_widget;
    return true;
  }

  difficulty_widget = widget_instance_get_nth_child(widget, 1);
  if (difficulty_widget == NULL) {
    display_assert(
      "failed to find 'difficulty' menu item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c",
      0x1307, 1);
    system_exit(-1);
  }
  *(void **)((char *)widget + 0x38) = difficulty_widget;
  *(short *)((char *)widget + 0x3c) = 1;
  return true;
}

/* begin music fade out (event handler table index 100, 0x0f0720) — stops
 * the main menu music if it is playing. */
bool begin_music_fade_out(void *widget, void *event_data, bool *widget_deleted)
{
  (void)widget;
  (void)event_data;
  (void)widget_deleted;

  if (ui_main_menu_music_active()) {
    ui_stop_main_menu_music();
  }
  return true;
}

/* new campaign if no custom player profiles exist (0xf0740) — calls the
 * saved-game profile enumerator at 0x1c0d50 with the same argument shape as
 * FUN_000e5590 (index -1, two out-param locals with local_4 pre-set to 1,
 * trailing flag 0 here). The enumerator's result is read back as a signed
 * 16-bit value: if it is positive the handler does nothing and returns true;
 * otherwise it forwards the event to ui_widget_new_campaign_chosen (whose
 * return value is discarded — the constant false is materialised in the
 * callee-saved BL before the call) and returns false. */
bool new_campaign_if_no_custom_player_profiles_exist(void *widget,
                                                     void *event_data,
                                                     bool *widget_deleted)
{
  int local_4;
  int local_8;

  local_4 = 1;
  FUN_001c0d50(-1, &local_4, &local_8, 0);
  if ((short)local_4 > 0) {
    return true;
  }

  new_campaign_chosen(widget, event_data, widget_deleted);
  return false;
}

/* solo level initialize list (single player) (0xf0790, event handler
 * registered via data table 0x31e16c) — reads widget/event_data/
 * widget_deleted at [EBP+8/0xc/0x10] and returns AL=1 on every exit.
 * When the signed 16-bit global at 0x31fa94 is >= 2 it clears the 0x106-byte
 * persistent-storage scratch at 0x46cd38 and forwards to
 * ui_widget_initialize_single_player_level_list (result discarded).
 * Otherwise it rebuilds the 10 x 8-byte level list at 0x46cce8 from player 0's
 * profile only, re-probing persistent storage when the active profile index
 * differs from the cached index at 0x31e4c0, asserts the spinner-list widget
 * shape (lines 0x25b/0x25c), publishes the list at widget +0x40/+0x44 and
 * clamps the selected index at +0x3c to [0, 9].  Finally either matches the
 * persisted map name at 0x46cd38 against the level table (0x31e498) or
 * toggles the deferred error-0x27 latch at 0x31e4c4. */
bool solo_level_initialize_list_single_player(void *widget, void *event_data,
                                              bool *widget_deleted)
{
  uint8_t profile[0x30];
  int16_t
    highest_level; /* name: PAL 2342 ui_widget_event_handler_functions.c:5786 */
  int16_t highest_difficulty; /* name: PAL 2342
                                 ui_widget_event_handler_functions.c:5787 */
  int profile_index;
  int i, name_stride;
  signed char level_flags;
  unsigned int flags;
  int16_t *list_tag;
  int selected;
  int16_t stored_index;

  if (*(int16_t *)0x31fa94 >= 2) {
    csmemset((void *)0x46cd38, 0, 0x106);
    ui_widget_initialize_single_player_level_list(widget, event_data,
                                                  widget_deleted);
    return true;
  }

  profile_index = player_ui_get_active_player_profile_index(0);
  csmemset((void *)0x46cce8, 0, 0x50);
  if (profile_index != *(int *)0x31e4c0) {
    csmemset((void *)0x46cd38, 0, 0x106);
    *(uint8_t *)0x46ce3b = (uint8_t)game_state_test_persistent_storage(
      (char *)0x46cd38, (int16_t *)0x46ce38, 0x46ce3c);
    *(int *)0x31e4c0 = profile_index;
  }

  player_ui_get_active_player_profile(0, profile);
  player_profile_save_last_level_played(profile, &highest_level,
                                        &highest_difficulty);

  name_stride = 4; /* scaled index, not a walking pointer */
  i = 0;
  do {
    *(const char **)(0x46cce8 + i * 8) =
      *(const char **)(0x31e498 + i * name_stride);
    level_flags = (signed char)profile[0x1c + i];
    if ((level_flags != 0) || (i == (int)highest_level + 1) || (i == 0)) {
      flags = (unsigned int)(int)level_flags;
      *(uint8_t *)(0x46cce8 + i * 8 + 5) = (uint8_t)((flags >> 1) & 1);
      *(uint8_t *)(0x46cce8 + i * 8 + 4) = 1;
      *(uint8_t *)(0x46cce8 + i * 8 + 6) = (uint8_t)((flags >> 2) & 1);
      *(uint8_t *)(0x46cce8 + i * 8 + 7) = (uint8_t)((flags >> 3) & 1);
    }
    i++;
  } while (i < 10);

  list_tag = (int16_t *)tag_get(0x44654c61, *(int *)widget);
  if (*list_tag != 2) {
    display_assert(
      "expected a spinner list widget for 'solo level list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x25b,
      true);
    system_exit(-1);
  }
  if (*(int *)((char *)list_tag + 0x3e0) != 3) {
    display_assert(
      "expected 3 list items for 'solo level list' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_event_handler_functions.c", 0x25c,
      true);
    system_exit(-1);
  }

  *(int *)((char *)widget + 0x40) = 0x46cce8;
  *(int16_t *)((char *)widget + 0x44) = 10;

  if (player_ui_get_last_single_player_level_played(0) < 0) {
    selected = 0;
  } else if (player_ui_get_last_single_player_level_played(0) > 9) {
    selected = 9;
  } else {
    selected = player_ui_get_last_single_player_level_played(0);
  }
  *(int16_t *)((char *)widget + 0x3c) = selected;

  if (*(uint8_t *)0x46ce3b == 1) {
    *(uint8_t *)0x46ce37 = 0;
    i = 0;
    do {
      if (crt_stricmp((const char *)0x46cd38,
                      *(const char **)(0x31e498 + i * name_stride)) == 0) {
        stored_index = *(int16_t *)0x46ce38;
        *(uint8_t *)0x46ce3a = (uint8_t)i;
        if (stored_index < 0) {
          *(int16_t *)0x46ce38 = 0;
        } else {
          *(int16_t *)0x46ce38 = 3;
          if (stored_index <= 3) {
            *(int16_t *)0x46ce38 = stored_index;
          }
        }
        break;
      }
      i++;
    } while (i < 10);
    if (i == 10) {
      *(uint8_t *)0x46ce3b = 0;
      return true;
    }
  } else if (*(uint8_t *)0x46ce3c == 1) {
    profile_index = player_ui_get_active_player_profile_index(0);
    if (profile_index != -1) {
      if (*(int *)0x31e4c4 == -1) {
        display_error_deferred(0x27, -1, true, false);
        *(int *)0x31e4c4 = profile_index;
        return true;
      }
      *(int *)0x31e4c4 = -1;
    }
  }
  return true;
}
