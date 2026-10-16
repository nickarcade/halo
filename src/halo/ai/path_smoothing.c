/* path_obstacles.c — AI path obstacle debug rendering.
 *
 * Corresponds to path_smoothing.obj (this TU is the path_obstacles.c
 * translation unit within it). Source path confirmed via __FILE__ assert
 * xref: c:\halo\SOURCE\ai\path_obstacles.c
 *
 * Ported:
 *   FUN_00062960 (0x62960) — path_obstacles_debug_render: iterate the
 *     obstacle-disc array, raycast each disc's point, and draw either a
 *     projected disc (on hit) or a 3D sphere (on miss), tinted by the disc's
 *     owning obstacle index.
 *
 * Key layout (obstacles record):
 *   +0x00  int16  obstacle_count
 *   +0x02  int16  disc_count            (both reloaded each iteration)
 * Per-disc record (stride 0x18, base = obstacles + disc_index*0x18):
 *   +0x0a  int16  obstacle_index
 *   +0x10  float  point.x
 *   +0x14  float  point.y
 *   +0x18  float  radius
 *   +0x1c  float  point.z
 * MAXIMUM_DISC_COUNT = 0x80.
 *
 * Globals:
 *   0x2533c0  float  zero/threshold constant (radius > this => extra draw)
 *   0x25eeac  float  ray-direction Z scale factor
 *   0x2c8fb8  color table, 0x10-byte entries indexed by obstacle_index
 *             (byte-offset pointer add, NOT float-element indexing).
 */

#include "../../common.h"

/* 0x00062960 — path_obstacles_debug_render
 *
 * Note the hit/miss asymmetry, preserved from the original:
 *   - On raycast HIT, the projected-disc call (render_debug_circle) receives
 *     &disc->point (pfVar1, pointing into the obstacles buffer, where the
 *     three contiguous floats are x, y, radius — z lives at +0x1c and is not
 *     part of that view; the disc renderer only uses x/y + the plane).
 *   - On raycast MISS, the sphere call (render_debug_sphere) receives the local
 *     {x,y,z} point copy, which reads z from disc+0x1c.
 */
void path_obstacles_debug_render(void *obstacles, float radius)
{
  short disc_index;
  short
    count; /* reused: disc_count, then disc->obstacle_index (matches orig) */
  int disc_off;
  float *disc_point;
  bool hit;
  short collision_result[18]; /* 36-byte buffer for FUN_0014df70 */
  float plane_scratch[11]; /* scratch/output buffer for render_debug_circle */
  float ray_dir[3]; /* {0, 0, dir.z} */
  float point[3]; /* contiguous {x, y, z} */

  count = *(short *)((char *)obstacles + 2); /* disc_count */
  disc_index = 0;
  if (0 < count) {
    do {
      if ((disc_index < 0) || (count <= disc_index) || (0x80 < count)) {
        display_assert("disc_index>=0 && disc_index<obstacles->disc_count && "
                       "obstacles->disc_count<=MAXIMUM_DISC_COUNT",
                       "c:\\halo\\source\\ai\\path.h", 0x18c, true);
        system_exit(-1);
      }
      disc_off = disc_index * 0x18;
      count =
        *(short *)((char *)obstacles + disc_off + 10); /* obstacle_index */
      if ((count < 0) || (*(short *)obstacles <= count)) {
        display_assert("disc->obstacle_index>=0 && "
                       "disc->obstacle_index<obstacles->obstacle_count",
                       "c:\\halo\\SOURCE\\ai\\path_obstacles.c", 0x211, true);
        system_exit(-1);
      }
      point[1] = *(float *)((char *)obstacles + disc_off + 0x14); /* y */
      point[0] = *(float *)((char *)obstacles + disc_off + 0x10); /* x */
      point[2] = *(float *)((char *)obstacles + disc_off + 0x1c); /* z */
      disc_point = (float *)((char *)obstacles + disc_off + 0x10);
      ray_dir[2] = (radius + *(float *)((char *)obstacles + disc_off + 0x18)) *
                   *(float *)0x25eeac;
      ray_dir[0] = 0.0f;
      ray_dir[1] = 0.0f;
      hit = FUN_0014df70(0x21, point, ray_dir, -1, collision_result);
      if (hit) {
        render_debug_circle(1, plane_scratch, 2, 1, disc_point,
                     *(float *)((char *)obstacles + disc_off + 0x18),
                     (char *)0x2c8fb8 +
                       *(short *)((char *)obstacles + disc_off + 10) * 0x10,
                     0.015625f);
        if (*(float *)0x2533c0 < radius) {
          render_debug_circle(1, plane_scratch, 2, 1, disc_point,
                       radius + *(float *)((char *)obstacles + disc_off + 0x18),
                       (char *)0x2c8fb8 +
                         *(short *)((char *)obstacles + disc_off + 10) * 0x10,
                       0.015625f);
        }
      } else {
        render_debug_sphere(1, point, *(float *)((char *)obstacles + disc_off + 0x18),
                     (char *)0x2c8fb8 +
                       *(short *)((char *)obstacles + disc_off + 10) * 0x10);
        if (*(float *)0x2533c0 < radius) {
          render_debug_sphere(1, point,
                       radius + *(float *)((char *)obstacles + disc_off + 0x18),
                       (char *)0x2c8fb8 +
                         *(short *)((char *)obstacles + disc_off + 10) * 0x10);
        }
      }
      count = *(short *)((char *)obstacles + 2); /* reload disc_count */
      disc_index = disc_index + 1;
    } while (disc_index < count);
  }
  return;
}

