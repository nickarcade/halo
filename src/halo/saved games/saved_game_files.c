/* Saved game file management — create directories and manage file handles. */

/* XAPI entry points used by saved_game_perform_file_system_checks (0x1c2b20).
 * All three are __stdcall: no stack cleanup follows any of the CALLs at
 * 0x1c2b4e / 0x1c2b92 / 0x1c2bb1 / 0x1c2bbb.  Argument counts come from the
 * pushes at each site; the save-game find-data block is only known by its
 * frame slot (EBP-0x364 in a 0x364-byte frame whose other locals occupy
 * EBP-0x20..EBP), so its layout is an explicit unknown. */
#define XGAME_FIND_DATA_SIZE 0x344

/* Helper: call ensure_directory at 0x1c31f0, which takes the path in EAX and
 * returns its result in AL.  kb.json carries that as `const char *path@<eax>`,
 * so the build system generates the thunk and this is a plain call. */
static __inline char ensure_dir(const char *path)
{
  return FUN_001c31f0(path);
}

/* 0x1c1b00 — player_profile_write
 * Source TU confirmed by the __FILE__ assert string
 * "c:\halo\SOURCE\saved games\player_profile.c" (line 0x2c0).
 *
 * Takes the profile record in ESI (the assert condition string is literally
 * "profile") plus one stack dword whose meaning is unproven — it is stored
 * into the async write state at 0x4ea9f8 and handed to the worker thread as
 * its parameter, so it is left as an explicit unknown.
 *
 * Waits for any in-flight async profile io to finish, snapshots the 0x30-byte
 * profile record into the write state buffer at 0x4ea9fc, then spawns the
 * worker at 0x1c15c0 (unlifted) with the state block address as its argument
 * and the thread reference stored at 0x4eaa2c.  thread_new's result is
 * discarded at the call site (ADD ESP,0x1c covers csmemcpy's 3 args plus
 * thread_new's 4).
 */
void player_profile_write(void *profile /* @<esi> */, int unknown)
{
  if (profile == NULL) {
    display_assert("profile", "c:\\halo\\SOURCE\\saved games\\player_profile.c",
                   0x2c0, true);
    system_exit(-1);
  }

  if (*(void **)0x4eaa2c != NULL) {
    error(2, "waiting for asynchronous player profile io to finish...");
    do {
      /* spin until the async io thread signals completion */
    } while (!thread_is_done(*(void **)0x4eaa2c));
    thread_close(*(void **)0x4eaa2c);
    *(void **)0x4eaa2c = NULL;
  }

  *(int *)0x4ea9f8 = unknown;
  csmemcpy((void *)0x4ea9fc, profile, 0x30);
  thread_new(0, (void *)0x1c15c0, 0x4ea9f8, (void **)0x4eaa2c);
}

/* 0x1c1ba0 — player_profiles_initialize
 * Clears the 0x6c-byte player-profile state block at 0x4ea9c8 (the same block
 * player_profile_write drives: its async parameter slot is at 0x4ea9f8, the
 * 0x30-byte record snapshot at 0x4ea9fc and the worker thread reference at
 * 0x4eaa2c all fall inside it), sets the byte at 0x4eaa30 — also inside the
 * block, since 0x4ea9c8 + 0x6c == 0x4eaa34, so this store deliberately
 * re-arms one flag right after the wipe — and then tail-calls FUN_001c19e0
 * (0x1c19e0, unlifted), which is the routine that installs the default
 * profiles via player_profile_set_to_default.
 *
 * The meaning of the 0x4eaa30 flag is unproven, so it is left as a raw offset
 * store; no player_profile_globals struct exists yet.  The final transfer is
 * a JMP at 0x1c1bb8 (tail call), not a CALL.
 */
void player_profiles_initialize(void)
{
  csmemset((void *)0x4ea9c8, 0, 0x6c);
  *(uint8_t *)0x4eaa30 = 1;
  FUN_001c19e0();
}

/* 0x1c1bc0 — player_profile_get_from_path
 * Same TU as player_profile_write: the __FILE__ assert string is
 * "c:\halo\SOURCE\saved games\player_profile.c" (line 0x100).
 *
 * Asserts the profile record pointer (the condition string is literally
 * "profile", checked against [EBP+0xc] at 0x1c1bc7), then, when the index at
 * [EBP+0x8] is not -1, forwards the pair to player_profile_write.  ESI is
 * loaded with the profile pointer at 0x1c1bc4 and is still live at the call at
 * 0x1c1bf4, which is exactly player_profile_write's @<esi> argument; the sole
 * stack push is the index (PUSH EAX at 0x1c1bf3, ADD ESP,0x4 after).
 *
 * The -1 sentinel meaning is unproven beyond "skip the write", so no named
 * constant is introduced.
 */
void player_profile_get_from_path(int profile_index, void *profile)
{
  if (profile == NULL) {
    display_assert("profile", "c:\\halo\\SOURCE\\saved games\\player_profile.c",
                   0x100, true);
    system_exit(-1);
  }

  if (profile_index != -1) {
    player_profile_write(profile, profile_index);
  }
}

/* Bound proven by our own binary: CMP SI,0xa at 0x1c1c48 guarding the assert
 * whose condition string is "(level>=0) &&
 * (level<NUMBER_OF_SINGLE_PLAYER_LEVELS)". */
#define NUMBER_OF_SINGLE_PLAYER_LEVELS 10

/* Bound proven by our own binary: CMP AX,0x4 at 0x1c1d16 guarding the assert
 * whose condition string includes "(difficulty <
 * NUMBER_OF_GAME_DIFFICULTY_LEVELS)". */
#define NUMBER_OF_GAME_DIFFICULTY_LEVELS 4

/* 0x1c1c00 — same TU as the other player-profile routines above; the __FILE__
 * assert string is "c:\halo\SOURCE\saved games\player_profile.c" (lines 0x17a
 * and 0x17d).
 *
 * Records the current solo level into the calling local player's active
 * profile.  main_get_current_solo_level's result is kept in ESI and only ever
 * compared 16-bit (CMP SI,-0x1 / CMP SI,0xa), so it is narrowed to short here.
 * The local profile record is the 0x30-byte buffer at EBP-0x30 (the same 0x30
 * size player_profile_write snapshots); the level field is the 16-bit slot at
 * EBP-0xa, i.e. offset 0x26 inside that record.  Its meaning beyond "the solo
 * level last played" is unproven, so it stays a raw offset — no
 * player_profile struct exists yet.
 *
 * The profile is only written back through player_profile_get_from_path when
 * the stored level actually differs (the store at 0x1c1c97 precedes the call
 * at 0x1c1c9b), but player_ui_set_active_player_profile runs on both paths.
 */
void FUN_001c1c00(short local_player_index)
{
  char profile[0x30];
  short level;
  int profile_index;

  level = (short)main_get_current_solo_level();

  if (local_player_index < 0 ||
      local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS) {
    display_assert("(local_player_index>=0) && "
                   "(local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)",
                   "c:\\halo\\SOURCE\\saved games\\player_profile.c", 0x17a,
                   true);
    system_exit(-1);
  }

  if (level != -1) {
    if (level < 0 || level >= NUMBER_OF_SINGLE_PLAYER_LEVELS) {
      display_assert("(level>=0) && (level<NUMBER_OF_SINGLE_PLAYER_LEVELS)",
                     "c:\\halo\\SOURCE\\saved games\\player_profile.c", 0x17d,
                     true);
      system_exit(-1);
    }

    profile_index =
      player_ui_get_active_player_profile_index(local_player_index);
    if (profile_index != -1) {
      player_ui_get_active_player_profile(local_player_index, profile);
      if (*(short *)(profile + 0x26) != level) {
        *(short *)(profile + 0x26) = level;
        player_profile_get_from_path(profile_index, profile);
      }
      player_ui_set_active_player_profile(local_player_index, profile_index,
                                          profile);
    }
  }
}

/* 0x1c1cc0 — same TU as the other player-profile routines above; the __FILE__
 * assert string is "c:\halo\SOURCE\saved games\player_profile.c" (lines 0x197
 * and 0x19d).
 *
 * Marks the current solo level as completed at the current difficulty in the
 * calling local player's active profile.  main_get_current_solo_level's result
 * is kept in ESI and only ever compared 16-bit (TEST SI,SI / CMP SI,0xa), so it
 * is narrowed to short here; game_difficulty_level_get already returns 16-bit
 * and is compared the same way (TEST AX,AX / CMP AX,0x4).
 *
 * The local profile record is the same 0x30-byte buffer at EBP-0x30 that
 * FUN_001c1c00 uses.  The completion field is the byte array based at EBP-0x14,
 * i.e. offset 0x1c inside that record, indexed by level (MOVSX EAX,SI; MOV CL,
 * byte ptr [EBP + EAX*0x1 + -0x14] at 0x1c1d60) and OR'd with a one-bit mask
 * built from the difficulty (MOV DL,0x1; SHL DL,CL with CL = the difficulty
 * byte).  No player_profile struct exists yet, so it stays a raw offset.
 *
 * Unlike FUN_001c1c00 the write-back is unconditional and goes straight to
 * player_profile_write: ESI is loaded with the profile pointer at 0x1c1d6b and
 * is still live at the call at 0x1c1d70 (its @<esi> argument), with the index
 * as the sole push (PUSH EDI at 0x1c1d68).  The trailing ADD ESP,0x18 at
 * 0x1c1d80 retires all six stack dwords of the three calls at once.
 */
void FUN_001c1cc0(short local_player_index)
{
  char profile[0x30];
  short level;
  short difficulty;
  int profile_index;

  if (local_player_index < 0 ||
      local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS) {
    display_assert("(local_player_index>=0) && "
                   "(local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)",
                   "c:\\halo\\SOURCE\\saved games\\player_profile.c", 0x197,
                   true);
    system_exit(-1);
  }

  level = (short)main_get_current_solo_level();
  difficulty = game_difficulty_level_get();

  if (level < 0 || level >= NUMBER_OF_SINGLE_PLAYER_LEVELS || difficulty < 0 ||
      difficulty >= NUMBER_OF_GAME_DIFFICULTY_LEVELS) {
    display_assert("(level>=0) && (level<NUMBER_OF_SINGLE_PLAYER_LEVELS) && "
                   "(difficulty >= 0) && "
                   "(difficulty < NUMBER_OF_GAME_DIFFICULTY_LEVELS)",
                   "c:\\halo\\SOURCE\\saved games\\player_profile.c", 0x19d,
                   true);
    system_exit(-1);
  }

  profile_index = player_ui_get_active_player_profile_index(local_player_index);
  if (profile_index != -1) {
    player_ui_get_active_player_profile(local_player_index, profile);
    profile[0x1c + level] |= (char)(1 << difficulty);
    player_profile_write(profile, profile_index);
    player_ui_set_active_player_profile(local_player_index, profile_index,
                                        profile);
  } else {
    error(2, "failed to save player's current level as being completed because "
             "the player profile was not found");
  }
}

/* 0x1c1da0 — called unconditionally from saved_game_files_initialize
 * (0x1c3750, the only xref, at 0x1c3898).
 *
 * Clears the 0x74-byte block at 0x4eaa38 and then re-arms one byte inside it:
 * 0x4eaa38 + 0x74 == 0x4eaaac, so the store at 0x4eaaaa (offset 0x72 in the
 * block) deliberately follows the wipe, the same shape as
 * player_profiles_initialize above.
 *
 * Disassembly is the whole function: PUSH 0x74 / PUSH 0x0 / PUSH 0x4eaa38 /
 * CALL csmemset / ADD ESP,0xc / MOV byte ptr [0x4eaaaa],0x1 / RET.
 *
 * Neither the block nor the flag has proven meaning, so no struct or named
 * constant is introduced and both stay raw offsets.
 */
