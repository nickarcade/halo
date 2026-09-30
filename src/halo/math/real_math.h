/* c:\halo\source\math\real_math.h
 *
 * Header path recovered from the XBE (the string "..\math\real_math.h" is the
 * __FILE__ of asserts in this header's inline functions).  Only the inline
 * helpers a lifted caller needs are recovered here; each is inlined by the
 * original at its call sites, so a lift that calls it reproduces the original
 * instruction schedule.  Names: T2. */

#ifndef HALO_MATH_REAL_MATH_H
#define HALO_MATH_REAL_MATH_H

#include "../../types.h"

/* result = b - a.  Inlined in lightning_submit (0x135818, 0x135c6f). */
static __inline real_vector3d *vector_from_points3d(real_point3d const *a,
                                                    real_point3d const *b,
                                                    real_vector3d *result)
{
  result->i = b->x - a->x;
  result->j = b->y - a->y;
  result->k = b->z - a->z;
  return result;
}

/* result = c * a.  Inlined in lightning_submit (0x1359c4). */
static __inline real_vector3d *scale_vector3d(real_vector3d const *a, real c,
                                              real_vector3d *result)
{
  result->i = c * a->i;
  result->j = c * a->j;
  result->k = c * a->k;
  return result;
}

#endif /* HALO_MATH_REAL_MATH_H */