/* 0x00062b20 — FUN_00062b20  (TU: c:\halo\SOURCE\ai\path_smoothing.c)
 *
 * Register-arg predicate (EAX/EDX/BL in, AL out) — no callers recovered yet
 * (xrefs_to is empty; reached via an inlined/indirect path), so the argument
 * meanings below are only as strong as the binary evidence noted.
 *
 * Confirmed from disassembly 0x62b20-0x62b93 (bounds-table exact):
 *   EAX      byte-array base, indexed by EDX   -> param_1 (meaning unproven)
 *   EDX      index; the SAME index is passed to tag_block_get_element for the
 *            collision-surface block, so it is the surface index
 *   BL       flag; nonzero short-circuits the whole body
 *   [EBP+8]  element whose +0x3c is the collision_surfaces tag_block (the
 *            "bsp_surfaces" element that FUN_0005e700 obtains via
 *            tag_block_get_element(structure_bsp + 0xb0, 0, 0x60)) — NOT the
 *            structure_bsp itself
 *   [EBP+0xc] dword bitmap indexed by collision_surface->breakable_surface
 *
 * collision_surface stride 0xc: flags byte at +8 (bit 3 =
 * _collision_surface_breakable_bit, named by the display_assert string),
 * breakable_surface byte at +9.
 *
 * `result` is computed unconditionally (SHR AL,6; AND AL,1) before the guard
 * chain, so all three early exits return that bit; only the fully-guarded
 * path overwrites it with the bitmap test. Note the polarity is the OPPOSITE
 * of the sibling FUN_0005e700: here SETNZ, i.e. result is true when the bit
 * IS set.
 */
bool FUN_00062b20(unsigned char *param_1, int surface_index,
                  unsigned char param_3, void *bsp_surfaces,
                  unsigned int *breakable_bitmap)
{
  unsigned char flags;
  bool result;
  unsigned char *collision_surface;
  unsigned int breakable_index;

  flags = param_1[surface_index];
  result = (flags >> 6) & 1;

  if (param_3 == 0 && result && (signed char)flags < 0) {
    collision_surface = (unsigned char *)tag_block_get_element(
      (char *)bsp_surfaces + 0x3c, surface_index, 0xc);
    if ((collision_surface[8] & 8) == 0) {
      display_assert(
        "TEST_FLAG(collision_surface->flags, _collision_surface_breakable_bit)",
        "c:\\halo\\SOURCE\\ai\\path_smoothing.c", 0x1e2, 1);
      system_exit(-1);
    }
    breakable_index = collision_surface[9];
    result = (breakable_bitmap[breakable_index >> 5] &
              (1u << (breakable_index & 0x1f))) != 0;
  }
  return result;
}

/* 0x00063710 — structure_test_ray2d  (TU:
 * c:\halo\SOURCE\ai\path_structure_bsp.c)
 *
 * March a 2D line query through the structure-bsp surface set, skipping
 * surfaces flagged in the per-surface skip byte-array and (unless
 * ignore_breakable) surfaces whose breakable bit is set in the runtime
 * breakable bitmap. Returns true (1) if the line is blocked by a surface
 * within the query, writing the hit into out_result as
 * {float t; int surface_index; int leaf/pass}; returns false (0) on miss,
 * writing {t, surface_index, -1}.
 *
 * ABI (cdecl, all stack params; returns bool in AL):
 *   +0x08 structure_bsp     (uses +0xb0 surface tag_block, +0x1e8 skip-flag
 * ptr) +0x0c ignore_breakable  (nonzero => skip the breakable test) +0x10 point
 * (forwarded to collision_surface_test_line2d) +0x14 surface_index     (initial
 * surf; reused as scratch/out surface index) +0x18 direction (forwarded) +0x1c
 * t                 (LHS of every FCOMP) +0x20 out_result        (3 dwords)
 *
 * out_result layout mirrors the 6-float scratch buffer filled by
 * collision_surface_test_line2d: two records {t, aux, surface_index}, the
 * "enter" record at buf[0..2] and the "exit" record at buf[3..5].
 */
