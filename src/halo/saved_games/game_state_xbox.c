/*
 * game_state_xbox.c — Xbox-specific game state buffer and file management.
 *
 * Corresponds to game_state_xbox.obj (game_state_xbox.c in the original
 * source tree at c:\halo\SOURCE\saved games\game_state_xbox.c).
 */

#ifdef XDK_BUILD
void __stdcall MmFreeContiguousMemory(void *BaseAddress);
#else
#include "xbox.h"
#endif

/* Xbox kernel file I/O wrappers (stdcall) */
typedef bool(__stdcall *close_handle_fn)(int handle);
typedef int(__stdcall *set_file_pointer_fn)(int handle, int distance_to_move,
                                            int *distance_high,
                                            uint32_t move_method);
typedef bool(__stdcall *read_file_fn)(int handle, void *buffer,
                                      uint32_t number_of_bytes_to_read,
                                      int *number_of_bytes_read,
                                      void *overlapped);
typedef bool(__stdcall *write_file_fn)(int handle, void *buffer,
                                       uint32_t number_of_bytes_to_write,
                                       int *number_of_bytes_written,
                                       void *overlapped);
typedef bool(__stdcall *delete_file_fn)(const char *path);
typedef int(__stdcall *create_file_fn)(
  const char *path, uint32_t desired_access, uint32_t share_mode,
  uint32_t security_attrs, uint32_t creation_disposition,
  uint32_t flags_and_attrs, uint32_t template_file);
typedef bool(__stdcall *set_end_of_file_fn)(int handle);
typedef int (*open_save_file_fn)(int param_1);
typedef char (*get_save_path_fn)(short index, void *out_path);
typedef int (*get_last_error_fn)(void);
typedef void (*crc_begin_fn)(uint32_t *checksum);

#define XCloseHandle CloseHandle
#define XSetFilePointer \
  ((set_file_pointer_fn)0x1d1610) /* hazard-ok: fnptr-conv */
#define XReadFile ((read_file_fn)0x1d13c9) /* hazard-ok: fnptr-conv */
#define XWriteFile ((write_file_fn)0x1d14b6) /* hazard-ok: fnptr-conv */
#define XDeleteFile ((delete_file_fn)0x1d0ff9) /* hazard-ok: fnptr-conv */
#define XCreateFile ((create_file_fn)0x1d1d85) /* hazard-ok: fnptr-conv */
#define XSetEndOfFile                                     \
  ((set_end_of_file_fn)0x1d158c) /* hazard-ok: fnptr-conv \
                                  */
#define xapi_GetLastError ((get_last_error_fn)0x1d2240)
#define xbox_game_state_open_file ((open_save_file_fn)0x1c0780)
#define xbox_saved_game_get_path ((get_save_path_fn)0xe0bf0)
#define crc_checksum_begin ((crc_begin_fn)0x1190b0)

/* xbox_game_state_globals layout (at 0x4ea9b0):
 *   +0x00 (0x4ea9b0): char  buffer_allocated
 *   +0x04 (0x4ea9b4): void* buffer
 *   +0x08 (0x4ea9b8): int   buffer_size
 *   +0x0C (0x4ea9bc): char  file_open
 *   +0x0D (0x4ea9bd): char  file_written
 *   +0x10 (0x4ea9c0): int   file_handle
 */

/* 0x1c0220
 * Release the contiguous physical memory buffer allocated for Xbox game-state
 * saves. Asserts that the buffer is marked allocated before freeing, then
 * clears the flag. The globals at 0x4ea9b0 (buffer_allocated flag) and
 * 0x4ea9b4 (buffer pointer) belong to xbox_game_state_globals.
 */
void xbox_game_state_dispose_buffer(void)
{
  assert_halt(*(char *)0x4ea9b0);
  MmFreeContiguousMemory(*(void **)0x4ea9b4);
  *(char *)0x4ea9b0 = 0;
}

/* 0x1c0260
 * Create or open the Xbox save-game file. Asserts the state buffer is
 * already allocated and no file is currently open, then creates/opens
 * "z:\savegame.bin" with generic read/write access (OPEN_ALWAYS), seeks
 * past the reserved region, and truncates the file there to pre-allocate
 * space. Sets file_open on success; otherwise halts with an error message
 * including the last Win32 error code.
 */
