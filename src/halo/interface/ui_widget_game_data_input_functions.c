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
  int16_t last_level;
  int16_t last_level_unused;
  int profile_index;
  int i;
  signed char level_flags;
  unsigned int flags;
  int16_t *list_tag;
  int16_t selected;
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
  player_profile_save_last_level_played(profile, &last_level,
                                        &last_level_unused);

  /* Bottom-tested rolled loop in the reference (INC EAX / CMP EAX,0xa / JL). */
  i = 0;
  do {
    *(const char **)(0x46cce8 + i * 8) = ((const char **)0x31e498)[i];
    level_flags = (signed char)profile[0x1c + i];
    if ((level_flags != 0) || (i == (int)last_level + 1) || (i == 0)) {
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
      if (crt_stricmp((const char *)0x46cd38, ((const char **)0x31e498)[i]) ==
          0) {
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

void ui_widget_game_data_function_invoke(
  void *widget, unsigned __int16 game_data_input_reference_function)
{
  assert_halt_at(
    "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c", 0x10a,
    widget);

  if (game_data_input_reference_function > 40u) {
    error(2, "invalid game_data_input_reference_function");
  } else {
    ui_widget_game_data_function_table[game_data_input_reference_function](
      widget);
  }
}

/* settings_menu_update_extended_description (0xf0aa0)
 * Updates the extended-description text/pic widgets for a "settings select"
 * list widget's currently highlighted item. Resolves the widget's owner's
 * definition tag via widget+0x48 (tag_get('DeLa', tag_index)) and asserts
 * it is a settings-select widget definition (tag+0x3e0 == 2). Walks the
 * sibling chain at widget+0x34 (via +0x2c "next sibling"), counting the
 * index of the previously-selected child (widget+0x38), then asserts the
 * shape of the extended-description container hanging off
 * (*(widget+0x48))+0x34 — a container widget (+0xe==0) whose first child
 * (+0x2c) is a text-box widget (+0xe==1) — and writes the resolved index
 * into both the container (+0x50) and its text child (+0x40).
 *
 * MSVC reuses the dead incoming widget parameter's stack slot (EBP+8) to
 * cache the text-box child pointer once it is no longer needed as widget;
 * represented below with a separate local (text_widget). */
void settings_menu_update_extended_description(void *widget)
{
  void *widget_def;
  void *description_container;
  void *text_widget;
  int child;
  short index;

  widget_def =
    tag_get(0x44654c61 /* 'DeLa' */, **(int **)((char *)widget + 0x48));

  if ((*(int *)((char *)widget + 0x38) == 0) ||
      (*(int *)((char *)widget + 0x48) == 0)) {
    display_assert(
      "invalid widget trying to update its extended list description",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x12c, 1);
    system_exit(-1);
  }

  if (*(int *)((char *)widget_def + 0x3e0) != 2) {
    display_assert(
      "this doesn't look like the settings select widget to me",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x12d, 1);
    system_exit(-1);
  }

  description_container =
    *(void **)((char *)*(int *)((char *)widget + 0x48) + 0x34);
  text_widget = *(void **)((char *)description_container + 0x2c);

  child = *(int *)((char *)widget + 0x34);
  index = 0;
  if (child != 0) {
    int selected_child = *(int *)((char *)widget + 0x38);
    do {
      if (child == selected_child)
        break;
      child = *(int *)((char *)child + 0x2c);
      index = index + 1;
    } while (child != 0);
    if (index == -1) {
      return;
    }
  }

  if (*(short *)((char *)description_container + 0xe) != 0) {
    display_assert(
      "expected a container widget for the settings select list extended "
      "description pic",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x138, 1);
    system_exit(-1);
  }

  if (*(short *)((char *)text_widget + 0xe) == 1) {
    *(short *)((char *)description_container + 0x50) = index;
    *(short *)((char *)text_widget + 0x40) = index;
    return;
  }

  display_assert(
    "expected a text box widget for the settings select list extended "
    "description text",
    "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c", 0x139,
    1);
  system_exit(-1);
}

/* playlist_settings_menu_update_extended_description (0xf0bb0)
 * Same purpose as settings_menu_update_extended_description above (updates an
 * extended-description widget for a "settings select" list widget's currently
 * highlighted item), but for a widget whose extended-description owner
 * (widget+0x48) holds the index directly on its container
 * ((*(widget+0x48))+0x34) and that container's first child (+0x2c), rather than
 * distinguishing a container/text-box pair by type. Counts the index of
 * widget's currently-selected sibling (widget+0x34 chain via +0x2c, compared
 * against widget+0x38), resolves the owner's definition tag via
 * tag_get('DeLa', *(widget+0x48)) and asserts it is a 2-child widget
 * definition (tag+0x3e0 == 2), then writes the resolved index into both
 * the container (+0x40) and its first child (+0x50). */
void playlist_settings_menu_update_extended_description(void *widget)
{
  int child;
  short index;
  void *widget_def;
  int container;

  if ((widget == 0) || (*(int *)((char *)widget + 0x38) == 0) ||
      (*(int *)((char *)widget + 0x48) == 0)) {
    display_assert(
      "invalid widget trying to update its extended list description",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x14d, 1);
    system_exit(-1);
  }

  child = *(int *)((char *)widget + 0x34);
  index = 0;
  if (child != 0) {
    int selected_child = *(int *)((char *)widget + 0x38);
    do {
      if (child == selected_child)
        break;
      child = *(int *)((char *)child + 0x2c);
      index = index + 1;
    } while (child != 0);
    if (index == -1) {
      return;
    }
  }

  widget_def =
    tag_get(0x44654c61 /* 'DeLa' */, *(int *)*(int *)((char *)widget + 0x48));

  if (*(int *)((char *)widget_def + 0x3e0) != 2) {
    display_assert(
      "expected a container widget w/ 2 children for the playlist settings "
      "list extended description",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x15b, 1);
    system_exit(-1);
  }

  container = *(int *)((char *)*(int *)((char *)widget + 0x48) + 0x34);
  *(short *)((char *)container + 0x40) = index;
  *(short *)((char *)*(int *)((char *)container + 0x2c) + 0x50) = index;
}

/* playlist_gametype_select_menu_update_extended_description (0xf0c60)
 * Same shape as playlist_settings_menu_update_extended_description above
 * (updates an extended-description widget for a "settings select" list widget's
 * currently highlighted item, owner holding the index directly on its
 * container), reusing the identical pooled string literals for both
 * display_assert messages — only the embedded __LINE__ values (0x173, 0x181 vs
 * 0x14d, 0x15b) differ. */
void playlist_gametype_select_menu_update_extended_description(void *widget)
{
  int child;
  short index;
  void *widget_def;
  int container;

  if ((widget == 0) || (*(int *)((char *)widget + 0x38) == 0) ||
      (*(int *)((char *)widget + 0x48) == 0)) {
    display_assert(
      "invalid widget trying to update its extended list description",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x173, 1);
    system_exit(-1);
  }

  child = *(int *)((char *)widget + 0x34);
  index = 0;
  if (child != 0) {
    int selected_child = *(int *)((char *)widget + 0x38);
    do {
      if (child == selected_child)
        break;
      child = *(int *)((char *)child + 0x2c);
      index = index + 1;
    } while (child != 0);
    if (index == -1) {
      return;
    }
  }

  widget_def =
    tag_get(0x44654c61 /* 'DeLa' */, *(int *)*(int *)((char *)widget + 0x48));

  if (*(int *)((char *)widget_def + 0x3e0) != 2) {
    display_assert(
      "expected a container widget w/ 2 children for the playlist settings "
      "list extended description",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x181, 1);
    system_exit(-1);
  }

  container = *(int *)((char *)*(int *)((char *)widget + 0x48) + 0x34);
  *(short *)((char *)container + 0x40) = index;
  *(short *)((char *)*(int *)((char *)container + 0x2c) + 0x50) = index;
}

/* multiplayer_type_menu_update_extended_description (0xf0d10)
 * Same purpose as
 * playlist_settings_menu_update_extended_description/playlist_gametype_select_menu_update_extended_description
 * above (updates an extended-description widget for a "settings select" list
 * widget's currently-highlighted item: counts the index of widget's currently
 * selected sibling via the +0x34/+0x2c chain compared against widget+0x38),
 * but does not resolve a 'DeLa' tag definition — instead it validates
 * widget+0x48's container (+0x34) and that container's first child (+0x2c)
 * directly, and writes the resolved index to container+0x50 and
 * (container's first child)+0x40, the offsets swapped relative to
 * playlist_settings_menu_update_extended_description/playlist_gametype_select_menu_update_extended_description's
 * +0x40/+0x50 writes. The final two stores each re-derive the container from
 * widget+0x48 independently (matching two separate reloads in the disassembly,
 * 0xf0d75 and 0xf0d7f), rather than reusing the value computed for the guard
 * check. */
void multiplayer_type_menu_update_extended_description(void *widget)
{
  int container;
  int child;
  short index;

  if ((widget == 0) || (*(int *)((char *)widget + 0x38) == 0) ||
      (*(int *)((char *)widget + 0x48) == 0) ||
      (container = *(int *)((char *)*(int *)((char *)widget + 0x48) + 0x34),
       container == 0) ||
      (*(int *)((char *)container + 0x2c) == 0)) {
    display_assert(
      "invalid widget trying to update its extended list description",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x1a6, 1);
    system_exit(-1);
  }

  child = *(int *)((char *)widget + 0x34);
  index = 0;
  if (child != 0) {
    int selected_child = *(int *)((char *)widget + 0x38);
    do {
      if (child == selected_child)
        break;
      child = *(int *)((char *)child + 0x2c);
      index = index + 1;
    } while (child != 0);
    if (index == -1) {
      return;
    }
  }

  *(short *)((char *)*(int *)((char *)*(int *)((char *)widget + 0x48) + 0x34) +
             0x50) = index;
  *(short *)((char *)*(
               int *)((char *)*(int *)((char *)*(int *)((char *)widget + 0x48) +
                                       0x34) +
                      0x2c) +
             0x40) = index;
}

/* difficulty_select_menu_update_extended_description (0xf0d90)
 * Same shape as settings_menu_update_extended_description above (updates the
 * extended-description text/pic widgets for a "settings select" list widget's
 * currently highlighted item), but for the difficulty-select widget: resolves
 * the widget's owner's definition tag via widget+0x48 (tag_get('DeLa',
 * tag_index)) and asserts it is a 2-child widget definition (tag+0x3e0 ==
 * 2). Walks the sibling chain at widget+0x34 (via +0x2c "next sibling"),
 * counting the index of the previously-selected child (widget+0x38), then
 * asserts the shape of the extended-description container hanging off
 * (*(widget+0x48))+0x34 — a container widget (+0xe==0) whose first child
 * (+0x2c) is a text-box widget (+0xe==1) — and writes the resolved index
 * into both the container (+0x50) and its text child (+0x40). */
void difficulty_select_menu_update_extended_description(void *widget)
{
  void *widget_def;
  void *description_container;
  void *text_widget;
  int child;
  short index;

  widget_def =
    tag_get(0x44654c61 /* 'DeLa' */, **(int **)((char *)widget + 0x48));

  if ((*(int *)((char *)widget + 0x38) == 0) ||
      (*(int *)((char *)widget + 0x48) == 0)) {
    display_assert(
      "invalid widget trying to update its extended list description",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x237, 1);
    system_exit(-1);
  }

  if (*(int *)((char *)widget_def + 0x3e0) != 2) {
    display_assert(
      "this doesn't look like the difficulty select widget to me",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x238, 1);
    system_exit(-1);
  }

  description_container =
    *(void **)((char *)*(int *)((char *)widget + 0x48) + 0x34);
  text_widget = *(void **)((char *)description_container + 0x2c);

  child = *(int *)((char *)widget + 0x34);
  index = 0;
  if (child != 0) {
    int selected_child = *(int *)((char *)widget + 0x38);
    do {
      if (child == selected_child)
        break;
      child = *(int *)((char *)child + 0x2c);
      index = index + 1;
    } while (child != 0);
    if (index == -1) {
      return;
    }
  }

  if (*(short *)((char *)description_container + 0xe) != 0) {
    display_assert(
      "expected a container widget for the difficulty list extended "
      "description pic",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x243, 1);
    system_exit(-1);
  }

  if (*(short *)((char *)text_widget + 0xe) == 1) {
    *(short *)((char *)description_container + 0x50) = index;
    *(short *)((char *)text_widget + 0x40) = index;
    return;
  }

  display_assert(
    "expected a text box widget for the difficulty list extended "
    "description text",
    "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c", 0x244,
    1);
  system_exit(-1);
}

void set_textbox_to_build_number(int widget)
{
  wchar_t *v1, *v2; // eax

  if (!ui_widget_game_data_build_version_wide_str[0]) {
    ascii_to_wide(
#if DECOMP_CUSTOM
      build_ui_widget_text,
#else
      "01.10.12.2276",
#endif
      ui_widget_game_data_build_version_wide_str,
      sizeof(ui_widget_game_data_build_version_wide_str));
  }

  if (!*(uint32_t *)(widget + 60)) {
    v1 =
      ui_widget_realloc(0, sizeof(ui_widget_game_data_build_version_wide_str),
                        __FILE__, __LINE__);
    *(uint32_t *)(widget + 60) = (uint32_t)v1;
    if (v1) {
      csmemset(v1, 0, sizeof(ui_widget_game_data_build_version_wide_str));
    }
  }
  v2 = *(wchar_t **)(widget + 60);
  if (v2) {
    ustrncpy(v2, ui_widget_game_data_build_version_wide_str, 0x3Fu);
    *(wchar_t *)(*(uint32_t *)(widget + 60) + 126) = 0;
  }
}

/* server_list_menu_update (0xf0f30)
 * Rebuilds the 9-entry advertised-server pointer list at 0x46ce40 (open
 * games with +0xe0 != 0 first, then +0xe0 == 0; both require
 * advertised_game_is_valid and +0xde == 0), stores it into the list widget
 * (+0x40 list, +0x44 count), clamps the selected index (+0x3c), writes each
 * child's text, then fills the extended-description textboxes hanging off
 * widget+0x48 from the selected advertised game (0xe4-byte records).
 * Textbox roles come from the binary's assert strings. unknown_widget is the
 * second sibling of the map-name container's next sibling; its role is
 * unproven. */
void server_list_menu_update(void *widget)
{
  void *client;
  void *widget_def;
  char *game;
  char *child;
  char *text;
  char *map_name;
  char *description_container;
  char *map_container;
  char *unknown_widget;
  char *open_text;
  char *map_text;
  char *ruleset_text;
  char *teams_text;
  char *players_text;
  char *score_limit_text;
  char *score_limit_type_text;
  unsigned int elapsed;
  int count;
  int remaining;
  int selected;
  int i;

  client = global_network_game_client_get();
  count = 0;
  csmemset((void *)0x46ce40, 0, 0x24);
  if (client == NULL) {
    return;
  }

  widget_def = tag_get(0x44654c61 /* 'DeLa' */, *(int *)widget);
  game = (char *)network_game_client_get_available_games(client);
  if ((*(short *)widget_def != 3) ||
      (*(int *)((char *)widget_def + 0x3e0) != 9)) {
    display_assert(
      "this doesn't look like the net game server list widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x297, 1);
    system_exit(-1);
  }

  child = game;
  remaining = 9;
  do {
    if (network_game_client_advertised_game_is_valid(child) &&
        *(short *)(child + 0xde) == 0 && *(char *)(child + 0xe0) != 0) {
      ((char **)0x46ce40)[count] = child;
      count++;
    }
    child += 0xe4;
    remaining--;
  } while (remaining != 0);

  remaining = 9;
  do {
    if (network_game_client_advertised_game_is_valid(game) &&
        *(short *)(game + 0xde) == 0 && *(char *)(game + 0xe0) == 0) {
      ((char **)0x46ce40)[count] = game;
      count++;
    }
    game += 0xe4;
    remaining--;
  } while (remaining != 0);

  *(int *)((char *)widget + 0x40) = 0x46ce40;
  *(short *)((char *)widget + 0x44) = (short)count;
  selected = *(short *)((char *)widget + 0x3c);
  if (selected > count - 1) {
    selected = count - 1;
  }
  child = *(char **)((char *)widget + 0x34);
  *(short *)((char *)widget + 0x3c) = (short)selected;

  for (i = 0; child != NULL && i < count; i++) {
    text = (char *)ui_widget_realloc(
      *(int *)(child + 0x3c), 0x40,
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x2c5);
    *(char **)(child + 0x3c) = text;
    if (text != NULL) {
      if (*(char *)(((char **)0x46ce40)[i] + 0xe0) == 1) {
        ustrncpy((wchar_t *)text, (wchar_t *)(((char **)0x46ce40)[i] + 0x30),
                 0x1f);
      } else {
        unicode_sprintf(
          *(wchar_t **)(child + 0x3c), 0x1f, L"%s %s",
          (wchar_t *)FUN_0019d420(
            tag_loaded(0x75737472 /* 'ustr' */, "ui\\multiplayer_game_text"),
            0x13),
          (wchar_t *)(((char **)0x46ce40)[i] + 0x30));
      }
      *(short *)(*(char **)(child + 0x3c) + 0x3e) = 0;
    }
    child = *(char **)(child + 0x2c);
  }

  if (count > 0 && *(short *)((char *)widget + 0x3c) < 0) {
    *(short *)((char *)widget + 0x3c) = 0;
  }

  elapsed = system_milliseconds() - *(unsigned int *)((char *)widget + 0x18);
  widget_def =
    tag_get(0x44654c61 /* 'DeLa' */, **(int **)((char *)widget + 0x48));
  if (*(int *)((char *)widget_def + 0x3e0) != 5) {
    display_assert(
      "this doesn't look like the server list extended description widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x2f7, 1);
    system_exit(-1);
  }

  description_container = *(char **)(*(char **)((char *)widget + 0x48) + 0x34);
  map_container = *(char **)(description_container + 0x2c);
  open_text = *(char **)(*(char **)(map_container + 0x2c) + 0x34);
  unknown_widget = *(char **)(*(char **)(map_container + 0x2c) + 0x2c);
  if (open_text == NULL || *(short *)(open_text + 0xe) != 1) {
    display_assert(
      "expected 'open/closed game' textbox",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x2ff, 1);
    system_exit(-1);
  }
  map_text = *(char **)(open_text + 0x2c);
  if (map_text == NULL || *(short *)(map_text + 0xe) != 1) {
    display_assert(
      "expected 'map name' textbox",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x301, 1);
    system_exit(-1);
  }
  ruleset_text = *(char **)(map_text + 0x2c);
  if (ruleset_text == NULL || *(short *)(ruleset_text + 0xe) != 1) {
    display_assert(
      "expected 'ruleset' textbox",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x303, 1);
    system_exit(-1);
  }
  teams_text = *(char **)(ruleset_text + 0x2c);
  if (teams_text == NULL || *(short *)(teams_text + 0xe) != 1) {
    display_assert(
      "expected 'teams on/off' textbox",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x305, 1);
    system_exit(-1);
  }
  players_text = *(char **)(teams_text + 0x2c);
  if (players_text == NULL || *(short *)(players_text + 0xe) != 1) {
    display_assert(
      "expected 'number of players' textbox",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x307, 1);
    system_exit(-1);
  }
  score_limit_text = *(char **)(players_text + 0x2c);
  if (score_limit_text == NULL || *(short *)(score_limit_text + 0xe) != 1) {
    display_assert(
      "expected 'score limit' textbox",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x309, 1);
    system_exit(-1);
  }
  score_limit_type_text = *(char **)(score_limit_text + 0x2c);
  if (score_limit_type_text == NULL ||
      *(short *)(score_limit_type_text + 0xe) != 1) {
    display_assert(
      "expected 'score limit type' textbox",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x30b, 1);
    system_exit(-1);
  }

  if (*(short *)((char *)widget + 0x3c) >= 0) {
    game = ((char **)0x46ce40)[*(short *)((char *)widget + 0x3c)];
    switch (*(short *)(game + 0xd4)) {
    case 1:
      *(short *)(description_container + 0x50) = 0;
      break;
    case 2:
      *(short *)(description_container + 0x50) = 2;
      break;
    case 3:
      *(short *)(description_container + 0x50) = 3;
      break;
    case 4:
      *(short *)(description_container + 0x50) = 1;
      break;
    case 5:
      *(short *)(description_container + 0x50) = 4;
      break;
    default:
      *(short *)(description_container + 0x50) = 5;
      break;
    }

    map_name = game + 0x54;
    if (crt_strstr(map_name, "beavercreek") != NULL) {
      *(short *)(map_container + 0x50) = 0;
    } else if (crt_strstr(map_name, "sidewinder") != NULL) {
      *(short *)(map_container + 0x50) = 1;
    } else if (crt_strstr(map_name, "damnation") != NULL) {
      *(short *)(map_container + 0x50) = 2;
    } else if (crt_strstr(map_name, "ratrace") != NULL) {
      *(short *)(map_container + 0x50) = 3;
    } else if (crt_strstr(map_name, "prisoner") != NULL) {
      *(short *)(map_container + 0x50) = 4;
    } else if (crt_strstr(map_name, "hangemhigh") != NULL) {
      *(short *)(map_container + 0x50) = 5;
    } else if (crt_strstr(map_name, "chillout") != NULL) {
      *(short *)(map_container + 0x50) = 6;
    } else if (crt_strstr(map_name, "carousel") != NULL) {
      *(short *)(map_container + 0x50) = 7;
    } else if (crt_strstr(map_name, "boardingaction") != NULL) {
      *(short *)(map_container + 0x50) = 8;
    } else if (crt_strstr(map_name, "bloodgulch") != NULL) {
      *(short *)(map_container + 0x50) = 9;
    } else if (crt_strstr(map_name, "wizard") != NULL) {
      *(short *)(map_container + 0x50) = 10;
    } else if (crt_strstr(map_name, "putput") != NULL) {
      *(short *)(map_container + 0x50) = 0xb;
    } else if (crt_strstr(map_name, "longest") != NULL) {
      *(short *)(map_container + 0x50) = 0xc;
    } else {
      *(short *)(map_container + 0x50) = 0xd;
    }

    *(short *)(open_text + 0x40) =
      (short)((*(char *)(game + 0xe0) != 1) + 0x14);
    *(short *)(map_text + 0x40) = *(short *)(map_container + 0x50);

    switch (*(short *)(game + 0xd4)) {
    case 1:
      *(short *)(ruleset_text + 0x40) = 3;
      break;
    case 2:
      *(short *)(ruleset_text + 0x40) = 4;
      break;
    case 3:
      *(short *)(ruleset_text + 0x40) = 5;
      break;
    case 4:
      *(short *)(ruleset_text + 0x40) = 6;
      break;
    case 5:
      *(short *)(ruleset_text + 0x40) = 7;
      break;
    default:
      *(short *)(ruleset_text + 0x40) = 8;
      break;
    }

    *(short *)(teams_text + 0x40) =
      (short)((*(char *)(game + 0xe2) != 1) + 0xc);

    text = (char *)ui_widget_realloc(
      *(int *)(players_text + 0x3c), 8,
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x364);
    *(char **)(players_text + 0x3c) = text;
    if (text != NULL) {
      unicode_sprintf((wchar_t *)text, 3, L"%d",
                      (unsigned int)*(unsigned short *)(game + 0xd8));
      *(short *)(*(char **)(players_text + 0x3c) + 6) = 0;
    }

    text = (char *)ui_widget_realloc(
      *(int *)(score_limit_text + 0x3c), 8,
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x369);
    *(char **)(score_limit_text + 0x3c) = text;
    if (text != NULL) {
      unicode_sprintf((wchar_t *)text, 3, L"%d", (int)*(short *)(game + 0xdc));
      *(short *)(*(char **)(score_limit_text + 0x3c) + 6) = 0;
    }

    switch (*(short *)(game + 0xd4)) {
    case 1:
      *(short *)(score_limit_type_text + 0x40) = 0x16;
      break;
    case 2:
      *(short *)(score_limit_type_text + 0x40) = 0x18;
      break;
    case 3:
      *(short *)(score_limit_type_text + 0x40) =
        (short)(0x18 - (*(char *)(game + 0xe3) != 1));
      break;
    case 4:
      *(short *)(score_limit_type_text + 0x40) = 0x17;
      break;
    case 5:
      *(short *)(score_limit_type_text + 0x40) = 0x19;
      break;
    default:
      *(short *)(score_limit_type_text + 0x40) = 1;
      break;
    }

    *(short *)(unknown_widget + 0x40) = 2;
    *(char *)(unknown_widget + 0x10) = 0;
    if (*(int *)((char *)widget + 0x38) == 0) {
      *(short *)((char *)widget + 0x3c) = 0;
      *(int *)((char *)widget + 0x38) = *(int *)((char *)widget + 0x34);
    }
  } else {
    *(short *)(description_container + 0x50) = 5;
    *(short *)(map_container + 0x50) = 0xd;
    *(short *)(open_text + 0x40) = 1;
    *(short *)(map_text + 0x40) = 0xe;
    *(short *)(ruleset_text + 0x40) = 1;
    *(short *)(teams_text + 0x40) = 1;

    text = (char *)ui_widget_realloc(
      *(int *)(players_text + 0x3c), 8,
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x3a3);
    *(char **)(players_text + 0x3c) = text;
    if (text != NULL) {
      *(short *)text = 0;
    }

    text = (char *)ui_widget_realloc(
      *(int *)(score_limit_text + 0x3c), 8,
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x3a7);
    *(char **)(score_limit_text + 0x3c) = text;
    if (text != NULL) {
      *(short *)text = 0;
    }

    *(short *)(score_limit_type_text + 0x40) = 1;
    *(short *)(unknown_widget + 0x40) = (short)(elapsed >= 1000);
    *(char *)(unknown_widget + 0x10) = 1;
  }
}

/* network_pregame_status_screen_update (0xf1710, update-function table data
 * xref 0x31e528). Reads the widget argument from [esp+4] (cdecl). Asserts the
 * widget definition's +0x3e0 type is 6, then (when a network game exists):
 * writes the game-start countdown ("-:--", "0:%02d", "%02d:%02d" or
 * "%d:%02d:%02d") into the fifth sibling after the first child, fills the
 * local machine name (machine records: 4 x 0x44 bytes at game+0x114, machine
 * index byte at +0x40) and the four local-player rows (player records:
 * 16 x 0x20 bytes at game+0x226, machine byte +0x1c, local index byte +0x1d,
 * team byte +0x1e), then the three remote-machine rows. The 4x3 byte table
 * at 0x2888f4 supplies the container +0x50 values; its meaning is unproven.
 * Widget roles come from the binary's assert strings. */
void network_pregame_status_screen_update(void *widget)
{
  char *game;
  void *widget_def;
  char *status_widget;
  char *sibling_1;
  char *sibling_2;
  char *sibling_3;
  char *sibling_4;
  char *countdown_text;
  char *local_status;
  char *local_name_container;
  char *machine;
  char *player;
  char *container;
  char *name_text;
  char *team_spinner;
  char *child;
  wchar_t *local_name;
  wchar_t *name;
  void *text;
  int player_indices[4];
  int length;
  int count;
  int index;
  int i;
  int j;
  short machine_index;
  short seconds;
  int hours;
  int minutes;
  char flag;
  char team;
  unsigned short value;
  const unsigned char *bitmap_table;

  bitmap_table = (const unsigned char *)0x2888f4;
  game = (char *)network_game_get_game();
  widget_def = tag_get(0x44654c61 /* 'DeLa' */, *(int *)widget);
  if (*(int *)((char *)widget_def + 0x3e0) != 6) {
    display_assert(
      "this doesn't look like the net pregame status screen to me",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x3fc, 1);
    system_exit(-1);
  }
  if (game == NULL) {
    return;
  }

  machine_index = (short)network_game_client_get_machine_index(
    global_network_game_client_get());
  status_widget = *(char **)((char *)widget + 0x34);
  sibling_1 = *(char **)(status_widget + 0x2c);
  sibling_2 = *(char **)(sibling_1 + 0x2c);
  sibling_3 = *(char **)(sibling_2 + 0x2c);
  sibling_4 = *(char **)(sibling_3 + 0x2c);
  countdown_text = *(char **)(sibling_4 + 0x2c);
  text = ui_widget_realloc(
    *(int *)(countdown_text + 0x3c), 0x20,
    "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
    0x40b);
  *(void **)(countdown_text + 0x3c) = text;
  if (text != NULL) {
    seconds = network_game_client_get_seconds_to_game_start(
      global_network_game_client_get());
    if (global_network_game_server_get() != NULL &&
        *(short *)(game + 0x112) < 2) {
      flag = 1;
    } else {
      flag = 0;
    }
    ustrncpy(*(wchar_t **)(countdown_text + 0x3c), L"-:--", 0xf);
    *(char *)(sibling_4 + 0x10) = 1;
    *(short *)(sibling_4 + 0x40) = 0;
    *(char *)(countdown_text + 0x10) = 1;
    if (seconds == 0) {
      *(short *)(sibling_4 + 0x40) = 1;
      *(char *)(countdown_text + 0x10) = 0;
    } else if (seconds > 0) {
      if (seconds < 60) {
        unicode_sprintf(*(wchar_t **)(countdown_text + 0x3c), 0xf, L"0:%02d",
                        (int)seconds);
      } else if (seconds < 3600) {
        unicode_sprintf(*(wchar_t **)(countdown_text + 0x3c), 0xf, L"%02d:%02d",
                        seconds / 60, seconds % 60);
      } else {
        hours = seconds / 3600;
        minutes = (seconds % 3600) / 60;
        unicode_sprintf(*(wchar_t **)(countdown_text + 0x3c), 0xf,
                        L"%d:%02d:%02d", hours, minutes,
                        seconds - (hours * 60 + minutes) * 60);
      }
    } else if (flag || *(char *)(game + 0xc0) == 1) {
      *(char *)(sibling_4 + 0x10) = 0;
      *(char *)(countdown_text + 0x10) = 0;
    }
    *(short *)(*(char **)(countdown_text + 0x3c) + 0x1e) = 0;
  }

  widget_def = tag_get(0x44654c61 /* 'DeLa' */, *(int *)status_widget);
  if (*(int *)((char *)widget_def + 0x3e0) != 6) {
    display_assert(
      "this doesn't look like the net pregame status screen to me",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x44c, 1);
    system_exit(-1);
  }

  {
    char *local_widgets[4];

    local_name_container = *(char **)(status_widget + 0x34);
    local_status = *(char **)(local_name_container + 0x2c);
    local_widgets[0] = *(char **)(local_status + 0x2c);
    local_widgets[1] = *(char **)(local_widgets[0] + 0x2c);
    local_widgets[2] = *(char **)(local_widgets[1] + 0x2c);
    local_widgets[3] = *(char **)(local_widgets[2] + 0x2c);

    flag = 0;
    local_name = L"?";
    for (i = 0; i < 4; i++) {
      machine = game + 0x114 + i * 0x44;
      if (machine != NULL && *(char *)(machine + 0x40) >= 0 &&
          *(char *)(machine + 0x40) < 4 &&
          (short)*(char *)(machine + 0x40) == machine_index) {
        name = (wchar_t *)network_game_client_get_machine(
          global_network_game_client_get());
        if (name != NULL && *name != 0) {
          local_name = name;
        }
        flag = 1;
      }
    }

    csmemset(player_indices, -1, 0x10);
    count = 0;
    for (i = 0; i < 0x10; i++) {
      player = game + 0x226 + i * 0x20;
      if (network_player_is_valid(player) &&
          (short)*(char *)(player + 0x1c) == machine_index) {
        count++;
        player_indices[(int)*(char *)(player + 0x1d)] = i;
        if (count == 4) {
          break;
        }
      }
    }

    length = ustrlen((unsigned short *)local_name);
    text = ui_widget_realloc(
      *(int *)(local_name_container + 0x3c), (unsigned short)(length * 2 + 2),
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x47b);
    *(void **)(local_name_container + 0x3c) = text;
    if (text != NULL) {
      ustrncpy((wchar_t *)text, local_name, length);
      (*(wchar_t **)(local_name_container + 0x3c))[length] = 0;
    }

    *(short *)(local_status + 0x50) = (short)(flag != 0);
    for (j = 0; j < 4; j++) {
      widget_def = tag_get(0x44654c61 /* 'DeLa' */, *(int *)local_widgets[j]);
      if (*(int *)((char *)widget_def + 0x3e0) != 3) {
        display_assert(
          "this doesn't look like the net pregame status screen to me",
          "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
          0x485, 1);
        system_exit(-1);
      }
      container = *(char **)(local_widgets[j] + 0x34);
      if (container == NULL || *(short *)(container + 0xe) != 0) {
        display_assert(
          "expected container widget for local player controller bitmap",
          "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
          0x48b, 1);
        system_exit(-1);
      }
      name_text = *(char **)(container + 0x2c);
      if (name_text == NULL || *(short *)(name_text + 0xe) != 1) {
        display_assert(
          "expected a text box for local player name field",
          "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
          0x48d, 1);
        system_exit(-1);
      }
      team_spinner = *(char **)(name_text + 0x2c);
      if (team_spinner == NULL || *(short *)(team_spinner + 0xe) != 2) {
        display_assert(
          "expected a spinner list for local player team display",
          "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
          0x48f, 1);
        system_exit(-1);
      }
      widget_instance_set_visibility_recursive(team_spinner,
                                               *(char *)(game + 0xc0) != 0);
      if (player_indices[j] == -1) {
        *(short *)(container + 0x50) = 0;
        text = ui_widget_realloc(
          *(int *)(name_text + 0x3c), 2,
          "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
          0x49d);
        *(void **)(name_text + 0x3c) = text;
        if (text != NULL) {
          *(wchar_t *)text = 0;
        }
        *(short *)(team_spinner + 0x3c) = 2;
      } else {
        length =
          ustrlen((unsigned short *)(game + 0x226 + player_indices[j] * 0x20));
        text = ui_widget_realloc(
          *(int *)(name_text + 0x3c), (unsigned short)(length * 2 + 2),
          "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
          0x4a6);
        *(void **)(name_text + 0x3c) = text;
        if (text != NULL) {
          ustrncpy((wchar_t *)text,
                   (wchar_t *)(game + 0x226 + player_indices[j] * 0x20),
                   length);
          (*(wchar_t **)(name_text + 0x3c))[length] = 0;
        }
        if (*(char *)(game + 0xc0) == 0) {
          *(short *)(container + 0x50) = 1;
        } else {
          switch (*(char *)(game + 0x244 + player_indices[j] * 0x20)) {
          case 0:
            *(short *)(container + 0x50) = bitmap_table[j * 3 + 2];
            *(short *)(team_spinner + 0x3c) = 0;
            break;
          case 1:
            *(short *)(container + 0x50) = bitmap_table[j * 3 + 1];
            *(short *)(team_spinner + 0x3c) = 1;
            break;
          default:
            *(short *)(container + 0x50) = bitmap_table[j * 3 + 0];
            *(short *)(team_spinner + 0x3c) = 2;
            break;
          }
        }
      }
    }
  }

  {
    char *remote_widgets[3];
    int remote_machines[3];

    remote_widgets[0] = sibling_1;
    remote_widgets[1] = sibling_2;
    remote_widgets[2] = sibling_3;
    csmemset(remote_machines, -1, 0xc);
    count = 0;
    for (i = 0; i < 4; i++) {
      machine = game + 0x114 + i * 0x44;
      if (machine != NULL && *(char *)(machine + 0x40) >= 0 &&
          *(char *)(machine + 0x40) < 4 &&
          (short)*(char *)(machine + 0x40) != machine_index) {
        if (!(count < 3)) {
          display_assert("j<(MAXIMUM_NETWORK_MACHINE_COUNT-1)",
                         "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_"
                         "input_functions.c",
                         0x4d4, 1);
          system_exit(-1);
        }
        remote_machines[count] = i;
        count++;
      }
    }

    for (j = 0; j < 3; j++) {
      widget_def = tag_get(0x44654c61 /* 'DeLa' */, *(int *)remote_widgets[j]);
      if (*(int *)((char *)widget_def + 0x3e0) != 6) {
        display_assert(
          "this doesn't look like the net pregame status screen to me",
          "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
          0x4dd, 1);
        system_exit(-1);
      }
      container = *(char **)(remote_widgets[j] + 0x34);
      name_text = *(char **)(container + 0x2c);
      if (remote_machines[j] == -1) {
        *(short *)(container + 0x50) = 0;
        text = ui_widget_realloc(
          *(int *)(name_text + 0x3c), 2,
          "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
          0x4e7);
        *(void **)(name_text + 0x3c) = text;
        if (text != NULL) {
          *(wchar_t *)text = 0;
        }
        child = *(char **)(name_text + 0x2c);
        for (i = 0; child != NULL && i < 4; i++) {
          *(short *)(child + 0x50) = 2;
          child = *(char **)(child + 0x2c);
        }
      } else {
        *(short *)(container + 0x50) = 1;
        length =
          ustrlen((unsigned short *)(game + 0x114 + remote_machines[j] * 0x44));
        text = ui_widget_realloc(
          *(int *)(name_text + 0x3c), (unsigned short)(length * 2 + 2),
          "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
          0x4f4);
        *(void **)(name_text + 0x3c) = text;
        if (text != NULL) {
          ustrncpy((wchar_t *)text,
                   (wchar_t *)(game + 0x114 + remote_machines[j] * 0x44),
                   length);
          (*(wchar_t **)(name_text + 0x3c))[length] = 0;
        }
        csmemset(player_indices, -1, 0x10);
        for (index = 0; index < 0x10; index++) {
          player = game + 0x226 + index * 0x20;
          if (network_player_is_valid(player) &&
              *(char *)(player + 0x1c) ==
                *(char *)(game + 0x114 + remote_machines[j] * 0x44 + 0x40)) {
            player_indices[(int)*(char *)(player + 0x1d)] = index;
          }
        }
        child = *(char **)(name_text + 0x2c);
        for (i = 0; i < 4; i++) {
          if (child == NULL) {
            break;
          }
          if (player_indices[i] == -1) {
            *(short *)(child + 0x50) = 2;
          } else {
            if (*(char *)(game + 0xc0) == 0) {
              value = bitmap_table[i * 3 + 0];
            } else {
              team = *(char *)(game + 0x244 + player_indices[i] * 0x20);
              switch (team) {
              case 0:
                value = bitmap_table[i * 3 + 2];
                break;
              case 1:
                value = bitmap_table[i * 3 + 1];
                break;
              default:
                value = bitmap_table[i * 3 + 0];
                break;
              }
            }
            *(short *)(child + 0x50) = (short)value;
          }
          child = *(char **)(child + 0x2c);
        }
      }
    }
  }
}

/* splitscreen_pregame_status_screen_update (0xf1ed0). Reads the widget
 * argument from [ebp+8] (cdecl; no direct callers, update-function table
 * entry like network_pregame_status_screen_update). Asserts the widget
 * definition's +0x3e0 type is 3, then (when a network game exists): writes
 * the game-start countdown into the second child after the first child, then
 * (status widget type 6) fills the local machine name and the four local
 * player rows. The 4x3 byte table at 0x288900 supplies the container +0x50
 * values; its meaning is unproven. The assert strings are the binary's own
 * ("net pregame status screen"). */
void splitscreen_pregame_status_screen_update(void *widget)
{
  char *game;
  void *widget_def;
  char *status_widget;
  char *sibling_1;
  char *countdown_text;
  char *local_name_container;
  char *local_status;
  char *machine;
  char *player;
  char *container;
  char *name_text;
  char *team_spinner;
  wchar_t *local_name;
  wchar_t *name;
  void *text;
  char *local_widgets[4];
  int player_indices[4];
  int length;
  int count;
  int i;
  int j;
  short machine_index;
  short seconds;
  int hours;
  int minutes;
  char flag;
  const unsigned char *bitmap_table;

  bitmap_table = (const unsigned char *)0x288900;
  game = (char *)network_game_get_game();
  widget_def = tag_get(0x44654c61 /* 'DeLa' */, *(int *)widget);
  if (*(int *)((char *)widget_def + 0x3e0) != 3) {
    display_assert(
      "this doesn't look like the net pregame status screen to me",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x550, 1);
    system_exit(-1);
  }
  if (game == NULL) {
    return;
  }

  machine_index = (short)network_game_client_get_machine_index(
    global_network_game_client_get());
  status_widget = *(char **)((char *)widget + 0x34);
  sibling_1 = *(char **)(status_widget + 0x2c);
  countdown_text = *(char **)(sibling_1 + 0x2c);
  text = ui_widget_realloc(
    *(int *)(countdown_text + 0x3c), 0x20,
    "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
    0x55c);
  *(void **)(countdown_text + 0x3c) = text;
  if (text != NULL) {
    seconds = network_game_client_get_seconds_to_game_start(
      global_network_game_client_get());
    ustrncpy(*(wchar_t **)(countdown_text + 0x3c), L"-:--", 0xf);
    *(char *)(sibling_1 + 0x10) = 1;
    *(short *)(sibling_1 + 0x40) = 0;
    *(char *)(countdown_text + 0x10) = 1;
    if (seconds == 0) {
      *(short *)(sibling_1 + 0x40) = 1;
      *(char *)(countdown_text + 0x10) = 0;
    } else if (seconds > 0) {
      if (seconds < 60) {
        unicode_sprintf(*(wchar_t **)(countdown_text + 0x3c), 0xf, L"0:%02d",
                        (int)seconds);
      } else if (seconds < 3600) {
        unicode_sprintf(*(wchar_t **)(countdown_text + 0x3c), 0xf, L"%02d:%02d",
                        seconds / 60, seconds % 60);
      } else {
        hours = seconds / 3600;
        minutes = (seconds - hours * 3600) / 60;
        unicode_sprintf(*(wchar_t **)(countdown_text + 0x3c), 0xf,
                        L"%d:%02d:%02d", hours, minutes,
                        seconds - (hours * 60 + minutes) * 60);
      }
    } else if (*(short *)(game + 0x224) < 2 || *(char *)(game + 0xc0) == 1) {
      *(char *)(sibling_1 + 0x10) = 0;
      *(char *)(countdown_text + 0x10) = 0;
    }
    *(short *)(*(char **)(countdown_text + 0x3c) + 0x1e) = 0;
  }

  widget_def = tag_get(0x44654c61 /* 'DeLa' */, *(int *)status_widget);
  if (*(int *)((char *)widget_def + 0x3e0) != 6) {
    display_assert(
      "this doesn't look like the net pregame status screen to me",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x592, 1);
    system_exit(-1);
  }

  local_name_container = *(char **)(status_widget + 0x34);
  local_status = *(char **)(local_name_container + 0x2c);
  local_widgets[0] = *(char **)(local_status + 0x2c);
  local_widgets[1] = *(char **)(local_widgets[0] + 0x2c);
  local_widgets[2] = *(char **)(local_widgets[1] + 0x2c);
  local_widgets[3] = *(char **)(local_widgets[2] + 0x2c);

  flag = 0;
  local_name = L"?";
  for (i = 0; i < 4; i++) {
    machine = game + 0x114 + i * 0x44;
    if (machine != NULL && *(char *)(machine + 0x40) >= 0 &&
        *(char *)(machine + 0x40) < 4 &&
        (short)*(char *)(machine + 0x40) == machine_index) {
      name = (wchar_t *)network_game_client_get_machine(
        global_network_game_client_get());
      if (name != NULL && *name != 0) {
        local_name = name;
      }
      flag = 1;
      break;
    }
  }

  csmemset(player_indices, -1, 0x10);
  count = 0;
  for (i = 0; i < 0x10; i++) {
    player = game + 0x226 + i * 0x20;
    if (network_player_is_valid(player) &&
        (short)*(char *)(player + 0x1c) == machine_index) {
      count++;
      player_indices[(int)*(char *)(player + 0x1d)] = i;
      if (count == 4) {
        break;
      }
    }
  }

  length = ustrlen((unsigned short *)local_name);
  text = ui_widget_realloc(
    *(int *)(local_name_container + 0x3c), (unsigned short)(length * 2 + 2),
    "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
    0x5c2);
  *(void **)(local_name_container + 0x3c) = text;
  if (text != NULL) {
    ustrncpy((wchar_t *)text, local_name, length);
    (*(wchar_t **)(local_name_container + 0x3c))[length] = 0;
  }

  *(short *)(local_status + 0x50) = (short)(flag != 0);
  for (j = 0; j < 4; j++) {
    widget_def = tag_get(0x44654c61 /* 'DeLa' */, *(int *)local_widgets[j]);
    if (*(int *)((char *)widget_def + 0x3e0) != 3) {
      display_assert(
        "this doesn't look like the net pregame status screen to me",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x5cc, 1);
      system_exit(-1);
    }
    container = *(char **)(local_widgets[j] + 0x34);
    name_text = *(char **)(container + 0x2c);
    team_spinner = *(char **)(name_text + 0x2c);
    if (*(char *)(game + 0xc0) == 0) {
      widget_instance_set_visibility_recursive(team_spinner, 0);
    }
    if (player_indices[j] == -1) {
      *(short *)(container + 0x50) = 0;
      text = ui_widget_realloc(
        *(int *)(name_text + 0x3c), 2,
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x5db);
      *(void **)(name_text + 0x3c) = text;
      if (text != NULL) {
        *(wchar_t *)text = 0;
      }
      *(short *)(team_spinner + 0x3c) = 0;
    } else {
      length =
        ustrlen((unsigned short *)(game + 0x226 + player_indices[j] * 0x20));
      text = ui_widget_realloc(
        *(int *)(name_text + 0x3c), (unsigned short)(length * 2 + 2),
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x5e4);
      *(void **)(name_text + 0x3c) = text;
      if (text != NULL) {
        ustrncpy((wchar_t *)text,
                 (wchar_t *)(game + 0x226 + player_indices[j] * 0x20), length);
        (*(wchar_t **)(name_text + 0x3c))[length] = 0;
      }
      if (*(char *)(game + 0xc0) == 0) {
        *(short *)(container + 0x50) = 1;
      } else {
        switch (*(char *)(game + 0x244 + player_indices[j] * 0x20)) {
        case 0:
          *(short *)(container + 0x50) = bitmap_table[j * 3 + 1];
          *(short *)(team_spinner + 0x3c) = 0;
          break;
        case 1:
          *(short *)(container + 0x50) = bitmap_table[j * 3 + 2];
          *(short *)(team_spinner + 0x3c) = 1;
          break;
        default:
          *(short *)(container + 0x50) = bitmap_table[j * 3 + 0];
          *(short *)(team_spinner + 0x3c) = 0;
          break;
        }
      }
    }
  }
}

/* netgame_prejoin_players (0xf2390). While the client is in state 2 (joining),
 * builds a 4-entry table of local players that want to play multiplayer,
 * clears the entries for local players already present in the network game
 * player list (16 slots of 0x20 bytes at game+0x226, local-player index byte
 * at +0x243), then sends a join request for each remaining wanted player. */
void netgame_prejoin_players(void)
{
  void *client;
  int game;
  int index;
  char wants_to_play[4];

  client = global_network_game_client_get();
  if (client == NULL) {
    return;
  }
  if (network_game_client_get_state(client, &index) != 2) {
    return;
  }

  game = network_game_get_game();
  if (game == 0) {
    display_assert(
      "game",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x614, 1);
    system_exit(-1);
  }

  index = 0;
  do {
    wants_to_play[(short)index] =
      (char)player_ui_local_player_wants_to_play_multiplayer((short)index);
    index++;
  } while ((short)index < 4);

  index = 0;
  do {
    if (network_player_is_valid(
          (void *)((char *)(uintptr_t)game + (short)index * 0x20 + 0x226)) &&
        network_game_player_is_local(
          (void *)((char *)(uintptr_t)game + (short)index * 0x20 + 0x226))) {
      wants_to_play[(int)*(char *)((char *)(uintptr_t)game +
                                   (short)index * 0x20 + 0x243)] = 0;
    }
    index++;
  } while ((short)index < 0x10);

  index = 0;
  do {
    if (wants_to_play[(short)index] != 0) {
      if (!network_game_client_add_player(client, (uint16_t)index)) {
        network_event("failed to send join request");
      }
    }
    index++;
  } while ((short)index < 4);
}

/* player_profile_edit_select_menu_update_extended_description (0xf24b0)
 * Same shape as playlist_gametype_select_menu_update_extended_description
 * above (updates the extended-description widgets for a "settings select"
 * list widget's currently-highlighted item): resolves widget+0x48's owner
 * definition tag via tag_get('DeLa', tag_index) and asserts it is a 2-child
 * widget definition (tag+0x3e0 == 2). Walks the sibling chain at widget+0x34
 * (via +0x2c "next sibling"), counting the index of the previously-selected
 * child (widget+0x38), then writes the resolved index into both the
 * container (+0x34 off widget+0x48, +0x40) and that container's first child
 * (+0x2c, +0x50). */
void player_profile_edit_select_menu_update_extended_description(void *widget)
{
  int child;
  short index;
  void *widget_def;
  int container;

  if ((widget == 0) || (*(int *)((char *)widget + 0x38) == 0) ||
      (*(int *)((char *)widget + 0x48) == 0)) {
    display_assert(
      "invalid widget trying to update its extended list description",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x836, 1);
    system_exit(-1);
  }

  child = *(int *)((char *)widget + 0x34);
  index = 0;
  if (child != 0) {
    int selected_child = *(int *)((char *)widget + 0x38);
    do {
      if (child == selected_child)
        break;
      child = *(int *)((char *)child + 0x2c);
      index = index + 1;
    } while (child != 0);
    if (index == -1) {
      return;
    }
  }

  widget_def =
    tag_get(0x44654c61 /* 'DeLa' */, *(int *)*(int *)((char *)widget + 0x48));

  if (*(int *)((char *)widget_def + 0x3e0) != 2) {
    display_assert(
      "expected a container widget w/ 2 children for the player profile edit "
      "settings list extended description",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x844, 1);
    system_exit(-1);
  }

  container = *(int *)((char *)*(int *)((char *)widget + 0x48) + 0x34);
  *(short *)((char *)container + 0x40) = index;
  *(short *)((char *)*(int *)((char *)container + 0x2c) + 0x50) = index;
}

/* player-profile three-column list update (0x0f2560). Validates the column
 * list and extended-description text widget, then stores the selected list
 * item's spinner setting into the text widget. */
void game_options_menu_update_text_desc(void *widget)
{
  void *text_widget;
  void *child;
  void *spinner;
  void *widget_definition;
  int spinner_value;
  short list_value;

  if (*(short *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for multiplayer game options list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x8f5, 1);
    system_exit(-1);
  }

  text_widget = *(void **)((char *)widget + 0x48);
  if (text_widget == NULL || *(short *)((char *)text_widget + 0xe) != 1) {
    display_assert(
      "expected a text box for multiplayer game options list extended "
      "description",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x8f8, 1);
    system_exit(-1);
  }

  widget_definition = tag_get(0x44654c61 /* 'DeLa' */, *(int *)widget);
  if (*(int *)((char *)widget_definition + 0x3e0) < 1) {
    display_assert(
      "expected some list items for multiplayer game settings list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x8fe, 1);
    system_exit(-1);
  }

  if (*(void **)((char *)widget + 0x38) == NULL) {
    *(short *)((char *)text_widget + 0x40) = (short)(uintptr_t)widget;
    return;
  }

  child = *(void **)((char *)widget + 0x34);
  list_value = 0;
  if (child != NULL) {
    do {
      spinner = *(void **)((char *)child + 0x34);
      while (spinner != NULL && *(short *)((char *)spinner + 0xe) != 2) {
        spinner = *(void **)((char *)spinner + 0x2c);
      }
      if (spinner == NULL) {
        display_assert(
          "expected a spinner list somewhere in the multiplayer game options "
          "settings list item makeup",
          "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
          0x90c, 1);
        system_exit(-1);
      }

      if (child == *(void **)((char *)widget + 0x38)) {
        list_value = (short)(list_value + *(short *)((char *)spinner + 0x3c));
        break;
      }

      spinner_value = *(unsigned short *)((char *)spinner + 0x44);
      list_value = (short)(list_value + spinner_value);
      child = *(void **)((char *)child + 0x2c);
    } while (child != NULL);
  }

  *(short *)((char *)text_widget + 0x40) = list_value;
}

/* FUN_000f2690 (0xf2690)
 * "objective text" data-driven text box widget update. Fetches the current
 * hud objective string (empty if hud_messaging_get_objective() returns NULL
 * or an empty string), requires the widget to be a text box (type == 1 at
 * +0xe), then, when the text is non-empty, reallocates the widget's text
 * buffer (+0x3c) to fit it and copies it in, null-terminated. */
void FUN_000f2690(void *widget)
{
  wchar_t *objective_text;
  wchar_t *new_buf;
  unsigned int len;

  objective_text = (wchar_t *)hud_messaging_get_objective();
  if (objective_text == NULL || *objective_text == 0) {
    len = 0;
  } else {
    len = (unsigned int)ustrlen((unsigned short *)objective_text);
  }

  if (*(short *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected a text box for objective text",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x924, 1);
    system_exit(-1);
  }

  if (0 < (int)len) {
    new_buf = (wchar_t *)ui_widget_realloc(
      *(int *)((char *)widget + 0x3c), (unsigned short)(len * 2 + 2),
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x928);
    *(wchar_t **)((char *)widget + 0x3c) = new_buf;
    if (new_buf != NULL) {
      ustrncpy(new_buf, objective_text, len);
      *(unsigned short *)((char *)*(wchar_t **)((char *)widget + 0x3c) +
                          len * 2) = 0;
    }
  }
}

/* FUN_000f2720 (0xf2720) — validates a three-column game-options list and
 * its extended-description picture container, then sums each preceding
 * spinner's item count and the selected spinner's selected-item index into
 * the picture widget's +0x50 word. */
void FUN_000f2720(void *widget)
{
  void *picture_widget;
  void *list_item;
  void *spinner;
  void *widget_definition;
  short list_value;

  if (*(short *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for game options list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x980, 1);
    system_exit(-1);
  }

  picture_widget = *(void **)((char *)widget + 0x48);
  if (picture_widget == NULL || *(short *)((char *)picture_widget + 0xe) != 0) {
    display_assert(
      "expected a picture (container) for game options list extended "
      "description",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x983, 1);
    system_exit(-1);
  }

  widget_definition = tag_get(0x44654c61 /* 'DeLa' */, *(int *)widget);
  if (*(int *)((char *)widget_definition + 0x3e0) < 1) {
    display_assert(
      "expected some list items for game settings list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x988, 1);
    system_exit(-1);
  }

  if (*(void **)((char *)widget + 0x38) == NULL) {
    *(short *)((char *)picture_widget + 0x50) = (short)(uintptr_t)widget;
    return;
  }

  list_item = *(void **)((char *)widget + 0x34);
  list_value = 0;
  if (list_item != NULL) {
    while (1) {
      spinner = *(void **)((char *)list_item + 0x34);
      while (spinner != NULL && *(short *)((char *)spinner + 0xe) != 2) {
        spinner = *(void **)((char *)spinner + 0x2c);
      }
      if (spinner == NULL) {
        display_assert(
          "expected a spinner list somewhere in the multiplayer game options "
          "settings list item makeup",
          "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
          0x996, 1);
        system_exit(-1);
      }

      if (list_item == *(void **)((char *)widget + 0x38)) {
        list_value = (short)(list_value + *(short *)((char *)spinner + 0x3c));
        break;
      }

      list_item = *(void **)((char *)list_item + 0x2c);
      list_value =
        (short)(list_value + *(unsigned short *)((char *)spinner + 0x44));
      if (list_item == NULL) {
        *(short *)((char *)picture_widget + 0x50) = list_value;
        return;
      }
    }
  }

  *(short *)((char *)picture_widget + 0x50) = list_value;
}

/* main_menu_animation_fakery (0xf2850) — requires the widget to be a column
 * list (type == 3 at +0xe) and its cached extended-description child at +0x48
 * to be a non-NULL container/picture widget (type == 0 at +0xe), then copies
 * the column list's selected index (+0x3c, signed 16-bit) into the picture
 * widget's +0x50 slot, clamped at zero. The reference re-loads widget+0x48
 * after the store and writes +0x50 on both clamp branches. Evidence:
 * reference disassembly at 0xf2850-0xf28d5 (assert strings/lines are the
 * reference's own PUSH immediates at 0xf2860/0xf2865/0xf286a and
 * 0xf288e/0xf2893/0xf2898). */
void main_menu_animation_fakery(void *widget)
{
  char *widget_bytes;
  void *picture_widget;
  int zero;
  short value;

  if (*(short *)((char *)widget + 0xe) != 3) {
    display_assert(
      "expected column list for main menu options list",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x9ab, 1);
    system_exit(-1);
  }

  picture_widget = *(void **)((char *)widget + 0x48);
  if (picture_widget == (void *)(zero = 0) ||
      *(short *)((char *)picture_widget + 0xe) != zero) {
    display_assert(
      "expected a picture (container) for main menu options list extended "
      "description",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x9ae, 1);
    system_exit(-1);
  }

  picture_widget = *(void **)((char *)widget + 0x48);
  widget_bytes = (char *)widget;
  *(short *)((char *)picture_widget + 0x50) = *(short *)(widget_bytes + 0x3c);

  picture_widget = *(void **)(widget_bytes + 0x48);
  value = *(short *)((char *)picture_widget + 0x50);
  if (value < zero) {
    *(short *)((char *)picture_widget + 0x50) = (short)zero;
  } else {
    *(short *)((char *)(*(void **)(widget_bytes + 0x48)) + 0x50) = value;
  }
}

/* get_active_player_profile_display_name (0xf28e0)
 * "profile display name" data-driven text box widget update. Requires the
 * widget to be a text box (type == 1 at +0xe) and its bound local player
 * index (+0x8, a signed 16-bit slot) to be in range
 * [0, MAXIMUM_NUMBER_OF_LOCAL_PLAYERS), fetches that player's active profile
 * (a 0x30-byte opaque record, see player_ui_get_active_player_profile),
 * reallocates the widget's text buffer (+0x3c) to a fixed 0x18 bytes, and
 * copies the first 11 wchar_t of the profile record (its name field) in,
 * null-terminated at wchar index 11 (byte offset 0x16). Evidence: reference
 * disassembly at 0xf28e0-0xf298c (assert strings/lines are the reference's
 * own PUSH immediates at 0xf2911/0xf2917/0xf2940/0xf2946). */
void get_active_player_profile_display_name(void *widget)
{
  unsigned char profile[0x30];
  wchar_t *new_buf;
  short local_player_index;

  if (*(short *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected a text box widget for profile display name",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x9ec, 1);
    system_exit(-1);
  }

  local_player_index = *(short *)((char *)widget + 8);
  if (local_player_index < 0 || local_player_index >= 4) {
    display_assert(
      "profile display name requires a valid local player index",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x9ee, 1);
    system_exit(-1);
  }

  player_ui_get_active_player_profile(local_player_index, profile);

  new_buf = (wchar_t *)ui_widget_realloc(
    *(int *)((char *)widget + 0x3c), 0x18,
    "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
    0x9f0);
  *(wchar_t **)((char *)widget + 0x3c) = new_buf;
  if (new_buf != NULL) {
    ustrncpy(new_buf, (wchar_t *)profile, 0xb);
    *(unsigned short *)((char *)new_buf + 0x16) = 0;
  }
}

/* get_editable_player_profile_display_name (0xf2990)
 * Sibling of get_active_player_profile_display_name, for the profile
 * currently being edited rather than the active one. Same widget-type
 * (+0xe == 1) and local-player-index (+0x8, [0,4)) guards. Unlike the
 * active-profile version, player_ui_get_edit_player_profile (FUN_000e0ea0)
 * takes no arguments and returns a pointer directly into the edit-profile
 * record (no local 0x30-byte copy) — a NULL return (no profile being
 * edited) is a normal, non-asserting no-op, matching the reference's
 * `if (extraout_EAX != NULL) { ... } return;` shape. Evidence: reference
 * disassembly at 0xf2990-0xf2a30 (assert strings/lines are the reference's
 * own PUSH immediates at 0xf29a1/0xf29a6/0xf29d0/0xf29da; realloc call site
 * line immediate at 0xf29fc). */
void get_editable_player_profile_display_name(void *widget)
{
  wchar_t *profile;
  wchar_t *new_buf;
  short local_player_index;

  if (*(short *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected a text box widget for profile display name",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x9fe, 1);
    system_exit(-1);
  }

  local_player_index = *(short *)((char *)widget + 8);
  if (local_player_index < 0 || local_player_index >= 4) {
    display_assert(
      "profile display name requires a valid local player index",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xa00, 1);
    system_exit(-1);
  }

  profile = (wchar_t *)player_ui_get_edit_player_profile();
  if (profile != NULL) {
    new_buf = (wchar_t *)ui_widget_realloc(
      *(int *)((char *)widget + 0x3c), 0x18,
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xa04);
    *(wchar_t **)((char *)widget + 0x3c) = new_buf;
    if (new_buf != NULL) {
      ustrncpy(new_buf, profile, 0xb);
      *(unsigned short *)((char *)new_buf + 0x16) = 0;
    }
  }
}

/* get_editable_playlist_profile_display_name (0xf2a40)
 * Playlist (game variant) sibling of get_editable_player_profile_display_name.
 * Same widget-type (+0xe == 1) and local-player-index (+0x8, [0,4)) guards.
 * A NULL player_ui_get_edit_playlist_profile (FUN_000e0ec0) return reports
 * error(2, "not currently editing a game variant") instead of silently
 * returning. The final terminator store re-reads widget+0x3c (reference
 * MOV ECX,[ESI+0x3c] at 0xf2ad1). Evidence: reference disassembly at
 * 0xf2a40-0xf2af3 (assert line immediates 0xa13/0xa15 at 0xf2a51/0xf2a80,
 * realloc line 0xa19 at 0xf2aac, error string push at 0xf2ae1). */
void get_editable_playlist_profile_display_name(void *widget)
{
  wchar_t *profile;
  wchar_t *new_buf;
  short local_player_index;

  if (*(short *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected a text box widget for profile display name",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xa13, 1);
    system_exit(-1);
  }

  local_player_index = *(short *)((char *)widget + 8);
  if (local_player_index < 0 || local_player_index >= 4) {
    display_assert(
      "profile display name requires a valid local player index",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xa15, 1);
    system_exit(-1);
  }

  profile = (wchar_t *)player_ui_get_edit_playlist_profile();
  if (profile != NULL) {
    new_buf = (wchar_t *)ui_widget_realloc(
      *(int *)((char *)widget + 0x3c), 0x18,
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xa19);
    *(wchar_t **)((char *)widget + 0x3c) = new_buf;
    if (new_buf != NULL) {
      ustrncpy(new_buf, profile, 0xb);
      *(unsigned short *)(*(char **)((char *)widget + 0x3c) + 0x16) = 0;
    }
  } else {
    error(2, "not currently editing a game variant");
  }
}

/* get_active_player_profile_color_index (0xf2b00) — "profile color picture"
 * data-driven widget update. Requires the widget's bound local player index
 * (+0x8, signed 16-bit) to be in [0, MAXIMUM_NUMBER_OF_LOCAL_PLAYERS), fetches
 * that player's active profile (the same 0x30-byte opaque record as
 * player_ui_get_active_player_profile), reads its color index at +0x18, and
 * stores it into the widget's +0x50 value slot clamped to
 * [0, FUN_001c0ed0() - 1]. A negative profile color index stores 0. The
 * reference calls FUN_001c0ed0 twice on the over-range path (0xf2b64 and
 * 0xf2b71) — both calls are preserved. Evidence: reference disassembly at
 * 0xf2b00-0xf2b8d (assert string/line are the reference's own PUSH immediates
 * at 0xf2b1b/0xf2b20/0xf2b25). */
void get_active_player_profile_color_index(void *widget)
{
  unsigned char profile[0x30];
  short local_player_index;
  short color_index;

  local_player_index = *(short *)((char *)widget + 8);
  if (local_player_index < 0 || local_player_index >= 4) {
    display_assert(
      "profile color picture requires a valid local player index",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xa2d, 1);
    system_exit(-1);
  }

  player_ui_get_active_player_profile(local_player_index, profile);

  color_index = *(short *)(profile + 0x18);
  if (color_index < 0) {
    *(short *)((char *)widget + 0x50) = 0;
    return;
  }
  if ((int)color_index > (int)FUN_001c0ed0() - 1) {
    *(short *)((char *)widget + 0x50) = (short)(FUN_001c0ed0() - 1);
    return;
  }
  *(short *)((char *)widget + 0x50) = color_index;
}

/* multiplayer_game_set_text_box_for_map_name (0xf2b90) — maps the active
 * multiplayer map name to its legacy game-settings text index. */
void multiplayer_game_set_text_box_for_map_name(void *widget)
{
  char *map_name;

  if (*(short *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected text box widget for mp game settings text",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xa39, 1);
    system_exit(-1);
  }

  map_name = (char *)network_game_get_game();
  if (map_name == NULL) {
    error(2, "no network game");
    return;
  }
  map_name = map_name + 0x24;

  if (crt_strstr(map_name, "beavercreek") != NULL) {
    *(unsigned short *)((char *)widget + 0x40) = 0;
    return;
  }
  if (crt_strstr(map_name, "sidewinder") != NULL) {
    *(unsigned short *)((char *)widget + 0x40) = 1;
    return;
  }
  if (crt_strstr(map_name, "damnation") != NULL) {
    *(unsigned short *)((char *)widget + 0x40) = 2;
    return;
  }
  if (crt_strstr(map_name, "ratrace") != NULL) {
    *(unsigned short *)((char *)widget + 0x40) = 3;
    return;
  }
  if (crt_strstr(map_name, "prisoner") != NULL) {
    *(unsigned short *)((char *)widget + 0x40) = 4;
    return;
  }
  if (crt_strstr(map_name, "hangemhigh") != NULL) {
    *(unsigned short *)((char *)widget + 0x40) = 5;
    return;
  }
  if (crt_strstr(map_name, "chillout") != NULL) {
    *(unsigned short *)((char *)widget + 0x40) = 6;
    return;
  }
  if (crt_strstr(map_name, "carousel") != NULL) {
    *(unsigned short *)((char *)widget + 0x40) = 7;
    return;
  }
  if (crt_strstr(map_name, "boardingaction") != NULL) {
    *(unsigned short *)((char *)widget + 0x40) = 8;
    return;
  }
  if (crt_strstr(map_name, "bloodgulch") != NULL) {
    *(unsigned short *)((char *)widget + 0x40) = 9;
    return;
  }
  if (crt_strstr(map_name, "wizard") != NULL) {
    *(unsigned short *)((char *)widget + 0x40) = 10;
    return;
  }
  if (crt_strstr(map_name, "putput") != NULL) {
    *(unsigned short *)((char *)widget + 0x40) = 0xb;
    return;
  }

  *(unsigned short *)((char *)widget + 0x40) =
    (unsigned short)(0xd - (crt_strstr(map_name, "longest") != NULL));
}

/* multiplayer_game_set_text_box_for_teams_noteams (0xf2e60)
 * "mp game settings text" data-driven text box widget update. Requires the
 * widget to be a text box (type == 1 at +0xe); otherwise asserts + exits
 * (reference PUSH immediates at 0xf2e6e/0xf2e70/0xf2e75/0xf2e7a). Fetches
 * the active network game object (network_game_get_game); if one exists, writes
 * a 2-state code to the widget's +0x40 word — 0xc when the game object's byte
 * at +0xc0 equals 1, else 0xd. If there is no active network game, reports
 * error(2, "no network game") instead (reference PUSH immediates at
 * 0xf2eaf/0xf2eb4). Evidence: reference disassembly at 0xf2e60-0xf2ec1. */
void multiplayer_game_set_text_box_for_teams_noteams(void *widget)
{
  int game;
  unsigned char state_byte;

  if (*(short *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected text box widget for mp game settings text",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xaaa, 1);
    system_exit(-1);
  }

  game = network_game_get_game();
  if (game != 0) {
    state_byte = *(unsigned char *)(game + 0xc0);
    *(unsigned short *)((char *)widget + 0x40) =
      (unsigned short)((state_byte != 1) + 0xc);
  } else {
    error(2, "no network game");
  }
}

/* multiplayer_game_set_text_box_for_score_limit (0xf2ed0)
 * "mp game settings text" numeric text box widget update for the network
 * game's score limit. Requires the widget to be a text box (type == 1 at
 * +0xe); otherwise asserts + exits (reference PUSH immediates at
 * 0xf2edf/0xf2ee1/0xf2ee6/0xf2eeb). Looks up the current network game via
 * network_game_get_game(); if none is active, reports error 2 "no network
 * game" (reference PUSH immediates at 0xf2f4d/0xf2f52) and leaves the text
 * buffer untouched. Otherwise reallocates the widget's text buffer (+0x3c)
 * to 0x10 bytes (8 wchar_t) and formats the game object's dword at +0xe4
 * into it with "%d" (format string at 0x26c118 = L"%d"), then explicitly
 * null-terminates at wchar index 7 (byte offset 0xe) by re-reading the
 * widget's +0x3c pointer. Evidence: reference disassembly at
 * 0xf2ed0-0xf2f5f (ui_widget_realloc call at 0xf2f0a-0xf2f1f;
 * unicode_sprintf call at 0xf2f29-0xf2f40; terminator store at
 * 0xf2f3d/0xf2f44). */
void multiplayer_game_set_text_box_for_score_limit(void *widget)
{
  int game;
  wchar_t *new_buf;

  if (*(short *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected text box widget for mp game settings text",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xac3, 1);
    system_exit(-1);
  }

  game = network_game_get_game();
  if (game != 0) {
    new_buf = (wchar_t *)ui_widget_realloc(
      *(int *)((char *)widget + 0x3c), 0x10,
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xac8);
    *(wchar_t **)((char *)widget + 0x3c) = new_buf;
    if (new_buf != NULL) {
      unicode_sprintf(new_buf, 7, L"%d", *(int *)(game + 0xe4));
      *(unsigned short *)((char *)*(wchar_t **)((char *)widget + 0x3c) + 0xe) =
        0;
    }
  } else {
    error(2, "no network game");
  }
}

/* multiplayer_game_set_text_box_for_score_limit_type (0xf2f60)
 * "mp game settings text" data-driven text box widget update (game-type
 * variant). Requires the widget to be a text box (type == 1 at +0xe);
 * otherwise asserts + exits (reference PUSH immediates at
 * 0xf2f6e/0xf2f70/0xf2f75/0xf2f7a). Fetches the active network game object
 * (network_game_get_game); if one exists, dispatches on the game object's dword
 * field at +0xbc (jump table at 0xf2ff8, values 1-5) to write the widget's
 * +0x40 word: 1                                -> 0x16 2, or any value
 * outside 1..5 (the out-of-range default falls into the same code as case 2 --
 * reference 0xf2fa1 JA 0xf2fb3)  -> 0x18 3     -> 0x18 if the game object's
 * dword at +0x100 == 2, else 0x17 4     -> 0x17 5     -> 0x19 If there is no
 * active network game, reports error(2, "no network game") instead (reference
 * PUSH immediates at 0xf2fe6/0xf2feb). Evidence: reference disassembly at
 * 0xf2f60-0xf2ff8 plus jump table dwords at 0xf2ff8-0xf300c
 * (0xf2faa/0xf2fb3/0xf2fbc/0xf2fd4/0xf2fdd). */
void multiplayer_game_set_text_box_for_score_limit_type(void *widget)
{
  int game;

  if (*(short *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected text box widget for mp game settings text",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xadc, 1);
    system_exit(-1);
  }

  game = network_game_get_game();
  if (game != 0) {
    switch (*(int *)(game + 0xbc)) {
    case 1:
      *(unsigned short *)((char *)widget + 0x40) = 0x16;
      break;
    case 3:
      *(unsigned short *)((char *)widget + 0x40) =
        (*(int *)(game + 0x100) == 2) ? 0x18 : 0x17;
      break;
    case 4:
      *(unsigned short *)((char *)widget + 0x40) = 0x17;
      break;
    case 5:
      *(unsigned short *)((char *)widget + 0x40) = 0x19;
      break;
    case 2:
    default:
      *(unsigned short *)((char *)widget + 0x40) = 0x18;
      break;
    }
  } else {
    error(2, "no network game");
  }
}

/* solo_game_objective_text (0xf3010, table xref 0x31e580)
 * Selects the multiplayer map index for the "mp game settings bitmap"
 * container widget. Requires the widget to be a container (type == 0 at
 * +0xe); otherwise asserts + exits (reference PUSH immediates at
 * 0xf301f/0xf3021/0xf3026/0xf302b). Fetches the active network game via
 * network_game_get_game(); with no active game it reports error(2, "no
 * network game") and leaves the widget untouched (reference 0xf31bc).
 * Otherwise it substring-matches the game's map name (game + 0x24) against
 * the stock multiplayer map scenario names in a fixed order and stores the
 * matching index into the widget's word at +0x50. The final test is emitted
 * as NEG/SBB/ADD 0xd (0xf31ad-0xf31b4): 12 when "longest" matches, 13
 * otherwise. Evidence: reference disassembly at 0xf3010-0xf31ce. */
void solo_game_objective_text(void *widget)
{
  int game;
  const char *map_name;
  char *found;

  if (*(short *)((char *)widget + 0xe) != 0) {
    display_assert(
      "expected container widget for mp game settings bitmap",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xb09, 1);
    system_exit(-1);
  }

  game = network_game_get_game();
  if (game != 0) {
    map_name = (const char *)(game + 0x24);

    found = crt_strstr(map_name, "beavercreek");
    if (found != NULL) {
      *(unsigned short *)((char *)widget + 0x50) = 0;
      return;
    }
    found = crt_strstr(map_name, "sidewinder");
    if (found != NULL) {
      *(unsigned short *)((char *)widget + 0x50) = 1;
      return;
    }
    found = crt_strstr(map_name, "damnation");
    if (found != NULL) {
      *(unsigned short *)((char *)widget + 0x50) = 2;
      return;
    }
    found = crt_strstr(map_name, "ratrace");
    if (found != NULL) {
      *(unsigned short *)((char *)widget + 0x50) = 3;
      return;
    }
    found = crt_strstr(map_name, "prisoner");
    if (found != NULL) {
      *(unsigned short *)((char *)widget + 0x50) = 4;
      return;
    }
    found = crt_strstr(map_name, "hangemhigh");
    if (found != NULL) {
      *(unsigned short *)((char *)widget + 0x50) = 5;
      return;
    }
    found = crt_strstr(map_name, "chillout");
    if (found != NULL) {
      *(unsigned short *)((char *)widget + 0x50) = 6;
      return;
    }
    found = crt_strstr(map_name, "carousel");
    if (found != NULL) {
      *(unsigned short *)((char *)widget + 0x50) = 7;
      return;
    }
    found = crt_strstr(map_name, "boardingaction");
    if (found != NULL) {
      *(unsigned short *)((char *)widget + 0x50) = 8;
      return;
    }
    found = crt_strstr(map_name, "bloodgulch");
    if (found != NULL) {
      *(unsigned short *)((char *)widget + 0x50) = 9;
      return;
    }
    found = crt_strstr(map_name, "wizard");
    if (found != NULL) {
      *(unsigned short *)((char *)widget + 0x50) = 10;
      return;
    }
    found = crt_strstr(map_name, "putput");
    if (found != NULL) {
      *(unsigned short *)((char *)widget + 0x50) = 11;
      return;
    }
    found = crt_strstr(map_name, "longest");
    *(unsigned short *)((char *)widget + 0x50) =
      (unsigned short)(0xd - (found != NULL));
  } else {
    error(2, "no network game");
  }
}

/* multiplayer_game_set_bitmap_for_ruleset (0xf31d0). The widget's type
 * must be zero; the active network game's ruleset at +0xbc maps to the
 * bitmap/string index at +0x50. Unknown ruleset values select index 5. */
void multiplayer_game_set_bitmap_for_ruleset(void *widget)
{
  int game;

  if (*(short *)((char *)widget + 0xe) != 0) {
    display_assert(
      "expected container widget for mp game settings bitmap",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xb28, 1);
    system_exit(-1);
  }

  game = network_game_get_game();
  if (game == 0) {
    error(2, "no network game");
    return;
  }

  switch (*(int *)((char *)game + 0xbc)) {
  case 1:
    *(unsigned short *)((char *)widget + 0x50) = 0;
    return;
  case 2:
    *(unsigned short *)((char *)widget + 0x50) = 2;
    return;
  case 3:
    *(unsigned short *)((char *)widget + 0x50) = 3;
    return;
  case 4:
    *(unsigned short *)((char *)widget + 0x50) = 1;
    return;
  case 5:
    *(unsigned short *)((char *)widget + 0x50) = 4;
    return;
  default:
    *(unsigned short *)((char *)widget + 0x50) = 5;
    return;
  }
}

/* multiplayer_game_set_text_box_for_number_of_players (0xf3280\)
 * "mp game settings text" numeric text box widget update. Requires the
 * widget to be a text box (type == 1 at +0xe). Looks up the current network
 * game via network_game_get_game(); if none is active, reports error 2 "no
 * network game" and leaves the widget's text buffer untouched. Otherwise
 * reallocates the widget's text buffer (+0x3c) to 8 bytes (4 wchar_t) and
 * formats a signed 16-bit game field at game+0x224 into it with "%d",
 * explicitly null-terminating at wchar index 3 (byte offset 6) regardless of
 * how many digits were written. Evidence: disassembly at 0xf3280-0xf3311
 * (assert string/line are the reference's own PUSH immediates at
 * 0xf3291/0xf3296/0xf329b; ui_widget_realloc call at 0xf32bd-0xf32ca; error
 * call at 0xf32fe-0xf3305). */
void multiplayer_game_set_text_box_for_number_of_players(void *widget)
{
  int game;
  wchar_t *new_buf;

  if (*(short *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected text box widget for mp game settings text",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xb4e, 1);
    system_exit(-1);
  }

  game = network_game_get_game();
  if (game != 0) {
    new_buf = (wchar_t *)ui_widget_realloc(
      *(int *)((char *)widget + 0x3c), 8,
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xb56);
    *(wchar_t **)((char *)widget + 0x3c) = new_buf;
    if (new_buf != NULL) {
      unicode_sprintf(new_buf, 3, L"%d", *(short *)(game + 0x224));
      *(unsigned short *)((char *)*(wchar_t **)((char *)widget + 0x3c) + 6) = 0;
    }
  } else {
    error(2, "no network game");
  }
}

/* multiplayer_edit_profile_set_ruleset_textbox_string_index (0xf3320)
 * "mp profile edit ruleset text" text box widget update. Requires the widget
 * to be a text box (type == 1 at +0xe); otherwise asserts + exits (assert
 * string/file/line are the reference's own PUSH immediates at
 * 0xf3330/0xf3335/0xf333a). Fetches the playlist profile currently being
 * edited (player_ui_get_edit_playlist_profile at 0xf334e); if none is being
 * edited, reports error 2 "not currently editing a game variant" and leaves
 * the widget untouched. Otherwise maps the profile's ruleset field at +0x18
 * (values 1..5, via the DEC/CMP 4/JA jump table at 0xf3357-0xf3360) onto the
 * widget's +0x40 string index 3..7, with 8 for any other value. */
void multiplayer_edit_profile_set_ruleset_textbox_string_index(void *widget)
{
  int profile;

  if (*(short *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected a text box widget for mp profile edit ruleset text widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xb69, 1);
    system_exit(-1);
  }

  profile = (int)player_ui_get_edit_playlist_profile();
  if (profile != 0) {
    switch (*(int *)(profile + 0x18)) {
    case 1:
      *(unsigned short *)((char *)widget + 0x40) = 3;
      return;
    case 2:
      *(unsigned short *)((char *)widget + 0x40) = 4;
      return;
    case 3:
      *(unsigned short *)((char *)widget + 0x40) = 5;
      return;
    case 4:
      *(unsigned short *)((char *)widget + 0x40) = 6;
      return;
    case 5:
      *(unsigned short *)((char *)widget + 0x40) = 7;
      return;
    default:
      *(unsigned short *)((char *)widget + 0x40) = 8;
      return;
    }
  } else {
    error(2, "not currently editing a game variant");
  }
}

/* game_options_menu_update_pic_desc (0xf3400). Text box widget update for the
 * "team game directions" pic/description box. Requires the widget to be a text
 * box (+0xe == 1); otherwise asserts + exits (assert string/file/line are the
 * reference's own PUSH immediates at 0xf3426/0xf342b/0xf3421).
 *
 * Reads the current network game (network_game_get_game, 0xf3407) and the
 * server object (network_game_server_get, 0xf340e). When a server exists:
 * non-splitscreen games with game+0x112 (signed 16-bit) < 2 select string
 * index 0x22; splitscreen games with game+0x224 < 2 select 0x23; a splitscreen
 * game with no game object clears the box. Both selecting paths also set the
 * widget's +0x10 visibility byte to 1.
 *
 * Otherwise, if the game's +0xc0 flag is 1 and
 * network_game_client_get_seconds_to_game_start on the current client returns a
 * negative int16 (TEST AX,AX / JGE at 0xf34bd), walks the 16 player slots at
 * game+0x244 with stride 0x20, counting slots whose player record (slot - 0x1e)
 * is valid and whose byte at the slot base is 0 or 1. Both counts non-zero
 * selects string index 0x1a, otherwise 0x1b; either way +0x10 becomes 1. Every
 * other path clears +0x10 to 0. Evidence: reference disassembly at
 * 0xf3400-0xf353a. */
void game_options_menu_update_pic_desc(void *widget)
{
  int game;
  void *server;
  char *slot;
  int count_free;
  int count_taken;
  int remaining;

  game = network_game_get_game();
  server = global_network_game_server_get();
  if (*(short *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected text box widget for team game directions",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xbad, 1);
    system_exit(-1);
  }

  if (server != NULL) {
    if (!network_game_is_splitscreen_local() && game != 0 &&
        *(short *)(game + 0x112) < 2) {
      *(unsigned short *)((char *)widget + 0x40) = 0x22;
      *(unsigned char *)((char *)widget + 0x10) = 1;
      return;
    }
    if (network_game_is_splitscreen_local()) {
      if (game == 0) {
        *(unsigned char *)((char *)widget + 0x10) = 0;
        return;
      }
      if (*(short *)(game + 0x224) < 2) {
        *(unsigned short *)((char *)widget + 0x40) = 0x23;
        *(unsigned char *)((char *)widget + 0x10) = 1;
        return;
      }
    }
  }

  if (game != 0 && *(char *)(game + 0xc0) == 1 &&
      network_game_client_get_seconds_to_game_start(
        global_network_game_client_get()) < 0) {
    count_free = 0;
    count_taken = 0;
    slot = (char *)(game + 0x244);
    remaining = 0x10;
    do {
      if (network_player_is_valid(slot - 0x1e)) {
        if (*slot == 0) {
          count_free++;
        } else if (*slot == 1) {
          count_taken++;
        }
      }
      slot += 0x20;
      remaining--;
    } while (remaining != 0);

    if (count_free != 0 && count_taken != 0) {
      *(unsigned short *)((char *)widget + 0x40) = 0x1a;
      *(unsigned char *)((char *)widget + 0x10) = 1;
      return;
    }
    *(unsigned short *)((char *)widget + 0x40) = 0x1b;
    *(unsigned char *)((char *)widget + 0x10) = 1;
    return;
  }

  *(unsigned char *)((char *)widget + 0x10) = 0;
}

/* teams_no_teams_mp_game_bitmap_update (0xf3540, ui_widget_game_data_
 * function_table). Bitmap widget update for the "teams / no teams"
 * pregame header art. Requires the widget to be a container bitmap
 * (+0xe == 0); otherwise asserts and exits (assert string/file/line are
 * the reference's own PUSH immediates at 0xf3562/0xf355d/0xf3558).
 *
 * Reads the current network game (network_game_get_game, 0xf3545). When a
 * game exists, the widget's +0x50 word is set to 1 unless the game's
 * +0xc0 flag equals 1 (i.e. teams-off maps to 1, teams-on maps to 0),
 * else left untouched when there is no game. Evidence: reference
 * disassembly at 0xf3540-0xf358f. */
void teams_no_teams_mp_game_bitmap_update(void *widget)
{
  int game;

  game = network_game_get_game();
  if (*(short *)((char *)widget + 0xe) != 0) {
    display_assert(
      "expected a container bitmap for mp pregame header widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xbfc, 1);
    system_exit(-1);
  }

  if (game != 0) {
    *(unsigned short *)((char *)widget + 0x50) =
      (unsigned short)(*(char *)(game + 0xc0) != 1);
  }
}

/* warn_if_difficulty_will_nuke_saved_game (0xf3590, ui_widget_game_data_
 * function_table[39]). The name is kb.json's pre-existing placeholder and
 * does not match the observed behavior: the assert strings and the globals
 * this touches (DAT_0046ce3b/0046cd38/0046ce38) are the exact same
 * "difficulty forced for this map" state that
 * difficulty_menu_initialize above reads, so this is a
 * paired difficulty-warning visibility updater, not a profile-name getter.
 * No source-derived name is available; kept as-is to avoid an unrequested
 * kb.json/table rename.
 *
 * Asserts widget+0x34 is a column list (+0xe == 3, "the difficulty list
 * widget"), then that list's sibling at +0x2c is a text box (+0xe == 1,
 * "expected warning text box"). If the "difficulty forced for this map"
 * flag (DAT_0046ce3b) is set and the current map (main_get_map_name())
 * case-insensitively matches the forced-map name (DAT_0046cd38), writes 1
 * into the warning textbox's +0x10 byte when the list's currently selected
 * index (+0x3c) differs from the forced difficulty index (DAT_0046ce38),
 * else 0. If the flag isn't set or the map doesn't match, unconditionally
 * clears +0x10 to 0. Evidence: reference disassembly at 0xf3590-0xf3630
 * (assert immediates at 0xf35a8/0xf35ad/0xf35b2/0xf35b7 and
 * 0xf35d6/0xf35db/0xf35e0/0xf35e5; crt_stricmp call at 0xf3608). */
void warn_if_difficulty_will_nuke_saved_game(void *widget)
{
  void *list_widget;
  void *warning_widget;
  const char *map_name;

  list_widget = *(void **)((char *)widget + 0x34);
  if (list_widget == NULL || *(short *)((char *)list_widget + 0xe) != 3) {
    display_assert(
      "this doesn't look like the difficulty list widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xc1f, 1);
    system_exit(-1);
  }

  warning_widget = *(void **)((char *)list_widget + 0x2c);
  if (warning_widget == NULL || *(short *)((char *)warning_widget + 0xe) != 1) {
    display_assert(
      "this doesn't look like the difficulty list widget (expected warning "
      "text box)",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xc23, 1);
    system_exit(-1);
  }

  if (*(unsigned char *)0x46ce3b == 1) {
    map_name = main_get_map_name();
    if (crt_stricmp((const char *)0x46cd38, map_name) == 0) {
      *(unsigned char *)((char *)warning_widget + 0x10) =
        (unsigned char)(*(short *)((char *)list_widget + 0x3c) !=
                        *(short *)0x46ce38);
      return;
    }
  }
  *(unsigned char *)((char *)warning_widget + 0x10) = 0;
}

/* dim_if_no_system_link_cable (0xf3640, ui_widget_game_data_
 * function_table). Like the sibling at 0xf3590, kb.json's name is a
 * pre-existing placeholder that does not match the observed behavior: there
 * is no profile lookup or name copy here at all. Kept as-is to avoid an
 * unrequested rename.
 *
 * Asserts the widget is a text box (+0xe == 1); otherwise halts (reference
 * PUSH immediates at 0xf364e/0xf3650/0xf3655/0xf365a: display_assert(
 * "expected a text box widget for system link menu text item", ..., 0xc3c,
 * 1); system_exit(-1)). Then unconditionally writes the widget's +0x24 float
 * field: 1.0f if transport_network_available(), else 0.333f (0x3eaa7efa) --
 * meaning of +0x24 is unconfirmed (no other function in this TU touches it).
 * Evidence: reference disassembly at 0xf3640-0xf368b. */
void dim_if_no_system_link_cable(void *widget)
{
  if (*(short *)((char *)widget + 0xe) != 1) {
    display_assert(
      "expected a text box widget for system link menu text item",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0xc3c, 1);
    system_exit(-1);
  }

  if (transport_network_available()) {
    *(float *)((char *)widget + 0x24) = 1.0f;
  } else {
    *(float *)((char *)widget + 0x24) = 0.333f;
  }
}

/* player_profile_update_cache_for_nwide_list (0xf3740,
 * ui_widget_game_data_input_functions.obj). Ensures the global profile record
 * table at 0x5aa3c0 (3 records, stride 0x34 -- see the layout comment on
 * player_profile_1wide_list_update below) has a cached record for every
 * distinct profile id present in ids[0..count). Evidence: reference
 * disassembly 0xf3740-0xf384b.
 *
 * Pass 1 (0xf3746-0xf3784): for each of the 3 table records whose id is live
 * (!= -1), linear-scan ids[] for a match and mark found[record_index] = 1 on
 * the first hit (found is a 3-byte local, zeroed up front).
 *
 * Pass 2 (0xf3786-0xf3845): for each requested id (skipping -1 placeholders)
 * already present in the table, do nothing further. For one NOT present,
 * scan for the first record index k with found[k] == 0. The bound check on
 * that scan ("if (k > 2) assert(...)") tests k BEFORE the increment, so it
 * never fires when all 3 records are occupied -- the scan instead falls
 * through with k == 3 and the call below addresses one record past the
 * table. This is the reference's own behavior (0xf37c4-0xf37f6), reproduced
 * as-is. On a free (or out-of-bounds) k, call player_profile_new(id,
 * &table[k].name) to fill in the record's name; on success store the id into
 * table[k].id and mark found[k] = 1 (skipped when k == 3, matching the
 * reference, which never touches found[3]); on failure log
 * error(2, "failed to cache player profile"). */
void player_profile_update_cache_for_nwide_list(int *ids, int count)
{
  /* found[3] is padding: the reference's k==3 fallthrough (see above) writes
   * one byte past its 3-entry array into an adjacent unused stack byte
   * (EBP-1). Sized to 4 so that write stays in-bounds C; it is never read. */
  char found[4];
  int *entry;
  int entry_index;
  int i;
  int j;
  int k;
  int *slot;

  found[0] = 0;
  found[1] = 0;
  found[2] = 0;

  entry = (int *)0x5aa3c0;
  entry_index = 0;
  do {
    if (*entry != -1) {
      for (i = 0; i < count; i++) {
        if (*entry == ids[i]) {
          found[entry_index] = 1;
          break;
        }
      }
    }
    entry = entry + 0xd; /* stride 0x34 bytes */
    entry_index = entry_index + 1;
  } while ((int)entry < 0x5aa45c);

  for (i = 0; i < count; i++) {
    if (ids[i] != -1) {
      j = 0;
      entry = (int *)0x5aa3c0;
      while (1) {
        if (*entry == ids[i]) {
          break;
        }
        entry = entry + 0xd; /* stride 0x34 bytes */
        j = j + 1;
        if ((int)entry >= 0x5aa45c) {
          break;
        }
      }
      if (j == 3) {
        k = 0;
        do {
          if (found[k] != 1) {
            break;
          }
          if (k > 2) {
            display_assert("not enough cache profiles",
                           "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_"
                           "input_functions.c",
                           0xca2, 1);
            system_exit(-1);
          }
          k = k + 1;
        } while (k < 3);

        slot = (int *)(k * 0x34 + 0x5aa3c0);
        if (player_profile_new(ids[i], (wchar_t *)((char *)slot + 4))) {
          *slot = ids[i];
          found[k] = 1;
        } else {
          error(2, "failed to cache player profile");
        }
      }
    }
  }
}

/* variant_profile_update_cache_for_nwide_list (0xf3850)
 * Same shape as player_profile_update_cache_for_nwide_list above, but for the
 * game-variant profile scratch block at DAT_005aa260 (3 entries, stride
 * 0x6c: int id followed by an embedded game_variant_t). Evidence: disasm
 * base 0x5aa260 / end 0x5aa3a4 / stride 0x6c (matches ui_widget.c's
 * csmemset((void *)0x5aa260, -1, 0x144) comment "profile scratch block"),
 * callee playlist_profile_delete(id, variant*) at 0x1c26f0 with args
 * (EAX=id, ECX=&slot+4) traced from the two PUSHes before the CALL, assert
 * line 0xcd5 / error string "failed to cache playlist profile" read
 * directly off the pushed literals. */
void variant_profile_update_cache_for_nwide_list(int *ids, int count)
{
  /* found[3] is padding: the reference's k==3 fallthrough (see below) writes
   * one byte past its 3-entry array into an adjacent unused stack byte
   * (EBP-1). Sized to 4 so that write stays in-bounds C; it is never read. */
  char found[4];
  int *entry;
  int entry_index;
  int i;
  int j;
  int k;
  int *slot;

  found[0] = 0;
  found[1] = 0;
  found[2] = 0;

  entry = (int *)0x5aa260;
  entry_index = 0;
  do {
    if (*entry != -1) {
      for (i = 0; i < count; i++) {
        if (*entry == ids[i]) {
          found[entry_index] = 1;
          break;
        }
      }
    }
    entry = entry + 0x1b; /* stride 0x6c bytes */
    entry_index = entry_index + 1;
  } while ((int)entry < 0x5aa3a4);

  for (i = 0; i < count; i++) {
    if (ids[i] != -1) {
      j = 0;
      entry = (int *)0x5aa260;
      while (1) {
        if (*entry == ids[i]) {
          break;
        }
        entry = entry + 0x1b; /* stride 0x6c bytes */
        j = j + 1;
        if ((int)entry >= 0x5aa3a4) {
          break;
        }
      }
      if (j == 3) {
        k = 0;
        do {
          if (found[k] != 1) {
            break;
          }
          if (k > 2) {
            display_assert("not enough cache profiles",
                           "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_"
                           "input_functions.c",
                           0xcd5, 1);
            system_exit(-1);
          }
          k = k + 1;
        } while (k < 3);

        slot = (int *)(k * 0x6c + 0x5aa260);
        if (playlist_profile_delete(ids[i],
                                    (game_variant_t *)((char *)slot + 4))) {
          *slot = ids[i];
          found[k] = 1;
        } else {
          error(2, "failed to cache playlist profile");
        }
      }
    }
  }
}

/* list_indices_sort_proc (0xf3960). qsort-style comparator over int
 * entries: -1 placeholders sort after every other value; all other pairs
 * compare equal. Returns 1 when only *a is -1, -1 when only *b is -1, else
 * 0. Evidence: reference disassembly 0xf3960-0xf3989. */
int __cdecl list_indices_sort_proc(const void *a, const void *b)
{
  int left;
  int right;

  left = *(const int *)a;
  right = *(const int *)b;
  if (left == -1) {
    if (right != left) {
      return 1;
    }
  } else if (right == -1) {
    return right;
  }
  return 0;
}

/* multiplayer_settings_select_list_update_item (0xf4210)
 * "player settings select" 3-wide spinner list update. Evidence: reference
 * disassembly 0xf4210-0xf46d5 (TU proven by the __FILE__ assert strings; the
 * function is reached only through the data reference at 0x31e538).
 *
 * FUN_000f3690(indices@<eax>, widget@<ecx>) fills the 3 visible list-item
 * indices (pre-set to -1). Each live index is mapped through the widget's
 * item-id array (widget+0x40) and the resulting ids are cached into the
 * profile record table at 0x5aa3c0 (3 records, stride 0x34; see the layout
 * comment on player_profile_1wide_list_update below). Then, per visible item
 * (stops at the first -1 index), the child widget chain is validated and the
 * item is filled from its cached record or blanked. As in the 1-wide update,
 * rec points at record+4 (the name), so record offsets are 4 less. */
void multiplayer_settings_select_list_update_item(int *widget)
{
  int indices[3]; /* [EBP-0x34] */
  int ids[3]; /* [EBP-0x40] */
  short *list_tag;
  int i;
  int j;
  int container;
  int name_box;
  int color_pic;
  int fields;
  int empty_label;
  int level_label;
  int level_text;
  int skill_label;
  int skill_text;
  int controls_label;
  int controls_text;
  int item_id;
  int *entry;
  int entry_index;
  wchar_t *rec;
  wchar_t *name_buf;
  wchar_t *src;
  unsigned short flags;
  int tag_index;
  int clamped;
  short last_level; /* [EBP-0x14] */
  short skill_level; /* [EBP-0x4] */

  indices[0] = -1;
  indices[1] = -1;
  indices[2] = -1;
  list_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */, *widget);
  if (*list_tag != 2) {
    display_assert(
      "expected a spinner list for 'player settings select' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x78a, 1);
    system_exit(-1);
  }
  if (*(int *)((char *)list_tag + 0x3e0) != 3) {
    display_assert(
      "expected 3 children (list items) for 'player settings select' widget",
      "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
      0x78b, 1);
    system_exit(-1);
  }
  FUN_000f3690(indices, widget);
  for (j = 0; j < 3; j++) {
    if (indices[j] == -1) {
      ids[j] = -1;
    } else {
      ids[j] = ((int *)widget[0x10])[indices[j]];
    }
  }
  player_profile_update_cache_for_nwide_list(ids, 3);

  for (i = 0; i < 3; i++) {
    if (indices[i] == -1) {
      return;
    }
    container = (int)widget_instance_get_nth_child(widget, i);
    if (container == 0 || *(short *)(container + 0xe) != 0) {
      display_assert(
        "expected profile item description container widget",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x7af, 1);
      system_exit(-1);
    }
    name_box = *(int *)(container + 0x34);
    if (name_box == 0 || *(short *)(name_box + 0xe) != 1) {
      display_assert(
        "expected a text box widget for profile name",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x7b2, 1);
      system_exit(-1);
    }
    color_pic = *(int *)(name_box + 0x2c);
    if (color_pic == 0 || *(short *)(color_pic + 0xe) != 0) {
      display_assert(
        "expected a container widget for the profile color picture",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x7b5, 1);
      system_exit(-1);
    }
    fields = *(int *)(color_pic + 0x2c);
    if (fields == 0 || *(short *)(fields + 0xe) != 0) {
      display_assert(
        "expected a container widget for the profile description fields",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x7b8, 1);
      system_exit(-1);
    }
    empty_label = *(int *)(fields + 0x34);
    if (empty_label == 0 || *(short *)(empty_label + 0xe) != 1) {
      display_assert(
        "expected a text box widget for the 'empty profile' label",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x7bb, 1);
      system_exit(-1);
    }
    level_label = *(int *)(empty_label + 0x2c);
    if (level_label == 0 || *(short *)(level_label + 0xe) != 1) {
      display_assert(
        "expected a text box widget for the current level label",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x7be, 1);
      system_exit(-1);
    }
    level_text = *(int *)(level_label + 0x2c);
    if (level_text == 0 || *(short *)(level_text + 0xe) != 1) {
      display_assert(
        "expected a text box widget for the current level text field",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x7c1, 1);
      system_exit(-1);
    }
    skill_label = *(int *)(level_text + 0x2c);
    if (skill_label == 0 || *(short *)(skill_label + 0xe) != 1) {
      display_assert(
        "expected a text box widget for the current skill level label",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x7c4, 1);
      system_exit(-1);
    }
    skill_text = *(int *)(skill_label + 0x2c);
    if (skill_text == 0 || *(short *)(skill_text + 0xe) != 1) {
      display_assert(
        "expected a text box widget for the profile's current skill level "
        "text field",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x7c7, 1);
      system_exit(-1);
    }
    controls_label = *(int *)(skill_text + 0x2c);
    if (controls_label == 0 || *(short *)(controls_label + 0xe) != 1) {
      display_assert(
        "expected a text box widget for the controls label",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x7ca, 1);
      system_exit(-1);
    }
    controls_text = *(int *)(controls_label + 0x2c);
    if (controls_text == 0 || *(short *)(controls_text + 0xe) != 1) {
      display_assert(
        "expected a text box widget for the controls text field",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x7cd, 1);
      system_exit(-1);
    }

    item_id = ((int *)widget[0x10])[indices[i]];
    if (item_id != -1) {
      entry_index = 0;
      entry = (int *)0x5aa3c0;
      do {
        if (*entry == item_id) {
          rec = (wchar_t *)(entry_index * 0x34 + 0x5aa3c4);
          if (rec != NULL) {
            *(char *)(name_box + 0x10) = 1;
            *(char *)(empty_label + 0x10) = 0;
            *(char *)(level_label + 0x10) = 1;
            *(char *)(level_text + 0x10) = 1;
            *(char *)(skill_label + 0x10) = 1;
            *(char *)(skill_text + 0x10) = 1;
            *(char *)(controls_label + 0x10) = 1;
            *(char *)(controls_text + 0x10) = 1;
            name_buf =
              (wchar_t *)ui_widget_realloc(*(int *)(name_box + 0x3c), 0x18,
                                           "c:\\halo\\SOURCE\\interface\\ui_"
                                           "widget_game_data_input_functions.c",
                                           0x7e7);
            *(wchar_t **)(name_box + 0x3c) = name_buf;
            if (name_buf != NULL) {
              flags = *(unsigned short *)((char *)rec + 0x1a);
              if ((flags & 1) != 0) {
                tag_index = tag_loaded(
                  0x75737472 /* 'ustr' */,
                  "ui\\shell\\strings\\default_player_profile_names");
                if (tag_index != -1) {
                  src = (wchar_t *)FUN_0019d420(tag_index, flags >> 8);
                } else {
                  src = (wchar_t *)0x281c38; /* L"<unknown>" */
                }
                ustrncpy(*(wchar_t **)(name_box + 0x3c), src, 0xb);
                *(short *)(*(int *)(name_box + 0x3c) + 0x16) = 0;
              } else {
                ustrncpy(name_buf, rec, 0xb);
                *(short *)(*(int *)(name_box + 0x3c) + 0x16) = 0;
              }
            }

            if (*(short *)((char *)rec + 0x18) < 0) {
              clamped = 0;
            } else {
              clamped = *(short *)((char *)rec + 0x18);
              if (clamped > (int)FUN_001c0ed0() - 1) {
                clamped = (int)FUN_001c0ed0() - 1;
              }
            }
            *(short *)(color_pic + 0x50) = (short)clamped;

            if ((*(unsigned char *)((char *)rec + 0x1a) & 1) != 0) {
              *(char *)(level_text + 0x10) = 0;
              *(char *)(skill_text + 0x10) = 0;
            } else {
              player_profile_save_last_level_played(rec, &last_level,
                                                    &skill_level);
              if (last_level + 1 > 9) {
                last_level = 9;
              } else {
                last_level = last_level + 1;
              }
              *(short *)(level_text + 0x40) = last_level;
              *(short *)(skill_text + 0x40) = skill_level;
              *(unsigned short *)(controls_text + 0x40) =
                (unsigned short)(*(char *)((char *)rec + 0x2b) == 1);
            }
            goto next_item;
          }
          break;
        }
        entry = entry + 0xd; /* stride 0x34 bytes */
        entry_index = entry_index + 1;
      } while ((int)entry < 0x5aa45c);
    }

    *(char *)(name_box + 0x10) = 0;
    *(short *)(color_pic + 0x50) = (short)FUN_001c0ed0();
    *(char *)(empty_label + 0x10) = 1;
    *(char *)(level_label + 0x10) = 0;
    *(char *)(level_text + 0x10) = 0;
    *(char *)(skill_label + 0x10) = 0;
    *(char *)(skill_text + 0x10) = 0;
    *(char *)(controls_label + 0x10) = 0;
    *(char *)(controls_text + 0x10) = 0;
  next_item:;
  }
}

/* player_profile_1wide_list_update (0xf46e0)
 * "mp player settings select" quarter-screen profile list widget update.
 *
 * Validates the widget hierarchy (container wrapper + 1-wide DeLa spinner +
 * 3 child widgets), then resolves the currently selected profile id against the
 * global profile record table at 0x5aa3c0 (3 records, stride 0x34).  On a hit
 * it fills the widget's name buffer (widget+0x4c, 0x18 bytes) and the sibling
 * widget's description buffer (sibling+0x3c, 0x200 bytes).  On a miss the
 * item-id array is re-sorted / compacted and the routine restarts from the top
 * (the back edges at 0x48e6 / 0x48fc target 0x46f0, i.e. the whole body is a
 * retry loop).
 *
 * Profile record layout (evidence: the accesses below, stride 0x34):
 *   +0x00 int    profile id
 *   +0x04 wchar_t name[12]
 *   +0x1c short  controller-setup index (< 0 => 0)
 *   +0x1e ushort flags; bit0 = "use default name", bits 8..15 = ustr string
 * index +0x2c byte   button description index +0x2d byte   joystick description
 * index MSVC keeps the record pointer as &record.name (base+4), so the offsets
 * used below are 4 less than the record-relative ones above. */
void player_profile_1wide_list_update(int *widget)
{
  short *list_tag;
  void *wrapper_tag;
  int child; /* [EBP-0x4] */
  int sibling; /* [EBP+0x8] — MSVC reuses the dead incoming param slot */
  int local_id; /* [EBP-0x8] */
  int item_id;
  int *item_ids;
  int *entry;
  int entry_index;
  wchar_t *rec_name;
  wchar_t *name_buf;
  wchar_t *desc_buf;
  wchar_t *src;
  unsigned short flags;
  int tag_index;
  int joystick_tag;
  int button_tag;
  int joystick_str;
  int button_str;
  int clamped;
  int count;
  int used;
  short list_index;

  while (1) {
    if (widget[0xc] == 0) {
      display_assert(
        "expected qtr-screen profile select list to be wrapped in a container "
        "widget",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x85e, 1);
      system_exit(-1);
    }
    list_tag = (short *)tag_get(0x44654c61 /* 'DeLa' */, *widget);
    if (*list_tag != 2) {
      display_assert(
        "expected a spinner list for 'mp player settings select' widget",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x864, 1);
      system_exit(-1);
    }
    if (*(int *)((char *)list_tag + 0x3e0) != 0) {
      display_assert(
        "expected 0 children (1-wide spinner) for 'mp player settings select' "
        "widget",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x865, 1);
      system_exit(-1);
    }
    wrapper_tag = tag_get(0x44654c61 /* 'DeLa' */, *(int *)widget[0xc]);
    if (*(int *)((char *)wrapper_tag + 0x3e0) != 3) {
      display_assert(
        "expected qtr-screen profile select wrapper screen to have 3 child "
        "widgets (pic, description, list... in that order)",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x867, 1);
      system_exit(-1);
    }
    child = *(int *)(widget[0xc] + 0x34);
    sibling = *(int *)(child + 0x2c);
    if (widget != *(int **)(sibling + 0x2c)) {
      display_assert(
        "expected qtr-screen profile select wrapper screen to have 3 child "
        "widgets (pic, description, list... in that order)",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x86d, 1);
      system_exit(-1);
    }
    if (*(short *)((char *)widget + 0x3c) < 0 ||
        (int)*(short *)((char *)widget + 0x3c) >=
          (int)*(unsigned short *)((char *)widget + 0x44)) {
      display_assert(
        "qtr-screen profile list has invalid list item index",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x86f, 1);
      system_exit(-1);
    }
    if (*(short *)((char *)widget + 0x3c) < 0 ||
        (int)*(short *)((char *)widget + 0x3c) >=
          (int)*(unsigned short *)((char *)widget + 0x44)) {
      display_assert(
        "invalid list item index",
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x872, 1);
      system_exit(-1);
    }

    item_id = ((int *)widget[0x10])[*(short *)((char *)widget + 0x3c)];
    local_id = item_id;
    player_profile_update_cache_for_nwide_list(&local_id, 1);

    if (item_id != -1) {
      entry_index = 0;
      entry = (int *)0x5aa3c0;
      do {
        if (*entry == item_id) {
          rec_name = (wchar_t *)(entry_index * 0x34 + 0x5aa3c4);
          if (rec_name != NULL) {
            name_buf =
              (wchar_t *)ui_widget_realloc(widget[0x13], 0x18,
                                           "c:\\halo\\SOURCE\\interface\\ui_"
                                           "widget_game_data_input_functions.c",
                                           0x886);
            widget[0x13] = (int)name_buf;
            if (name_buf == NULL) {
              return;
            }
            flags = *(unsigned short *)((char *)rec_name + 0x1a);
            if ((flags & 1) != 0) {
              tag_index =
                tag_loaded(0x75737472 /* 'ustr' */,
                           "ui\\shell\\strings\\default_player_profile_names");
              src = (wchar_t *)0x26cdf0; /* L"" */
              if (tag_index != -1) {
                src = (wchar_t *)FUN_0019d420(tag_index, flags >> 8);
              }
              ustrncpy((wchar_t *)widget[0x13], src, 0xb);
              *(short *)(widget[0x13] + 0x16) = 0;
            } else {
              ustrncpy((wchar_t *)widget[0x13], rec_name, 0xb);
              *(short *)(widget[0x13] + 0x16) = 0;
            }

            if (*(short *)((char *)rec_name + 0x18) < 0) {
              clamped = 0;
            } else {
              clamped = *(short *)((char *)rec_name + 0x18);
              if (clamped > (int)FUN_001c0ed0() - 1) {
                clamped = (int)FUN_001c0ed0() - 1;
              }
            }
            *(short *)(child + 0x50) = (short)clamped;

            desc_buf =
              (wchar_t *)ui_widget_realloc(*(int *)(sibling + 0x3c), 0x200,
                                           "c:\\halo\\SOURCE\\interface\\ui_"
                                           "widget_game_data_input_functions.c",
                                           0x89c);
            *(wchar_t **)(sibling + 0x3c) = desc_buf;
            if (desc_buf == NULL) {
              return;
            }

            if ((*(unsigned char *)((char *)rec_name + 0x1a) & 1) != 0) {
              joystick_tag =
                tag_loaded(0x75737472 /* 'ustr' */,
                           "ui\\shell\\main_menu\\player_profiles_"
                           "select\\joystick_set_defaults_descriptions");
              button_tag = tag_loaded(0x75737472 /* 'ustr' */,
                                      "ui\\shell\\main_menu\\player_profiles_"
                                      "select\\button_set_long_descriptions");
              if (joystick_tag == -1 || button_tag == -1) {
                **(short **)(sibling + 0x3c) = 0;
                *(short *)(*(int *)(sibling + 0x3c) + 0x1fe) = 0;
                return;
              }
              joystick_str = FUN_0019d420(
                joystick_tag,
                (unsigned short)*(unsigned char *)((char *)rec_name + 0x29));
              button_str = FUN_0019d420(
                button_tag,
                (unsigned short)*(unsigned char *)((char *)rec_name + 0x28));
              unicode_sprintf(*(wchar_t **)(sibling + 0x3c), 0xff, L"%s%hs%s",
                              (wchar_t *)joystick_str, "\r\n",
                              (wchar_t *)button_str);
              *(short *)(*(int *)(sibling + 0x3c) + 0x1fe) = 0;
            } else {
              joystick_tag =
                tag_loaded(0x75737472 /* 'ustr' */,
                           "ui\\shell\\main_menu\\player_profiles_"
                           "select\\joystick_set_short_descriptions");
              button_tag = tag_loaded(0x75737472 /* 'ustr' */,
                                      "ui\\shell\\main_menu\\player_profiles_"
                                      "select\\button_set_short_descriptions");
              if (joystick_tag != -1 && button_tag != -1) {
                joystick_str = FUN_0019d420(
                  joystick_tag,
                  (unsigned short)*(unsigned char *)((char *)rec_name + 0x29));
                button_str = FUN_0019d420(
                  button_tag,
                  (unsigned short)*(unsigned char *)((char *)rec_name + 0x28));
                unicode_sprintf(*(wchar_t **)(sibling + 0x3c), 0xff, L"%s%hs%s",
                                (wchar_t *)joystick_str, "\r\n",
                                (wchar_t *)button_str);
              }
            }
            *(short *)(*(int *)(sibling + 0x3c) + 0x1fe) = 0;
            return;
          }
          break;
        }
        entry = entry + 0xd; /* stride 0x34 bytes */
        entry_index = entry_index + 1;
      } while ((int)entry < 0x5aa45c);
    }

    if (*(unsigned short *)((char *)widget + 0x44) == 0) {
      name_buf = (wchar_t *)ui_widget_realloc(
        widget[0x13], 4,
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x8d2);
      widget[0x13] = (int)name_buf;
      if (name_buf != NULL) {
        *name_buf = 0;
      }
      *(short *)(child + 0x50) = 0;
      desc_buf = (wchar_t *)ui_widget_realloc(
        *(int *)(sibling + 0x3c), 4,
        "c:\\halo\\SOURCE\\interface\\ui_widget_game_data_input_functions.c",
        0x8d7);
      *(wchar_t **)(sibling + 0x3c) = desc_buf;
      if (desc_buf != NULL) {
        *desc_buf = 0;
      }
      return;
    }

    item_ids = (int *)widget[0x10];
    count = (int)*(unsigned short *)((char *)widget + 0x44);
    qsort(item_ids, count, 4, list_indices_sort_proc);
    for (used = 0; used < count; used++) {
      if (item_ids[used] == -1) {
        break;
      }
    }
    list_index = *(short *)((char *)widget + 0x3c);
    *(unsigned short *)((char *)widget + 0x44) = (unsigned short)used;
    if (list_index < 0) {
      *(short *)((char *)widget + 0x3c) = 0;
    } else {
      clamped = (int)(unsigned short)used - 1;
      if ((int)list_index <= clamped) {
        clamped = list_index;
      }
      *(short *)((char *)widget + 0x3c) = (short)clamped;
    }
  }
}