char structure_test_ray2d(void *structure_bsp, char ignore_breakable,
                          float *point, int surface_index, float *direction,
                          float t, void *out_result)
{
  void *bsp_surfaces; /* EDI: surface tag_block element base */
  unsigned int *breakable_bitmap; /* EBX: per-surface breakable dwords */
  char *skip_flags; /* [structure_bsp+0x1e8]: skip byte-array */
  int cur_surf;
  char flagbyte;
  char surf_blocked;
  char *coll_surface;
  volatile unsigned int
    word; /* store/reload shape lever to match original codegen */
  int *out;
  float buf[6]; /* out_result of collision_surface_test_line2d:
                   enter{t,aux,surf}, exit{t,aux,surf} */

  bsp_surfaces = tag_block_get_element((char *)structure_bsp + 0xb0, 0, 0x60);
  breakable_bitmap = (unsigned int *)breakable_surfaces_get_bsp_surface_data();
  skip_flags = *(char **)((char *)structure_bsp + 0x1e8);

  collision_surface_test_line2d((int)bsp_surfaces, surface_index, 2, 1, point,
                                direction, buf);

  while (1) {
    /* enter-surface handling (only when the query point is before enter_t) */
    if (t < buf[0]) {
      cur_surf = *(int *)&buf[2]; /* enter surface index */
      flagbyte = skip_flags[cur_surf];
      if (flagbyte != 0) {
        if (ignore_breakable == 0 && flagbyte < 0) {
          coll_surface = (char *)tag_block_get_element(
            (char *)bsp_surfaces + 0x3c, cur_surf, 0xc);
          if ((coll_surface[8] & 8) == 0) {
            display_assert("TEST_FLAG(collision_surface->flags, "
                           "_collision_surface_breakable_bit)",
                           "c:\\halo\\SOURCE\\ai\\path_structure_bsp.c", 0x69,
                           true);
            system_exit(-1);
          }
          word = (unsigned char)coll_surface[9];
          surf_blocked =
            (breakable_bitmap[word >> 5] & (1u << (word & 0x1f))) != 0;
          if (surf_blocked == 0)
            goto exit_check; /* breakable & broken => not blocking here */
          cur_surf = *(int *)&buf[2];
        }
        if (cur_surf != -1)
          goto march;
      }
    }

  exit_check:
    /* exit-surface handling; writeout when the query point is at/before exit_t
     */
    if (t <= buf[3])
      break;
    cur_surf = *(int *)&buf[5]; /* exit surface index */
    flagbyte = skip_flags[cur_surf];
    if (flagbyte == 0)
      break;
    if (ignore_breakable == 0 && flagbyte < 0) {
      coll_surface = (char *)tag_block_get_element((char *)bsp_surfaces + 0x3c,
                                                   cur_surf, 0xc);
      if ((coll_surface[8] & 8) == 0) {
        display_assert("TEST_FLAG(collision_surface->flags, "
                       "_collision_surface_breakable_bit)",
                       "c:\\halo\\SOURCE\\ai\\path_structure_bsp.c", 0x7e,
                       true);
        system_exit(-1);
      }
      word = (unsigned char)coll_surface[9];
      surf_blocked = (breakable_bitmap[word >> 5] & (1u << (word & 0x1f))) != 0;
      if (surf_blocked == 0)
        break;
      cur_surf = *(int *)&buf[5];
    }
    if (cur_surf == -1)
      break;

  march:
    surface_index = cur_surf;
    collision_surface_test_line2d((int)bsp_surfaces, cur_surf, 2, 1, point,
                                  direction, buf);
  }

  /* writeout (0x63890) */
  out = (int *)out_result;
  if (t < buf[0]) { /* Path A: blocked at enter surface */
    *(float *)out = buf[0];
    out[1] = surface_index;
    out[2] = *(int *)&buf[1];
    return 1;
  }
  out[1] = surface_index;
  if (t <= buf[3]) { /* Path C: miss */
    out[2] = -1;
    *(float *)out = t;
    return 0;
  }
  /* Path B: blocked at exit surface */
  *(float *)out = buf[3];
  out[2] = *(int *)&buf[4];
  return 1;
}

/* 0x000638f0 — path_smoothing surface-height coplanarity test.
 *
 * Projects `point` onto two collision surfaces (surf_a, surf_b) of the
 * structure-BSP block at def+0xB0 and compares the projected height
 * (third component of each projection output). Returns false when either
 * surface index is -1, or when the two heights differ by less than the
 * global tolerance (i.e. the two projections are coplanar/close enough);
 * returns true when the height difference exceeds the tolerance.
 *
 * ABI (disasm): 4 cdecl stack args, BYTE-BOOL return in AL. Default AL=0;
 * MOV AL,1 only on the fall-through where ABS(diff) is NOT < threshold.
 *
 * Projection-output-size (§5): collision_surface_project_point2d writes 3
 * floats to out_point (Ghidra sized the buffers float[2] — wrong). The two
 * output buffers are contiguous in the 0x18-byte frame; the compare reads
 * the THIRD component of each (out1[2] @ EBP-0x4, out2[2] @ EBP-0x10).
 *
 * Threshold at 0x25f0c8 is a DOUBLE (FCOMP double ptr): the x87 ABS of the
 * float difference is promoted to double for the compare. */
bool FUN_000638f0(int def, float *point, int surf_a, int surf_b)
{
  void *block_elem;
  float out1[3];
  float out2[3];

  block_elem = tag_block_get_element((void *)(def + 0xb0), 0, 0x60);
  if ((surf_a != -1) && (surf_b != -1)) {
    collision_surface_project_point2d((int)block_elem, surf_a, 2, 1, point,
                                      out1);
    collision_surface_project_point2d((int)block_elem, surf_b, 2, 1, point,
                                      out2);
    if (fabs(out1[2] - out2[2]) < *(double *)0x25f0c8)
      return 0;
    return 1;
  }
  return 0;
}

/*
 * FUN_00063970 (0x63970) — interval/segment clamp.
 * Clamps hi upward to *a and lo downward to *b, then collapses one endpoint:
 * compares the two gaps (*b - hi) vs (lo - *a) and moves only the
 * smaller-shortfall side. Returns 1 when the clamp left the interval
 * inverted/empty (*a > *b), else 0.
 * (Tail at 0x639c2: FCOMP *a,*b; TEST AH,0x41; JNE -> XOR EAX,EAX (return 0);
 *  fall-through -> MOV EAX,1 (return 1). So EAX = (*a > *b) ? 1 : 0.)
 * cdecl, 4 stack args, BOOL return in EAX (Ghidra misread this as void(void)).
 *
 * Shape notes (delinked 00063970.obj):
 *  - Both clamps are TERNARIES with both-arm assignment: the hi clamp's else
 *    arm is a literal self-store (MOV ECX,[EBP+0x14]; MOV [EBP+0x14],ECX) and
 *    the taken arm copies the float via integer MOV EAX. Spelling them as
 *    ternaries (not `if`) reproduces this under VC71; clang folds them to
 *    selects (same semantics, both predicates false on NaN). VC71 store-merges
 *    our hi-clamp arms (3-insn ceiling vs the original's distinct-arm copies;
 *    the self-assign spelling that would split them is clang-hostile).
 *  - The lo/min select stays ST-resident (FLD lo/FLD *b straight into the
 *    gap compare) — t_lo is a single-assignment non-volatile local.
 *  - Gap compare: FCOMPP; TEST AH,0x5; JP => (*b - hi) <= (t_lo - *a) takes
 *    the *b = t_lo path (NaN also lands there). Equivalence-verified 100/100
 *    (unicorn, high confidence) on this predicate orientation.
 */