void game_state_create_or_open_file(void)
{
  assert_halt_msg_at("xbox_game_state_globals.buffer_allocated",
                     "c:\\halo\\SOURCE\\saved games\\game_state_xbox.c", 0x56,
                     *(char *)0x4ea9b0);
  assert_halt_msg_at("!xbox_game_state_globals.file_open",
                     "c:\\halo\\SOURCE\\saved games\\game_state_xbox.c", 0x57,
                     !*(char *)0x4ea9bc);

  *(int *)0x4ea9c0 =
    XCreateFile("z:\\savegame.bin", 0xc0000000, 0, 0, 4, 0x28000000, 0);
  if (*(int *)0x4ea9c0 != -1) {
    if (XSetFilePointer(*(int *)0x4ea9c0, 0x380000, NULL, 0) != (uint32_t)-1) {
      if (XSetEndOfFile(*(int *)0x4ea9c0) != 0) {
        *(char *)0x4ea9bc = 1;
        return;
      }
    }
  }

  display_assert(csprintf((char *)0x5ab100,
                          "couldn't open or create saved game file (#%d)",
                          xapi_GetLastError()),
                 "c:\\halo\\SOURCE\\saved games\\game_state_xbox.c", 0x61, 1);
  system_exit(-1);
}

/* 0x1c0330
 * Close the Xbox game-state file handle. Asserts that the file is marked open,
 * then calls the close-file routine and clears the open flag.
 */
void xbox_game_state_close_file(void)
{
  assert_halt(*(char *)0x4ea9bc);
  XCloseHandle(*(int *)0x4ea9c0);
  *(char *)0x4ea9bc = 0;
}

/* 0x1c0370
 * Write the game-state buffer to the save file. Asserts that both the buffer
 * is allocated and the file is open, seeks to the beginning, then writes the
 * entire buffer. Sets the file_written flag on success.
 */
char game_state_write_to_file(void)
{
  int bytes_written;

  assert_halt(*(char *)0x4ea9b0); /* buffer_allocated */
  assert_halt(*(char *)0x4ea9bc); /* file_open */

  if (XSetFilePointer(*(int *)0x4ea9c0, 0, NULL, 0) != -1) {
    if (XWriteFile(*(int *)0x4ea9c0, *(void **)0x4ea9b4, *(uint32_t *)0x4ea9b8,
                   &bytes_written, NULL) &&
        bytes_written == *(int *)0x4ea9b8) {
      *(char *)0x4ea9bd = 1; /* file_written */
      return 1;
    }
  }

  display_assert(csprintf((char *)0x5ab100,
                          "couldn't write saved game file (#%d)",
                          xapi_GetLastError()),
                 "c:\\halo\\SOURCE\\saved games\\game_state_xbox.c", 0x84, 1);
  system_exit(-1);
  return 0;
}

/* 0x1c0450
 * Read the game-state buffer back from the save file. Asserts that the
 * buffer is allocated, the file is open, and the file is either valid for
 * read or the recover-saved-games hack is active, then seeks to the
 * beginning and reads buffer_size bytes into the buffer. Returns 1 on
 * success; on failure, halts with an error message including the last
 * Win32 error code.
 */
char game_state_read_from_file(void)
{
  int bytes_read;

  assert_halt_msg_at("xbox_game_state_globals.buffer_allocated",
                     "c:\\halo\\SOURCE\\saved games\\game_state_xbox.c", 0x90,
                     *(char *)0x4ea9b0);
  assert_halt_msg_at("xbox_game_state_globals.file_open",
                     "c:\\halo\\SOURCE\\saved games\\game_state_xbox.c", 0x91,
                     *(char *)0x4ea9bc);
  assert_halt_msg_at(
    "xbox_game_state_globals.file_valid_for_read || recover_saved_games_hack",
    "c:\\halo\\SOURCE\\saved games\\game_state_xbox.c", 0x92,
    *(char *)0x4ea9bd || *(char *)0x5054e8);

  if (XSetFilePointer(*(int *)0x4ea9c0, 0, NULL, 0) != (uint32_t)-1) {
    if (XReadFile(*(int *)0x4ea9c0, *(void **)0x4ea9b4, *(uint32_t *)0x4ea9b8,
                  &bytes_read, NULL) &&
        bytes_read == *(int *)0x4ea9b8) {
      return 1;
    }
  }

  display_assert(csprintf((char *)0x5ab100,
                          "couldn't read saved game file (#%d)",
                          xapi_GetLastError()),
                 "c:\\halo\\SOURCE\\saved games\\game_state_xbox.c", 0x9c, 1);
  system_exit(-1);
  return 0;
}