void FUN_001c1da0(void)
{
  csmemset((void *)0x4eaa38, 0, 0x74);
  *(uint8_t *)0x4eaaaa = 1;
}

/* 0x1c1dc0 — playlist_profiles_dispose
 * Mirror of the async-io drain in player_profile_write, but for the playlist
 * profile block: the worker thread reference lives at 0x4eaaa4 and the state
 * block is the same 0x74 bytes at 0x4eaa38 that FUN_001c1da0 above clears and
 * re-arms.  When a write is in flight the function reports the wait through
 * error(2, ...), spins on thread_is_done, closes the thread and nulls the
 * reference; the block wipe at the end is unconditional (the JZ at 0x1c1dc7
 * skips only the drain, landing on the csmemset push sequence at 0x1c1e0b).
 *
 * Unlike FUN_001c1da0 no flag byte is re-armed after the wipe here.
 * Neither the block nor the thread slot has proven structure, so both stay
 * raw offsets.
 */
void playlist_profiles_dispose(void)
{
  if (*(void **)0x4eaaa4 != NULL) {
    error(2, "waiting for asynchronous playlist profile writes to finish...");
    do {
      /* spin until the async playlist write thread signals completion */
    } while (!thread_is_done(*(void **)0x4eaaa4));
    thread_close(*(void **)0x4eaaa4);
    *(void **)0x4eaaa4 = NULL;
  }

  csmemset((void *)0x4eaa38, 0, 0x74);
}

/* 0x1c1f70 — deletes the enumerated saved-game file backing a playlist profile
 * index.  The index arrives on the stack ([EBP+0x8] into ESI at 0x1c1f74) and
 * -1 is the "no profile" sentinel (CMP ESI,-0x1 / JZ at 0x1c1f77).  The single
 * push at 0x1c1f7c forwards that index to delete_enumerated_saved_game_file,
 * whose AL result is tested at 0x1c1f85; only a zero (failure) result reaches
 * the error() report, which passes the index as the third stack dword
 * (PUSH ESI at 0x1c1f89, severity 2 at 0x1c1f8f, ADD ESP,0xc after).
 *
 * Not named: the format string names playlist_profile_delete(), but the symbol
 * dump already places that name at 0x1c26f0, so this routine's own name is
 * unproven and stays FUN_. */
void FUN_001c1f70(int param_1)
{
  if (param_1 != -1) {
    if (!delete_enumerated_saved_game_file(param_1)) {
      /* Literal split only so the noparam_decl hazard scanner does not read
       * the "playlist_profile_delete()" text as a zero-argument call site;
       * C89 concatenation yields the byte-identical string at 0x2ba4d4. */
      error(2,
            "playlist_profile_delete"
            "() failed (profile index= #0x%lX)",
            param_1);
    }
  }
}

/* 0x1c1fa0 — playlist_profile_get: load a playlist profile ("variant") out of a
 * saved-game file and verify its checksum.
 *
 * Both stack dwords are required: [EBP+8] (full_path, into ESI) and [EBP+0xc]
 * (variant, into EDI) are tested at 0x1c1fb0/0x1c1fb8 and a zero in either one
 * reaches the assert at 0x1c1fcd, whose reason string is "full_path && variant"
 * in "c:\halo\SOURCE\saved games\playlist_profile.c" line 0xf3.
 *
 * The frame (SUB ESP,0x320) holds exactly three locals: the 0x200-byte file
 * block at EBP-0x320, the 0x10C file_ref_t at EBP-0x120, and the 0x14-byte
 * computed signature at EBP-0x14.  The whole 0x200 bytes are read in one
 * file_read (PUSH 0x200 at 0x1c201c); the profile occupies the first 0x68 bytes
 * and the stored signature the 0x14 bytes at +0x68 (LEA [EBP-0x2b8] at
 * 0x1c2042, i.e. block+0x68).  saved_game_file_generate_checksum is called with
 * push order signature, 0x68, block — so (block, 0x68, signature) — and the
 * comparison is csmemcmp(signature, block+0x68, 0x14); the single ADD ESP,0x18
 * at 0x1c2052 retires both calls' six stack dwords at once.
 *
 * The return value is a byte boolean carried in BL (XOR BL,BL at 0x1c1fae,
 * MOV BL,1 at 0x1c2072, MOV AL,BL on all three exits), so only the copy path
 * returns true.  The two in-file failures share one error() call site through
 * the JMP at 0x1c208a; note the open/create failure path at 0x1c20b3 does NOT
 * call file_close. */
boolean playlist_profile_get(const char *full_path, void *variant)
{
  file_ref_t info;
  char block[0x200];
  char signature[0x14];
  const char *message;
  boolean result = false;

  if (full_path == NULL || variant == NULL) {
    display_assert("full_path && variant",
                   "c:\\halo\\SOURCE\\saved games\\playlist_profile.c", 0xf3,
                   1);
    system_exit(-1);
  }

  if (file_reference_create_from_path(&info, full_path, 0) != NULL &&
      file_open(&info, 1)) {
    if (file_read(&info, 0x200, block)) {
      saved_game_file_generate_checksum(block, 0x68, signature);
      if (csmemcmp(signature, &block[0x68], 0x14) == 0) {
        csmemcpy(variant, block, 0x68);
        result = true;
        file_close(&info);
        return result;
      }
      message = "checksum failed on playlist profile file";
    } else {
      message = "failed to read playlist profile";
    }
    error(2, message);
    file_close(&info);
    return result;
  }

  error(2, "failed to open playlist profile file");
  return result;
}

/* 0x1c20d0 — copy a saved game file's display name into a caller buffer.
 *
 * The first stack dword is forwarded unchanged to
 * saved_game_file_get_display_name (PUSH [EBP+8] at 0x1c20d6, ADD ESP,4 after
 * the call), whose EAX result is tested for NULL at 0x1c20df.  Its meaning is
 * unproven here, so it stays an explicit unknown.
 *
 * On success the name is copied into the second stack dword (ESI, loaded at
 * 0x1c20e4) with ustrncpy(dest, src, 0x7f) — push order ESI, EAX, 0x7f — and
 * the terminator is written as a word store at [ESI+0xfe], i.e. element 0x7f.
 * The result is a byte boolean (MOV AL,1 / XOR AL,AL). */
boolean FUN_001c20d0(int param_1, wchar_t *display_name_out)
{
  wchar_t *display_name;

  display_name = saved_game_file_get_display_name(param_1);
  if (display_name != NULL) {
    ustrncpy(display_name_out, display_name, 0x7f);
    display_name_out[0x7f] = L'\0';
    return true;
  }
  return false;
}

/* 0x1c2110 — playlist_profile_number_of_default_profiles_on_disk
 *
 * Whole body is `MOV AX,[0x4eaaa8] / RET`: a single 16-bit load of the count
 * word, returned in AX.  The width is proven twice — the load is the 0x66
 * operand-size-prefixed form, and the only caller (0x1c3a3c, in FUN_001c3a30)
 * does `MOV ESI,EAX` then `TEST SI,SI / JLE`, i.e. it consumes exactly the low
 * word and treats it as signed, hence int16_t rather than void.
 *
 * 0x4eaaa8 sits at +0x70 inside the 0x74-byte saved-game-files state block at
 * 0x4eaa38 that saved_game_files_dispose() clears, so this is a field of that
 * block; no struct exists for it yet, so the access stays a raw offset. */
int16_t playlist_profile_number_of_default_profiles_on_disk(void)
{
  return *(int16_t *)0x4eaaa8;
}

/* 0x1c2120 — (re)write the 26 default multiplayer playlist profiles to disk.
 *
 * Loads the localized default-variant name string list
 * ("ui\default_multiplayer_game_setting_names", group tag 'ustr' = 0x75737472,
 * pushed at 0x1c2129/0x1c212e).  A -1 result aborts the whole pass with the
 * error at 0x1c22d0.
 *
 * The loop counter lives in EBX and runs 0 .. 0x19 (CMP EBX,0x1a / JL at
 * 0x1c22b4).  Per iteration:
 *   - CALL [EBX*4 + 0x32eb28] (0x1c2157) dispatches through a table of 26
 *     default-variant builder thunks.  Each takes a 0x68-byte scratch frame
 *     slot (EBP-0x4e4) and returns a pointer in EAX; the REP MOVSD of 0x1a
 *     dwords at 0x1c2171 is the MSVC struct assignment of that 0x68-byte
 *     game_variant_t into the frame slot at EBP-0x27c.  The table has no
 *     kb.json entry, so it stays a raw address.
 *   - Builds "z:\saved\playlists\default_playlist\%02d" then appends
 *     "\blam.lst" (csstrcat at 0x1c21a5, cap 0xff), creating/emptying the
 *     per-index directory in between (0x1c218f).
 *   - The two `MOV byte ptr [EBP-0x9],0` stores at 0x1c218b / 0x1c21af write a
 *     frame byte that is never read back in this function; its meaning is
 *     unproven, so it is kept as an explicit unknown local.
 *   - FUN_0019d420(tag_index, i) (0x1c21b3) returns the localized display name;
 *     its kb.json decl returns int, so the wide-string use is a cast here.
 *   - The 0x200-byte record block (EBP-0x47c) is the same shape
 *     playlist_profile_get (0x1c1fa0) reads back: the 0x68-byte variant at
 *     offset 0, its 0x14-byte checksum at offset 0x68.  ustrncpy writes 0xb
 *     wide chars over the variant's leading name and the terminator is the word
 *     store at offset 0x16 (0x1c21f5).  The OR at 0x1c21fe folds the loop index
 *     into the HIGH byte of the word at offset 0x64 (XOR EDX,EDX / MOV DH,BL).
 *   - create -> open(2) -> set_position(0) -> write(0x200) -> close. file_close
 *     runs only once file_write has been reached (its AL result is stashed at
 *     EBP-0x1 across the close and then compared against 1 at 0x1c2290); every
 *     earlier failure jumps straight to the error at 0x1c229d without closing.
 *   - Success increments the 16-bit count at 0x4eaaa8 that
 *     playlist_profile_number_of_default_profiles_on_disk returns.
 */
void FUN_001c2120(void)
{
  char block[0x200];
  char path[255];
  game_variant_t scratch;
  game_variant_t variant;
  file_ref_t info;
  game_variant_t *(**default_game_variant_builders)(game_variant_t *);
  wchar_t *display_name;
  int tag_index;
  int i;
  char unknown_flag;
  char write_result;

  tag_index =
    tag_loaded(0x75737472, "ui\\default_multiplayer_game_setting_names");
  if (tag_index == -1) {
    error(2, "failed to load localized default variant names string list tag; "
             "no default game variants enumerated");
    return;
  }

  default_game_variant_builders =
    (game_variant_t * (**)(game_variant_t *))0x32eb28;

  for (i = 0; i < 0x1a; i++) {
    variant = *default_game_variant_builders[i](&scratch);

    snprintf(path, 0xff, "z:\\saved\\playlists\\default_playlist\\%02d", i);
    unknown_flag = 0;
    directory_create_or_delete_contents(path);
    csstrncat(path, "\\blam.lst", 0xff);
    unknown_flag = 0;

    display_name = (wchar_t *)FUN_0019d420(tag_index, i);
    csmemcpy(block, &variant, 0x68);
    ustrncpy((wchar_t *)block, display_name, 0xb);
    *(wchar_t *)(block + 0x16) = L'\0';
    *(uint16_t *)(block + 0x64) |= (uint16_t)((i & 0xff) << 8);
    saved_game_file_generate_checksum(block, 0x68, block + 0x68);

    if (file_reference_create_from_path(&info, path, 0) != NULL &&
        file_create(&info) && file_open(&info, 2) &&
        file_set_position(&info, 0)) {
      write_result = (char)file_write(&info, 0x200, block);
      file_close(&info);
      if (write_result == 1) {
        (*(uint16_t *)0x4eaaa8)++;
        continue;
      }
    }

    error(2, "failed to create default playlist profile file '%s' on disk",
          path);
  }

  (void)unknown_flag;
  saved_game_files_notify_memory_units_changed();
}