int FUN_00063970(float *a, float *b, float lo, float hi)
{
  float t_lo;

  hi = (*a > hi) ? *a : hi;
  t_lo = (lo > *b) ? *b : lo;
  if (*b - hi > t_lo - *a)
    *a = hi;
  else
    *b = t_lo;
  return (*a > *b) ? 1 : 0;
}

/* 0x00062ba0 — find_tangent_point
 * Computes a 2D tangent point from point_a to a circle of radius 'radius'
 * around point_b, selecting between left/right tangent based on 'side_flag'.
 */
void find_tangent_point(float radius, bool side_flag, float *point_a, float *point_b, float *out_tangent)
{
  float dx;
  float dy;
  float dist_sq;
  float inv_dist_sq;
  float discr;
  float sqrt_discr;
  float t_x;
  float t_y;
  float tangents[4];
  bool select;
  float dir[2];

  dx = point_a[0] - point_b[0];
  dy = point_a[1] - point_b[1];
  dist_sq = dy * dy + dx * dx;
  inv_dist_sq = radius / dist_sq;
  discr = dist_sq - radius * radius;
  if (discr > *(float *)0x2533c0) {
    sqrt_discr = sqrtf(discr);
    t_x = dy * sqrt_discr;
    t_y = dx * sqrt_discr;
    tangents[0] = (dx * radius + t_x) * inv_dist_sq + point_b[0];
    tangents[1] = (dy * radius - t_y) * inv_dist_sq + point_b[1];
    tangents[2] = (dx * radius - t_x) * inv_dist_sq + point_b[0];
    tangents[3] = (t_y + dy * radius) * inv_dist_sq + point_b[1];
    select = (((tangents[3] - point_a[1]) * (tangents[0] - point_a[0]) - (tangents[2] - point_a[0]) * (tangents[1] - point_a[1])) > *(float *)0x2533c0) != side_flag;
    out_tangent[0] = tangents[select ? 2 : 0];
    out_tangent[1] = tangents[select ? 3 : 1];
    return;
  }
  dir[0] = dx;
  dir[1] = dy;
  if (normalize2d(dir) == *(float *)0x2533c0) {
    dir[0] = *(float *)0x31fc10;
    dir[1] = *(float *)0x31fc14;
  }
  out_tangent[0] = dir[0] * radius + point_b[0];
  out_tangent[1] = dir[1] * radius + point_b[1];
}

/* 0x00062cf0 — find_avoidance_point
 * Computes an avoidance point around an obstacle from tangent/bounding points.
 */
void find_avoidance_point(float radius, float *tangents, float *center, float *out_point)
{
  float cross;
  float scale;
  float dx;
  float dy;
  float dist;

  cross = (tangents[3] - center[1]) * (tangents[0] - center[0]) - (tangents[1] - center[1]) * (tangents[2] - center[0]);
  if (fabsf(cross) >= *(float *)0x2533d0) {
    scale = (radius * radius) / cross;
    out_point[0] = center[0] - ((tangents[1] - center[1]) - (tangents[3] - center[1])) * scale;
    out_point[1] = ((tangents[0] - center[0]) - (tangents[2] - center[0])) * scale + center[1];
    if ((out_point[0] - center[0]) * (out_point[0] - center[0]) + (out_point[1] - center[1]) * (out_point[1] - center[1]) <= radius * radius * *(float *)0x2533d8) {
      return;
    }
  }
  dx = tangents[0] - center[0];
  dy = tangents[1] - center[1];
  dist = sqrtf(dx * dx + dy * dy);
  if (fabsf(dist) >= *(float *)0x2533d0) {
    dx = dx * (*(float *)0x2533c8 / dist);
    dy = dy * (*(float *)0x2533c8 / dist);
  } else {
    dx = *(float *)0x31fc10;
    dy = *(float *)0x31fc14;
  }
  out_point[0] = dx * radius + tangents[0];
  out_point[1] = dy * radius + tangents[1];
}

/* 0x00062e10 — choose_turning_point
 * Chooses between two turning points by comparing turn angles.
 */