/* 0x1c0600
 * Read the header of a core save file back from persistent storage. Builds
 * the path "d:\core\<name>" with sprintf, opens it read-only (OPEN_EXISTING),
 * reads header_size bytes into the caller's buffer, and always closes the
 * handle -- even when CreateFileA failed and the handle is -1, matching the
 * original (CloseHandle is unconditional in the disassembly). Returns 1 only
 * if the file opened and the full header_size bytes were read.
 */
char game_state_read_core_header(const char *name, void *header,
                                 int header_size)
{
  char path[0x400];
  int bytes_read;
  int file_handle;
  char result;

  result = 0;

  crt_sprintf(path, "d:\\core\\%s", name);

  file_handle = XCreateFile(path, 0x80000000, 0, 0, 3, 0x80, 0);
  if (file_handle != -1) {
    if (XReadFile(file_handle, header, (uint32_t)header_size, &bytes_read,
                  NULL) &&
        bytes_read == header_size) {
      result = 1;
    }
  }

  XCloseHandle(file_handle);

  return result;
}

/* 0x1c0680
 * Read the body of a core save file back from persistent storage. Builds
 * the path "d:\core\<name>" with sprintf, opens it read-only (OPEN_EXISTING),
 * and reads size bytes into the caller's buffer. Unlike
 * game_state_read_core_header, the handle is closed only on the success
 * path -- on any failure (open failed, read failed, or short read) this
 * halts via display_assert + system_exit(-1), which never returns, so
 * CloseHandle is never reached in the disassembly's error path. */
void game_state_read_core(const char *name, void *buffer, int size)
{
  char path[0x400];
  int bytes_read;
  int file_handle;

  crt_sprintf(path, "d:\\core\\%s", name);

  file_handle = XCreateFile(path, 0x80000000, 0, 0, 3, 0x80, 0);
  if (file_handle != -1) {
    if (XReadFile(file_handle, buffer, (uint32_t)size, &bytes_read, NULL) &&
        bytes_read == size) {
      XCloseHandle(file_handle);
      return;
    }
  }

  display_assert("game state has been corrupted (thank you, come again)",
                 "c:\\halo\\SOURCE\\saved games\\game_state_xbox.c", 0xe2, 1);
  system_exit(-1);
}

/* 0x1c0720 — return the Xbox save-game filename.
 * Used as the leaf file name when constructing the save path. */
const char *FUN_001c0720(void)
{
  return "savegame.bin";
}

/* 0x1c0750
 * Delete the local player's save game file. Gets the profile directory
 * path and, if valid, deletes the file at that path. */
void FUN_001c0750(void)
{
  char path_buffer[256];

  if (xbox_saved_game_get_path(0, path_buffer)) {
    XDeleteFile(path_buffer);
  }
}

/* 0x1c0780
 * Open (creating if needed) the "savegame.bin" persistent-storage file.
 * With a NULL path argument the local player's profile directory is used
 * (returning NONE when it is unavailable); otherwise the caller-supplied
 * directory string is copied. If the file is not already the expected
 * 0x380000 bytes it is grown: a zeroed 16KB block is written, the file
 * pointer is moved to 0x380000 and the file truncated there. Any failure
 * halts with an assert. The reference reuses the parameter's home slot as
 * the WriteFile bytes-written output; the lift mirrors that.
 */