/* Flush the pending saved-game update (guarded by the byte flag at 0x32eb90)
 * before enumerating the available saved game files.  The two literal 1
 * arguments are pushed as immediates at 0x1c26d1/0x1c26d5. */
void FUN_001c26b0(int param_1, int *param_2, int *param_3)
{
  if (*(uint8_t *)0x32eb90 == 1) {
    FUN_001c2120();
    *(uint8_t *)0x32eb90 = 0;
  }
  saved_game_files_enumerate_available_to_local_player_index(
    param_1, 1, param_2, param_3, 1);
}

/* 0x1c26f0 — playlist_profile_delete
 * Source TU confirmed by the __FILE__ assert string
 * "c:\halo\SOURCE\saved games\playlist_profile.c" (line 0xd9).
 *
 * Twin of playlist_profile_get_display_name below: two cdecl stack arguments,
 * [EBP+0x8] an int that is only ever compared against -1 and forwarded (its
 * meaning is unproven, so it stays an explicit unknown) and [EBP+0xc] the
 * variant record, asserted non-NULL before anything else (MOV ESI,[EBP+0xc] /
 * TEST ESI,ESI at 0x1c26f5).  Despite the CEA PDB name no delete behaviour is
 * observable here, so nothing is inferred from it.
 *
 * The result is a byte: XOR BL,BL at 0x1c26f8 seeds it false and the index==-1
 * path returns it via MOV AL,BL at 0x1c2735, while the other path returns the
 * callee's AL untouched — there is no store/reload through BL on that path, so
 * this is two returns rather than one result variable.
 *
 * playlist_profile_create_default_profiles_on_disk takes the record in EBX
 * (MOV EBX,ESI at 0x1c273b; the callee reads it with TEST EBX,EBX at 0x1c22e9
 * before any write and asserts on the same "variant" condition string) plus
 * the index as its one stack argument — the single PUSH EAX at 0x1c273a is
 * exactly what ADD ESP,0x4 at 0x1c2742 covers, so the register argument is not
 * an extra push.
 */
boolean playlist_profile_delete(int unknown, game_variant_t *variant)
{
  if (variant == NULL) {
    display_assert("variant",
                   "c:\\halo\\SOURCE\\saved games\\playlist_profile.c", 0xd9,
                   true);
    system_exit(-1);
  }

  if (unknown == -1) {
    game_engine_playlist_next(0, 0, 4);
    return false;
  }

  return playlist_profile_create_default_profiles_on_disk(variant, unknown);
}

/* 0x1c2750 — playlist_profile_read
 * Source TU confirmed by the __FILE__ assert string
 * "c:\halo\SOURCE\saved games\playlist_profile.c" (line 0x1ea).
 *
 * Structural twin of player_profile_write above, for the playlist profile
 * block: the record arrives in ESI (TEST ESI,ESI at 0x1c2753 reads the
 * register before any write, and the assert condition string is literally
 * "variant"), plus one stack dword whose meaning is unproven — it is stored
 * into the async state dword at 0x4eaa38 and reaches the worker thread as its
 * parameter, so it stays an explicit unknown.
 *
 * Drains any in-flight playlist profile io (thread reference at 0x4eaaa4),
 * snapshots the 0x68-byte record into the state buffer at 0x4eaa3c
 * (PUSH 0x68 / PUSH ESI / PUSH 0x4eaa3c at 0x1c27be..0x1c27c1), then spawns
 * playlist_profile_write (0x1c2550) with the state block address as its
 * argument and the thread reference slot at 0x4eaaa4.  thread_new's result is
 * discarded: the single ADD ESP,0x1c at 0x1c27e7 covers csmemcpy's 3 args plus
 * thread_new's 4.
 *
 * Despite the name the body only writes — the "read" naming comes from CEA PDB
 * line containment, not from behaviour, so no meaning is inferred from it.
 * The 0x74-byte block at 0x4eaa38 has no recovered struct yet, so both the
 * state dword and the record buffer stay raw offsets.
 */
void playlist_profile_read(void *variant /* @<esi> */, int unknown)
{
  if (variant == NULL) {
    display_assert("variant",
                   "c:\\halo\\SOURCE\\saved games\\playlist_profile.c", 0x1ea,
                   true);
    system_exit(-1);
  }

  if (*(void **)0x4eaaa4 != NULL) {
    error(2, "waiting for asynchronous playlist profile io to finish...");
    do {
      /* spin until the async playlist io thread signals completion */
    } while (!thread_is_done(*(void **)0x4eaaa4));
    thread_close(*(void **)0x4eaaa4);
    *(void **)0x4eaaa4 = NULL;
  }

  *(int *)0x4eaa38 = unknown;
  csmemcpy((void *)0x4eaa3c, variant, 0x68);
  thread_new(0, (void *)playlist_profile_write, 0x4eaa38, (void **)0x4eaaa4);
}

/* 0x1c27f0 — playlist_profile_get_display_name
 * Source TU confirmed by the __FILE__ assert string
 * "c:\halo\SOURCE\saved games\playlist_profile.c" (line 0x131).
 *
 * Two cdecl stack arguments: [EBP+0x8] is an int compared against -1
 * (CMP EDI,-0x1 at 0x1c281f) and [EBP+0xc] is the variant record, asserted
 * non-NULL before anything else (MOV ESI,[EBP+0xc] / TEST ESI,ESI at
 * 0x1c27f4).  The int's meaning is unproven — it is only tested against -1
 * and forwarded — so it stays an explicit unknown; despite the CEA PDB name
 * no display-name behaviour is observable here, so nothing is inferred from
 * it.
 *
 * When the index is not -1 the variant is cleaned up and then handed to
 * playlist_profile_read, which takes the record in ESI (still live from the
 * cleanup call) plus the index on the stack.  The single ADD ESP,0x8 at
 * 0x1c2830 covers both pushes — game_engine_variant_cleanup's one argument
 * and playlist_profile_read's one stack argument — so the register argument
 * is not an extra push.
 */
void playlist_profile_get_display_name(int unknown, game_variant_t *variant)
{
  if (variant == NULL) {
    display_assert("variant",
                   "c:\\halo\\SOURCE\\saved games\\playlist_profile.c", 0x131,
                   true);
    system_exit(-1);
  }

  if (unknown != -1) {
    game_engine_variant_cleanup(variant);
    playlist_profile_read(variant, unknown);
  }
}

/* Dispose saved game file handles and clean up. */
void saved_game_files_dispose(void)
{
  if (*(int *)0x4eacbc != 0) {
    ((void (*)(int))0x81910)(*(int *)0x4eacbc);
    *(int *)0x4eacbc = 0;
  }
  if (*(int *)0x4eacc0 != 0) {
    ((void (*)(int))0x81910)(*(int *)0x4eacc0);
    *(int *)0x4eacc0 = 0;
  }
  ((void (*)(void))0x1c0cf0)();
  ((void (*)(void))0x1c1dc0)();
  *(uint8_t *)0x4eacc6 = 0;
}

/* 0x1c2890 — saved_game_file_close
 * Source TU confirmed by the __FILE__ assert string
 * "c:\halo\SOURCE\saved games\saved_game_files.c" (lines 0x25b-0x261).
 *
 * Two cdecl stack arguments: [EBP+0x8] is the saved game file record (asserted
 * against the condition string "saved_game_file" and pushed unchanged into
 * file_close at 0x1c2974/0x1c2975, ADD ESP,0x4 at 0x1c297a) and [EBP+0xc] is a
 * packed saved-game-file index.  The three fields read out of it are exactly
 * the ones build_saved_game_file_index (0x1c36f0) packs in, and the assert
 * condition strings name them: bits 8-15 (MOVZX EDI,AH at 0x1c289b) are
 * `memory_unit`, bits 16-27 (SAR EAX,0x10 / AND EAX,0xfff at 0x1c289e) are
 * `n`, and bits 0-3 (AND ESI,0xf at 0x1c28a6) are `type`.
 *
 * The range checks are signed on both ends (TEST/JL then CMP/JL at 0x1c28f6,
 * 0x1c291f and 0x1c2948), giving the literal `(x >= 0) && (x < LIMIT)` form of
 * each assert; the limits are 2 saved game file types, 9 memory units and 100
 * enumerated files.
 *
 * The result is the conjunction at 0x1c297d-0x1c298b: file_close's AL must be
 * non-zero AND `memory_unit` must still be the hard drive (0), otherwise AL is
 * cleared at 0x1c298e.  The second half is only reachable in a build whose
 * assert at line 0x25b does not halt.
 */