int choose_turning_point(float *turn_a, float *turn_b, float *start, float *dest, float *out_chosen)
{
  float d_dest_a[2];
  float d_start_a[2];
  float d_turn_a[2];
  float d_dest_b[2];
  float d_start_b[2];
  float d_turn_b[2];
  float len;
  float angle_a1;
  float angle_a2;
  float total_angle_a;
  float angle_b1;
  float angle_b2;
  float total_angle_b;

  d_dest_a[0] = dest[0] - turn_a[0];
  d_dest_a[1] = dest[1] - turn_a[1];
  len = sqrtf(d_dest_a[0] * d_dest_a[0] + d_dest_a[1] * d_dest_a[1]);
  if (fabsf(len) >= *(float *)0x2533d0) {
    d_dest_a[0] *= (*(float *)0x2533c8 / len);
    d_dest_a[1] *= (*(float *)0x2533c8 / len);
  }
  d_start_a[0] = start[0] - turn_a[0];
  d_start_a[1] = start[1] - turn_a[1];
  len = sqrtf(d_start_a[0] * d_start_a[0] + d_start_a[1] * d_start_a[1]);
  if (fabsf(len) >= *(float *)0x2533d0) {
    d_start_a[0] *= (*(float *)0x2533c8 / len);
    d_start_a[1] *= (*(float *)0x2533c8 / len);
  }
  d_turn_a[0] = turn_b[0] - turn_a[0];
  d_turn_a[1] = turn_b[1] - turn_a[1];
  len = sqrtf(d_turn_a[0] * d_turn_a[0] + d_turn_a[1] * d_turn_a[1]);
  if (fabsf(len) >= *(float *)0x2533d0) {
    d_turn_a[0] *= (*(float *)0x2533c8 / len);
    d_turn_a[1] *= (*(float *)0x2533c8 / len);
  }

  d_dest_b[0] = dest[0] - turn_b[0];
  d_dest_b[1] = dest[1] - turn_b[1];
  len = sqrtf(d_dest_b[0] * d_dest_b[0] + d_dest_b[1] * d_dest_b[1]);
  if (fabsf(len) >= *(float *)0x2533d0) {
    d_dest_b[0] *= (*(float *)0x2533c8 / len);
    d_dest_b[1] *= (*(float *)0x2533c8 / len);
  }
  d_start_b[0] = start[0] - turn_b[0];
  d_start_b[1] = start[1] - turn_b[1];
  len = sqrtf(d_start_b[0] * d_start_b[0] + d_start_b[1] * d_start_b[1]);
  if (fabsf(len) >= *(float *)0x2533d0) {
    d_start_b[0] *= (*(float *)0x2533c8 / len);
    d_start_b[1] *= (*(float *)0x2533c8 / len);
  }
  d_turn_b[0] = turn_a[0] - turn_b[0];
  d_turn_b[1] = turn_a[1] - turn_b[1];
  len = sqrtf(d_turn_b[0] * d_turn_b[0] + d_turn_b[1] * d_turn_b[1]);
  if (fabsf(len) >= *(float *)0x2533d0) {
    d_turn_b[0] *= (*(float *)0x2533c8 / len);
    d_turn_b[1] *= (*(float *)0x2533c8 / len);
  }

  angle_a1 = signed_angle_between_vectors2d(d_start_a, d_turn_a);
  angle_a2 = signed_angle_between_vectors2d(d_dest_a, d_start_a);
  total_angle_a = angle_a1 + angle_a2;

  angle_b1 = signed_angle_between_vectors2d(d_start_b, d_turn_b);
  angle_b2 = signed_angle_between_vectors2d(d_dest_b, d_start_b);
  total_angle_b = -(angle_b1 + angle_b2);

  if (total_angle_b < total_angle_a) {
    out_chosen[0] = turn_a[0];
    out_chosen[1] = turn_a[1];
    return 1;
  }
  out_chosen[0] = turn_b[0];
  out_chosen[1] = turn_b[1];
  return 0;
}

/* 0x00063030 — find_turning_point
 * Finds a turning point along structure collision surface boundaries.
 */