int game_state_open_persistent_storage(int param_1)
{
  char scratch[0x4000];
  char path[256];
  int file_handle;

  if (param_1 == 0) {
    if (!player_ui_get_path_to_local_player_profile_directory(0, path)) {
      goto fail;
    }
    player_ui_get_path_to_local_player_profile_directory(0, path);
  } else {
    goto copy_caller_path;
  }
have_path:
  csstrcat(path, "savegame.bin");

  file_handle = CreateFileA(path, 0xc0000000, 0, 0, 4, 0, 0);
  if (file_handle != -1) {
    if (GetFileSize(file_handle, (unsigned int *)0) != 0x380000) {
      csmemset(scratch, 0, 0x4000);
      if (!(WriteFile(file_handle, scratch, 0x4000, (uint32_t *)&param_1,
                      (void *)0) &&
            param_1 == 0x4000 &&
            SetFilePointer(file_handle, 0x380000, (int *)0, 0) != 0xffffffff &&
            SetEndOfFile(file_handle))) {
        display_assert(
          csprintf((char *)0x5ab100,
                   "couldn't resize persistent storage \"%s\"", path),
          "c:\\halo\\SOURCE\\saved games\\game_state_xbox.c", 0x1eb, 1);
        system_exit(-1);
        /* The reference continues past the (non-returning) assert: delete the
         * save file, close the handle and report failure. */
        FUN_001c0750();
        CloseHandle(file_handle);
        return -1;
      }
    }
    return file_handle;
  }

  display_assert(csprintf((char *)0x5ab100,
                          "couldn't open or create persistent storage \"%s\"",
                          path),
                 "c:\\halo\\SOURCE\\saved games\\game_state_xbox.c", 0x1f2, 1);
  system_exit(-1);
  goto fail;

copy_caller_path:
  csstrcpy(path, (const char *)param_1);
  goto have_path;

fail:
  return -1;
}

/* 0x1c0910
 * Read and verify a saved game from persistent storage. Reads header first,
 * then checksums the remaining data in 128KB chunks. Returns 1 on success.
 *
 * param_1 (header):      destination buffer for the header portion
 * param_2 (scratch):     pointer to a uint32_t holding the expected checksum;
 *                         zeroed before CRC computation and restored on
 * mismatch param_3 (header_size): byte count for the header read param_4
 * (buffer_size): total byte count (header + body) to checksum param_5 (flags):
 * optional output byte; set to 1 if checksum mismatch with a non-zero expected
 * checksum
 */
char game_state_read_header_from_persistent_storage(void *header,
                                                    uint32_t *scratch,
                                                    int header_size,
                                                    int buffer_size,
                                                    char *flags)
{
  char scratch_buffer[0x20000]; /* Reference frame: _chkstk(0x20114). */
  char path_buffer[0x100];

  int file_handle;
  volatile char result;
  int bytes_transferred;
  uint32_t checksum;
  uint32_t saved_checksum;
  int remaining;
  int chunk;

  file_handle = xbox_game_state_open_file(0);
  result = 0;

  if (flags != NULL) {
    *flags = 0;
  }

  if (file_handle == -1) {
    return result;
  }

  /* Seek to beginning */
  if (XSetFilePointer(file_handle, 0, NULL, 0) == -1) {
    goto read_error;
  }

  /* Read the header */
  if (!XReadFile(file_handle, header, (uint32_t)header_size, &bytes_transferred,
                 NULL) ||
      bytes_transferred != header_size) {
    goto read_error;
  }

  /* Save the expected checksum and prepare for computation */
  saved_checksum = *scratch;
  crc_checksum_begin(&checksum);
  *scratch = 0;
  crc_checksum_buffer(&checksum, header, header_size);

  /* Read and checksum remaining data in 128KB chunks */
  remaining = buffer_size - header_size;
  while (remaining > 0) {
    chunk = remaining;
    if ((unsigned int)remaining > 0x20000) {
      chunk = 0x20000;
    }

    if (XReadFile(file_handle, scratch_buffer, (uint32_t)chunk,
                  &bytes_transferred, NULL) &&
        bytes_transferred == chunk) {
      crc_checksum_buffer(&checksum, scratch_buffer, chunk);
    }

    sound_idle(); /* sound_pump / idle tick */
    remaining -= chunk;
  }

  /* Verify checksum */
  if (checksum == saved_checksum) {
    result = 1;
    XCloseHandle(file_handle);
    return result;
  }

  /* Checksum mismatch */
  if (flags != NULL && saved_checksum != 0) {
    *flags = 1;
  }
  error(2, "checksum failed on persistent storage");
  XCloseHandle(file_handle);
  return result;

read_error:
  display_assert(csprintf((char *)0x5ab100,
                          "couldn't read header from persistent storage (#%d)",
                          xapi_GetLastError()),
                 "c:\\halo\\SOURCE\\saved games\\game_state_xbox.c", 0x12a, 1);
  system_exit(-1);

  /* After the fatal assert, attempt to delete the corrupt save file */
  if (xbox_saved_game_get_path(0, path_buffer)) {
    XDeleteFile(path_buffer);
  }
  XCloseHandle(file_handle);
  return result;
}

