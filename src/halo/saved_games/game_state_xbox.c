/*
 * game_state_xbox.c — Xbox-specific game state buffer and file management.
 *
 * Corresponds to game_state_xbox.obj (game_state_xbox.c in the original
 * source tree at c:\halo\SOURCE\saved games\game_state_xbox.c).
 */

#include "xbox.h"

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

/* 0x1c0330
 * Close the Xbox game-state file handle. Asserts that the file is marked open,
 * then calls the close-file routine and clears the open flag.
 */
void xbox_game_state_close_file(void)
{
  assert_halt(*(char *)0x4ea9bc);
  ((bool (*)(int))0x1cf900)(*(int *)0x4ea9c0);
  *(char *)0x4ea9bc = 0;
}