uint32_t find_turning_point(int scenario, float *origin, float radius, uint32_t edge_index, uint8_t side, char bsp_idx, unsigned int *out_pt)
{
  int surf_block;
  int path_flags;
  int breakable_flags;
  uint32_t *edge;
  uint8_t flags;
  bool is_breakable;
  float *v1;
  float *v2;
  float dx;
  float dy;
  float perp_x;
  float perp_y;
  float len;
  float normal_x;
  float normal_y;
  float d_orig_x;
  float d_orig_y;
  bool flag;
  uint32_t next_edge;
  uint32_t first_edge;
  uint32_t current_edge;

  surf_block = (int)tag_block_get_element_with_size((void *)(scenario + 0xb0), 0, 0x60);
  path_flags = *(int *)(scenario + 0x1e8);
  breakable_flags = breakable_surface_flags_get();
  first_edge = 0xffffffff;
  current_edge = 0xffffffff;

  if (side != 0 && side != 1) {
    display_assert("side==0 || side==1", "c:\\halo\\SOURCE\\ai\\path_smoothing.c", 0x1ff, 1);
    system_exit(-1);
  }

  while (1) {
    edge = (uint32_t *)tag_block_get_element_with_size((void *)(surf_block + 0x48), edge_index, 0x18);
    flags = *(uint8_t *)(edge[4] + path_flags);
    is_breakable = (bool)((flags >> 6) & 1);
    if (!bsp_idx && is_breakable && (flags <= 0xff)) {
      int s_elem = (int)tag_block_get_element_with_size((void *)(surf_block + 0x3c), edge[4], 0xc);
      if (!(*(uint8_t *)(s_elem + 8) & 8)) {
        display_assert("TEST_FLAG(surface->flags, _collision_surface_breakable_bit)", "c:\\halo\\SOURCE\\ai\\path_smoothing.c", 0x1e2, 1);
        system_exit(-1);
      }
      is_breakable = (*(uint32_t *)(breakable_flags + ((uint32_t)(*(uint8_t *)(s_elem + 9) >> 5) * 4)) & (1 << (*(uint8_t *)(s_elem + 9) & 0x1f))) != 0;
    }

    v1 = (float *)tag_block_get_element_with_size((void *)(surf_block + 0x54), edge[is_breakable ? 1 : 0], 0x10);
    v2 = (float *)tag_block_get_element_with_size((void *)(surf_block + 0x54), edge[!is_breakable ? 1 : 0], 0x10);
    dx = v2[0] - v1[0];
    dy = v2[1] - v1[1];
    perp_x = dy;
    perp_y = -dx;
    len = sqrtf(perp_x * perp_x + perp_y * perp_y);
    if (fabsf(len) < *(float *)0x2533d0) {
      normal_x = perp_x;
      normal_y = perp_y;
    } else {
      normal_x = perp_x * (*(float *)0x2533c8 / len);
      normal_y = perp_y * (*(float *)0x2533c8 / len);
    }
    d_orig_x = v1[0] - (normal_x * radius + origin[0]);
    d_orig_y = v1[1] - (normal_y * radius + origin[1]);
    flag = 0;
    if (((d_orig_x * dx + d_orig_y * dy < *(float *)0x2533c0) == (bool)side) && (d_orig_x * dy - d_orig_y * dx < *(float *)0x2533c0)) {
      flag = 1;
    }
    if ((v1[0] - (-radius * normal_x + origin[0])) * dy - (v1[1] - (normal_y * -radius + origin[1])) * dx < *(float *)0x2533c0) {
      flag = 1;
    }
    if (first_edge == 0xffffffff) {
      flag = 1;
    }
    next_edge = edge[(flag != is_breakable) == (bool)side ? 1 : 0];
    if (next_edge == current_edge) {
      unsigned int *pt = (unsigned int *)tag_block_get_element_with_size((void *)(surf_block + 0x54), next_edge, 0x10);
      out_pt[0] = pt[0];
      out_pt[1] = pt[1];
      return 1;
    }
    if (next_edge == first_edge) {
      break;
    }
    if (first_edge == 0xffffffff) {
      first_edge = next_edge;
    }
    while (1) {
      uint32_t opp_v = edge[1];
      flags = *(uint8_t *)(edge[(next_edge != opp_v ? 1 : 0) + 4] + path_flags);
      is_breakable = (bool)((flags >> 6) & 1);
      if (!bsp_idx && is_breakable && (flags <= 0xff)) {
        int s_elem = (int)tag_block_get_element_with_size((void *)(surf_block + 0x3c), edge[(next_edge != opp_v ? 1 : 0) + 4], 0xc);
        if (!(*(uint8_t *)(s_elem + 8) & 8)) {
          display_assert("TEST_FLAG(surface->flags, _collision_surface_breakable_bit)", "c:\\halo\\SOURCE\\ai\\path_smoothing.c", 0x1e2, 1);
          system_exit(-1);
        }
        is_breakable = (*(uint32_t *)(breakable_flags + ((uint32_t)(*(uint8_t *)(s_elem + 9) >> 5) * 4)) & (1 << (*(uint8_t *)(s_elem + 9) & 0x1f))) != 0;
      }
      if (is_breakable == (bool)side) {
        break;
      }
      edge_index = edge[(next_edge != opp_v ? 1 : 0) + 2];
      edge = (uint32_t *)tag_block_get_element_with_size((void *)(surf_block + 0x48), edge_index, 0x18);
      if (edge[0] != next_edge && edge[1] != next_edge) {
        display_assert("vertex==edge->vertex_indices[0] || vertex==edge->vertex_indices[1]", "c:\\halo\\SOURCE\\ai\\path_smoothing.c", 0x277, 1);
        system_exit(-1);
      }
    }
    current_edge = next_edge;
  }
  return 0;
}

/* 0x000633b0 — path_smooth / FUN_000633b0
 * Smooths raw AI navigation path steps into a collision-free smoothed path.
 */