/* 0x1c0c20
 * Read game-state data from persistent storage into a caller-supplied buffer.
 * Opens the save file, seeks to the beginning, reads `size` bytes into `dst`,
 * and verifies the byte count. On failure, asserts with the last error code,
 * exits, and attempts to delete the corrupt save file.
 */
void FUN_001c0c20(void *dst, int size)
{
  char path_buffer[0x100];
  int bytes_read;

  int file_handle = xbox_game_state_open_file(0);
  if (file_handle == -1) {
    return;
  }

  if (XSetFilePointer(file_handle, 0, NULL, 0) == -1 ||
      !XReadFile(file_handle, dst, size, &bytes_read, NULL) ||
      bytes_read != size) {
    display_assert(
      csprintf((char *)0x5ab100, "failed to read from persistent storage (#%d)",
               xapi_GetLastError()),
      "c:\\halo\\SOURCE\\saved games\\game_state_xbox.c", 0x17f, 1);
    system_exit(-1);

    if (xbox_saved_game_get_path(0, path_buffer)) {
      XDeleteFile(path_buffer);
    }
  }

  XCloseHandle(file_handle);
}

/* 0x1c0cd0
 * Close the file handle obtained from xbox_game_state_open_file for a given
 * parameter. Opens the file, and if valid, closes the resulting handle.
 */
void FUN_001c0cd0(int param_1)
{
  int handle;

  handle = xbox_game_state_open_file(param_1);
  if (handle != -1) {
    XCloseHandle(handle);
  }
}

/* 0x1c0cf0
 * Wait for any in-flight asynchronous player profile writes to complete, then
 * clear the profile write state buffer at 0x4ea9c8 (0x6c bytes).
 */
void FUN_001c0cf0(void)
{
  if (*(int *)0x4eaa2c != 0) {
    error(2, "waiting for asynchronous player profile writes to finish...");
    do {
      /* spin until the async write thread signals completion */
    } while (!thread_is_done(*(void **)0x4eaa2c));
    thread_close(*(void **)0x4eaa2c);
    *(int *)0x4eaa2c = 0;
  }
  csmemset((void *)0x4ea9c8, 0, 0x6c);
}

/* 0x1c0d50
 * Forward to saved_game_files_enumerate_available_to_local_player_index with
 * a constant 0 as the second argument; the other four arguments pass through
 * in order (binary: PUSH [ebp+14], [ebp+10], [ebp+c], 0, [ebp+8]).
 */
void FUN_001c0d50(int param_1, int *param_2, int *param_3, int param_4)
{
  saved_game_files_enumerate_available_to_local_player_index(
    param_1, 0, param_2, param_3, param_4);
}

/* 0x1c0d70
 * Delete a player profile by index. Calls the saved-game file deletion
 * routine and logs an error if it fails.
 */
void FUN_001c0d70(int param_1)
{
  if (param_1 != -1) {
    if (!delete_enumerated_saved_game_file(param_1)) {
      error(2, "player_profile_delete() failed (profile index= #0x%lX)",
            param_1);
    }
  }
}

/* 0x1c0da0
 * Load a 0x30-byte player profile record from a profile file and verify its
 * trailing checksum.
 *
 * kb.json name is player_profile_delete (PDB line-containment); the binary
 * body reads rather than deletes, so the recovered name and the observed
 * behavior disagree — name left as recovered, meaning unproven.
 *
 * Layout evidence (disassembly at 0x1c0da0): frame is SUB ESP,0x320 =
 * 0x200 read buffer at [EBP-0x320] + 0x10C file_ref_t at [EBP-0x120] +
 * 0x14 digest at [EBP-0x14]. The stored signature compared against the
 * computed one is at buffer+0x30 ([EBP-0x2f0]), immediately after the
 * 0x30-byte profile payload.
 *
 * Returns true only on a successful read with a matching checksum
 * (BL=0 at entry, BL=1 only on the copy path, MOV AL,BL at every exit).
 */
