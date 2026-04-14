/* HaloScript runtime — thread management and script execution. */

/* Dispose runtime state from old map: invalidate thread data and
 * clean up any allocated script globals. */
void hs_runtime_dispose_from_old_map(void)
{
  int16_t idx;
  char *data;

  ((void (*)(void *))0x119550)(*(void **)0x5aa6c4);

  idx = *(int16_t *)0x27d504;
  data = *(char **)0x5aa6c0;
  while (idx < *(int16_t *)(data + 0x2e)) {
    if (((int (*)(void *, int))0x119270)(data, (int)idx) != 0)
      ((void (*)(void *, int))0x1196d0)(data, (int)idx);
    idx++;
    data = *(char **)0x5aa6c0;
  }

  *(uint8_t *)0x46b810 = 0;
}

/* NOTE: hs_runtime_initialize_for_new_map (0xcdb30) is complex (~100 lines)
 * and left to the original binary via weak thunk. */