void FUN_000633b0(unsigned int param_1, int param_2, void *param_3, int *param_4, void *param_5, char *param_6)
{
  unsigned int *raw_steps;
  unsigned short *out_count;
  unsigned int *out_steps;
  int smooth_count;
  char finish_path;
  int curr_step;
  int raw_count;
  unsigned int cur_pt[3];
  unsigned int cur_surf;
  int raw_idx;
  int blocked_step;
  int blocked_edge;
  bool is_blocked;
  char test_result[20];
  float turn_pt_left[2];
  float turn_pt_right[2];
  float chosen_turn[2];
  float tangent_left[2];
  float tangent_right[2];
  float avoid_pt[2];
  int next_surf;
  unsigned int *curr_out;
  float proj_in[3];

  raw_count = param_2;
  raw_steps = (unsigned int *)param_3;
  out_count = (unsigned short *)param_4;
  out_steps = (unsigned int *)param_5;

  if (raw_count <= 0) {
    display_assert("raw_step_count > 0", "c:\\halo\\SOURCE\\ai\\path_smoothing.c", 0x21, 1);
    system_exit(-1);
  }
  if (!raw_steps) {
    display_assert("raw_steps", "c:\\halo\\SOURCE\\ai\\path_smoothing.c", 0x22, 1);
    system_exit(-1);
  }
  if (!out_count) {
    display_assert("smoothed_step_count", "c:\\halo\\SOURCE\\ai\\path_smoothing.c", 0x23, 1);
    system_exit(-1);
  }
  if (!out_steps) {
    display_assert("smoothed_steps", "c:\\halo\\SOURCE\\ai\\path_smoothing.c", 0x24, 1);
    system_exit(-1);
  }
  if (!param_6) {
    display_assert("steps_finish_path", "c:\\halo\\SOURCE\\ai\\path_smoothing.c", 0x25, 1);
    system_exit(-1);
  }

  if (raw_count <= 1) {
    *out_count = 1;
    out_steps[0] = raw_steps[0];
    out_steps[1] = raw_steps[1];
    out_steps[2] = raw_steps[2];
    out_steps[3] = raw_steps[3];
    return;
  }

  cur_pt[0] = *(unsigned int *)(param_1 + 0x14);
  cur_pt[1] = *(unsigned int *)(param_1 + 0x18);
  cur_surf = *(unsigned int *)(param_1 + 0x20);
  smooth_count = 0;
  finish_path = 0;
  curr_step = 1;

  while (1) {
    blocked_edge = -1;
    is_blocked = 0;
    blocked_step = -1;

    if (raw_count <= curr_step) {
      break;
    }

    for (raw_idx = curr_step; raw_idx < raw_count; raw_idx++) {
      unsigned int *step_data = &raw_steps[raw_idx * 4];
      if (structure_test_pill2d(*(unsigned int *)(param_1 + 100), *(unsigned char *)(param_1 + 4), cur_pt, cur_surf, &step_data[1], step_data[0], 0.3f, 1, (unsigned int *)test_result)) {
        if (!is_blocked) {
          blocked_step = raw_idx;
          is_blocked = 1;
          blocked_edge = *(int *)&test_result[16];
        }
      } else if (is_blocked) {
        blocked_edge = -1;
        blocked_step = -1;
        is_blocked = 0;
      }
    }

    if (!is_blocked || blocked_edge == -1) {
      break;
    }

    if (!find_turning_point(*(int *)(param_1 + 100), (float *)cur_pt, 0.3f, blocked_edge, 1, *(char *)(param_1 + 4), (unsigned int *)turn_pt_left) ||
        !find_turning_point(*(int *)(param_1 + 100), (float *)cur_pt, 0.3f, blocked_edge, 0, *(char *)(param_1 + 4), (unsigned int *)turn_pt_right)) {
      goto finish_up;
    }

    choose_turning_point(turn_pt_left, turn_pt_right, (float *)cur_pt, (float *)&raw_steps[blocked_step * 4 + 1], chosen_turn);
    find_tangent_point(0.35f, 1, (float *)cur_pt, chosen_turn, tangent_left);
    find_tangent_point(0.35f, 0, (float *)cur_pt, chosen_turn, tangent_right);
    find_avoidance_point(0.35f, tangent_left, chosen_turn, avoid_pt);

    proj_in[0] = cur_pt[0];
    proj_in[1] = cur_pt[1];
    cur_pt[0] = *(unsigned int *)&avoid_pt[0];
    cur_pt[1] = *(unsigned int *)&avoid_pt[1];
    next_surf = structure_surface_index_from_point(*(unsigned int *)(param_1 + 100), *(unsigned char *)(param_1 + 4), (float *)proj_in, cur_surf, (float *)cur_pt);
    curr_out = &out_steps[smooth_count * 4];
    cur_surf = next_surf;
    smooth_count++;
    collision_surface_project_point2d((int)tag_block_get_element_with_size((void *)(*(int *)(param_1 + 100) + 0xb0), 0, 0x60), next_surf, 2, 1, (float *)cur_pt, (float *)&curr_out[1]);
    curr_out[0] = next_surf;

    if (smooth_count >= 4) {
      goto finish_up;
    }
    curr_step = blocked_step;
  }

  {
    unsigned int *last_raw = &raw_steps[(raw_count - 1) * 4];
    curr_out = &out_steps[smooth_count * 4];
    curr_out[0] = last_raw[0];
    curr_out[1] = last_raw[1];
    curr_out[2] = last_raw[2];
    curr_out[3] = last_raw[3];
    smooth_count++;
    finish_path = 1;
  }

finish_up:
  *out_count = (unsigned short)smooth_count;
  if (!finish_path) {
    *param_6 = 0;
  }
}

/* 0x000639e0 — structure_test_line2d / FUN_000639e0
 * Tests a 2D line against structure BSP 2D surface projection.
 */
