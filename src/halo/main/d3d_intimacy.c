/* 0x1cf840: sets bit 0x40000 in the object flags dword (+0x4) of a
 * sound_scenery object (type mask 0x800) and returns true.
 * Evidence: disasm OR ECX,0x40000 / MOV [EAX+4],ECX / MOV AL,1. */
bool sound_scenery_new(int sound_scenery_handle)
{
  char *object;

  object = (char *)object_get_and_verify_type(sound_scenery_handle, 0x800);
  *(unsigned int *)(object + 0x4) |= 0x40000;
  return 1;
}

/* Find the D3D flipcount by probing internal D3D state (0x1cf880).
 * Validates the D3D context: interrupts enabled, vblank callback matches,
 * and vblank count is sane. Returns pointer to the flipcount variable. */
int *d3d_find_flipcount(void)
{
  if (*(int *)0x1fdebc == 1)
    goto callback_check;

  error(2,
        "### WARNING: direct3d context unreadable "
        "(interrupts 0x%08X != 0x00000001)...",
        *(unsigned int *)0x1fdebc);

fatal:
  display_assert(
    "### FATAL ERROR LOCATING DIRECT3D FLIPCOUNT, THIS IS HORRIBLY BAD",
    "c:\\halo\\SOURCE\\main\\d3d_intimacy.cpp", 0x35, 1);
  system_exit(-1);

callback_check:
  if (*(int *)0x1fdffc != (int)0x101cd0) {
    error(2,
          "### WARNING: direct3d context unreadable "
          "(callback 0x%08X != 0x%08X)...",
          *(int *)0x1fdffc, 0x101cd0);
    goto fatal;
  }

  if (*(unsigned int *)0x1fe634 <= 0x10000)
    return (int *)0x1fe028;

  error(2,
        "### WARNING: direct3d context unreadable "
        "(vblank 0x%08X greater than 0x00010000)...",
        *(unsigned int *)0x1fe634);
  goto fatal;
  return NULL;
}

/* 0x1cf97c: XAPI UnhandledExceptionFilter. If the current top-level filter
 * (global 0x632a2c) is set, call it with the exception pointers; return
 * -1 (EXCEPTION_CONTINUE_EXECUTION) only if it returned -1, else 0
 * (EXCEPTION_CONTINUE_SEARCH).
 * Evidence: disasm MOV EAX,[0x632a2c] / PUSH [ESP+4] / CALL EAX (no ADD ESP)
 * / CMP EAX,-1 / XOR EAX,EAX / RET 4. */
typedef int(__stdcall *xapi_top_level_filter_t)(void *exception_pointers);

int __stdcall UnhandledExceptionFilter(void *exception_pointers)
{
  xapi_top_level_filter_t filter;

  filter = *(xapi_top_level_filter_t *)0x632a2c;
  if (filter != NULL) {
    if (filter(exception_pointers) == -1)
      return -1;
  }
  return 0;
}