bool player_profile_delete(const char *full_path, void *profile)
{
  char buffer[0x200];
  file_ref_t file_info;
  char digest[0x14];
  const char *message;
  bool result;

  assert_halt_at("c:\\halo\\SOURCE\\saved games\\player_profile.c", 0xd8,
                 full_path && profile);

  result = false;
  if (file_reference_create_from_path(&file_info, full_path, false) != NULL &&
      file_open(&file_info, 1)) {
    if (file_read(&file_info, 0x200, buffer)) {
      saved_game_file_generate_checksum(buffer, 0x30, digest);
      if (csmemcmp(digest, buffer + 0x30, 0x14) == 0) {
        csmemcpy(profile, buffer, 0x30);
        result = true;
        file_close(&file_info);
        return result;
      }
      message = "checksum failed on player profile file";
    } else {
      message = "failed to read player profile";
    }
    error(2, message);
    file_close(&file_info);
    return result;
  }
  error(2, "failed to open player profile file");
  return result;
}

/* 0x1c0ed0
 * Returns the fixed constant 0x12 (18). No parameters, no memory access,
 * no side effects. Callers (playlist_profile_initialize_ctf_rules,
 * multiplayer_settings_select_list_update_item, and others) use the result
 * as an immediate value; its semantic meaning (count/id/type) is unproven.
 */
unsigned short FUN_001c0ed0(void)
{
  return 0x12;
}

/* 0x1c1290
 * kb.json previously listed this address as game_state_read_from_persistent_
 * storage(void) — that decl/name does not match the binary. Disassembly and
 * the __FILE__ assert string ("c:\halo\SOURCE\saved games\player_profile.c")
 * show a two-arg register/stack function that zero-initializes a 0x30-byte
 * profile record and stamps default fields, selected by index i in
 * [0, NUMBER_OF_DEFAULT_PROFILES). Renamed and re-signatured to match;
 * "profile" and "i" are taken verbatim from the assert condition string.
 * Immediate caller (FUN_001c19e0, unlifted) and sibling player_profile_new
 * (0x1c18f0, unlifted) are consistent with this being a profile bootstrap
 * helper. Field offsets (0x18, 0x1a, 0x26, 0x28-0x2f within the 0x30-byte
 * record) are raw/unproven — no player_profile struct exists yet, so they
 * are kept as offset writes rather than named struct fields.
 */
void player_profile_set_to_default(void *profile /* @<esi> */, int i)
{
  if ((profile == NULL) || (i < 0) ||
      (i >= 2 /* NUMBER_OF_DEFAULT_PROFILES */)) {
    display_assert(
      "(profile != NULL) && (i>=0) && (i<NUMBER_OF_DEFAULT_PROFILES)",
      "c:\\halo\\SOURCE\\saved games\\player_profile.c", 0x237, true);
    system_exit(-1);
  }

  csmemset(profile, 0, 0x30);

  *(uint16_t *)((char *)profile + 0x18) = 0xffff;
  *(uint8_t *)((char *)profile + 0x2a) = 3;
  *(uint8_t *)((char *)profile + 0x2b) = 0;
  *(uint8_t *)((char *)profile + 0x2d) = 0;
  *(uint8_t *)((char *)profile + 0x2f) = 0;
  *(uint16_t *)((char *)profile + 0x1a) |= (uint16_t)(((i & 0xff) << 8) | 1);
  *(uint8_t *)((char *)profile + 0x2c) = 0;
  *(uint16_t *)((char *)profile + 0x26) = 0;

  if (i != 0) {
    if (i != 1) {
      display_assert("unknown default profile configuration requested",
                     "c:\\halo\\SOURCE\\saved games\\player_profile.c", 0x252,
                     true);
      system_exit(-1);
    }
    *(uint8_t *)((char *)profile + 0x2b) = 1;
  }

  *(uint8_t *)((char *)profile + 0x28) = 0;
  *(uint8_t *)((char *)profile + 0x29) = 0;
}