int FUN_000639e0(int scenario, unsigned char bsp_idx, float *origin, int node_handle, float *target, int flags, char *result_buf)
{
  int surf_block;
  int path_flags;
  int breakable_flags;
  int current_surf;
  char flag_35;
  float dx;
  float dy;
  char *surf_elem;
  int edge_idx;
  float avg_x;
  float avg_y;
  float avg_z;
  int vertex_count;
  char has_intersection;
  char reached_goal;
  int i;
  int next_edge;
  float *v1;
  float *v2;
  float edge_dx;
  float edge_dy;
  float d1_x;
  float d1_y;
  float d2_x;
  float d2_y;
  float t;
  float proj_pt[3];
  char sub_buf[16];

  surf_block = (int)tag_block_get_element_with_size((void *)(scenario + 0xb0), 0, 0x60);
  path_flags = *(int *)(scenario + 0x1e8);
  breakable_flags = breakable_surface_flags_get();
  current_surf = node_handle;
  flag_35 = 0;

  if (!result_buf) {
    display_assert("result", "c:\\halo\\SOURCE\\ai\\path_structure_bsp.c", 0xd9, 1);
    system_exit(-1);
  }

  dx = target[0] - origin[0];
  dy = target[1] - origin[1];

start_search:
  surf_elem = (char *)tag_block_get_element_with_size((void *)(surf_block + 0x3c), current_surf, 0xc);
  edge_idx = *(int *)(surf_elem + 4);
  avg_x = *(float *)0x31fc1c;
  avg_y = *(float *)0x31fc20;
  avg_z = *(float *)0x31fc24;
  vertex_count = 0;
  has_intersection = 0;
  reached_goal = 0;

  do {
    uint32_t *edge = (uint32_t *)tag_block_get_element_with_size((void *)(surf_block + 0x48), edge_idx, 0x18);
    bool match_surf = (current_surf == (int)edge[5]);
    uint32_t v_idx1 = edge[!match_surf ? 1 : 0];
    uint32_t v_idx2 = edge[match_surf ? 1 : 0];
    v1 = (float *)tag_block_get_element_with_size((void *)(surf_block + 0x54), v_idx1, 0x10);
    v2 = (float *)tag_block_get_element_with_size((void *)(surf_block + 0x54), v_idx2, 0x10);

    edge_dx = v2[0] - v1[0];
    edge_dy = v2[1] - v1[1];
    d1_x = v1[0] - origin[0];
    d1_y = v1[1] - origin[1];
    d2_x = v2[0] - origin[0];
    d2_y = v2[1] - origin[1];

    if ((int)edge[4 + (!match_surf ? 1 : 0)] == flags) {
      reached_goal = 1;
    }

    avg_x += v1[0];
    avg_y += v1[1];
    avg_z += v1[2];
    vertex_count++;

    if ((*(float *)0x2533c0 < edge_dx * (target[1] - v1[1]) - (target[0] - v1[0]) * edge_dy) &&
        (has_intersection = 1, *(float *)0x2533c0 < d1_y * dx - dy * d1_x) &&
        (*(float *)0x2533c0 < dy * d2_x - d2_y * dx)) {
      int adj_surf = (int)edge[4 + (match_surf ? 1 : 0)];
      uint8_t flags_byte = *(uint8_t *)(adj_surf + path_flags);
      bool is_brk = (bool)((flags_byte >> 6) & 1);
      if (!bsp_idx) {
        if (is_brk && (flags_byte <= 0xff)) {
          int s_elem = (int)tag_block_get_element_with_size((void *)(surf_block + 0x3c), adj_surf, 0xc);
          if (!(*(uint8_t *)(s_elem + 8) & 8)) {
            display_assert("TEST_FLAG(surface->flags, _collision_surface_breakable_bit)", "c:\\halo\\SOURCE\\ai\\path_structure_bsp.c", 0x112, 1);
            system_exit(-1);
          }
          is_brk = (*(uint32_t *)(breakable_flags + ((uint32_t)(*(uint8_t *)(s_elem + 9) >> 5) * 4)) & (1 << (*(uint8_t *)(s_elem + 9) & 0x1f))) != 0;
        }
      }
      if (!is_brk) {
        t = ((d1_y * edge_dx - d1_x * edge_dy) - sqrtf(edge_dy * edge_dy + edge_dx * edge_dx) * *(float *)0x25f0d0) / (dy * edge_dx - dx * edge_dy);
        proj_pt[0] = dx * t + origin[0];
        proj_pt[1] = dy * t + origin[1];
        collision_surface_project_point2d(surf_block, current_surf, 2, 1, proj_pt, (float *)&result_buf[4]);
        *(int *)&result_buf[0x10] = current_surf;
        *(int *)&result_buf[0x14] = edge_idx;
        result_buf[0] = 1;
        *(float *)&result_buf[0x18] = t;
        return 1;
      }
      current_surf = adj_surf;
      goto start_search;
    }
    edge_idx = (int)edge[2 + (match_surf ? 1 : 0)];
    if (edge_idx == *(int *)(surf_elem + 4)) {
      break;
    }
  } while (1);

  if (has_intersection) {
    if (current_surf < 0 || *(int *)(scenario + 0x1e4) <= current_surf) {
      display_assert("surface_index>=0 && surface_index<structure_bsp->surfaces.count", "c:\\halo\\SOURCE\\ai\\path_structure_bsp.c", 0x13c, 1);
      system_exit(-1);
    }
    avg_x *= (*(float *)0x2533c8 / (float)vertex_count);
    avg_y *= (*(float *)0x2533c8 / (float)vertex_count);
    if (!flag_35 && *(char *)(current_surf + path_flags)) {
      proj_pt[0] = avg_x;
      proj_pt[1] = avg_y;
      if (!FUN_000639e0(scenario, bsp_idx, proj_pt, current_surf, origin, -1, sub_buf)) {
        flag_35 = 1;
        goto start_search;
      }
    }
  } else if ((current_surf == flags) || reached_goal || (flags == -1)) {
    collision_surface_project_point2d(surf_block, current_surf, 2, 1, target, (float *)&result_buf[4]);
    *(int *)&result_buf[0x10] = current_surf;
    *(unsigned int *)&result_buf[0x14] = 0xffffffff;
    result_buf[0] = 0;
    *(float *)&result_buf[0x18] = 1.0f;
    return 0;
  }

  collision_surface_project_point2d(surf_block, node_handle, 2, 1, origin, (float *)&result_buf[4]);
  *(unsigned int *)&result_buf[0x10] = 0xffffffff;
  *(unsigned int *)&result_buf[0x14] = 0xffffffff;
  result_buf[0] = 1;
  *(float *)&result_buf[0x18] = 0.0f;
  return 1;
}