bool saved_game_file_close(file_ref_t *saved_game_file,
                           int32_t saved_game_file_index)
{
  int32_t memory_unit;
  int32_t n;
  int32_t type;

  memory_unit = (saved_game_file_index >> 8) & 0xff;
  n = (saved_game_file_index >> 16) & 0xfff;
  type = saved_game_file_index & 0xf;

  if (memory_unit != 0) {
    display_assert("memory_unit==_memory_unit_hard_drive",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x25b,
                   true);
    system_exit(-1);
  }

  if (saved_game_file == NULL) {
    display_assert("saved_game_file",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x25e,
                   true);
    system_exit(-1);
  }

  if (type < 0 || type >= 2) {
    display_assert("(type >= 0) && (type < NUMBER_OF_SAVED_GAME_FILE_TYPES)",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x25f,
                   true);
    system_exit(-1);
  }

  if (memory_unit < 0 || memory_unit >= 9) {
    display_assert(
      "(memory_unit >= 0) && (memory_unit < NUMBER_OF_MEMORY_UNITS)",
      "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x260, true);
    system_exit(-1);
  }

  if (n < 0 || n >= 100) {
    display_assert(
      "(n >= 0) && (n < "
      "MAXIMUM_ENUMERATED_SAVED_GAME_FILES_ANY_TYPE_PER_MEMORY_UNIT)",
      "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x261, true);
    system_exit(-1);
  }

  return file_close(saved_game_file) && memory_unit == 0;
}

/* Extract the type bits from a saved game file index. */
unsigned short saved_game_file_get_type(int saved_game_file_index)
{
  return (unsigned short)(saved_game_file_index & 0xf);
}

/* Notify the saved-game file system that the set of attached memory units
 * changed.  Sets the byte flag at 0x4eacc7 (also set to 1 by
 * saved_game_files_initialize); its consumers are not established here. */
void saved_game_files_notify_memory_units_changed(void)
{
  *(uint8_t *)0x4eacc7 = 1;
}

/* 0x1c29c0 — build the first "untitled" display name that is not already taken
 * on disk.  The caller's buffer arrives in [EBP+8] and is asserted non-NULL
 * with the reason string "display_name" at line 0x2c1 (PUSH 0x2c1 at
 * 0x1c29d3), so the parameter is named after that assert.
 *
 * The name template comes from string index 2 of the 'ustr' tag
 * "ui\saved_game_file_strings" (PUSH 0x75737472 at 0x1c29f6); when the tag is
 * not loaded the routine only reports the error and leaves the buffer empty
 * (MOV word ptr [ESI],0x0 at 0x1c29fb happens before the lookup).
 *
 * Two zero-initialized locals are set up for the existence probe: an 8-byte
 * ASCII buffer at EBP-8 holding wide_to_ascii of the wide string whose pointer
 * lives in the dword at 0x32eb94 (MOV EDX,[0x32eb94] at 0x1c2a14 loads the
 * VALUE, so the global is a wchar_t *), and a 0x100-byte buffer at EBP-0x108
 * (REP STOSD of 0x3f dwords plus the STOSW/STOSB tail at 0x1c2a39-0x1c2a48).
 *
 * 0x1d2f22 is unnamed in kb.json.  Its ABI is read straight off this call
 * site: six stack arguments are pushed at 0x1c2a71-0x1c2a85 and NO stack
 * cleanup follows the CALL at 0x1c2a8f (the loop back-edge at 0x1c2aa0 re-
 * enters with the same ESP), so it is __stdcall, and its EAX is tested at
 * 0x1c2a94, so it returns an int.  A non-zero result ends the search; zero
 * means the candidate name is in use and the counter advances.  The probe
 * argument meanings beyond the buffers are unproven, hence param_3/param_4.
 *
 * The counter is EBX, seeded by XOR EBX,EBX at 0x1c2a51; the value formatted
 * into the name is EBX+1 (LEA EDI,[EBX+1] at 0x1c2a56, pushed as
 * unicode_sprintf's variadic argument at 0x1c2a59) and EBX only takes that
 * value after a zero probe result (MOV EBX,EDI at 0x1c2a98).  Exhausting the
 * 999 candidates (CMP EBX,0x3e7 at 0x1c2aa3) reports the error and clears the
 * buffer again. */
void saved_game_file_get_useable_untitled_profile_name(wchar_t *display_name)
{
  int string_list_tag;

  if (display_name == NULL) {
    display_assert("display_name",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x2c1,
                   1);
    system_exit(-1);
  }

  display_name[0] = L'\0';
  string_list_tag =
    tag_loaded(0x75737472 /* 'ustr' */, "ui\\saved_game_file_strings");
  if (string_list_tag != -1) {
    const wchar_t *root_name = *(const wchar_t **)0x32eb94;
    char root_path[8] = "";
    char save_game_directory[0x100] = "";
    int index;
    int next_index;

    wide_to_ascii(root_name, root_path, 8);

    index = 0;
    do {
      next_index = index + 1;
      unicode_sprintf(display_name, 0x7f,
                      (const wchar_t *)FUN_0019d420(string_list_tag, 2),
                      next_index);
      display_name[0x7f] = L'\0';
      if (FUN_001d2f22(root_path, display_name, 3, 0, save_game_directory,
                       0x100) != 0)
        break;
      index = next_index;
    } while (index < 999);

    if (index == 999) {
      error(2, "%d untitled saved games! clean up your hard drive!!", 999);
      display_name[0] = L'\0';
    }
  } else {
    error(2, "unicode string lis tag '%s' not loaded",
          "ui\\saved_game_file_strings");
  }
}

/* Acquire the saved-game file system mutex.  The mutex handle lives in the
 * dword at 0x4eacbc (initialized by saved_game_files_initialize via 0x817e0);
 * the disassembly loads its VALUE into EAX and passes that as take_mutex's
 * `mutex_reference`.  Timeout is the immediate 0x36ee80 = 3600000 ms.
 * The boolean result is discarded (no test after the call at 0x1c2afb). */
bool saved_game_files_take_mutex(void)
{
  return take_mutex(*(int **)0x4eacbc, 3600000);
}

/* Release the saved-game file system mutex.  Same handle dword at 0x4eacbc as
 * saved_game_files_take_mutex: the disassembly loads its VALUE into EAX
 * (MOV EAX,[0x4eacbc]) and passes that as release_mutex's `mutex_reference`
 * (PUSH EAX; CALL 0x818d0; POP ECX).  No return value. */
void saved_game_files_release_mutex(void)
{
  release_mutex(*(int **)0x4eacbc);
}

/* 0x1c2b20 — two file-system health checks over the saved-game root drive.
 *
 * The drive is named by the wide string whose pointer lives in the dword at
 * 0x32eb94 (MOV ECX,[0x32eb94] at 0x1c2b32 loads the VALUE, so the global is
 * a wchar_t *, same as in saved_game_file_get_useable_untitled_profile_name);
 * it is converted into the 8-byte ASCII buffer at EBP-0x10 before each check.
 *
 * Check 1 (0x1c2b45-0x1c2b72): wide_to_ascii's return is pushed straight into
 * XGetDiskFreeSpaceEx as the directory name, the three out-pointers being the
 * 8-byte slots at EBP-0x8 (free bytes available), EBP-0x20 (total bytes) and
 * EBP-0x18 (total free bytes) pushed at 0x1c2b2d-0x1c2b3b.  The free-space
 * test is a 64-bit unsigned compare against 0x2800000 (TEST high / JA / JC /
 * CMP low,0x2800000 / JNC at 0x1c2b57-0x1c2b67), i.e. "less than 40 MB free"
 * returns 1.  A failed query (EAX == 0 at 0x1c2b53) skips the size test.
 *
 * Check 2 (0x1c2b73-0x1c2bed): walk the saved games on that drive with
 * XFindFirstSaveGame / XFindNextSaveGame into the find-data block at
 * EBP-0x364, counting from 1 (MOV ESI,1 at 0x1c2b9c) and stopping at 100
 * (CMP ESI,0x64).  The counter is incremented before each XFindNextSaveGame
 * (INC ESI at 0x1c2bb0), and the loop continues only while that call returns
 * exactly 1 (CMP AL,1 at 0x1c2bb6).  Hitting the limit returns 2; anything
 * else — including an invalid find handle (CMP EDI,-1 at 0x1c2b99) — returns
 * the zeroed EBX (XOR EBX,EBX at 0x1c2b43, MOV AX,BX at 0x1c2be6).
 *
 * The meaning of the three result codes is not proven by this function, so
 * they are left as the literal values the binary returns. */
int16_t saved_game_perform_file_system_checks(void)
{
  char root_path[8];
  uint64_t free_bytes_available;
  uint64_t total_bytes;
  uint64_t total_free_bytes;
  char find_data[XGAME_FIND_DATA_SIZE];
  int find_handle;
  uint32_t count;
  int16_t result;

  result = 0;
  if (GetDiskFreeSpaceExA(
        wide_to_ascii(*(const wchar_t **)0x32eb94, root_path, 8),
        &free_bytes_available, &total_bytes, &total_free_bytes) != 0 &&
      free_bytes_available < 0x2800000)
    return 1;

  find_handle = XFindFirstSaveGame(
    wide_to_ascii(*(const wchar_t **)0x32eb94, root_path, 8), find_data);
  count = 1;
  if (find_handle != -1) {
    do {
      if (count >= 100)
        break;
      count++;
    } while (XFindNextSaveGame(find_handle, find_data) == true);

    if (XFindClose(find_handle) == 0)
      error(2, "XFindClose() failed");

    if (count >= 100)
      result = 2;
  }

  return result;
}

/* 0x1c2bf0 — probe whether a saved-game display name is still unused.
 *
 * A NULL name, or one whose first wide character is already NUL (CMP word ptr
 * [EAX],0x0 at 0x1c2c03), returns the zeroed BL (XOR BL,BL at 0x1c2bfd) with
 * no probe at all.
 *
 * Otherwise the same six-argument __stdcall probe used by
 * saved_game_file_get_useable_untitled_profile_name is issued: the arguments
 * are pushed at 0x1c2c09-0x1c2c2e and no stack cleanup follows the CALL at
 * 0x1c2c2f (MOV ESP,EBP restores the frame), so 0x1d2f22 cleans its own six
 * dwords.  The drive string is wide_to_ascii of the wide string whose pointer
 * lives in the dword at 0x32eb94 (MOV EAX,[0x32eb94] at 0x1c2c1a loads the
 * VALUE) converted into the 8-byte buffer at EBP-0x8; the ADD ESP,0xc at
 * 0x1c2c2b cleans only wide_to_ascii's three cdecl arguments and its EAX
 * return is pushed straight through as the probe's first argument.  Neither
 * the 8-byte buffer nor the 0x100-byte buffer at EBP-0x108 is pre-cleared
 * here (no REP STOSD, unlike 0x1c29xx), and the 0x100-byte buffer is
 * write-only to this function.
 *
 * TEST EAX,EAX / MOV AL,1 / JNZ at 0x1c2c34-0x1c2c38 returns true exactly
 * when the probe result is non-zero, matching the zero-means-name-in-use
 * sense already read off the 0x1c2a8f call site. */
char saved_game_file_name_unique(const wchar_t *name)
{
  char save_game_directory[0x100];
  char root_path[8];
  char result;

  result = 0;
  if (name != NULL && name[0] != L'\0') {
    if (FUN_001d2f22(wide_to_ascii(*(const wchar_t **)0x32eb94, root_path, 8),
                     name, 3, 0, save_game_directory, 0x100) != 0)
      result = 1;
  }

  return result;
}

/* 0x1c2c50 — write the last used player-profile directory for player 1 to
 * "z:\lastprof.txt".  Structural twin of
 * saved_game_file_remember_last_used_multiplayer_map (0x1c2fb0): the file
 * reference is created from the path (0x1c2c8f), then file_create (0x1c2ca2)
 * and file_open with flags 2 (PUSH 0x2 at 0x1c2cb4), then a fixed 0x100-byte
 * file_write of the caller's buffer — the length is the constant 0x100 pushed
 * at 0x1c2cca, not a strlen.  The buffer is [EBP+0x8], held in ESI from
 * 0x1c2c5a and pushed as the last file_write argument at 0x1c2cc3.  Assert
 * line number 0x41a is the immediate pushed at 0x1c2c63.  Both error() calls
 * pass severity 2 and the same path string; the create/open failure path at
 * 0x1c2d04 reports "failed to open" and skips file_close entirely. */
void saved_game_file_remember_player1_last_used_profile_directory(
  const char *directory_path)
{
  file_ref_t info;

  if (directory_path == NULL) {
    display_assert("directory_path",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x41a,
                   1);
    system_exit(-1);
  }

  if (file_reference_create_from_path(&info, "z:\\lastprof.txt", 0) != NULL &&
      file_create(&info) && file_open(&info, 2)) {
    if (!file_write(&info, 0x100, directory_path))
      error(2, "failed to write to '%s'", "z:\\lastprof.txt");
    file_close(&info);
    return;
  }

  error(2, "failed to open '%s'", "z:\\lastprof.txt");
}

/* Read the last used player-profile directory for player 1 out of
 * "z:\lastprof.txt".  The caller supplies a 0x100-byte buffer; the read is a
 * single 0x100-byte file_read and the final byte is always forced to NUL
 * (MOV byte ptr [ESI+0xff],0 on both exits).  The return value is the
 * file_read result (BL, zero-initialized at 0x1c2d2e), so every failure path
 * returns false. */
bool saved_game_file_retrieve_player1_last_used_profile_directory(
  char *directory_path)
{
  file_ref_t info;
  bool result = false;

  if (directory_path == NULL) {
    display_assert("directory_path",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x434,
                   1);
    system_exit(-1);
  }

  if (file_reference_create_from_path(&info, "z:\\lastprof.txt", 0) != NULL &&
      file_open(&info, 1)) {
    result = file_read(&info, 0x100, directory_path);
    if (!result)
      error(2, "failed to read from '%s'", "z:\\lastprof.txt");
    file_close(&info);
    directory_path[0xff] = 0;
    return result;
  }

  error(2, "failed to open '%s'", "z:\\lastprof.txt");
  directory_path[0xff] = 0;
  return result;
}

/* 0x1c2e00 — write the last used multiplayer game-variant directory to
 * "z:\lastmpvr.txt".  Structural twin of
 * saved_game_file_remember_player1_last_used_profile_directory (0x1c2c50):
 * file_reference_create_from_path (0x1c2e3f), file_create (0x1c2e52),
 * file_open with flags 2 (PUSH 0x2 at 0x1c2e64), then a fixed 0x100-byte
 * file_write of the caller's buffer — the length is the constant 0x100 pushed
 * at 0x1c2e7a, not a strlen.  The buffer is [EBP+0x8], held in ESI from
 * 0x1c2e0a and pushed as the last file_write argument at 0x1c2e73.  Assert
 * line number 0x44e is the immediate pushed at 0x1c2e13.  Both error() calls
 * pass severity 2 and the same path string; the create/open failure path at
 * 0x1c2eb4 reports "failed to open" and skips file_close entirely. */
void saved_game_file_remember_last_used_multiplayer_variant_directory(
  const char *directory_path)
{
  file_ref_t info;

  if (directory_path == NULL) {
    display_assert("directory_path",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x44e,
                   1);
    system_exit(-1);
  }

  if (file_reference_create_from_path(&info, "z:\\lastmpvr.txt", 0) != NULL &&
      file_create(&info) && file_open(&info, 2)) {
    if (!file_write(&info, 0x100, directory_path))
      error(2, "failed to write to '%s'", "z:\\lastmpvr.txt");
    file_close(&info);
    return;
  }

  error(2, "failed to open '%s'", "z:\\lastmpvr.txt");
}

/* Read the last used multiplayer game-variant directory out of
 * "z:\lastmpvr.txt".  Structurally identical to the player-profile retrieve
 * above: 0x100-byte file_read into the caller's buffer, final byte forced to
 * NUL on both exits (MOV byte ptr [ESI+0xff],0 at 0x1c2f71 and 0x1c2f94), and
 * the return value is BL (zero-initialized by XOR BL,BL at 0x1c2ede, set from
 * the file_read result at 0x1c2f45), so every failure path returns false.
 * Assert line number 0x468 is the immediate pushed at 0x1c2ee6. */
bool saved_game_file_retrieve_last_used_multiplayer_variant_directory(
  char *directory_path)
{
  file_ref_t info;
  bool result = false;

  if (directory_path == NULL) {
    display_assert("directory_path",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x468,
                   1);
    system_exit(-1);
  }

  if (file_reference_create_from_path(&info, "z:\\lastmpvr.txt", 0) != NULL &&
      file_open(&info, 1)) {
    result = file_read(&info, 0x100, directory_path);
    if (!result)
      error(2, "failed to read from '%s'", "z:\\lastmpvr.txt");
    file_close(&info);
    directory_path[0xff] = 0;
    return result;
  }

  error(2, "failed to open '%s'", "z:\\lastmpvr.txt");
  directory_path[0xff] = 0;
  return result;
}

/* Write the last used multiplayer map name to "z:\lastmpmp.txt".  The file is
 * created (file_create at 0x1c3002) then opened with flags 2 (write) and a
 * fixed 0x100-byte file_write of the caller's buffer at 0x1c3030 — the source
 * length is the constant 0x100, not a strlen.  Assert line number 0x4b7 is the
 * immediate pushed at 0x1c2fc3.  Both error() calls pass severity 2 and the
 * same path string; the create/open/prepare failure path at 0x1c3064 reports
 * "failed to open" and skips file_close entirely. */
void saved_game_file_remember_last_used_multiplayer_map(const char *map_name)
{
  file_ref_t info;

  if (map_name == NULL) {
    display_assert("map_name",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x4b7,
                   1);
    system_exit(-1);
  }

  if (file_reference_create_from_path(&info, "z:\\lastmpmp.txt", 0) != NULL &&
      file_create(&info) && file_open(&info, 2)) {
    if (!file_write(&info, 0x100, map_name))
      error(2, "failed to write to '%s'", "z:\\lastmpmp.txt");
    file_close(&info);
    return;
  }

  error(2, "failed to open '%s'", "z:\\lastmpmp.txt");
}

/* Read the last used multiplayer map name out of "z:\lastmpmp.txt".  Mirrors
 * the variant-directory retrieve above: 0x100-byte file_read into the caller's
 * buffer, final byte forced to NUL on both exits (MOV byte ptr [ESI+0xff],0 at
 * 0x1c3121 and 0x1c3144), and the return value is BL (zero-initialized by XOR
 * BL,BL at 0x1c308e, set from the file_read result by MOV BL,AL at 0x1c30f5),
 * so every failure path returns false.  Assert line number 0x4d1 is the
 * immediate pushed at 0x1c3096.  Unlike the remember path there is no
 * file_create; open flags are 1 (read) at 0x1c30d4. */
bool saved_game_file_retrieve_last_used_multiplayer_map(char *map_name)
{
  file_ref_t info;
  bool result = false;

  if (map_name == NULL) {
    display_assert("map_name",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x4d1,
                   1);
    system_exit(-1);
  }

  if (file_reference_create_from_path(&info, "z:\\lastmpmp.txt", 0) != NULL &&
      file_open(&info, 1)) {
    result = file_read(&info, 0x100, map_name);
    if (!result)
      error(2, "failed to read from '%s'", "z:\\lastmpmp.txt");
    file_close(&info);
    map_name[0xff] = 0;
    return result;
  }

  error(2, "failed to open '%s'", "z:\\lastmpmp.txt");
  map_name[0xff] = 0;
  return result;
}

/* 0x1c3160 — sign a saved-game buffer with the Xbox signature API.
 *
 * Three cdecl stack params: the buffer at EBP+8 (the assert condition string
 * is literally "buffer"), a word-sized length read with MOVZX at 0x1c319a
 * from EBP+0xc, and an output block at EBP+0x10 handed to the End call.  The
 * signature block's size and layout are not established by this function.
 *
 * All three XAPI entry points are __stdcall: no stack cleanup follows the
 * CALLs at 0x1c318e / 0x1c31a1 / 0x1c31be.  Begin takes a single flags word
 * (PUSH 0) and returns the handle in EAX, which is rejected only on the
 * INVALID_HANDLE_VALUE compare CMP ESI,-1 at 0x1c3195; Update and End return
 * a status tested with TEST EAX,EAX.  The names come from the error strings
 * reported at each site.  Note the Update failure is only reported — the End
 * call is still made on the same handle. */
void saved_game_file_generate_checksum(const void *buffer, unsigned short size,
                                       void *signature)
{
  int signature_handle;

  if (buffer == NULL) {
    display_assert(
      "buffer", "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x4eb, 1);
    system_exit(-1);
  }

  signature_handle = XCalculateSignatureBegin(0);
  if (signature_handle != -1) {
    if (XCalculateSignatureUpdate(signature_handle, buffer, size) != 0)
      error(2, "XCalculateSignatureUpdate() failed");
    if (XCalculateSignatureEnd(signature_handle, signature) != 0)
      error(2, "XCalculateSignatureEnd() failed");
    return;
  }

  /* The begin-failure report is the out-of-line tail block at 0x1c31da, which
   * the JZ at 0x1c3198 jumps forward to; written last here to keep that
   * block order. */
  error(2, "XCalculateSignatureBegin() failed");
}

/* Begin enumerating the saved game files on a memory unit.  `memory_unit`
 * arrives in AX (MOV SI,AX at 0x1c3251) and is asserted to be the hard drive
 * (0) before anything else happens.  It then indexes the mapfile-path table at
 * 0x32eb98 (MOVZX ESI,SI / MOV EAX,[ESI*4+0x32eb98] at 0x1c32a2), whose entry
 * 0 is "z:\saved\hdmu.map".  The zero-extended unit is also the %d argument of
 * the failure message (PUSH ESI at 0x1c32f6), which the decompiler renders as
 * a literal 0.
 *
 * Globals: 0x4eabb0 is the saved_game_files_globals file reference cleared by
 * saved_game_files_initialize; 0x4eacc4 is a word set to 0 on success and to
 * 0xffff on failure; 0x4eacc8 is the enumeration_in_progress flag named by the
 * assert at line 0x66f. */
void enumerate_saved_game_files_start(int16_t memory_unit)
{
  uint16_t unit;

  if (memory_unit != 0) {
    display_assert("memory_unit==_memory_unit_hard_drive",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x66b,
                   1);
    system_exit(-1);
  }

  if (*(uint8_t *)0x4eacc8 != 0) {
    display_assert("!saved_game_files_globals.enumeration_in_progress",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x66f,
                   1);
    system_exit(-1);
  }

  unit = (uint16_t)memory_unit;

  if (file_reference_create_from_path(
        (file_ref_t *)0x4eabb0, ((const char **)0x32eb98)[unit], 0) != 0) {
    if (file_create((file_ref_t *)0x4eabb0)) {
      if (file_open((file_ref_t *)0x4eabb0, 2)) {
        *(uint16_t *)0x4eacc4 = 0;
        *(uint8_t *)0x4eacc8 = 1;
        return;
      }
    }
  }

  error(2, "failed to create/open memory unit mapfile for memory unit #%d",
        unit);
  *(uint16_t *)0x4eacc4 = 0xffff;
}

/* End the memory-unit enumeration opened by enumerate_saved_game_files_start.
 * `memory_unit` arrives in SI: it is read before any write (TEST SI,SI at
 * 0x1c3320) and zero-extended as the %d argument of the close-failure message
 * (MOVZX EAX,SI / PUSH EAX at 0x1c337f).  The second assert is the inverse of
 * the start-side one — here the enumeration flag at 0x4eacc8 must be set.
 * The tail sets the same word/flag pair start uses (0x4eacc4 = 0xffff,
 * 0x4eacc8 = 0) and returns true unconditionally (MOV AL,1 at 0x1c33a2). */
bool enumerate_saved_game_files_end(int16_t memory_unit)
{
  if (memory_unit != 0) {
    display_assert("memory_unit==_memory_unit_hard_drive",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x685,
                   1);
    system_exit(-1);
  }

  if (*(uint8_t *)0x4eacc8 == 0) {
    display_assert("saved_game_files_globals.enumeration_in_progress",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x687,
                   1);
    system_exit(-1);
  }

  if (!file_close((file_ref_t *)0x4eabb0)) {
    error(2, "failed to close memory unit mapfile for memory unit #%d",
          (uint16_t)memory_unit);
  }

  *(uint16_t *)0x4eacc4 = 0xffff;
  *(uint8_t *)0x4eacc8 = 0;
  return 1;
}

/* Write the next profile record into the memory-unit mapfile opened by
 * enumerate_saved_game_files_start.  `record` arrives in ESI and is used
 * unchecked as both the destination of the index write and the file_write
 * source buffer.  A single combined assert (line 0x741) guards both
 * preconditions: the enumeration_in_progress byte at 0x4eacc8 must be set
 * (TEST AL,AL / JZ at 0x1c33b7 short-circuits before the second check), and
 * the running count word at 0x4eacc4 must not be negative (TEST AX,AX / JGE
 * at 0x1c33bf). That same AX value is reused (not reread) for the 100-record
 * cap compare (CMP AX,0x64 at 0x1c33ea), the word store into record+0x202
 * (MOV word ptr [ESI+0x202],AX at 0x1c33f6), and is then incremented in
 * place (INC word ptr [0x4eacc4] at 0x1c33fd) -- the record gets the
 * pre-increment (0-based) index. The success path returns file_write's
 * result unmodified (ADD ESP,0xc / RET with no MOV into AL after the CALL at
 * 0x1c3409); the cap-exceeded path returns false explicitly (XOR AL,AL at
 * 0x1c3421). */
bool enumerate_saved_game_file(void *record)
{
  int16_t index;

  index = *(int16_t *)0x4eacc4;

  if ((*(uint8_t *)0x4eacc8 == 0) || (index < 0)) {
    display_assert(
      "(saved_game_files_globals.enumeration_in_progress) && "
      "(saved_game_files_globals.next_enumerated_profile_index >= 0)",
      "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x741, 1);
    system_exit(-1);
  }

  if (index < 100) {
    *(int16_t *)((char *)record + 0x202) = index;
    (*(int16_t *)0x4eacc4)++;

    return file_write((file_ref_t *)0x4eabb0, 0x206, record);
  }

  error(2, "the maximum number of game files have already been enumerated "
           "(time to clean up your hard drive and/or memory cards)");
  return 0;
}

/* Open the memory-unit mapfile for reading and sanity-check its length.
 * `memory_unit_index` arrives in AX (MOV SI,AX at 0x1c3431) and is asserted to
 * be the hard drive (0); the enumeration flag at 0x4eacc8 must be clear.  The
 * zero-extended unit indexes the same mapfile-path table at 0x32eb98 that
 * enumerate_saved_game_files_start uses, and is the %d argument of both error
 * messages (PUSH ESI at 0x1c34cd/0x1c34e1) — the decompiler renders it as a
 * literal 0.  Unlike the start-side function this only opens (file_open flag
 * 1), it never creates, and it leaves the 0x4eacc4/0x4eacc8 pair alone.  The
 * length check is an unsigned DIV by 0x206 (0x1c34bf) testing the remainder;
 * 0x206 is the mapfile record size.  Returns true whenever the open succeeded
 * (MOV AL,1 at 0x1c34dd), corrupt-length report included. */
__declspec(noinline) bool enumerate_mapfile_start(int16_t memory_unit_index)
{
  uint16_t unit;

  if (memory_unit_index != 0) {
    display_assert("memory_unit_index==_memory_unit_hard_drive",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x75b,
                   1);
    system_exit(-1);
  }

  if (*(uint8_t *)0x4eacc8 != 0) {
    display_assert("!saved_game_files_globals.enumeration_in_progress",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x75f,
                   1);
    system_exit(-1);
  }

  unit = (uint16_t)memory_unit_index;

  if (file_reference_create_from_path(
        (file_ref_t *)0x4eabb0, ((const char **)0x32eb98)[unit], 0) != 0) {
    if (file_open((file_ref_t *)0x4eabb0, 1)) {
      if ((uint32_t)file_get_eof((file_ref_t *)0x4eabb0) % 0x206 != 0) {
        error(2, "memory unit mapfile for memory unit #%d is possibly corrupt",
              unit);
      }
      return 1;
    }
  }

  error(2, "failed to open memory unit mapfile for memory unit #%d", unit);
  return 0;
}

/* Close the memory-unit mapfile opened by enumerate_mapfile_start.
 * `memory_unit_index` arrives in SI: it is read before any write (TEST SI,SI at
 * 0x1c3500) and zero-extended as the %d argument of the failure message
 * (MOVZX EAX,SI / PUSH EAX at 0x1c3588).  Three asserts guard the entry: the
 * unit must be the hard drive (0), the enumeration flag at 0x4eacc8 must be
 * clear, and the unit must be below NUMBER_OF_MEMORY_UNITS (CMP SI,0x9 / JC at
 * 0x1c354e, an unsigned compare).  Unlike enumerate_saved_game_files_end this
 * touches neither 0x4eacc4 nor 0x4eacc8, and it returns the file_close result
 * itself (MOV BL,AL at 0x1c357f / MOV AL,BL at 0x1c359b) rather than a
 * constant. */
__declspec(noinline) bool enumerate_mapfile_end(int16_t memory_unit_index)
{
  bool closed;

  if (memory_unit_index != 0) {
    display_assert("memory_unit_index==_memory_unit_hard_drive",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x77c,
                   1);
    system_exit(-1);
  }

  if (*(uint8_t *)0x4eacc8 != 0) {
    display_assert("!saved_game_files_globals.enumeration_in_progress",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x77e,
                   1);
    system_exit(-1);
  }

  if ((uint16_t)memory_unit_index >= 9) {
    display_assert("memory_unit_index < NUMBER_OF_MEMORY_UNITS",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x77f,
                   1);
    system_exit(-1);
  }

  closed = file_close((file_ref_t *)0x4eabb0);
  if (!closed) {
    error(2, "enumerate_mapfile_end FAILED on memory unit #%d",
          (uint16_t)memory_unit_index);
  }

  return closed;
}

/* Read the next 0x206-byte record out of the memory-unit mapfile opened by
 * enumerate_mapfile_start.  `file` arrives in ESI: it is read before any write
 * (TEST ESI,ESI at 0x1c35c9) and pushed as the destination buffer of file_read
 * (PUSH ESI at 0x1c35ed).  Two asserts guard the entry — the enumeration flag
 * at 0x4eacc8 must be clear and `file` must be non-NULL.  The file_read result
 * is RETURNED, not discarded: the CALL at 0x1c35f8 is followed by a plain ADD
 * ESP,0xc / RET with no MOV into AL, and the caller at 0x1c3969
 * (saved_game_file_find_profile_index_for_directory_path) does TEST AL,AL / JZ
 * on it, so EAX is load-bearing.  The layout of the 0x206-byte record is not
 * established here. */
__declspec(noinline) bool enumerate_saved_game_file_from_mapfile(void *file)
{
  if (*(uint8_t *)0x4eacc8 != 0) {
    display_assert("!saved_game_files_globals.enumeration_in_progress",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x793,
                   1);
    system_exit(-1);
  }

  if (file == NULL) {
    display_assert("file", "c:\\halo\\SOURCE\\saved games\\saved_game_files.c",
                   0x794, 1);
    system_exit(-1);
  }

  return file_read((file_ref_t *)0x4eabb0, 0x206, file);
}

/* Return how many 0x206-byte records the memory-unit mapfile holds.
 * `memory_unit_index` arrives in SI: it is read before any write (TEST SI,SI at
 * 0x1c3614), compared unsigned against NUMBER_OF_MEMORY_UNITS (CMP SI,0x9 / JC
 * at 0x1c3662) and zero-extended to index the mapfile-path table at 0x32eb98
 * (MOVZX EAX,SI / MOV ECX,[EAX*4+0x32eb98] at 0x1c3688).  The same three entry
 * asserts as enumerate_mapfile_end guard it.  Unlike enumerate_mapfile_start
 * this never opens the file — it only builds the file reference and asks for
 * the size (file_get_size at 0x1c36af writes the dword at EBP-4), then divides
 * it by the 0x206 record size; the MUL 0xfd08e551 / SHR EDX,9 pair at
 * 0x1c36bb-0x1c36c5 is the unsigned magic form of `size / 0x206` (verified
 * exact over the whole 32-bit range).  Both failure paths return 0 (XOR EAX,EAX
 * at 0x1c36cc). */
__declspec(noinline) uint32_t
count_enumerated_profiles_in_mapfile(int16_t memory_unit_index)
{
  uint32_t size;

  if (memory_unit_index != 0) {
    display_assert("memory_unit_index==_memory_unit_hard_drive",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x860,
                   1);
    system_exit(-1);
  }

  if (*(uint8_t *)0x4eacc8 != 0) {
    display_assert("!saved_game_files_globals.enumeration_in_progress",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x862,
                   1);
    system_exit(-1);
  }

  if ((uint16_t)memory_unit_index >= 9) {
    display_assert("memory_unit_index < NUMBER_OF_MEMORY_UNITS",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x863,
                   1);
    system_exit(-1);
  }

  if (file_reference_create_from_path(
        (file_ref_t *)0x4eabb0,
        ((const char **)0x32eb98)[(uint16_t)memory_unit_index], 0) != 0) {
    if (file_get_size((file_ref_t *)0x4eabb0, &size)) {
      return size / 0x206;
    }
  }

  return 0;
}

/* Pack a saved game file index out of three register-passed fields plus two
 * stack-passed flags.  The field widths come straight from the masks at
 * 0x1c3713/0x1c371b/0x1c3723: EAX keeps 12 bits, ECX 8 bits, EDX 4 bits, and
 * the two SHL EAX,8 steps at 0x1c3718/0x1c3726 place them at bits 16-27, 8-15
 * and 0-3.  The low nibble is the file type — saved_game_file_get_type
 * (0x1c29a0) reads exactly `index & 0xf` back out.  Both flags are compared
 * against the constant 1 held in CL (MOV CL,1 at 0x1c372e), not merely tested
 * for non-zero, and set bits 30 and 31.  The meaning of the 12-bit and 8-bit
 * fields and of the two flags is not established by this function. */
__declspec(noinline) uint32_t build_saved_game_file_index(int32_t param_eax,
                                                          int32_t param_ecx,
                                                          int32_t file_type,
                                                          bool param_1,
                                                          bool param_2)
{
  uint32_t index;

  index = (uint32_t)((((param_eax & 0xfff) << 8) | (param_ecx & 0xff)) << 8) |
          (uint32_t)(file_type & 0xf);
  if (param_1 == 1)
    index |= 0x40000000;
  if (param_2 == 1)
    index |= 0x80000000;
  return index;
}

/* Initialize saved game files: create directory structure on the
 * Xbox hard drive, allocate file handles, and load profile data. */
void saved_game_files_initialize(void)
{
  if (!ensure_dir((const char *)0x2bae58))
    error(2, "failed to find/create '%s' directory", "z:\\saved");
  if (!ensure_dir((const char *)0x2bae14))
    error(2, "failed to find/create '%s' directory",
          "z:\\saved\\player_profiles");
  if (!ensure_dir((const char *)0x2bade8))
    error(2, "failed to find/create '%s' directory",
          "z:\\saved\\player_profiles\\default_profile");
  if (!ensure_dir((const char *)0x2badd4))
    error(2, "failed to find/create '%s' directory", "z:\\saved\\playlists");
  if (!ensure_dir((const char *)0x2badb0))
    error(2, "failed to find/create '%s' directory",
          "z:\\saved\\playlists\\default_playlist");
  if (!ensure_dir((const char *)0x2bad9c))
    error(2, "failed to find/create '%s' directory", "z:\\saved\\recordings");
  if (!ensure_dir((const char *)0x2bad78))
    error(2, "failed to find/create '%s' directory",
          "z:\\saved\\recordings\\last_recording");

  csmemset((void *)0x4eabb0, 0, 0x11c);
  *(uint8_t *)0x4eacc7 = 1;
  *(int *)0x4eacbc = 0;
  *(int *)0x4eacc0 = 0;

  if (((char (*)(void *))0x817e0)((void *)0x4eacbc)) {
    if (((char (*)(void *))0x817e0)((void *)0x4eacc0)) {
      *(uint8_t *)0x4eacc6 = 1;
      ((void (*)(void))0x1c1ba0)();
      ((void (*)(void))0x1c1da0)();
      return;
    }
  }

  *(uint8_t *)0x4eacc6 = 0;
  display_assert("failed to initialize saved game files",
                 "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0xbf, 1);
  system_exit(-1);
  ((void (*)(void))0x1c1ba0)();
  ((void (*)(void))0x1c1da0)();
}

/* 0x1c38d0 — scan the memory-unit mapfile for the entry whose directory path
 * matches `directory_path` and return that entry's packed saved-game file
 * index, or -1 when nothing matches.
 *
 * `directory_path` is the stack dword at EBP+8 (asserted non-NULL, condition
 * string "directory_path", line 0x486) and `file_type` is the stack dword at
 * EBP+0xc, read as a WORD (MOV SI,word ptr [EBP+0xc] at 0x1c3972) and
 * sign-extended into EDX for build_saved_game_file_index (MOVSX EDX,SI at
 * 0x1c39a6).
 *
 * The result lives in EDI/[EBP-4], preset to -1 by OR EDI,0xffffffff at
 * 0x1c38de and reloaded from [EBP-4] at 0x1c39bf; all three RETs move EDI into
 * EAX, so every failure path returns -1.  Both mutex handles are the VALUES of
 * the dwords at 0x4eacbc (save game files) and 0x4eacc0 (mapfile), the same
 * form saved_game_files_take_mutex uses, with the 0x36ee80 = 3600000 ms
 * timeout.  The mapfile mutex failure path releases only the first mutex.
 *
 * The 0x206-byte record buffer is the base of the 0x214-byte frame
 * (LEA ESI,[EBP-0x214] at 0x1c3963), so the decompiler's local_18/local_10
 * are fields inside it: EBP-0x14 is record+0x200 (the int16 file type compared
 * against the parameter) and the dwords at EBP-0x10 / EBP-0xf are the bytes at
 * record+0x204 / record+0x205 pushed as build_saved_game_file_index's two bool
 * flags (PUSH EDX from EBP-0xf first, so it is the LAST argument).  The path
 * field is record+0 — the same 0x206-byte layout whose display name at +0x100
 * saved_game_file_get_display_name reads.
 *
 * __strnicmp's arguments are PUSH EDX (the csstrlen result cached at EBP-8),
 * PUSH EAX (the record buffer), PUSH ECX (directory_path): first push is the
 * last argument, so it is __strnicmp(directory_path, record, length).
 *
 * The three register-arg callees take zero (XOR ESI,ESI at 0x1c3949 and
 * 0x1c39b8, XOR EAX,EAX at 0x1c3952) — the hard-drive memory unit.  The record
 * read is the loop's break condition (TEST AL,AL / JZ at 0x1c396e), and the
 * loop count from count_enumerated_profiles_in_mapfile is compared signed
 * (JLE at 0x1c3961, JL at 0x1c399a). */
int saved_game_file_find_profile_index_for_directory_path(char *directory_path,
                                                          int file_type)
{
  char record[0x206];
  int32_t length;
  int32_t result;
  int32_t count;
  int32_t i;

  result = -1;

  if (directory_path == NULL) {
    display_assert("directory_path",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x486,
                   1);
    system_exit(-1);
  }

  length = csstrlen(directory_path);

  if (take_mutex(*(int **)0x4eacbc, 3600000)) {
    if (take_mutex(*(int **)0x4eacc0, 3600000)) {
      count = (int32_t)count_enumerated_profiles_in_mapfile(0);

      if (enumerate_mapfile_start(0)) {
        for (i = 0; i < count; i++) {
          if (!enumerate_saved_game_file_from_mapfile(record))
            break;

          if (*(int16_t *)(record + 0x200) == (int16_t)file_type &&
              __strnicmp(directory_path, record, (unsigned int)length) == 0) {
            result = (int32_t)build_saved_game_file_index(
              i, 0, (int32_t)(int16_t)file_type, (bool)record[0x204],
              (bool)record[0x205]);
            break;
          }
        }

        enumerate_mapfile_end(0);
      }

      release_mutex(*(int **)0x4eacc0);
      release_mutex(*(int **)0x4eacbc);
    } else {
      error(2, "failed to take mapfile mutex");
      release_mutex(*(int **)0x4eacbc);
    }
  } else {
    error(2, "failed to take save game files mutex");
  }

  return result;
}

/* 0x1c3a30 — enumerate_default_playlist_profiles: walk the default playlist
 * files written by FUN_001c2120 (0x1c2120) and hand each one to
 * enumerate_saved_game_file, verifying its checksum on the way.
 *
 * The profile count comes from
 * playlist_profile_number_of_default_profiles_on_disk (ESI, cached at EBP-0x8)
 * and the localized name string list tag from tag_loaded('ustr',
 * "ui\default_multiplayer_game_setting_names") (EDI, cached at EBP-0x4 and
 * reloaded at 0x1c3be8 because ESI/EDI are clobbered inside the loop).  A -1
 * tag index reports and returns; a count <= 0 returns silently (TEST SI,SI /
 * JLE at 0x1c3a68).  All three RETs do MOV AX,BX, so the return value is the
 * 16-bit loop counter — the number of indices walked — which is why the early
 * failure exit at 0x1c3c00 returns a partial count.  The loop bound is a 16-bit
 * signed compare (CMP BX,word ptr [EBP-0x8] at 0x1c3bec) and the snprintf index
 * argument is sign-extended from BX (MOVSX EAX,BX at 0x1c3a7a), hence the
 * explicit int16_t casts.
 *
 * The record built here is the same 0x206-byte mapfile entry
 * saved_game_file_find_profile_index_for_directory_path reads back: path at
 * +0, wide display name at +0x100 (0x7f wide chars, terminator at +0x1fe),
 * the int16 file type at +0x200 (1 = playlist profile) and the two bool flags
 * at +0x204 / +0x205.  The +0x205 flag is set only when the checksum matches,
 * so a corrupt file is still enumerated, just flagged.  The MOV byte/REP
 * STOSD(0x81)/STOSB triple at 0x1c3acb zeroes exactly 0x206 bytes and is the
 * MSVC expansion of a `= ""` initializer on the block-scoped record.
 *
 * The checksum path reads the whole 0x200-byte file into a scratch block,
 * signs its leading 0x68 bytes into a separate 0x14-byte frame slot (EBP-0x1c)
 * and compares that against the stored signature at block+0x68 (LEA ECX,
 * [EBP-0x5c8] at 0x1c3b6c, i.e. block+0x68).  file_close runs on both the read
 * success and read failure paths, but not when file_open failed.
 *
 * enumerate_saved_game_file (0x1c33b0) takes the record in ESI (LEA ESI,
 * [EBP-0x224] at 0x1c3bd9, no other use of ESI at that point) and returns its
 * result in AL (TEST AL,AL at 0x1c3be4). */
int16_t enumerate_default_playlist_profiles(void)
{
  char block[0x200];
  char path[0x100];
  file_ref_t info;
  char signature[0x14];
  wchar_t *display_name;
  int16_t count;
  int tag_index;
  int i;

  count = playlist_profile_number_of_default_profiles_on_disk();
  tag_index =
    tag_loaded(0x75737472, "ui\\default_multiplayer_game_setting_names");
  i = 0;

  if (tag_index == -1) {
    error(2, "failed to enumerate default playlist files because their name "
             "string list tag was not loaded");
  } else if (count > 0) {
    do {
      display_name = (wchar_t *)FUN_0019d420(tag_index, i);
      snprintf(path, 0xff,
               "z:\\saved\\playlists\\default_playlist\\%02d\\blam.lst",
               (int)(int16_t)i);

      if (file_reference_create_from_path(&info, path, 0) != NULL &&
          file_exists(&info)) {
        char record[0x206] = "";

        csstrncpy(record, path, 0xff);
        record[0xff] = '\0';
        ustrncpy((wchar_t *)(record + 0x100), display_name, 0x7f);
        *(wchar_t *)(record + 0x1fe) = L'\0';
        *(int16_t *)(record + 0x200) = 1;
        record[0x204] = 1;

        if (file_open(&info, 1)) {
          if (file_read(&info, 0x200, block)) {
            saved_game_file_generate_checksum(block, 0x68, signature);

            if (csmemcmp(signature, block + 0x68, 0x14) == 0) {
              record[0x205] = 1;
            } else {
              error(2, "checksum validation failed for '%s'", record);
            }
          } else {
            error(2,
                  "failed to read saved game variant file to verify checksum");
          }

          if (!file_close(&info)) {
            error(2, "failed to close saved game variant file after verifying "
                     "checksum");
          }
        } else {
          error(2, "failed to open saved game variant file to verify checksum");
        }

        if (!enumerate_saved_game_file(record)) {
          error(2, "failed to enumerate default playlist file '%s'", path);
          return (int16_t)i;
        }
      }

      i++;
    } while ((int16_t)i < count);
  }

  return (int16_t)i;
}

/* 0x1c3c40 — enumerate_default_player_profiles: the player-profile twin of
 * enumerate_default_playlist_profiles above.  Same record shape, same
 * checksum-then-enumerate sequence, four differences, all read off the
 * disassembly:
 *
 *   - the localized names come from tag_loaded('ustr',
 *     "ui\shell\strings\default_player_profile_names") (PUSH 0x2898d0 / PUSH
 *     0x75737472 at 0x1c3c4c) and the tag index is cached at EBP-0x4 and
 *     reloaded at 0x1c3de5 because EDI is clobbered inside the loop;
 *   - the loop bound is the literal 2 (CMP BX,0x2 / JL at 0x1c3de9), not a
 *     disk count, so there is no pre-loop count test and the tag failure exit
 *     returns immediately;
 *   - the file type stored at record+0x200 is 0, not 1 (XOR EAX,EAX then MOV
 *     word ptr [EBP-0x22],AX / [EBP-0x20],AX at 0x1c3d1a — the same zero also
 *     terminates the wide name at record+0x1fe);
 *   - the checksum covers the leading 0x30 bytes of the 0x200-byte block
 *     (PUSH 0x30 at 0x1c3d5f) and the stored signature sits at block+0x30
 *     (LEA ECX,[EBP-0x5fc] at 0x1c3d69 against the block base EBP-0x62c).
 *
 * All three RETs do MOV AX,BX, so the return value is the 16-bit loop counter;
 * the enumerate failure exit at 0x1c3dfd returns the partial count.  The
 * MOV byte / REP STOSD(0x81) / STOSB triple at 0x1c3cca zeroes exactly 0x206
 * bytes — the MSVC expansion of a `= ""` initializer on the block-scoped
 * record.  enumerate_saved_game_file takes that record in ESI (LEA ESI,
 * [EBP-0x220] at 0x1c3dd6, ESI dead at that point) and answers in AL (TEST
 * AL,AL at 0x1c3de1).  file_close runs on both the read-success and
 * read-failure paths, but not when file_open failed. */
int16_t enumerate_default_player_profiles(void)
{
  char block[0x200];
  char path[0x100];
  file_ref_t info;
  char signature[0x14];
  wchar_t *display_name;
  int tag_index;
  int i;

  tag_index =
    tag_loaded(0x75737472, "ui\\shell\\strings\\default_player_profile_names");
  i = 0;

  if (tag_index == -1) {
    error(2, "failed to enumerate default player profile files because their "
             "name string list tag was not loaded");
    return (int16_t)i;
  }

  do {
    display_name = (wchar_t *)FUN_0019d420(tag_index, i);
    snprintf(path, 0xff,
             "z:\\saved\\player_profiles\\default_profile\\%02d.sav",
             (int)(int16_t)i);

    if (file_reference_create_from_path(&info, path, 0) != NULL &&
        file_exists(&info)) {
      char record[0x206] = "";

      csstrncpy(record, path, 0xff);
      record[0xff] = '\0';
      ustrncpy((wchar_t *)(record + 0x100), display_name, 0x7f);
      *(wchar_t *)(record + 0x1fe) = L'\0';
      *(int16_t *)(record + 0x200) = 0;
      record[0x204] = 1;

      if (file_open(&info, 1)) {
        if (file_read(&info, 0x200, block)) {
          saved_game_file_generate_checksum(block, 0x30, signature);

          if (csmemcmp(signature, block + 0x30, 0x14) == 0) {
            record[0x205] = 1;
          } else {
            error(2, "checksum validation failed for '%s'", record);
          }
        } else {
          error(2, "failed to read saved game player profile file to verify "
                   "checksum");
        }

        if (!file_close(&info)) {
          error(2, "failed to close saved game player profile file after "
                   "verifying checksum");
        }
      } else {
        error(2, "failed to open saved game player profile file to verify "
                 "checksum");
      }

      if (!enumerate_saved_game_file(record)) {
        error(2, "failed to enumerate default player profile file '%s'", path);
        return (int16_t)i;
      }
    }

    i++;
  } while ((int16_t)i < 2);

  return (int16_t)i;
}

/* Overwrite the nth 0x206-byte record of the memory-unit mapfile in place.
 * `memory_unit_index` arrives in AX (MOV SI,AX at 0x1c4038); the entry index is
 * the 16-bit stack slot at [EBP+8] (MOVZX EBX,word ptr at 0x1c4126) and the
 * source record is the pointer at [EBP+0xc].  The same three entry asserts as
 * enumerate_mapfile_end guard it (lines 0x7e2/0x7e4/0x7e5), then the mapfile
 * mutex at 0x4eacc0 is taken with the 0x36ee80 = 3600000 ms timeout; failing
 * that reports and returns false with the flag still in BL (MOV AL,BL at
 * 0x1c4203).  The zero-extended unit indexes the mapfile-path table at
 * 0x32eb98 and is the trailing %d of every message; the file is opened with
 * flag 2 (write).  file_get_eof's result is checked for a 0x206 remainder
 * (unsigned DIV at 0x1c4124) purely to report corruption, and the record is
 * written only when offset + 0x206 <= size (LEA EDX,[ESI+0x206] / CMP EDX,EAX /
 * JA at 0x1c414c).  The out-of-range path leaves the result flag at its
 * initial 0 without re-storing it; the write-failure path stores 0 explicitly
 * (MOV byte ptr [EBP-1],0 at 0x1c418a).  A failed close clears the flag again.
 * The result byte at [EBP-1] is returned in AL at 0x1c41e6. */
bool set_nth_entry_in_mapfile(int16_t memory_unit_index, int16_t entry_index,
                              const void *entry)
{
  bool result;
  uint16_t unit;
  uint16_t index;
  uint32_t size;
  uint32_t offset;

  result = 0;

  if (memory_unit_index != 0) {
    display_assert("memory_unit_index==_memory_unit_hard_drive",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x7e2,
                   1);
    system_exit(-1);
  }

  if (*(uint8_t *)0x4eacc8 != 0) {
    display_assert("!saved_game_files_globals.enumeration_in_progress",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x7e4,
                   1);
    system_exit(-1);
  }

  if ((uint16_t)memory_unit_index >= 9 || entry == NULL) {
    display_assert(
      "(memory_unit_index < NUMBER_OF_MEMORY_UNITS) && (file != NULL)",
      "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x7e5, 1);
    system_exit(-1);
  }

  if (!take_mutex(*(int **)0x4eacc0, 3600000)) {
    error(2, "failed to take mapfile mutex");
    return result;
  }

  unit = (uint16_t)memory_unit_index;

  if (file_reference_create_from_path(
        (file_ref_t *)0x4eabb0, ((const char **)0x32eb98)[unit], 0) != 0 &&
      file_open((file_ref_t *)0x4eabb0, 2)) {
    size = (uint32_t)file_get_eof((file_ref_t *)0x4eabb0);
    index = (uint16_t)entry_index;
    offset = (uint32_t)index * 0x206;

    if (size % 0x206 != 0) {
      error(2, "memory unit mapfile for memory unit #%d is possibly corrupt",
            unit);
    }

    if (offset + 0x206 <= size) {
      if (file_set_position((file_ref_t *)0x4eabb0, (int32_t)offset) &&
          file_write((file_ref_t *)0x4eabb0, 0x206, entry)) {
        result = 1;
      } else {
        result = 0;
        error(2, "failed to update entry #%d from memory unit mapfile (#%d)",
              index, unit);
      }
    } else {
      error(2, "invalid profile index (#%d) into memory unit #%d specified",
            index, unit);
    }

    if (!file_close((file_ref_t *)0x4eabb0)) {
      error(2, "failed to close memory unit mapfile for memory unit #%d", unit);
      result = 0;
    }
  } else {
    error(2, "failed to open memory unit mapfile for memory unit #%d", unit);
  }

  release_mutex(*(int **)0x4eacc0);
  return result;
}

/* Append a new 0x206-byte record to the end of the memory-unit mapfile and
 * report the index it landed at.  `memory_unit_index` arrives in AX (MOV SI,AX
 * at 0x1c4216); `file` is the record pointer at [EBP+8] and `profile_index` the
 * out-parameter at [EBP+0xc].  The three entry asserts are lines 0x821/0x823/
 * 0x824, the third covering all of index/file/profile_index (TEST at
 * 0x1c426c-0x1c427e).  The mapfile mutex at 0x4eacc0 is taken with the same
 * 0x36ee80 = 3600000 ms timeout; failing it reports and returns the flag still
 * in BL (MOV AL,BL at 0x1c43e7).  The zero-extended unit indexes the
 * mapfile-path table at 0x32eb98 and is the trailing %d of every message that
 * has one.  file_get_eof is divided by 0x206 (unsigned DIV at 0x1c4306): the
 * quotient is the new entry index and the append offset (IMUL ESI,0x206 at
 * 0x1c430f, computed before the limit test), the remainder only reports
 * corruption.  The 100-profile limit is a SIGNED compare (CMP EDI,0x64 / JGE at
 * 0x1c4315) and its message takes no argument.  On a successful write the flag
 * is stored first (MOV byte ptr [EBP-1],1 at 0x1c435d) and then the count is
 * written through the out-pointer.  A failed close clears the flag again.  The
 * result byte at [EBP-1] is returned in AL at 0x1c43cb. */
bool append_entry_to_mapfile(int16_t memory_unit_index, const void *file,
                             uint32_t *profile_index)
{
  bool result;
  uint16_t unit;
  uint32_t size;
  uint32_t count;
  uint32_t offset;

  result = 0;

  if (memory_unit_index != 0) {
    display_assert("memory_unit_index==_memory_unit_hard_drive",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x821,
                   1);
    system_exit(-1);
  }

  if (*(uint8_t *)0x4eacc8 != 0) {
    display_assert("!saved_game_files_globals.enumeration_in_progress",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x823,
                   1);
    system_exit(-1);
  }

  if ((uint16_t)memory_unit_index >= 9 || file == NULL ||
      profile_index == NULL) {
    display_assert(
      "(memory_unit_index < NUMBER_OF_MEMORY_UNITS) && (file != NULL) && "
      "(profile_index != NULL)",
      "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x824, 1);
    system_exit(-1);
  }

  if (!take_mutex(*(int **)0x4eacc0, 3600000)) {
    error(2, "failed to take mapfile mutex");
    return result;
  }

  unit = (uint16_t)memory_unit_index;

  if (file_reference_create_from_path(
        (file_ref_t *)0x4eabb0, ((const char **)0x32eb98)[unit], 0) != 0 &&
      file_open((file_ref_t *)0x4eabb0, 2)) {
    size = (uint32_t)file_get_eof((file_ref_t *)0x4eabb0);
    count = size / 0x206;
    offset = count * 0x206;

    if ((int32_t)count < 100) {
      if (size % 0x206 != 0) {
        error(2, "memory unit mapfile for memory unit #%d is possibly corrupt",
              unit);
      }

      if (file_set_position((file_ref_t *)0x4eabb0, (int32_t)offset) &&
          file_write((file_ref_t *)0x4eabb0, 0x206, file)) {
        result = 1;
        *profile_index = count;
      } else {
        result = 0;
        error(2, "failed to append entry to memory unit mapfile (#%d)", unit);
      }
    } else {
      error(2, "can't add new entry to memory unit mapfile because the maximum "
               "number of profiles have already been added");
    }

    if (!file_close((file_ref_t *)0x4eabb0)) {
      error(2, "failed to close memory unit mapfile for memory unit #%d", unit);
      result = 0;
    }
  } else {
    error(2, "failed to open memory unit mapfile for memory unit #%d", unit);
  }

  release_mutex(*(int **)0x4eacc0);
  return result;
}

/* Unpack a saved game file index and return the display name of the entry it
 * names.  The two fields read here are the ones build_saved_game_file_index
 * (0x1c36f0) packs: MOVZX ESI,AH at 0x1c460d takes the 8-bit memory unit at
 * bits 8-15, SAR EAX,0x10 / AND EAX,0xfff at 0x1c4610 takes the 12-bit file
 * index at bits 16-27.  Both are range-checked (TEST/JL plus CMP 9 and CMP
 * 0x64) before use; either failure reports through error() and leaves the
 * cleared name buffer.  get_nth_entry_in_mapfile takes the memory unit in AX
 * (MOV SI,AX at 0x1c3e48) and the entry index in DI (MOVZX EDI,DI at 0x1c3f37,
 * with EDI never written in the callee) and fills the 0x206-byte stack entry
 * (PUSH 0x206 / file_read at 0x1c3f7c).  The display name lives at +0x100
 * inside that entry — the ustrncpy source is EBP-0x108 against a buffer based
 * at EBP-0x208.  Both exits return the static name buffer at 0x4eaab0 (MOV
 * EAX,0x4eaab0 at 0x1c468f/0x1c46a9). */
wchar_t *saved_game_file_get_display_name(int32_t saved_game_file_index)
{
  char entry[0x208];
  int16_t memory_unit_index;
  int32_t file_index;

  memory_unit_index = (int16_t)((saved_game_file_index >> 8) & 0xff);
  file_index = (saved_game_file_index >> 0x10) & 0xfff;

  if (memory_unit_index != 0) {
    display_assert("memory_unit==_memory_unit_hard_drive",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 299, 1);
    system_exit(-1);
  }

  *(wchar_t *)0x4eaab0 = 0;

  if (memory_unit_index >= 0 && memory_unit_index < 9 && file_index >= 0 &&
      file_index < 100) {
    if (get_nth_entry_in_mapfile(memory_unit_index, file_index, entry)) {
      ustrncpy((wchar_t *)0x4eaab0, (wchar_t *)(entry + 0x100), 0x7f);
      *(wchar_t *)0x4eabae = 0;
    }
  } else {
    error(2, "invalid saved game file index");
  }

  return (wchar_t *)0x4eaab0;
}