/* 0x1c15c0
 * Asynchronous player-profile write worker.  The only xref to this address is
 * a DATA reference from player_profile_write (0x1c1b00) at 0x1c1b8b, i.e. the
 * address is handed to thread_new as the thread procedure, which is why the
 * binary ends in RET 0x4 (__stdcall, one dword parameter) and returns a
 * constant 0 in EAX (XOR EAX,EAX at 0x1c1714).
 *
 * `input` points at the request record the caller allocated: dword 0 is the
 * saved-game file index (EDI, loaded at 0x1c160f) and the following 0x30 bytes
 * are the profile payload (ESI after ADD ESI,0x4 at 0x1c161b), used both as
 * the csmemcpy source and as the second argument to
 * synchronize_metadata_display_name_with_profile_name.  No request struct is
 * proven, so the two members are reached by offset.
 *
 * Frame evidence (SUB ESP,0x30c): 0x200 write buffer at [EBP-0x30c] and a
 * 0x10C file_ref_t at [EBP-0x10c].  The checksum is written to buffer+0x30
 * ([EBP-0x2dc]), matching the layout player_profile_delete (0x1c0da0) reads
 * back.  Only the first 0x30 bytes plus the signature are initialized; the
 * remainder of the 0x200 bytes written to the file is whatever the stack
 * held, exactly as in the original.
 *
 * Callee decls corrected from this call site's disassembly:
 *   0x1c2af0 saved_game_files_take_mutex — TEST AL,AL at 0x1c1605 proves it
 *     propagates take_mutex's boolean result, so its decl is bool, not void.
 *   0x1c4850 — PUSH EDI/PUSH EAX + ADD ESP,0x8 + TEST AL,AL: two cdecl args
 *     (file_ref_t out, file index) and a boolean result.  Its recovered name
 *     (enumerate_memory_units_test, PDB line-containment) disagrees with the
 *     observed behavior here ("failed to open player profile file" on false);
 *     the name is left as recovered, its meaning unproven.
 *   0x1c4990 synchronize_metadata_display_name_with_profile_name — PUSH ESI/
 *     PUSH EDI + ADD ESP,0x8 + TEST AL,AL: (file index, profile) and bool.
 */
int __stdcall FUN_001c15c0(void *input)
{
  char buffer[0x200];
  file_ref_t file_info;
  int32_t saved_game_file_index;
  void *profile;
  bool write_failed;

  assert_halt_at("c:\\halo\\SOURCE\\saved games\\player_profile.c", 0x2d7,
                 input);

  error(2, "begin player profile write");
  if (saved_game_files_take_mutex()) {
    saved_game_file_index = *(int32_t *)input;
    write_failed = false;
    profile = (char *)input + 4;
    if (enumerate_memory_units_test(&file_info, saved_game_file_index)) {
      csmemcpy(buffer, profile, 0x30);
      saved_game_file_generate_checksum(buffer, 0x30, buffer + 0x30);
      if (!file_set_position(&file_info, 0) ||
          !file_write(&file_info, 0x200, buffer)) {
        error(2, "failed to write player profile to file");
        write_failed = true;
      }
      if (saved_game_file_close(&file_info, saved_game_file_index) &&
          !synchronize_metadata_display_name_with_profile_name(
            saved_game_file_index, profile)) {
        error(2, "metadata name may not match game display name");
      }
      if (write_failed) {
        delete_enumerated_saved_game_file(saved_game_file_index);
      }
      saved_game_files_release_mutex();
    } else {
      error(2, "failed to open player profile file");
      saved_game_files_release_mutex();
    }
  } else {
    error(2,
          "failed to get saved game files mutex; perhaps another operation is "
          "in progress?");
  }
  error(2, "end player profile write");
  return 0;
}

/* 0x1c1720
 * Create a new player profile saved-game file, stamp it with default profile
 * fields plus the caller-supplied display name, and write the initial 0x200
 * byte record.  Returns the new saved-game file index, or -1 on any failure.
 *
 * Frame evidence (SUB ESP,0x310 at 0x1c1723): 0x10C file_ref_t at
 * [EBP-0x310], 0x200 record buffer at [EBP-0x204] (zero-initialized inline by
 * MOV byte + REP STOSD/STOSW/STOSB at 0x1c1765-0x1c1786, i.e. `= {0}`), and
 * the saved-game file index at [EBP-0x4].  buffer+0x30 receives the checksum,
 * matching the layout player_profile_delete (0x1c0da0) reads back and
 * FUN_001c15c0 (0x1c15c0) writes.
 *
 * Profile field stores (offsets 0x16/0x18/0x1a/0x26/0x28-0x2f inside the
 * 0x30-byte record) mirror player_profile_set_to_default (0x1c1290) but are
 * emitted inline here; no player_profile struct is proven, so they stay raw
 * offset writes.  Offset 0x1a is stored as 0 here, not the packed default
 * set_to_default writes -- meaning unproven.
 *
 * The 10-byte run at record+0x1c is OR'ed with bits 0-3 by the nested loop at
 * 0x1c17f3-0x1c1817; the "### DEBUG unlocking all solo levels" message
 * printed immediately before identifies it as the solo-level unlock bitfield.
 *
 * Callee decl corrected from this call site's disassembly:
 *   0x1c5560 FUN_001c5560 -- PUSH ESI/PUSH EAX/PUSH EBX + ADD ESP,0xc at
 *     0x1c1732-0x1c173e proves three cdecl args, and CMP EDI,-1 on the EAX
 *     result proves an int return; its decl was void(void).
 */
