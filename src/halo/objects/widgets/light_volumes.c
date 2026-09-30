/* Light volume widget pool lifecycle.
 *
 * TU: c:\halo\SOURCE\objects\widgets\light_volumes.c (confirmed via the
 * __FILE__ assert string at 0x29ac98 PUSHed by light_volumes_initialize).
 *
 * The rest of this object (light_volume_new, light_volume_render, ...) is
 * still attributed to objects.obj in kb.json and lives in objects.c. */

#include "halo/objects/objects.h"

/* light_volumes_initialize (0x134b50)
 *
 * Allocates the "light volumes" data array.  The assert reason is the
 * original struct-member expression, so it is given verbatim rather than
 * stringized from our condition. */
void light_volumes_initialize(void)
{
  light_volume_data = game_state_data_new("light volumes", MAXIMUM_LIGHT_VOLUMES,
                                          sizeof(light_volume_datum_t));
  assert_halt_msg_at("light_volume_globals.light_volume_data",
                     "c:\\halo\\SOURCE\\objects\\widgets\\light_volumes.c", 44,
                     light_volume_data);
}

/* light_volumes_dispose (0x134b90)
 *
 * Empty in 2276: the body is a bare RET. */
void light_volumes_dispose(void)
{
}

/* light_volumes_initialize_for_new_map (0x134ba0)
 *
 * Resets the pool if allocated; the call target is the function
 * kb.json names data_delete_all (0x119b20). */
void light_volumes_initialize_for_new_map(void)
{
  if (light_volume_data)
    data_delete_all(light_volume_data);
}

/* light_volumes_dispose_from_old_map (0x134bc0) */
void light_volumes_dispose_from_old_map(void)
{
  if (light_volume_data)
    data_make_invalid(light_volume_data);
}