int FUN_001c1720(int a1, wchar_t *name)
{
  file_ref_t file_info;
  int saved_game_file_index;

  saved_game_file_index = FUN_001c5560(0, a1, name);
  if (saved_game_file_index != -1) {
    if (enumerate_memory_units_test(&file_info, saved_game_file_index)) {
      char buffer[0x200] = { 0 };
      int level;
      int bit;
      uint8_t unlock_flags;

      csmemset(buffer, 0, 0x30);
      *(uint16_t *)(buffer + 0x18) = 0xffff;
      *(uint8_t *)(buffer + 0x2a) = 3;
      *(uint8_t *)(buffer + 0x2b) = 0;
      *(uint8_t *)(buffer + 0x2d) = 0;
      *(uint8_t *)(buffer + 0x2f) = 0;
      *(uint8_t *)(buffer + 0x2c) = 0;
      *(uint16_t *)(buffer + 0x26) = 0;
      *(uint8_t *)(buffer + 0x28) = 0;
      *(uint8_t *)(buffer + 0x29) = 0;
      *(uint16_t *)(buffer + 0x1a) = 0;
      ustrncpy((wchar_t *)buffer, name, 0xb);
      *(uint16_t *)(buffer + 0x16) = 0;

      error(2, "### DEBUG unlocking all solo levels for newly created profile");
      for (level = 0; level < 10; level++) {
        unlock_flags = *(uint8_t *)(buffer + 0x1c + level);
        for (bit = 0; bit < 4; bit++) {
          unlock_flags |= (uint8_t)(1 << bit);
        }
        *(uint8_t *)(buffer + 0x1c + level) = unlock_flags;
      }

      saved_game_file_generate_checksum(buffer, 0x30, buffer + 0x30);
      if (file_set_position(&file_info, 0) &&
          file_write(&file_info, 0x200, buffer)) {
        saved_game_file_close(&file_info, saved_game_file_index);
        return saved_game_file_index;
      }

      error(2, "failed to initialize newly created player profile");
      delete_enumerated_saved_game_file(saved_game_file_index);
      saved_game_file_close(&file_info, -1);
      return -1;
    }

    error(2, "failed to open newly created player profile");
    delete_enumerated_saved_game_file(saved_game_file_index);
    return -1;
  }

  error(2, "failed to create new player profile");
  return saved_game_file_index;
}

/* 0x1c1950
 * Fills the 0x10-byte record at param_1: dword 0 is the fixed bit pattern
 * 0x3f800000 (float 1.0f); dwords 1-3 are copied from the 3-dword
 * struct-return of FUN_001c0ee0(param_2). FUN_001c0ee0 is called with a
 * local scratch buffer as its hidden output pointer and echoes that same
 * pointer back in EAX (MSVC struct-return-by-value convention), which is
 * where the 3 dwords are read from. This function likewise echoes param_1
 * in its own return value. Field/record semantics are unproven — kept as
 * raw dwords rather than typed floats/struct fields.
 */
void *FUN_001c1950(void *param_1, int param_2)
{
  uint32_t local_buf[3];
  uint32_t *src;
  uint32_t *volatile dest;
  uint32_t tmp2;

  dest = (uint32_t *)param_1;
  src = (uint32_t *)FUN_001c0ee0(local_buf, param_2);
  dest[0] = 0x3f800000; /* 1.0f */
  tmp2 = src[2];
  dest[1] = src[0];
  dest[2] = src[1];
  dest[3] = tmp2;
  return param_1;
}

/* 0x1c19c0
 * Returns a random value in [0, 0x11] (0-17 inclusive) using the local
 * random seed. Single caller: get_unique_random_color.
 */
int FUN_001c19c0(void)
{
  return (int)seed_random_range(random_math_get_local_seed_address(), 0, 0x11);
}
