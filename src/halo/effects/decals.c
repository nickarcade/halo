#include "x87_math.h"

extern double floor(double);

/*
 * FUN_00098970 — consistency check for a doubly-linked decal entry.
 * Verifies that the decal's prev (0x30) and next (0x34) neighbors share the
 * same cluster_index (+4) and, if layer_check is true, the same layer (+6).
 * Calls datum_get twice per neighbor to read each field independently,
 * matching the original MSVC codegen.
 *
 * 0x98970 / decals.obj
 */
void FUN_00098970(int handle, bool layer_check)
{
  decal_datum_t *decal;
  decal_datum_t *other;
  int16_t cluster;
  int16_t lyr;

  decal = (decal_datum_t *)datum_get(global_decal_data, handle);

  if (decal->previous_decal_index != NONE) {
    other = (decal_datum_t *)datum_get(global_decal_data,
                                       decal->previous_decal_index);
    cluster = other->cluster_index;
    other = (decal_datum_t *)datum_get(global_decal_data,
                                       decal->previous_decal_index);
    lyr = other->layer;
    if (cluster != decal->cluster_index) {
      display_assert("cluster_index==decal->cluster_index",
                     "c:\\halo\\SOURCE\\effects\\decals.c", 0xc2, 1);
      system_exit(-1);
    }
    if (layer_check && lyr != decal->layer) {
      display_assert("!layer_check || layer==decal->layer",
                     "c:\\halo\\SOURCE\\effects\\decals.c", 0xc3, 1);
      system_exit(-1);
    }
  }

  if (decal->next_decal_index != NONE) {
    other = (decal_datum_t *)datum_get(global_decal_data,
                                       decal->next_decal_index);
    cluster = other->cluster_index;
    other = (decal_datum_t *)datum_get(global_decal_data,
                                       decal->next_decal_index);
    lyr = other->layer;
    if (cluster != decal->cluster_index) {
      display_assert("cluster_index==decal->cluster_index",
                     "c:\\halo\\SOURCE\\effects\\decals.c", 0xcb, 1);
      system_exit(-1);
    }
    if (layer_check && lyr != decal->layer) {
      display_assert("!layer_check || layer==decal->layer",
                     "c:\\halo\\SOURCE\\effects\\decals.c", 0xcc, 1);
      system_exit(-1);
    }
  }
}

/*
 * FUN_00098aa0 — set the first decal datum index for a cluster/layer slot.
 * Validates cluster_index in [0, 512) and layer in [0, 5), then writes
 * param_1 into decal_globals at [layer * 512 + cluster_index].
 * Counterpart to FUN_00098fe0 (getter). Takes cluster_index in SI, layer in DI.
 *
 * 0x98aa0 / decals.obj
 */
void FUN_00098aa0(int16_t cluster_index, int16_t layer, int param_1)
{
  if (cluster_index < 0 || cluster_index >= MAXIMUM_CLUSTERS_PER_STRUCTURE) {
    display_assert(
      "cluster_index>=0 && cluster_index<MAXIMUM_CLUSTERS_PER_STRUCTURE",
      "c:\\halo\\SOURCE\\effects\\decals.c", 0xd8, 1);
    system_exit(-1);
  }
  if (layer < 0 || layer >= NUMBER_OF_DECAL_LAYERS) {
    display_assert("layer>=0 && layer<NUMBER_OF_DECAL_LAYERS",
                   "c:\\halo\\SOURCE\\effects\\decals.c", 0xd9, 1);
    system_exit(-1);
  }
  decal_globals->first_decal_index[layer][cluster_index] = param_1;
}

void FUN_00098b20(float *sprite_bounds, void *definition,
                  int16_t sequence_index, int16_t sprite_index, float extent,
                  float *out_extent)
{
  char *definition_data;
  char *bitmap_tag;
  char *sequence;
  int16_t *sprite;
  char *bitmap;
  float ratio;
  float x_scale;
  float y_scale;

  ratio = 1.0f;
  if (definition == NULL) {
    display_assert("definition", "c:\\halo\\SOURCE\\effects\\decals.c", 0x107,
                   true);
    system_exit(-1);
  }

  if (sprite_bounds == NULL) {
    display_assert("sprite_bounds", "c:\\halo\\SOURCE\\effects\\decals.c",
                   0x108, true);
    system_exit(-1);
  }

  if (out_extent == NULL) {
    display_assert("extent", "c:\\halo\\SOURCE\\effects\\decals.c", 0x109,
                   true);
    system_exit(-1);
  }

  definition_data = (char *)definition;

  bitmap_tag = (char *)tag_get(TAG_GROUP_BITM, *(int *)(definition_data + 0xe4));
  sequence =
    (char *)tag_block_get_element(bitmap_tag + 0x54, (int)sequence_index, 0x40);
  sprite =
    (int16_t *)tag_block_get_element(sequence + 0x34, (int)sprite_index, 0x20);
  bitmap =
    (char *)tag_block_get_element(bitmap_tag + 0x60, (int)sprite[0], 0x30);

  sprite_bounds[0] = *(float *)(sprite + 4);
  sprite_bounds[1] = *(float *)(sprite + 6);
  sprite_bounds[2] = *(float *)(sprite + 8);
  sprite_bounds[3] = *(float *)(sprite + 10);

  if ((definition_data[1] & 1) != 0) {
    ratio = ((float)(int)*(int16_t *)(bitmap + 6) /
             (float)(int)*(int16_t *)(bitmap + 4)) *
            ((*(float *)(sprite + 6) - *(float *)(sprite + 4)) /
             (*(float *)(sprite + 10) - *(float *)(sprite + 8)));
  }

  extent = extent / *(float *)(definition_data + 0xfc);
  x_scale = (float)(int)*(int16_t *)(bitmap + 4) * extent;
  y_scale = (float)(int)*(int16_t *)(bitmap + 6) * extent * ratio;

  out_extent[0] = -*(float *)(sprite + 0xc) * x_scale;
  out_extent[1] = ((*(float *)(sprite + 6) - *(float *)(sprite + 0xc)) -
                   *(float *)(sprite + 4)) *
                  x_scale;
  out_extent[2] = -*(float *)(sprite + 0xe) * y_scale;
  out_extent[3] = ((*(float *)(sprite + 10) - *(float *)(sprite + 0xe)) -
                   *(float *)(sprite + 8)) *
                  y_scale;
}

void decals_initialize(void)
{
  global_decal_data = game_state_data_new("decals", MAXIMUM_DECALS,
                                         sizeof(decal_datum_t));
  assert_halt(global_decal_data);
  global_decal_data->identifier_zero_invalid = 1;
  decal_globals = (decal_globals_t *)game_state_malloc(
    "decal globals", 0, sizeof(decal_globals_t));
  assert_halt(decal_globals);
  rasterizer_decals_initialize();
  decal_counts_0 = 0;
  decal_counts_1 = 0;
}

void decals_initialize_for_new_map(void)
{
  assert_halt(global_decal_data);
  assert_halt(decal_globals);
  csmemset(decal_globals->first_decal_index, 0xFF,
           sizeof(decal_globals->first_decal_index));
  decal_globals->first_disconnected_decal_index = NONE;
  decal_globals->locked_count = 0;
  decal_globals->permanent_count = 0;
  data_delete_all(global_decal_data);
  rasterizer_decals_initialize_for_new_map();
  decal_counts_0 = 0;
  decal_counts_1 = 0;
}

void decals_dispose_from_old_map(void)
{
  assert_halt(global_decal_data);
  assert_halt(decal_globals);
  rasterizer_decals_dispose_from_old_map();
  data_make_invalid(global_decal_data);
}

void decals_dispose(void)
{
  global_decal_data = 0;
  rasterizer_decals_dispose();
}

/* decals_update_for_new_map (0x98e70)
 *
 * Scans every live decal entry and clears transient-lifetime flags:
 *   bit 0 (0x1) = "locked"   — always cleared, decrements locked count
 *   bit 1 (0x2) = "permanent" — only cleared when full_reset is true,
 *                               decrements permanent count
 *
 * After the scan, if either count is non-zero an error is logged once
 * (per-static error-once flag at 0x4557de / 0x4557df) and the count is
 * reset to zero.  Finally, the two decal counters (decal_counts_0,
 * decal_counts_1) are zeroed.
 *
 * The bit fields are in the uint16_t at decal_entry+0x2; the counts
 * live in decal_globals+0x2804 (locked) and decal_globals+0x2808 (permanent).
 */
void decals_update_for_new_map(bool full_reset)
{
  data_iter_t iter;
  decal_datum_t *decal;

  assert_halt(global_decal_data);

  if (global_decal_data->valid) {
    assert_halt(decal_globals);

    data_iterator_new(&iter, global_decal_data);
    while ((decal = (decal_datum_t *)data_iterator_next(&iter)) != NULL) {
      if (decal->flags & 1) {
        /* Clear locked flag and decrement locked count. */
        decal->flags &= ~1;
        decal_globals->locked_count--;
      }
      if (full_reset && (decal->flags & 2)) {
        /* Clear permanent flag and decrement permanent count. */
        decal->flags &= ~2;
        decal_globals->permanent_count--;
      }
    }

    /* Sanity-check locked count.
     * Original uses absolute BSS bytes at 0x4557de/0x4557df (function-static
     * latches). Named kb.json symbols compile through a reloc and drop VC71. */
    if (decal_globals->locked_count != 0) {
      if (*(uint8_t *)0x4557de == 0) {
        error(
          2, "### ERROR decals: locked count is invalid (#%d) -- tell Bernie!!",
          decal_globals->locked_count);
        *(uint8_t *)0x4557de = 1;
      }
      decal_globals->locked_count = 0;
    }

    /* Sanity-check permanent count (only meaningful on full reset). */
    if (full_reset) {
      if (decal_globals->permanent_count != 0) {
        if (*(uint8_t *)0x4557df == 0) {
          error(
            2,
            "### ERROR decals: permanent count is invalid (#%d) -- tell Bernie!!",
            decal_globals->permanent_count);
          *(uint8_t *)0x4557df = 1;
        }
        decal_globals->permanent_count = 0;
      }
    }
  }

  decal_counts_0 = 0;
  decal_counts_1 = 0;
}

/* decal_cluster_get_first_index (0x98fe0)
 *
 * Returns the first decal datum index for the given cluster and layer.
 * Validates that cluster_index is in [0, 512) and layer is in [0, 5).
 * Indexes into the decal_globals array: [layer * 512 + cluster_index]. */
int FUN_00098fe0(int16_t cluster_index, int16_t layer)
{
  assert_halt(cluster_index >= 0 &&
              cluster_index < MAXIMUM_CLUSTERS_PER_STRUCTURE);
  assert_halt(layer >= 0 && layer < NUMBER_OF_DECAL_LAYERS);

  return decal_globals->first_decal_index[layer][cluster_index];
}

/* Projection axis remapping table at 0x28cb10. */
static const int16_t g_projection_axes[6][2] = {
  { 2, 1 }, { 1, 2 }, { 0, 2 }, { 2, 0 }, { 1, 0 }, { 0, 1 },
};

/* project_point2d (0x992d0)
 *
 * Unprojects a 2D point back to 3D using a plane equation and a projection
 * axis.  The projection axis (0=x, 1=y, 2=z) selects which coordinate to
 * solve for; the sign bit picks one of two axis-remapping orders from
 * g_projection_axes.  The two known 2D coordinates are placed into
 * out_point at the remapped axis slots, and the third is computed from
 * the plane equation  (d - a*u - b*v) / c.  If the plane's coefficient
 * along the projection axis is near zero, a large sentinel value is stored
 * instead.
 *
 * Inlined from ..\\math\\real_math.h (line 0x36f).
 */
void project_point2d(float *point_2d, float *plane, int16_t projection,
                     uint8_t sign, float *out_point)
{
  int proj_i;
  int table_index;
  int16_t axis_a;
  int16_t axis_b;

  proj_i = (int)projection;
  table_index = (uint32_t)sign + proj_i * 2;
  axis_a = g_projection_axes[table_index][0];
  axis_b = g_projection_axes[table_index][1];

  if (projection < 0 || projection > 2) {
    display_assert("projection>=_x && projection<=_z", "..\\math\\real_math.h",
                   0x36f, true);
    system_exit(-1);
  }

  if (~(sign & ~1) == 0) {
    display_assert("~(sign&~1)", "..\\math\\real_math.h", 0x370, true);
    system_exit(-1);
  }

  out_point[(int)axis_a] = point_2d[0];
  out_point[(int)axis_b] = point_2d[1];

  if (x87_fabs(plane[proj_i]) < *(double *)0x2533d0) {
    out_point[proj_i] = *(float *)0x2533c0;
    return;
  }

  out_point[proj_i] = (plane[3] - plane[(int)axis_a] * point_2d[0] -
                       plane[(int)axis_b] * point_2d[1]) /
                      plane[proj_i];
}

/* triple_product3d (0x993b0)
 *
 * Computes the scalar triple product: dot(cross(p, q), r).
 * Equivalent to the signed volume of the parallelepiped formed by vectors
 * p, q, r. Returns a float. */
float triple_product3d(float *p, float *q, float *r)
{
  float cross[3];

  cross[0] = p[1] * q[2] - p[2] * q[1];
  cross[1] = p[2] * q[0] - p[0] * q[2];
  cross[2] = p[0] * q[1] - q[0] * p[1];

  return cross[0] * r[0] + cross[1] * r[1] + cross[2] * r[2];
}

/* plane2d_from_points (0x99400)
 *
 * Computes a 2D line equation (normal + distance) from two 2D points.
 * The line normal is the perpendicular of (point_b - point_a), normalized.
 * out_line[0..1] = unit normal, out_line[2] = signed distance from origin.
 * Returns out_line on success, or NULL if the two points are coincident
 * (length below epsilon) or if the length equals the sentinel value at
 * 0x2533c0.
 */
float *plane2d_from_points(float *out_line, float *point_a, float *point_b)
{
  double length;
  float inv_length;
  float norm_a;
  float norm_b;

  out_line[0] = point_b[1] - point_a[1];
  out_line[1] = point_a[0] - point_b[0];

  length = x87_sqrtd(out_line[0] * out_line[0] + out_line[1] * out_line[1]);

  if (!(fabs(length) < *(double *)0x2533d0)) {
    inv_length = *(float *)0x2533c8 / (float)length;
    norm_a = inv_length * out_line[0];
    out_line[0] = norm_a;
    norm_b = inv_length * out_line[1];
    out_line[1] = norm_b;

    if ((float)length != *(float *)0x2533c0) {
      out_line[2] = norm_a * point_a[0] + norm_b * point_a[1];
      return out_line;
    }
  }

  out_line[2] = 0.0f;
  return NULL;
}

/*
 * FUN_00099490 — build a 3D plane from a point on the plane and its normal.
 *
 * plane_out[0..2] = normal[0..2] (copied via integer moves in the original,
 * which is what VC71 emits for plain float assignment), and
 * plane_out[3] = dot(plane_out[0..2], point).
 *
 * NOTE (binary fidelity): the dot product's left operand is re-read out of
 * plane_out (EAX), not out of the normal argument, even though the two hold
 * equal values after the copy. Preserve that shape — substituting normal[i]
 * changes the emitted loads. Terms are written x + y + z in source order; the
 * x87 push order in the original (z, then y, then x with FADDPs) is exactly
 * what MSVC emits for left-to-right evaluation of that expression.
 *
 * 0x99490 / decals.obj
 */
void FUN_00099490(float *plane_out, float *point, float *normal)
{
  *(vector3_t *)plane_out = *(vector3_t *)normal;
  plane_out[3] =
    ((plane_out[2] * point[2] + plane_out[1] * point[1]) + plane_out[0] * point[0]);
}

/* Signed distance from a point to a plane (normal·point - d). */
float plane3d_distance_to_point(float *plane, float *point)
{
  return plane[0] * point[0] + plane[1] * point[1] + plane[2] * point[2] -
         plane[3];
}

uint32_t real_a_rgb_color_to_pixel32(float alpha, float *color)
{
  volatile float scale;
  long temp;
  long pixel;

  scale = 255.0f;
  if (!(alpha >= 0.0f && alpha <= 1.0f)) {
    display_assert("alpha>=0.0f && alpha<=1.0f",
                   "..\\bitmaps\\bitmaps_inlines.h", 0xf3, true);
    system_exit(-1);
  }

  if (!valid_real_rgb_color(color)) {
    display_assert(
      csprintf(error_string_buffer,
               "%s: assert_valid_real_rgb_color(%f, %f, %f)", "color",
               (double)color[0], (double)color[1], (double)color[2]),
      "..\\bitmaps\\bitmaps_inlines.h", 0xf4, true);
    system_exit(-1);
  }

  temp = x87_round_to_int(color[2] * scale);
  temp &= 0xff;
  pixel = temp;

  temp = x87_round_to_int(color[1] * scale);
  temp &= 0xff;
  temp <<= 8;
  pixel |= temp;

  temp = x87_round_to_int(color[0] * scale);
  temp &= 0xff;
  temp <<= 16;
  pixel |= temp;

  temp = x87_round_to_int(alpha * scale);
  temp <<= 24;
  pixel |= temp;

  return (uint32_t)pixel;
}

/* bsp3d_get_plane_from_designator (0x99640)
 *
 * Extracts a plane equation from a BSP's plane tag block.  The plane_reference
 * encodes both the plane index (low 31 bits) and a sign-flip flag (bit 31).
 * When the sign bit is set, all four plane components (normal xyz + d) are
 * negated, effectively flipping the plane to face the opposite direction.
 * Each plane element is 0x10 bytes (four floats: i, j, k, d).
 */
void bsp3d_get_plane_from_designator(int structure_bsp,
                                     uint32_t plane_reference, float *out_plane)
{
  real_plane3d *plane_data;

  plane_data = (real_plane3d *)tag_block_get_element(
    (char *)&((collision_bsp_t *)structure_bsp)->planes,
    (int)(plane_reference & 0x7fffffff), 0x10);

  if (plane_reference & 0x80000000) {
    out_plane[0] = -plane_data->normal[0];
    out_plane[1] = -plane_data->normal[1];
    out_plane[2] = -plane_data->normal[2];
    out_plane[3] = -plane_data->d;
  } else {
    *(real_plane3d *)out_plane = *plane_data;
  }
}

/*
 * decal_update — age one decal, fading or retiring it.
 *
 * Computes the decal's age in seconds from the game clock, then either
 * retires it (age has reached its lifetime) or sets its alpha from the
 * fade-out ramp. Decals flagged permanent (bit 1) stay at full alpha.
 *
 * Retirement releases the decal's claim on the locked-decal budget if it held
 * one (flag bit 0), then calls the rasterizer-side free FUN_0017cb10. The
 * "tell Bernie" underflow warning is the same one-shot pattern as in
 * decals_update_for_new_map above, with its own latch byte (0x4557dc here,
 * 0x4557dd there) so the two sites report independently.
 *
 * Decal fields used:
 *   +0x02 int16_t  flags: bit 0 = counted against the locked budget,
 *                         bit 1 = permanent (never fades or expires)
 *   +0x14 int      birth game time, in ticks
 *   +0x1c float    lifetime, in seconds
 *   +0x20 float    fade-out duration, in seconds
 *   +0x28 uint8_t  alpha
 *   +0x2c int      definition_index ('deca' tag index)
 *
 * ABI: one register param, no stack params (plain RET, and the single call
 * site @00099fb3 in decals_update does no cleanup). `decal_index` arrives in
 * EDI (@<edi>): PUSH EDI @000996bc forwards it as datum_get's second argument
 * with no prior write to EDI in this function, and EDI is never popped --
 * ADD ESP,8 @000996c3 after three pushes proves PUSH ESI @000996bb is the
 * callee-save (ESI is popped at all three exits) while EDI and EAX are the two
 * arguments. It is forwarded again to FUN_0017cb10 @0009978b. Ghidra typed the
 * function void(void).
 *
 * Confirmed: FCOMP branch polarities, decoded from the FNSTSW mask bits
 *            (AH bit0 = C0/less, bit2 = C2/unordered, bit6 = C3/equal) rather
 *            than from an idiom table:
 *              TEST AH,0x44 + JNP @00099732 -> taken iff equal
 *              TEST AH,0x05 + JNP @0009973f -> taken iff age < lifetime
 *              TEST AH,0x41 + JNE @000997a3 -> taken iff lifetime <= 0
 *              TEST AH,0x05 + JP  @000997ca -> taken iff remaining >= fade
 *              TEST AH,0x01 + JNE @000997da -> taken iff f < 0
 *              TEST AH,0x41 + JNP @000997ea -> taken iff f <= 1
 * Confirmed: constants read out of the XBE -- 0x2533c0 = 0.0f,
 *            0x2533c8 = 1.0f, 0x269dc0 = 0.033333335f (one tick in seconds),
 *            0x2602c8 = 255.0f. Assert strings at 0x26a030 and 0x26a01c,
 *            file string 0x269e0c; both tails call system_exit(-1), not
 *            halt_and_catch_fire.
 * Confirmed: the alpha=0xff store @0009971d sits between the TEST and the Jcc
 *            of the permanent-flag branch, so it runs unconditionally.
 * Uncertain: the tag_get result is loaded into EAX and never read. The call is
 *            kept because it is a load-bearing side effect (it faults on an
 *            invalid tag index), but the original source presumably bound it to
 *            a variable this path does not use.
 * Uncertain: the final conversion is a bare FLD/FISTP @0009981b with no
 *            control-word change, so it uses the FPU's round-to-nearest mode --
 *            NOT a C (int) cast, which must truncate. x87_round_to_int
 *            reproduces the instruction; a plain cast would differ by one for
 *            fractional products.
 *
 * 0x996b0 / decals.obj
 */
void decal_update(int decal_index)
{
  decal_datum_t *decal;
  int16_t flags;
  float age;
  float f;

  decal = (decal_datum_t *)datum_get(global_decal_data, decal_index);
  age = (float)(game_time_get() - decal->birth_time) * 0.033333335f;

  if (decal->definition_index == NONE) {
    display_assert("decal->definition_index!=NONE",
                   "c:\\halo\\SOURCE\\effects\\decals.c", 0x133, true);
    system_exit(-1);
  }
  tag_get(TAG_GROUP_DECAL, decal->definition_index);

  flags = decal->flags;
  decal->alpha = 0xff;
  if ((flags & (1 << _decal_permanent_bit)) != 0)
    return;

  if (decal->lifetime != 0.0f && age >= decal->lifetime) {
    if ((flags & (1 << _decal_locked_bit)) != 0) {
      decal->flags = (int16_t)(flags & 0xfffe);
      decal_globals->locked_count -= 1;
      if (decal_globals->locked_count < 0 &&
          decals_reported_locked_count_update == 0) {
        error(
          2, "### ERROR decals: locked count is invalid (#%d) -- tell Bernie!!",
          decal_globals->locked_count);
        decals_reported_locked_count_update = 1;
      }
    }
    FUN_0017cb10(decal_index);
    return;
  }

  if (decal->lifetime <= 0.0f)
    return;
  if (decal->decay_time <= 0.0f)
    return;
  if (decal->lifetime - age >= decal->decay_time)
    return;

  f = (decal->lifetime - age) / decal->decay_time;
  if (!(f >= 0.0f && f <= 1.0f)) {
    display_assert("f>=0.0f && f<=1.0f", "c:\\halo\\SOURCE\\effects\\decals.c",
                   0x142, true);
    system_exit(-1);
  }
  decal->alpha = (uint8_t)x87_round_to_int(f * 255.0f);
}

/*
 * FUN_00099840 — prepend a decal to the cluster/layer linked list.
 *
 * Reads the current list head via FUN_00098fe0, then initialises the decal's
 * link fields (prev=-1, next=old_head, cluster_index, layer) via datum_get on
 * global_decal_data. If the old head exists it back-links its prev to the new
 * decal. Finally calls FUN_00098aa0 to update the list head.
 *
 * cluster_index@<ecx>, layer@<ax> are register args; decal_handle is on the
 * stack. ESI=cluster_index, EDI=layer are preserved throughout for the
 * FUN_00098aa0 call.
 *
 * 0x99840 / decals.obj
 */
void FUN_00099840(int16_t cluster_index, int16_t layer, int decal_handle)
{
  int old_head;
  decal_datum_t *decal;
  decal_datum_t *old_head_decal;

  old_head = FUN_00098fe0(cluster_index, layer);
  decal = (decal_datum_t *)datum_get(global_decal_data, decal_handle);
  decal->previous_decal_index = NONE;
  decal->next_decal_index = old_head;
  decal->cluster_index = cluster_index;
  decal->layer = layer;
  if (old_head != NONE) {
    old_head_decal = (decal_datum_t *)datum_get(global_decal_data, old_head);
    old_head_decal->previous_decal_index = decal_handle;
  }
  FUN_00098aa0(cluster_index, layer, decal_handle);
}

static void decals_log_invalid_decal_type_once(
  int16_t decal_type, int decal_tag_index, const char *decal_name,
  int bitmap_tag_index, const char *bitmap_name, const char *context);

int FUN_000998b0(int new_index_hint, int16_t cluster_index, int16_t layer,
                 int old_index, bool randomize)
{
  int decal_index;
  decal_datum_t *decal;
  int previous_index;
  decal_datum_t *previous_decal;
  decal_datum_t *existing_decal;
  int random_scaled;
  int16_t unlock_passes;
  data_iter_t iter;
  decal_datum_t *candidate;

  decal_index = data_new_datum(global_decal_data, new_index_hint);
  unlock_passes = 0;

  if (cluster_index < 0 || cluster_index >= MAXIMUM_CLUSTERS_PER_STRUCTURE) {
    display_assert(
      "cluster_index>=0 && cluster_index<MAXIMUM_CLUSTERS_PER_STRUCTURE",
      "c:\\halo\\SOURCE\\effects\\decals.c", 0x1df, true);
    system_exit(-1);
  }

  if (layer < 0 || layer >= NUMBER_OF_DECAL_LAYERS) {
    display_assert("layer>=0 && layer<NUMBER_OF_DECAL_LAYERS",
                   "c:\\halo\\SOURCE\\effects\\decals.c", 0x1e0, true);
    system_exit(-1);
  }

  if (decal_index != NONE) {
    decal = (decal_datum_t *)datum_get(global_decal_data, decal_index);

    if (randomize) {
      decal->flags = (int16_t)(1 << _decal_permanent_bit);
      decal_globals->permanent_count += 1;
    } else {
      random_scaled = (int)random_seed_step(random_math_get_local_seed_address()) * 100;

      if (random_scaled < 0x9fff6) {
        decal->flags = (int16_t)(1 << _decal_locked_bit);
        decal_globals->locked_count += 1;

        if (decal_globals->locked_count > MAXIMUM_CLUSTERS_PER_STRUCTURE) {
          data_iterator_new(&iter, global_decal_data);

          while (decal_globals->locked_count > 0x100) {
            candidate = (decal_datum_t *)data_iterator_next(&iter);
            if (candidate == NULL) {
              data_iterator_new(&iter, global_decal_data);
              unlock_passes += 1;
              if (unlock_passes >= 100) {
                error(2, "### ERROR decals: failed to unlock decals during "
                         "insert -- tell Bernie!!");
                return NONE;
              }
            } else if ((candidate->flags & (1 << _decal_locked_bit)) != 0) {
              random_scaled = (int)random_seed_step(random_math_get_local_seed_address()) * 100;

              if (random_scaled < 0x28ffd7 || candidate->cluster_index == NONE) {
                *(uint8_t *)&candidate->flags &= (uint8_t)~(1 << _decal_locked_bit);
                decal_globals->locked_count -= 1;
              }
            }
          }

          if (decal_globals->locked_count < 0 &&
              decals_reported_locked_count_insert == 0) {
            error(
              2,
              "### ERROR decals: locked count is invalid (#%d) -- tell Bernie!!",
              decal_globals->locked_count);
            decals_reported_locked_count_insert = 1;
          }
        }
      } else {
        decal->flags = 0;
      }
    }

    if (old_index != NONE) {
      existing_decal = (decal_datum_t *)datum_get(global_decal_data, old_index);
      if (existing_decal->cluster_index != cluster_index) {
        display_assert("next->cluster_index==cluster_index",
                       "c:\\halo\\SOURCE\\effects\\decals.c", 0x22c, true);
        system_exit(-1);
      }

      previous_index = existing_decal->previous_decal_index;
      if (previous_index != NONE) {
        previous_decal =
          (decal_datum_t *)datum_get(global_decal_data, previous_index);
        previous_decal->next_decal_index = decal_index;
      } else {
        FUN_00098aa0(cluster_index, layer, decal_index);
      }

      existing_decal->previous_decal_index = decal_index;
      decal->previous_decal_index = decal_index;
      decal->layer = layer;
      decal->next_decal_index = old_index;
      decal->cluster_index = cluster_index;
    } else {
      FUN_00099840(cluster_index, layer, decal_index);
    }
  } else {
    error(2, "### ERROR failed to insert decal");
  }

  return decal_index;
}

/*
 * decals_reconnect_to_structure_bsp — walk the disconnected-decal list
 * (decal_globals->first_disconnected_decal_index at +0x2800) and reattach
 * each decal to its structure-BSP cluster. For every decal on the list the
 * next handle (+0x34) is cached BEFORE any relinking, the decal is
 * consistency-checked (FUN_00098970), its cluster is resolved from the decal
 * position (+8) via scenario_location_from_point, and when a valid cluster
 * is found the decal is unlinked from the disconnected list (repairing
 * neighbour prev/next at +0x30/+0x34, or the list head at +0x2800 when it is
 * the first entry) and prepended to the cluster/layer list via FUN_00099840.
 * A post-incremented guard counter aborts with an error after 0x801
 * iterations.
 *
 * 0x99b70 / decals.obj
 */
void decals_reconnect_to_structure_bsp(void)
{
  int guard;
  int location[2]; /* scenario_location, 6 bytes; cluster_index at +4 */
  int decal_index;
  int next;
  decal_datum_t *decal;
  decal_datum_t *other;

  if (global_decal_data == NULL) {
    display_assert("global_decal_data", "c:\\halo\\SOURCE\\effects\\decals.c",
                   0x279, true);
    system_exit(-1);
  }

  if (global_decal_data->valid) {
    guard = 0;
    if (decal_globals == NULL) {
      display_assert("decal_globals", "c:\\halo\\SOURCE\\effects\\decals.c",
                     0x281, true);
      system_exit(-1);
    }
    decal_index = decal_globals->first_disconnected_decal_index;
    if (decal_index != NONE) {
      do {
        decal = (decal_datum_t *)datum_get(global_decal_data, decal_index);
        /* cache next BEFORE the unlink below rewrites neighbour links */
        next = decal->next_decal_index;
        if (guard++ > MAXIMUM_DECALS) {
          error(2, "### ERROR decals: infinite loop -- tell Bernie!!");
          break;
        }
        if (decal->cluster_index != NONE) {
          display_assert("decal->cluster_index==NONE",
                         "c:\\halo\\SOURCE\\effects\\decals.c", 0x294, true);
          system_exit(-1);
        }
        if (decal->layer < 0 || decal->layer >= NUMBER_OF_DECAL_LAYERS) {
          display_assert(
            "decal->layer>=0 && decal->layer<NUMBER_OF_DECAL_LAYERS",
            "c:\\halo\\SOURCE\\effects\\decals.c", 0x295, true);
          system_exit(-1);
        }
        FUN_00098970(decal_index, false);
        scenario_location_from_point(location, decal->position);
        if (*(int16_t *)((char *)location + 4) != NONE) {
          if (decal->next_decal_index != NONE) {
            other = (decal_datum_t *)datum_get(global_decal_data,
                                               decal->next_decal_index);
            other->previous_decal_index = decal->previous_decal_index;
          }
          if (decal->previous_decal_index != NONE) {
            other = (decal_datum_t *)datum_get(global_decal_data,
                                               decal->previous_decal_index);
            other->next_decal_index = decal->next_decal_index;
          } else {
            if (decal_globals->first_disconnected_decal_index != decal_index) {
              display_assert(
                "decal_globals->first_disconnected_decal_index==decal_index",
                "c:\\halo\\SOURCE\\effects\\decals.c", 0x2aa, true);
              system_exit(-1);
            }
            decal_globals->first_disconnected_decal_index =
              decal->next_decal_index;
          }
          FUN_00099840(*(int16_t *)((char *)location + 4), decal->layer,
                       decal_index);
        }
        FUN_00098970(decal_index, false);
        decal_index = next;
      } while (next != NONE);
    }
  }
}

/*
 * decals_disconnect_from_structure_bsp — move every clustered decal onto the
 * disconnected list.
 *
 * Walks all [cluster][layer] lists (layer inner, cluster outer). Every decal
 * visited has its cluster_index (+4, int16_t) cleared to NONE. When the walk
 * reaches the tail of a list (next at +0x34 == NONE) the whole list is spliced
 * onto the front of the disconnected list: the tail's next takes the old
 * disconnected head (decal_globals + 0x2800), that old head's prev (+0x30) is
 * repaired to point at the tail, the disconnected head becomes the list's
 * ORIGINAL first index (the FUN_00098fe0 result, not the current node), and
 * the [layer][cluster] slot is cleared to NONE. The trailing bound asserts
 * (source lines 0xd8/0xd9) come from decal_set_first_decal_index being inlined
 * here. A post-incremented guard aborts a list after 0x801 iterations.
 *
 * 0x99d60 / decals.obj
 */
void decals_disconnect_from_structure_bsp(void)
{
  int16_t cluster_index;
  int16_t layer;
  int first;
  int guard;
  int decal_index;
  int current;
  decal_datum_t *decal;
  decal_datum_t *other;

  if (global_decal_data == NULL) {
    display_assert("global_decal_data", "c:\\halo\\SOURCE\\effects\\decals.c",
                   0x2c9, true);
    system_exit(-1);
  }

  if (global_decal_data->valid) {
    if (decal_globals == NULL) {
      display_assert("decal_globals", "c:\\halo\\SOURCE\\effects\\decals.c",
                     0x2cf, true);
      system_exit(-1);
    }

    for (cluster_index = 0; cluster_index < MAXIMUM_CLUSTERS_PER_STRUCTURE;
         ++cluster_index) {
      for (layer = 0; layer < NUMBER_OF_DECAL_LAYERS; ++layer) {
        first = FUN_00098fe0(cluster_index, layer);
        guard = 0;
        decal_index = first;

        while (decal_index != NONE) {
          current = decal_index;
          decal = (decal_datum_t *)datum_get(global_decal_data, current);
          /* latch next BEFORE the splice below rewrites next_decal_index */
          decal_index = decal->next_decal_index;

          if (guard++ > MAXIMUM_DECALS) {
            error(2, "### ERROR decals: infinite loop -- tell Bernie!!");
            break;
          }

          if (decal->cluster_index != cluster_index) {
            display_assert("decal->cluster_index==cluster_index",
                           "c:\\halo\\SOURCE\\effects\\decals.c", 0x2ea, true);
            system_exit(-1);
          }
          decal->cluster_index = (int16_t)NONE;

          if (decal->next_decal_index == NONE) {
            decal->next_decal_index =
              decal_globals->first_disconnected_decal_index;
            if (decal_globals->first_disconnected_decal_index != NONE) {
              other = (decal_datum_t *)datum_get(
                global_decal_data,
                decal_globals->first_disconnected_decal_index);
              other->previous_decal_index = current;
            }
            decal_globals->first_disconnected_decal_index = first;

            if (cluster_index < 0 ||
                cluster_index >= MAXIMUM_CLUSTERS_PER_STRUCTURE) {
              display_assert("cluster_index>=0 && "
                             "cluster_index<MAXIMUM_CLUSTERS_PER_STRUCTURE",
                             "c:\\halo\\SOURCE\\effects\\decals.c", 0xd8, true);
              system_exit(-1);
            }
            if (layer < 0 || layer >= NUMBER_OF_DECAL_LAYERS) {
              display_assert("layer>=0 && layer<NUMBER_OF_DECAL_LAYERS",
                             "c:\\halo\\SOURCE\\effects\\decals.c", 0xd9, true);
              system_exit(-1);
            }
            decal_globals->first_decal_index[layer][cluster_index] = NONE;
          }
        }
      }
    }
  }
}

/*
 * decals_update — age every live decal once per tick.
 *
 * Walks the decal pool with the standard data_iterator pair and calls
 * decal_update for each element. The whole pass is skipped when the pool's
 * "valid" byte at +0x24 is clear (pool not initialised for the current map),
 * matching decals_update_for_new_map above -- but note there is no
 * assert_halt on the pool pointer here: the original loads [0x005aa8b8]
 * straight into EAX @00099f86 and dereferences +0x24 with no null check.
 *
 * The pool pointer is read ONCE and reused as data_iterator_new's argument
 * (PUSH EAX @00099f92 re-uses the same EAX the +0x24 test loaded), so the
 * local `data` here is deliberate, not a caching optimisation.
 *
 * decal_update takes its decal handle in EDI (@<edi>); the original sources
 * it from the iterator's datum_handle field at iter+0x8
 * (MOV EDI,[EBP-0x8] @00099fb0 with iter based at EBP-0x10), not from
 * data_iterator_next's returned element pointer.
 */
void decals_update(void)
{
  data_t *data;
  data_iter_t iter;
  void *elem;

  data = global_decal_data;
  if (*(uint8_t *)((char *)data + 0x24) != 0) {
    data_iterator_new(&iter, data);
    elem = data_iterator_next(&iter);
    while (elem != NULL) {
      decal_update((int)iter.datum_handle);
      elem = data_iterator_next(&iter);
    }
  }
}

void decals_delete_permanent_from_cluster(int16_t cluster_index)
{
  if (cluster_index < 0 || cluster_index >= MAXIMUM_CLUSTERS_PER_STRUCTURE) {
    display_assert(
      "cluster_index>=0 && cluster_index<MAXIMUM_CLUSTERS_PER_STRUCTURE",
      "c:\\halo\\SOURCE\\effects\\decals.c", 0x377, true);
    system_exit(-1);
  }

  if (*(uint8_t *)((char *)global_decal_data + 0x24) != 0) {
    int16_t layer;

    if (decal_globals == NULL) {
      display_assert("decal_globals", "c:\\halo\\SOURCE\\effects\\decals.c",
                     0x37d, true);
      system_exit(-1);
    }

    for (layer = 0; layer < NUMBER_OF_DECAL_LAYERS; ++layer) {
      int decal_index = NONE;

      if (cluster_index == NONE) {
        if (layer == 0) {
          decal_index = decal_globals->first_disconnected_decal_index;
        }
      } else {
        decal_index = decal_globals->first_decal_index[layer][cluster_index];
      }

      while (decal_index != NONE) {
        int current_decal_index = decal_index;
        decal_datum_t *decal =
          (decal_datum_t *)datum_get(global_decal_data, current_decal_index);
        decal_index = decal->next_decal_index;

        if (decal->cluster_index != cluster_index) {
          display_assert("decal->cluster_index==cluster_index",
                         "c:\\halo\\SOURCE\\effects\\decals.c", 0x398, true);
          system_exit(-1);
        }

        if ((*(uint16_t *)&decal->flags & (1 << _decal_permanent_bit)) != 0) {
          *(uint16_t *)&decal->flags &=
            (uint16_t) ~(1 << _decal_permanent_bit);
          decal_globals->permanent_count -= 1;

          if ((*(uint8_t *)&decal->flags & (1 << _decal_locked_bit)) != 0) {
            display_assert("!TEST_FLAG(decal->flags, _decal_locked_bit)",
                           "c:\\halo\\SOURCE\\effects\\decals.c", 0x39f, true);
            system_exit(-1);
          }

          FUN_0017cb10(current_decal_index);
        }
      }
    }

    if (decal_globals->permanent_count < 0) {
      display_assert("decal_globals->permanent_count>=0",
                     "c:\\halo\\SOURCE\\effects\\decals.c", 0x3a8, true);
      system_exit(-1);
    }
  }
}

/*
 * decal_delete — unlink a decal from its list and release its datum.
 *
 * Warns once per session (two independent byte latches) when a locked
 * (+2 bit 0) or permanent (+2 bit 1) decal is deleted, repairs the
 * doubly-linked neighbours (prev at +0x30, next at +0x34), then updates the
 * list head: decal_globals->first_disconnected_decal_index (+0x2800) when
 * cluster_index (+4) is -1, otherwise the [layer][cluster] slot via
 * FUN_00098aa0. Every path ends in datum_delete.
 *
 * 0x9a160 / decals.obj
 */
void decal_delete(int decal_index)
{
  decal_datum_t *decal;
  decal_datum_t *other;
  int first;

  decal = (decal_datum_t *)datum_get(global_decal_data, decal_index);
  if (decal == NULL) {
    display_assert("decal", "c:\\halo\\SOURCE\\effects\\decals.c", 0x3b3, true);
    system_exit(-1);
  }

  if ((*(uint8_t *)&decal->flags & (1 << _decal_locked_bit)) != 0 &&
      decals_reported_locked_delete == 0) {
    error(2, "### ERROR decals: deleting locked decal (#%d) -- tell Bernie!!",
          decal_index);
    decals_reported_locked_delete = 1;
  }

  if ((*(uint8_t *)&decal->flags & (1 << _decal_permanent_bit)) != 0 &&
      decals_reported_permanent_delete == 0) {
    error(2,
          "### ERROR decals: deleting permanent decal (#%d) -- tell Bernie!!",
          decal_index);
    decals_reported_permanent_delete = 1;
  }

  if (decal->next_decal_index != NONE) {
    other = (decal_datum_t *)datum_get(global_decal_data,
                                       decal->next_decal_index);
    other->previous_decal_index = decal->previous_decal_index;
  }

  if (decal->previous_decal_index != NONE) {
    other = (decal_datum_t *)datum_get(global_decal_data,
                                       decal->previous_decal_index);
    other->next_decal_index = decal->next_decal_index;
    datum_delete(global_decal_data, decal_index);
    return;
  }

  if (decal->cluster_index == NONE) {
    if (decal_globals->first_disconnected_decal_index != decal_index) {
      display_assert(
        "decal_globals->first_disconnected_decal_index==decal_index",
        "c:\\halo\\SOURCE\\effects\\decals.c", 0x3db, true);
      system_exit(-1);
    }
    decal_globals->first_disconnected_decal_index = decal->next_decal_index;
    datum_delete(global_decal_data, decal_index);
    return;
  }

  first = FUN_00098fe0(decal->cluster_index, decal->layer);
  if (first != decal_index) {
    display_assert(
      "decal_get_first_decal_index(decal->cluster_index, decal->layer)"
      "==decal_index",
      "c:\\halo\\SOURCE\\effects\\decals.c", 0x3e0, true);
    system_exit(-1);
  }

  FUN_00098aa0(decal->cluster_index, decal->layer, decal->next_decal_index);
  datum_delete(global_decal_data, decal_index);
}

void FUN_0009a300(float *bounds, float *projection, float *basis)
{
  decal_projection_t *proj;
  float *normal;
  float projected[3];
  float nx, ny, nz;
  int16_t axis;

  if (basis == NULL) {
    display_assert("basis", "c:\\halo\\SOURCE\\effects\\decals.c", 0x410, true);
    system_exit(-1);
  }

  if (projection == NULL) {
    display_assert("projection", "c:\\halo\\SOURCE\\effects\\decals.c", 0x411,
                   true);
    system_exit(-1);
  }

  proj = (decal_projection_t *)projection;

  qmemcpy(proj->basis, basis, sizeof(proj->basis));

  *(real_plane3d *)proj->bounds = *(real_plane3d *)bounds;

  normal = proj->normal;
  *(vector3_t *)normal = *(vector3_t *)&basis[7];
  proj->field_50 = (normal[2] * basis[12] + normal[1] * basis[11]) +
                   normal[0] * basis[10];

  nx = (float)fabs(normal[0]);
  ny = (float)fabs(normal[1]);
  nz = (float)fabs(normal[2]);

  if (nz >= ny && nz >= nx) {
    axis = 2;
  } else if (ny >= nx) {
    axis = 1;
  } else {
    axis = 0;
  }

  proj->projection = axis;
  proj->sign = (uint8_t)FUN_00099270(normal, axis);

  projected[0] = bounds[0] * basis[1] + bounds[2] * basis[4] + basis[10];
  projected[1] = basis[5] * bounds[2] + basis[2] * bounds[0] + basis[11];
  projected[2] = basis[6] * bounds[2] + basis[3] * bounds[0] + basis[12];
  FUN_00061df0(projected, proj->projection, proj->sign, proj->corner[0]);

  projected[0] = bounds[1] * basis[1] + bounds[2] * basis[4] + basis[10];
  projected[1] = basis[5] * bounds[2] + basis[2] * bounds[1] + basis[11];
  projected[2] = basis[6] * bounds[2] + basis[3] * bounds[1] + basis[12];
  FUN_00061df0(projected, proj->projection, proj->sign, proj->corner[1]);

  projected[0] = bounds[1] * basis[1] + bounds[3] * basis[4] + basis[10];
  projected[1] = bounds[3] * basis[5] + basis[2] * bounds[1] + basis[11];
  projected[2] = basis[3] * bounds[1] + basis[6] * bounds[3] + basis[12];
  FUN_00061df0(projected, proj->projection, proj->sign, proj->corner[2]);

  projected[0] = bounds[3] * basis[4] + bounds[0] * basis[1] + basis[10];
  projected[1] = bounds[3] * basis[5] + basis[2] * bounds[0] + basis[11];
  projected[2] = basis[6] * bounds[3] + basis[3] * bounds[0] + basis[12];
  FUN_00061df0(projected, proj->projection, proj->sign, proj->corner[3]);

  proj->field_78 = proj->corner[1][0] - proj->corner[0][0];
  proj->field_7c = proj->corner[1][1] - proj->corner[0][1];
  proj->field_80 = proj->corner[3][0] - proj->corner[0][0];
  proj->field_84 = proj->corner[3][1] - proj->corner[0][1];
  proj->field_88 = 1.0f / (proj->field_84 * proj->field_78 -
                           proj->field_7c * proj->field_80);
}

void FUN_0009a5a0(void *geometry, float *projection, int surface_index,
                  bool allow_deviants, float scale, int16_t type,
                  int *surface_queue, int16_t *surface_queue_write_index,
                  int *deviant_surface_list, int16_t *deviant_surface_count)
{
  decal_geometry_scratch_t *geom;
  int structure_bsp;
  int *surface;
  int edges_block;
  int vertices_block;
  int edge_index;
  float plane[4];
  float angle;
  int16_t queue_write_index;
  int16_t deviant_count;
  int vertex_iteration;
  int16_t clipped_count;
  uint32_t clipped_mask;
  float *input_points;
  float *output_points;
  float projected_previous[2];
  float projected_current[2];
  float line[3];
  uint8_t clipped;
  int *edge;
  bool surface_match;
  int remote_vertex_index;
  float *remote_vertex;
  int first_vertex_index;
  float *first_vertex;
  int current_vertex_index;
  float *current_vertex;
  float segment[3];
  int candidate_surface;
  int16_t i;
  int16_t surface_count;
  float *point;
  float dx;
  float dy;
  int16_t geometry_vertex_index;
  decal_geometry_vertex_t *geometry_vertex;
  uint32_t bit;

  geom = (decal_geometry_scratch_t *)geometry;

  if (!(type >= 0 && type < NUMBER_OF_DECAL_TYPES)) {
    display_assert("type>=0 && type<NUMBER_OF_DECAL_TYPES",
                   "c:\\halo\\SOURCE\\effects\\decals.c", 0x47b, true);
    system_exit(-1);
  }

  if (surface_index == -1) {
    return;
  }

  structure_bsp = (int)global_collision_bsp_get();
  surface = (int *)tag_block_get_element((char *)structure_bsp + 0x3c,
                                         surface_index, 0xc);

  if (projection == NULL) {
    display_assert("projection", "c:\\halo\\SOURCE\\effects\\decals.c", 0x488,
                   true);
    system_exit(-1);
  }

  if (geometry == NULL) {
    display_assert("geometry", "c:\\halo\\SOURCE\\effects\\decals.c", 0x489,
                   true);
    system_exit(-1);
  }

  if (allow_deviants) {
    if (surface_queue == NULL) {
      display_assert("surface_queue", "c:\\halo\\SOURCE\\effects\\decals.c",
                     0x48d, true);
      system_exit(-1);
    }

    if (surface_queue_write_index == NULL || *surface_queue_write_index < 0 ||
        *surface_queue_write_index > 0x400) {
      display_assert(
        "surface_queue_write_index && *surface_queue_write_index>=0 "
        "&& *surface_queue_write_index<=MAXIMUM_DECAL_SURFACE_QUE"
        "UE_SIZE",
        "c:\\halo\\SOURCE\\effects\\decals.c", 0x48e, true);
      system_exit(-1);
    }

    if (deviant_surface_list == NULL) {
      display_assert("deviant_surface_list",
                     "c:\\halo\\SOURCE\\effects\\decals.c", 0x48f, true);
      system_exit(-1);
    }

    if (deviant_surface_count == NULL || *deviant_surface_count < 0 ||
        *deviant_surface_count > 0x400) {
      display_assert("deviant_surface_count && *deviant_surface_count>=0 && "
                     "*deviant_surface_count<=MAXIMUM_DECAL_SURFACE_QUEUE_SIZE",
                     "c:\\halo\\SOURCE\\effects\\decals.c", 0x490, true);
      system_exit(-1);
    }

    queue_write_index = *surface_queue_write_index;
    deviant_count = *deviant_surface_count;
  }

  bsp3d_get_plane_from_designator(structure_bsp, (uint32_t)surface[0], plane);
  angle = angle_between_normals3d(plane, projection + 0x11);

  if (!allow_deviants ||
      angle <= g_decal_type_parameters[type].field_00 * *(float *)0x253d4c) {
    edge_index = surface[1];
    edges_block = structure_bsp + 0x48;
    vertices_block = structure_bsp + 0x54;
    vertex_iteration = 0;
    clipped_count = 4;
    clipped_mask = 0;
    input_points = projection + 0x16;
    projected_previous[0] = 0.0f;
    projected_previous[1] = 0.0f;

    do {
      edge = (int *)tag_block_get_element((char *)edges_block, edge_index, 0x18);
      surface_match = edge[5] == surface_index;
      remote_vertex_index = edge[surface_match ? 0 : 1];
      remote_vertex = (float *)tag_block_get_element(
        (char *)vertices_block, remote_vertex_index, 0x10);

      output_points = g_decal_clip_buffers[vertex_iteration & 1];
      if (vertex_iteration == 0) {
        first_vertex_index = edge[surface_match ? 1 : 0];
        first_vertex = (float *)tag_block_get_element(
          (char *)vertices_block, first_vertex_index, 0x10);
        FUN_00061df0(first_vertex, *(int16_t *)((char *)projection + 0x54),
                     *(uint8_t *)((char *)projection + 0x56),
                     projected_previous);
      }

      FUN_00061df0(remote_vertex, *(int16_t *)((char *)projection + 0x54),
                   *(uint8_t *)((char *)projection + 0x56), projected_current);
      if (plane2d_from_points(line, projected_current, projected_previous) ==
          NULL) {
        clipped_count = 0;
      } else {
        clipped = 0;
        clipped_count = convex_polygon2d_clip_to_plane(
          clipped_count, input_points, line, 0xc, output_points, &clipped_mask,
          &clipped, 0.0f);

        if (allow_deviants && clipped != 0 && queue_write_index < 0x400) {
          current_vertex_index = edge[surface_match ? 1 : 0];
          current_vertex = (float *)tag_block_get_element(
            (char *)vertices_block, current_vertex_index, 0x10);

          segment[0] = current_vertex[0] - remote_vertex[0];
          segment[1] = current_vertex[1] - remote_vertex[1];
          segment[2] = current_vertex[2] - remote_vertex[2];
          if (fast_vector_intersects_sphere(
                remote_vertex, segment, projection + 0xa,
                scale * g_decal_type_parameters[type].field_08)) {
            candidate_surface = edge[surface_match ? 4 : 5];
            i = 0;

            while (candidate_surface != -1) {
              if (queue_write_index <= i) {
                surface_queue[(int)queue_write_index] = candidate_surface;
                queue_write_index += 1;
                break;
              }

              if (surface_queue[(int)i] == candidate_surface) {
                candidate_surface = -1;
              }

              i += 1;
            }
          }
        }
      }

      edge_index = edge[surface_match ? 3 : 2];
      vertex_iteration += 1;
      projected_previous[0] = projected_current[0];
      projected_previous[1] = projected_current[1];
      input_points = output_points;
    } while (edge_index != surface[1] && clipped_count > 0);

    if (clipped_count > 2 &&
        clipped_count <= MAXIMUM_DECAL_SURFACE_QUEUE_SIZE - geom->vertex_count &&
        ((*(uint8_t *)(surface + 2) & 0xb) == 0)) {
      if (geom->decal_surface_count > MAXIMUM_DECAL_SURFACE_QUEUE_SIZE - 1) {
        display_assert(
          "geometry->decal_surface_count<MAXIMUM_DECAL_SURFACE_QUEU"
          "E_SIZE",
          "c:\\halo\\SOURCE\\effects\\decals.c", 0x555, true);
        system_exit(-1);
      }

      surface_count = geom->decal_surface_count;
      geom->surfaces[(int)surface_count] = surface_index;
      geom->surface_vertex_counts[(int)surface_count] = clipped_count;
      geom->decal_surface_count = (int16_t)(surface_count + 1);

      if (clipped_count > 0) {
        point = output_points;

        for (i = 0; i < clipped_count; ++i) {
          dx = point[0] - projection[0x16];
          dy = point[1] - projection[0x17];
          geometry_vertex_index = geom->vertex_count;
          geometry_vertex =
            &geom->vertices[(int)geometry_vertex_index];
          bit = 1u << (i & 0x1f);

          geometry_vertex->uv[0] =
            (dx * projection[0x21] - dy * projection[0x20]) * projection[0x22];
          geometry_vertex->uv[1] = -(
            (dx * projection[0x1f] - dy * projection[0x1e]) * projection[0x22]);
          geometry_vertex->clipped = (boolean)((clipped_mask & bit) != 0);

          project_point2d(point, plane, *(int16_t *)((char *)projection + 0x54),
                          *(uint8_t *)((char *)projection + 0x56),
                          geometry_vertex->position);

          if ((clipped_mask & bit) == 0) {
            geometry_vertex->position[0] += plane[0] * rasterizer_zoffset;
            geometry_vertex->position[1] += plane[1] * rasterizer_zoffset;
            geometry_vertex->position[2] += plane[2] * rasterizer_zoffset;
          }

          geom->vertex_count = (int16_t)(geometry_vertex_index + 1);
          point += 2;
        }
      }
    }
  } else {
    edge_index = surface[1];
    edges_block = structure_bsp + 0x48;
    vertices_block = structure_bsp + 0x54;

    do {
      edge = (int *)tag_block_get_element((char *)edges_block, edge_index, 0x18);
      surface_match = edge[5] == surface_index;
      remote_vertex = (float *)tag_block_get_element(
        (char *)vertices_block, edge[surface_match ? 0 : 1], 0x10);

      if (queue_write_index < 0x400) {
        current_vertex = (float *)tag_block_get_element(
          (char *)vertices_block, edge[surface_match ? 1 : 0], 0x10);

        segment[0] = current_vertex[0] - remote_vertex[0];
        segment[1] = current_vertex[1] - remote_vertex[1];
        segment[2] = current_vertex[2] - remote_vertex[2];

        if (fast_vector_intersects_sphere(
              remote_vertex, segment, projection + 0xa,
              scale * g_decal_type_parameters[type].field_08)) {
          candidate_surface = edge[surface_match ? 4 : 5];
          i = 0;

          while (candidate_surface != -1) {
            if (queue_write_index <= i) {
              surface_queue[(int)queue_write_index] = candidate_surface;
              queue_write_index += 1;
              break;
            }

            if (surface_queue[(int)i] == candidate_surface) {
              candidate_surface = -1;
            }

            i += 1;
          }
        }
      }

      edge_index = edge[surface_match ? 3 : 2];
    } while (edge_index != surface[1]);

    if (angle <= g_decal_type_parameters[type].field_04 * *(float *)0x253d4c &&
        deviant_count < 0x400) {
      deviant_surface_list[(int)deviant_count] = surface_index;
      deviant_count += 1;
    }
  }

  if (allow_deviants) {
    *surface_queue_write_index = queue_write_index;
    *deviant_surface_count = deviant_count;
  }
}

static void decals_assert_or_exit(const char *condition, int line)
{
  display_assert(condition, "c:\\halo\\SOURCE\\effects\\decals.c", line, true);
  system_exit(-1);
}

static __inline float decals_dot3(const float *a, const float *b)
{
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static __inline void decals_cross3(float *out, const float *a, const float *b)
{
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

static __inline float decals_random_real(float min, float max)
{
  return random_real_range((int *)random_math_get_local_seed_address(), min,
                           max);
}

static __inline int16_t decals_random_short(int16_t min, int16_t max)
{
  return random_range(random_math_get_local_seed_address(), min, max);
}

static __inline void decals_get_signed_plane(int structure_bsp, int plane_reference,
                                            float *out_plane)
{
  bsp3d_get_plane_from_designator(
    structure_bsp, (uint32_t)plane_reference & 0x7fffffff, out_plane);

  if (plane_reference < 0) {
    out_plane[0] = -out_plane[0];
    out_plane[1] = -out_plane[1];
    out_plane[2] = -out_plane[2];
    out_plane[3] = -out_plane[3];
  }
}

static __inline void decals_build_axis(float *axis_vector, uint32_t basis, float sign,
                                      int assert_line)
{
  axis_vector[0] = 0.0f;
  axis_vector[1] = 0.0f;
  axis_vector[2] = 0.0f;

  if (basis == 0) {
    axis_vector[0] = sign;
  } else if (basis == 1) {
    axis_vector[1] = sign;
  } else if (basis == 2) {
    axis_vector[2] = sign;
  } else {
    decals_assert_or_exit((char *)0x26a7e4, assert_line);
  }
}

static void
decals_log_invalid_decal_type_once(int16_t decal_type, int decal_tag_index,
                                   const char *decal_name, int bitmap_tag_index,
                                   const char *bitmap_name, const char *context)
{
  static uint32_t reported_mask;

  if (decal_type >= 0 && decal_type < 32) {
    uint32_t bit = 1u << decal_type;
    if ((reported_mask & bit) != 0) {
      return;
    }
    reported_mask |= bit;
  }

  error(2,
        "### ERROR decals: invalid decal type %d in %s (decal=%d '%s' bitm=%d "
        "'%s') -- skipping",
        decal_type, context, decal_tag_index,
        decal_name ? tag_name_strip_path((char *)decal_name) : "<null>",
        bitmap_tag_index,
        bitmap_name ? tag_name_strip_path((char *)bitmap_name) : "<null>");
}

static decal_staged_vertex_t g_decal_staged_vertices[MAXIMUM_DECAL_SURFACE_QUEUE_SIZE];
static int g_decal_grouped_surfaces[0x400];
static int g_decal_deviant_surfaces[0x400];
static int g_decal_surface_queue[0x400];
static float g_decal_projection[35];
static float g_decal_transformed_projection[35];
static bool g_warned_decal_vertex_overflow;
static bool g_warned_decal_quad_overflow;

void decal_new_from_collision(int decal_tag_index, int16_t *collision_result,
                              void *direction, float scale, bool randomize,
                              int16_t color_index, int flags)
{
  int structure_bsp;
  decal_geometry_scratch_t *geometry;
  decal_staged_vertex_t *staged_vertices;
  int *grouped_surfaces;
  int *deviant_surfaces;
  int *surface_queue;
  float *projection;
  float *transformed_projection;
  float bounds[4];
  float uv_bounds[4];
  float basis[13];
  float rotated_basis[13];
  float rotation_matrix[13];
  float center_offset[3];
  float min_normal[3];
  float max_normal[3];
  float color[3];
  int16_t sprite_index;
  int16_t selected_color;
  int16_t sequence_index;
  float size;
  bool reuse_previous;
  float tiny;
  float tiny_squared;

  structure_bsp = (int)global_collision_bsp_get();
  geometry = &g_decal_geometry;
  staged_vertices = g_decal_staged_vertices;
  grouped_surfaces = g_decal_grouped_surfaces;
  deviant_surfaces = g_decal_deviant_surfaces;
  surface_queue = g_decal_surface_queue;
  projection = g_decal_projection;
  transformed_projection = g_decal_transformed_projection;
  reuse_previous = false;
  size = 0.0f;
  sprite_index = 0;
  selected_color = 0;
  sequence_index = 0;
  tiny = (float)*(double *)0x2533d0;
  tiny_squared = *(float *)0x253f44;

  if (collision_result == NULL) {
    decals_assert_or_exit((char *)0x26a844, 0x7f1);
  }

  if (direction == NULL) {
    decals_assert_or_exit((char *)0x26a838, 0x7f2);
  }

  if (rasterizer_environment_decals == 0) {
    decals_assert_or_exit((char *)0x26a828, 0x7f3);
  }

  if (flags != 0) {
    decals_assert_or_exit((char *)0x26a814, 0x7f5);
  }

  while (decal_tag_index != -1) {
    char *decal_tag;
    char *bitmap_tag;
    float *normal;
    float *direction3;
    int16_t decal_type;
    int16_t queue_read_index;
    int16_t queue_write_index;
    int16_t deviant_surface_count;
    int16_t primitive_count;
    int cache_index;
    int decal_index;
    decal_datum_t *decal;
    decal_cached_quad_t *cache_quads;
    int i;

    decal_tag = (char *)tag_get(TAG_GROUP_DECAL, decal_tag_index);
    bitmap_tag = (char *)tag_get(TAG_GROUP_BITM, *(int *)(decal_tag + 0xe4));
    direction3 = (float *)direction;
    normal = (float *)(collision_result + 0x12);
    decal_type = *(int16_t *)(decal_tag + 2);

    if (decal_type < 0 || decal_type >= 4) {
      int bitmap_tag_index = *(int *)(decal_tag + 0xe4);
      const char *decal_name = tag_get_name(decal_tag_index);
      const char *bitmap_name = tag_get_name(bitmap_tag_index);
      decals_log_invalid_decal_type_once(decal_type, decal_tag_index,
                                         decal_name, bitmap_tag_index,
                                         bitmap_name, "decal_new");
      reuse_previous = (*(uint8_t *)decal_tag & 1) != 0;
      decal_tag_index = *(int *)(decal_tag + 0x14);
      continue;
    }

    if (!reuse_previous) {
      float tangent[3];
      float bitangent[3];
      float rotated_u[3];
      float rotated_v[3];
      float rotation_cos;
      float rotation_sin;
      float tangent_length;
      float bitangent_length;

      if (((*(uint16_t *)decal_tag & 8) != 0) &&
          (decals_dot3(direction3, normal) < *(float *)0x26a810)) {
        rotation_cos = -1.0f;
        rotation_sin = 0.0f;

        if ((*(uint16_t *)decal_tag & 0x20) == 0) {
          decals_cross3(tangent, normal, direction3);
          decals_cross3(bitangent, normal, tangent);
        } else {
          float axis_vector[3];
          int16_t axis = (int16_t)FUN_00099220(direction3);
          float axis_sign =
            (bool)FUN_00099270(direction3, (uint16_t)axis) ? -1.0f : 1.0f;

          decals_build_axis(axis_vector, (uint16_t)axis, axis_sign, 0x848);

          if (decals_dot3(axis_vector, normal) > *(float *)0x2533c0) {
            axis_vector[0] += normal[0];
            axis_vector[1] += normal[1];
            axis_vector[2] += normal[2];
          } else {
            axis_vector[0] -= normal[0];
            axis_vector[1] -= normal[1];
            axis_vector[2] -= normal[2];
          }

          normalize3d(axis_vector);
          decals_cross3(tangent, normal, axis_vector);
          decals_cross3(bitangent, normal, tangent);

          if (decals_dot3(tangent, tangent) < tiny_squared || /* dup-args-ok */
              decals_dot3(bitangent, bitangent) < /* dup-args-ok */
                tiny_squared) {
            float reflected[3];
            float reflected_scale = -decals_dot3(direction3, normal);

            reflected[0] = reflected_scale * normal[0] + direction3[0];
            reflected[1] = reflected_scale * normal[1] + direction3[1];
            reflected[2] = reflected_scale * normal[2] + direction3[2];

            axis = (int16_t)FUN_00099220(reflected);
            axis_sign = (bool)FUN_00099270(reflected, (uint16_t)axis) ? -1.0f : 1.0f;
            decals_build_axis(axis_vector, (uint16_t)axis, axis_sign, 0x868);

            if (decals_dot3(axis_vector, normal) > *(float *)0x2533c0) {
              axis_vector[0] += normal[0];
              axis_vector[1] += normal[1];
              axis_vector[2] += normal[2];
            } else {
              axis_vector[0] -= normal[0];
              axis_vector[1] -= normal[1];
              axis_vector[2] -= normal[2];
            }

            normalize3d(axis_vector);
            decals_cross3(tangent, normal, axis_vector);
            decals_cross3(bitangent, normal, tangent);
          }
        }
      } else {
        float angle = decals_random_real(0.0f, 6.2831855f);

        rotation_cos = x87_fcos(angle);
        rotation_sin = x87_fsin(angle);
        perpendicular3d(normal, tangent);
        decals_cross3(bitangent, normal, tangent);
      }

      tangent_length = sqrtf(tangent[0] * tangent[0] + tangent[1] * tangent[1] +
                             tangent[2] * tangent[2]);
      if (!(x87_fabs(tangent_length) < *(double *)0x2533d0)) {
        float inv_tangent_length = 1.0f / tangent_length;
        tangent[0] *= inv_tangent_length;
        tangent[1] *= inv_tangent_length;
        tangent[2] *= inv_tangent_length;
      }

      bitangent_length =
        sqrtf(bitangent[0] * bitangent[0] + bitangent[1] * bitangent[1] +
              bitangent[2] * bitangent[2]);
      if (!(x87_fabs(bitangent_length) < *(double *)0x2533d0)) {
        float inv_bitangent_length = 1.0f / bitangent_length;
        bitangent[0] *= inv_bitangent_length;
        bitangent[1] *= inv_bitangent_length;
        bitangent[2] *= inv_bitangent_length;
      }

      rotated_u[0] = rotation_cos * bitangent[0] - tangent[0] * rotation_sin;
      rotated_u[1] = rotation_cos * bitangent[1] - tangent[1] * rotation_sin;
      rotated_u[2] = rotation_cos * bitangent[2] - tangent[2] * rotation_sin;
      rotated_v[0] = tangent[0] * rotation_cos + bitangent[0] * rotation_sin;
      rotated_v[1] = tangent[1] * rotation_cos + bitangent[1] * rotation_sin;
      rotated_v[2] = tangent[2] * rotation_cos + bitangent[2] * rotation_sin;

      basis[0] = 1.0f;
      basis[1] = rotated_u[0];
      basis[2] = rotated_u[1];
      basis[3] = rotated_u[2];
      basis[4] = rotated_v[0];
      basis[5] = rotated_v[1];
      basis[6] = rotated_v[2];
      basis[7] = normal[0];
      basis[8] = normal[1];
      basis[9] = normal[2];
      basis[10] = *(float *)(collision_result + 0xc);
      basis[11] = *(float *)(collision_result + 0xe);
      basis[12] = *(float *)(collision_result + 0x10);

      selected_color = color_index;
      if (selected_color == -1) {
        selected_color =
          decals_random_short(0, *(int16_t *)(bitmap_tag + 0x54));
        if (selected_color >= *(int *)(bitmap_tag + 0x54)) {
          error(2, (char *)0x26a788);
          selected_color = *(int16_t *)(bitmap_tag + 0x54) - 1;
        }
      }

      sequence_index = 0;

      if (scale == *(float *)0x2533c0) {
        scale = 1.0f;
      }

      size = decals_random_real(*(float *)(decal_tag + 0x18),
                                *(float *)(decal_tag + 0x1c)) *
             scale;
    }

    if (*(int16_t *)bitmap_tag == 3) {
      char *sequence =
        (char *)tag_block_get_element(bitmap_tag + 0x54, selected_color, 0x40);
      int16_t *sprite =
        (int16_t *)tag_block_get_element(sequence + 0x34, sequence_index, 0x20);

      sprite_index = sprite[0];
      FUN_00098b20(uv_bounds, decal_tag, selected_color, sequence_index, size,
                   bounds);
    } else {
      float aspect = 1.0f;

      sprite_index = 0;

      if ((*(uint16_t *)decal_tag & 0x100) != 0) {
        char *bitmap =
          (char *)tag_block_get_element(bitmap_tag + 0x60, 0, 0x30);
        aspect = (float)(int)*(int16_t *)(bitmap + 6) /
                 (float)(int)*(int16_t *)(bitmap + 4);
      }

      bounds[0] = -size;
      bounds[1] = size;
      bounds[2] = -(aspect * size);
      bounds[3] = aspect * size;

      uv_bounds[0] = 0.0f;
      uv_bounds[1] = 1.0f;
      uv_bounds[2] = 0.0f;
      uv_bounds[3] = 1.0f;
    }

    if (!randomize) {
      int hardware_format =
        (int)tag_block_get_element(bitmap_tag + 0x60, sprite_index, 0x30);

      if (!xbox_texture_cache_get_hardware_format((void *)hardware_format, 0,
                                                  true)) {
        return;
      }
    }

    FUN_0009a300(bounds, projection, basis);

    min_normal[0] = basis[7];
    max_normal[0] = basis[7];
    min_normal[1] = basis[8];
    max_normal[1] = basis[8];
    min_normal[2] = basis[9];
    max_normal[2] = basis[9];

    geometry->decal_surface_count = 0;
    geometry->vertex_count = 0;

    surface_queue[0] = *(int *)((char *)collision_result + 0x44);
    queue_read_index = 0;
    queue_write_index = 1;
    deviant_surface_count = 0;

    while (queue_read_index < queue_write_index) {
      if (queue_read_index >= 0x400) {
        decals_assert_or_exit((char *)0x26a74c, 0x931);
      }

      if (queue_write_index > 0x400) {
        decals_assert_or_exit((char *)0x26a710, 0x932);
      }

      FUN_0009a5a0(geometry, projection, surface_queue[queue_read_index], true,
                   size, decal_type, surface_queue, &queue_write_index,
                   deviant_surfaces, &deviant_surface_count);
      queue_read_index += 1;
    }

    if (g_decal_type_parameters[(int)decal_type].field_0c != 0) {
      int16_t remaining = deviant_surface_count;

      while (remaining > 0) {
        bool processed_group = false;

        {
          int list_index;
          for (list_index = 0; list_index < deviant_surface_count;
               ++list_index) {
            int seed_surface;
            int grouped_count;
            float collision_plane_distance;
            float best_plane[4];
            float best_start[3];
            float best_end[3];
            float best_near;
            float best_far;
            int best_plane_reference;
            int i;

            seed_surface = deviant_surfaces[list_index];

            if (seed_surface == -1) {
              continue;
            }

            grouped_count = 0;
            collision_plane_distance = decals_dot3(basis + 7, basis + 10);
            best_start[0] = 0.0f;
            best_start[1] = 0.0f;
            best_start[2] = 0.0f;
            best_end[0] = 0.0f;
            best_end[1] = 0.0f;
            best_end[2] = 0.0f;
            best_near = 0.0f;
            best_far = 0.0f;
            best_plane_reference = -1;

            if (grouped_count >= 0x400) {
              decals_assert_or_exit((char *)0x26a6d4, 0x95f);
            }

            grouped_surfaces[grouped_count++] = seed_surface;
            deviant_surfaces[list_index] = -1;

            for (i = list_index + 1; i < deviant_surface_count; ++i) {
              int candidate_surface = deviant_surfaces[i];

              if (candidate_surface != -1) {
                int *surface = (int *)tag_block_get_element(
                  (char *)structure_bsp + 0x3c, candidate_surface, 0xc);
                float candidate_plane[4];
                float seed_plane[4];
                float angle;

                decals_get_signed_plane(structure_bsp, *(int *)surface,
                                        candidate_plane);

                surface = (int *)tag_block_get_element(
                  (char *)structure_bsp + 0x3c, seed_surface, 0xc);
                decals_get_signed_plane(structure_bsp, *(int *)surface,
                                        seed_plane);

                angle = angle_between_normals3d(seed_plane, candidate_plane);
                if (angle <= g_decal_type_parameters[(int)decal_type].field_00 *
                              *(float *)0x253d4c) {
                  if (grouped_count >= 0x400) {
                    decals_assert_or_exit((char *)0x26a6d4, 0x976);
                  }

                  grouped_surfaces[grouped_count++] = candidate_surface;
                  deviant_surfaces[i] = -1;
                }
              }
            }

            if (grouped_count < 1) {
              decals_assert_or_exit((char *)0x26a6b8, 0x9bd);
            }

            for (i = 0; i < grouped_count; ++i) {
              int grouped_surface = grouped_surfaces[i];
              int *surface = (int *)tag_block_get_element(
                (char *)structure_bsp + 0x3c, grouped_surface, 0xc);
              int edge_index = surface[1];

              do {
                int *edge = (int *)tag_block_get_element(
                  (char *)structure_bsp + 0x48, edge_index, 0x18);
                bool surface_match = edge[5] == grouped_surface;
                float *start = (float *)tag_block_get_element(
                  (char *)structure_bsp + 0x54, edge[surface_match ? 0 : 1],
                  0x10);
                float *end = (float *)tag_block_get_element(
                  (char *)structure_bsp + 0x54, edge[surface_match ? 1 : 0],
                  0x10);
                float start_distance = (float)fabs(decals_dot3(basis + 7, start) -
                                             collision_plane_distance);
                float end_distance =
                  (float)fabs(decals_dot3(basis + 7, end) - collision_plane_distance);
                float near_distance =
                  start_distance < end_distance ? start_distance : end_distance;
                float far_distance =
                  start_distance < end_distance ? end_distance : start_distance;

                if (best_plane_reference == -1 ||
                    (near_distance < best_near && far_distance < best_far)) {
                  best_plane_reference = surface[0];
                  decals_get_signed_plane(structure_bsp, best_plane_reference,
                                          best_plane);
                  best_near = near_distance;
                  best_far = far_distance;
                  best_start[0] = start[0];
                  best_start[1] = start[1];
                  best_start[2] = start[2];
                  best_end[0] = end[0];
                  best_end[1] = end[1];
                  best_end[2] = end[2];
                }

                edge_index = edge[surface_match ? 3 : 2];
              } while (edge_index != surface[1]);
            }

            for (i = 0; i < 35; ++i) {
              transformed_projection[i] = projection[i];
            }

            if (best_plane_reference == -1) {
              decals_assert_or_exit((char *)0x26a6b8, 0x9bd);
            } else {
              float axis[3];
              float axis_length;

              axis[0] = best_end[0] - best_start[0];
              axis[1] = best_end[1] - best_start[1];
              axis[2] = best_end[2] - best_start[2];

              axis_length = sqrtf(axis[0] * axis[0] + axis[1] * axis[1] +
                                  axis[2] * axis[2]);

              if ((float)fabs(axis_length) < tiny) {
                error(2, (char *)0x26a670);
              } else {
                float handedness;
                float sign;
                float angle;
                float rotated_origin[3];
                float sine;
                float cosine;

                axis[0] /= axis_length;
                axis[1] /= axis_length;
                axis[2] /= axis_length;

                if (axis_length > *(float *)0x2533c0) {
                  sign = 1.0f;
                  handedness =
                    (best_plane[1] * basis[9] - best_plane[2] * basis[8]) *
                      axis[0] +
                    (best_plane[2] * basis[7] - basis[9] * best_plane[0]) *
                      axis[1] +
                    (basis[8] * best_plane[0] - best_plane[1] * basis[7]) *
                      axis[2];

                  if (handedness > *(float *)0x2533c0) {
                    sign = -1.0f;
                  }

                  angle = angle_between_normals3d(basis + 7, best_plane) * sign;
                  sine = x87_fsin(angle);
                  cosine = x87_fcos(angle);

                  FUN_001092d0(rotation_matrix, axis, sine, cosine);

                  rotated_origin[0] = basis[10] - best_start[0];
                  rotated_origin[1] = basis[11] - best_start[1];
                  rotated_origin[2] = basis[12] - best_start[2];
                  matrix_transform_point(rotation_matrix,
                                         rotated_origin, /* dup-args-ok */
                                         rotated_origin);

                  matrix_transform_vector(rotation_matrix, basis + 1,
                                          rotated_basis + 1);
                  matrix_transform_vector(rotation_matrix, basis + 4,
                                          rotated_basis + 4);
                  matrix_transform_vector(rotation_matrix, basis + 7,
                                          rotated_basis + 7);

                  rotated_basis[0] = 1.0f;
                  rotated_basis[10] = rotated_origin[0] + best_start[0];
                  rotated_basis[11] = rotated_origin[1] + best_start[1];
                  rotated_basis[12] = rotated_origin[2] + best_start[2];

                  FUN_0009a300(bounds, transformed_projection, rotated_basis);

                  if (!(rotated_basis[7] > min_normal[0])) {
                    min_normal[0] = rotated_basis[7];
                  }
                  if (rotated_basis[7] > max_normal[0]) {
                    max_normal[0] = rotated_basis[7];
                  }
                  if (!(rotated_basis[8] > min_normal[1])) {
                    min_normal[1] = rotated_basis[8];
                  }
                  if (rotated_basis[8] > max_normal[1]) {
                    max_normal[1] = rotated_basis[8];
                  }
                  if (!(rotated_basis[9] > min_normal[2])) {
                    min_normal[2] = rotated_basis[9];
                  }
                  if (rotated_basis[9] > max_normal[2]) {
                    max_normal[2] = rotated_basis[9];
                  }
                }
              }
            }

            for (i = 0; i < grouped_count; ++i) {
              FUN_0009a5a0(geometry, transformed_projection,
                           grouped_surfaces[i], false, size, decal_type, NULL,
                           NULL, NULL, NULL);
            }

            remaining -= (int16_t)grouped_count;
            processed_group = true;
            break;
          }
        }

        if (!processed_group) {
          break;
        }
      }
    }

    if (geometry->decal_surface_count < 1 || geometry->vertex_count < 1) {
      return;
    }

    if (geometry->vertex_count > 0x400) {
      if (!g_warned_decal_vertex_overflow) {
        error(2,
              "### ERROR decals: vertex overflow (count=%d) -- skipping decal",
              geometry->vertex_count);
        g_warned_decal_vertex_overflow = true;
      }
      return;
    }

    center_offset[0] = 0.0f;
    center_offset[1] = 0.0f;
    center_offset[2] = 0.0f;

    if (max_normal[0] - min_normal[0] < *(float *)0x253398 &&
        max_normal[1] - min_normal[1] < *(float *)0x253398 &&
        max_normal[2] - min_normal[2] < *(float *)0x253398) {
      center_offset[0] = min_normal[0] + max_normal[0];
      center_offset[1] = min_normal[1] + max_normal[1];
      center_offset[2] = min_normal[2] + max_normal[2];
      normalize3d(center_offset);
      center_offset[0] *= rasterizer_zoffset;
      center_offset[1] *= rasterizer_zoffset;
      center_offset[2] *= rasterizer_zoffset;
    }

    primitive_count = 0;
    for (i = 0; i < geometry->decal_surface_count; ++i) {
      int16_t surface_vertex_count = geometry->surface_vertex_counts[i];

      if (surface_vertex_count < 3) {
        decals_assert_or_exit((char *)0x26a608, 0xa5a);
      }

      primitive_count += (int16_t)((surface_vertex_count - 1) / 2);
    }

    if (primitive_count < 0 || primitive_count > 0x400) {
      if (!g_warned_decal_quad_overflow) {
        error(2, "### ERROR decals: quad overflow (count=%d) -- skipping decal",
              primitive_count);
        g_warned_decal_quad_overflow = true;
      }
      return;
    }

    cache_index = FUN_0017cae0((uint32_t)primitive_count << 6);
    if (cache_index == -1) {
      if (*(uint8_t *)0x5aa8b4 == 0) {
        return;
      }

      error(2, (char *)0x26a498, decal_globals->locked_count,
            decal_globals->permanent_count);
      return;
    }

    decal_index = FUN_000998b0(cache_index, collision_result[8],
                               *(int16_t *)(decal_tag + 4), -1, randomize);
    if (decal_index == -1) {
      FUN_0017cb10(cache_index);

      if (*(uint8_t *)0x5aa8b4 == 0) {
        return;
      }

      error(2, (char *)0x26a4e0, decal_globals->locked_count,
            decal_globals->permanent_count);
      return;
    }

    decal = (decal_datum_t *)datum_get(global_decal_data, decal_index);
    cache_quads = (decal_cached_quad_t *)FUN_0017caf0(
      cache_index, (uint32_t)primitive_count << 6);

    if (cache_quads == NULL) {
      FUN_0017cb10(cache_index);

      if (*(uint8_t *)0x5aa8b4 == 0) {
        return;
      }

      error(2, (char *)0x26a524);
      return;
    }

    if (geometry->vertex_count > 0) {
      float u_range = uv_bounds[1] - uv_bounds[0];
      float v_range = uv_bounds[3] - uv_bounds[2];

      for (i = 0; i < geometry->vertex_count; ++i) {
        decal_geometry_vertex_t *vertex = &geometry->vertices[i];
        float u = u_range * vertex->uv[0] + uv_bounds[0];
        float v = v_range * vertex->uv[1] + uv_bounds[2];
        int32_t packed_u;
        int32_t packed_v;

        if (u < *(float *)0x2533c0) {
          u = 0.0f;
        } else if (*(float *)0x2533c8 < u) {
          u = 1.0f;
        }

        if (v < *(float *)0x2533c0) {
          v = 0.0f;
        } else if (*(float *)0x2533c8 < v) {
          v = 1.0f;
        }

        u *= *(float *)0x26a604;
        if (u < *(float *)0x2533c0) {
          u = 0.0f;
        } else if (*(float *)0x26a600 < u) {
          u = *(float *)0x26a600;
        }

        v *= *(float *)0x26a604;
        if (v < *(float *)0x2533c0) {
          v = 0.0f;
        } else if (*(float *)0x26a600 < v) {
          v = *(float *)0x26a600;
        }

        packed_u = (int32_t)floor((double)(u + *(float *)0x253398));
        packed_v = (int32_t)floor((double)(v + *(float *)0x253398));

        if (((packed_u & 0x8000) != 0) || ((packed_v & 0x8000) != 0)) {
          decals_assert_or_exit((char *)0x26a5e0, 0xa88);
        }

        staged_vertices[i].position[0] = center_offset[0] + vertex->position[0];
        staged_vertices[i].position[1] = center_offset[1] + vertex->position[1];
        staged_vertices[i].position[2] = center_offset[2] + vertex->position[2];
        staged_vertices[i].uv[0] = (int16_t)packed_u;
        staged_vertices[i].uv[1] = (int16_t)packed_v;
      }
    }

    decal->position[0] = *(float *)(collision_result + 0xc);
    decal->position[1] = *(float *)(collision_result + 0xe);
    decal->position[2] = *(float *)(collision_result + 0x10);
    decal->birth_time = game_time_get();
    decal->sprite_index = (uint8_t)sprite_index;
    decal->color_index = (uint8_t)selected_color;
    decal->field_1a = 0;
    decal->lifetime = decals_random_real(*(float *)(decal_tag + 0x78),
                                         *(float *)(decal_tag + 0x7c));
    decal->definition_index = decal_tag_index;
    decal->primitive_count = primitive_count;
    decal->decay_time = decals_random_real(*(float *)(decal_tag + 0x80),
                                           *(float *)(decal_tag + 0x84));

    FUN_0007c270(color, (*(uint8_t *)decal_tag >> 1) & 3,
                 (float *)(decal_tag + 0x34), (float *)(decal_tag + 0x40),
                 decals_random_real(0.0f, 1.0f));
    decal->color = real_a_rgb_color_to_pixel32(
      decals_random_real(*(float *)(decal_tag + 0x2c),
                         *(float *)(decal_tag + 0x30)),
      color);
    decal->alpha = 0xff;

    {
      int16_t produced_quads = 0;
      int16_t vertex_cursor = 0;
      int i;

      for (i = 0; i < geometry->decal_surface_count; ++i) {
        int16_t surface_vertex_count = geometry->surface_vertex_counts[i];

        if (surface_vertex_count < 3) {
          decals_assert_or_exit((char *)0x26a5a4, 0xab8);
        }

        if (surface_vertex_count > 2) {
          int step;
          for (step = 1; step + 1 < surface_vertex_count; step += 2) {
            int anchor = (step + 2 < surface_vertex_count) ?
                           vertex_cursor + step + 2 :
                           vertex_cursor;
            decal_cached_quad_t *quad;

            if (produced_quads >= primitive_count) {
              if (!g_warned_decal_quad_overflow) {
                error(2,
                      "### ERROR decals: produced quad overflow (produced=%d "
                      "expected=%d) -- skipping decal",
                      produced_quads, primitive_count);
                g_warned_decal_quad_overflow = true;
              }
              return;
            }

            quad = &cache_quads[produced_quads];

            if (step + 1 >= surface_vertex_count) {
              decals_assert_or_exit((char *)0x26a56c, 0xabc);
            }

            quad->vertices[0] = staged_vertices[vertex_cursor];
            quad->vertices[1] = staged_vertices[vertex_cursor + step];
            quad->vertices[2] = staged_vertices[vertex_cursor + step + 1];
            quad->vertices[3] = staged_vertices[anchor];
            produced_quads += 1;
          }
        }

        vertex_cursor += surface_vertex_count;
      }

      if (produced_quads != primitive_count) {
        decals_assert_or_exit((char *)0x26a54c, 0xacc);
      }
    }

    thunk_FUN_0015b960();

    reuse_previous = (*(uint8_t *)decal_tag & 1) != 0;
    decal_tag_index = *(int *)(decal_tag + 0x14);
  }
}

void FUN_0009c4b0(int decal_tag_index, void *origin, void *direction,
                  float scale, bool randomize, int16_t color_index, int flags)
{
  if (rasterizer_environment_decals != 0) {
    uint32_t *local_random_seed_address = random_math_get_local_seed_address();
    uint32_t local_seed = 0;
    int16_t collision_result[40];

    if (randomize) {
      if (local_random_seed_address == NULL) {
        display_assert("local_random_seed_address",
                       "c:\\halo\\SOURCE\\effects\\decals.c", 0x5fd, true);
        system_exit(-1);
      }

      if (origin == NULL) {
        display_assert("origin", "c:\\halo\\SOURCE\\effects\\decals.c", 0x5fe,
                       true);
        system_exit(-1);
      }

      local_seed = *local_random_seed_address;
      *local_random_seed_address = ((uint32_t *)origin)[2] ^
                                   ((uint32_t *)origin)[1] ^
                                   ((uint32_t *)origin)[0] ^ 0xdeadc0de;
    }

    if (*(int16_t *)0x4761d8 >= 0x20) {
      display_assert("global_current_collision_user_depth < "
                     "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                     "c:\\halo\\SOURCE\\effects\\decals.c", 0x60e, true);
      system_exit(-1);
    }

    {
      short depth = *(short *)0x4761d8;
      (*(short *)0x4761d8)++;
      *(short *)(0x5a8c80 + (int)depth * 2) = 9;
    }

    if (FUN_0014df70(0x100061, (float *)origin, (float *)direction, -1,
                     collision_result) &&
        collision_result[0] != 0 && collision_result[0] == 2) {
      uint8_t *decal_tag = (uint8_t *)tag_get(TAG_GROUP_DECAL, decal_tag_index);
      if ((decal_tag[0] & 0x10) == 0) {
        decal_new_from_collision(decal_tag_index, collision_result, direction,
                                 scale, randomize, color_index, flags);
      }
    }

    if (*(short *)0x4761d8 <= 1) {
      display_assert("global_current_collision_user_depth > 1",
                     "c:\\halo\\SOURCE\\effects\\decals.c", 0x632, true);
      system_exit(-1);
    }

    --*(short *)0x4761d8;

    if (randomize) {
      if (local_random_seed_address == NULL) {
        display_assert("local_random_seed_address",
                       "c:\\halo\\SOURCE\\effects\\decals.c", 0x636, true);
        system_exit(-1);
      }

      *local_random_seed_address = local_seed;
    }
  }
}

/* Tail-call thunk to rasterizer decal initialization (FUN_0015abe0).
 * Inherits the caller's pushed args and forwards them unchanged (cdecl). */
void FUN_0017ca50(short *p0, short *p1, float *color0, float *color1)
{
  FUN_0015abe0(p0, p1, color0, color1);
}

/* Tail-call thunk to rasterizer debug 2D polyline drawer (FUN_0015acc0).
 * 0x17ca60: PUSH EBP; MOV EBP,ESP; POP EBP; JMP 0x15acc0 — forwards 3 stack
 * args: [EBP+8]=points (short[2] array), [EBP+C]=point_count (int16_t),
 * [EBP+10]=color (real_rgb_color *). */
void FUN_0017ca60(short *points, int16_t point_count, float *color)
{
  FUN_0015acc0(points, point_count, color);
}

/* Tail-call thunk to rasterizer decal setup (FUN_0015a4e0). */
void FUN_0017ca70(void)
{
  FUN_0015a4e0();
}

/* Tail-call thunk to rasterizer_decals_initialize (0x15b6d0). */
void thunk_rasterizer_decals_initialize(void)
{
  rasterizer_decals_initialize();
}

/* Tail-call thunk to rasterizer_decals_initialize_for_new_map (0x15b190). */
void thunk_rasterizer_decals_initialize_for_new_map(void)
{
  rasterizer_decals_initialize_for_new_map();
}

/* Tail-call thunk to rasterizer_decals_dispose_from_old_map (0x15b1a0). */
void thunk_rasterizer_decals_dispose_from_old_map(void)
{
  rasterizer_decals_dispose_from_old_map();
}

/* Tail-call thunk to rasterizer decal (FUN_0015b1e0). */
void FUN_0017cac0(void)
{
  FUN_0015b1e0();
}

/* Tail-call thunk to rasterizer_decals_dispose (0x15b7e0). */
void thunk_rasterizer_decals_dispose(void)
{
  rasterizer_decals_dispose();
}

int FUN_0017cae0(uint32_t cache_size)
{
  return FUN_0015b460(cache_size);
}

void *FUN_0017caf0(int cache_index, uint32_t cache_size)
{
  return FUN_0015b890(cache_index, cache_size);
}

void thunk_FUN_0015b960(void)
{
  FUN_0015b960();
}

void FUN_0017cb10(int decal_index)
{
  FUN_0015b530(decal_index);
}

/* Tail-call thunk to decal rendering pass setup (FUN_0015b970).
 * pass_index selects the rendering pass type. */
void FUN_0017cb20(short pass_index)
{
  FUN_0015b970(pass_index);
}

/* Tail-call thunk to per-cluster decal rendering (FUN_0015bc40).
 * rendered_cluster_data is a pointer to the cluster render data. */
void FUN_0017cb30(int rendered_cluster_data)
{
  FUN_0015bc40(rendered_cluster_data);
}

/* Tail-call thunk to rasterizer decal geometry (FUN_0015b5e0). */
void FUN_0017cb40(void)
{
  FUN_0015b5e0();
}

/* Tail-call thunk to rasterizer decal geometry (FUN_0015c6f0). */
void FUN_0017cb50(void)
{
  FUN_0015c6f0();
}

/* Tail-call thunk to rasterizer decal geometry initialization (FUN_0015c980).
 * 0x17cb60: PUSH EBP; MOV EBP,ESP; POP EBP; JMP 0x15c980 — forwards 1 stack
 * arg: [EBP+8]=decal_group ptr (MOV ESI,[EBP+8] at 0x15c9aa). */
void FUN_0017cb60(void *decal_group)
{
  FUN_0015c980(decal_group);
}

/* Tail-call thunk to rasterizer decal geometry disposal (FUN_0015cbb0).
 * 0x17cb70: PUSH EBP; MOV EBP,ESP; POP EBP; JMP 0x15cbb0 — forwards 1 stack
 * arg: [EBP+8]=decal_group ptr (MOV ESI,[EBP+8] at 0x15cbe1). */
void FUN_0017cb70(void *decal_group)
{
  FUN_0015cbb0(decal_group);
}

/* Tail-call thunk to rasterizer decal geometry (FUN_0015c5f0). */
void FUN_0017cb80(void)
{
  FUN_0015c5f0();
}

/* Tail-call thunk to rasterizer decal rendering (FUN_00170c90).
 * 0x17cb90: PUSH EBP; MOV EBP,ESP; POP EBP; JMP 0x170c90 — forwards 1 stack
 * arg: [EBP+8]=decal ptr (MOV EAX,[EBP+8] at 0x170ccb; passed to 0x17dc70). */
void FUN_0017cb90(void *decal)
{
  FUN_00170c90(decal);
}

/* Tail-call thunk to rasterizer_xbox_screen_effect (FUN_00171bc0).
 * 0x17cba0: JMP 0x171bc0 — 0 stack args (no ADD ESP at sole caller
 * 0x185236; target frame sub esp,0x74 with no [EBP+N>=8]). */
void rasterizer_screen_flash(void)
{
  FUN_00171bc0();
}

/* Tail-call thunk to dynamic vertex geometry decal flush (FUN_0016bed0). */
void FUN_0017cbb0(void *param_1, int param_2)
{
  FUN_0016bed0(param_1, param_2);
}

/* Tail-call thunk to rasterizer dynamic vertex geometry decal (FUN_0016c5a0).
 */
void FUN_0017cbc0(int shader, int p2, int p3, int widget_handle, int p5, int p6,
                  int zbuf_handle)
{
  FUN_0016c5a0(shader, p2, p3, widget_handle, p5, p6, zbuf_handle);
}

/* Tail-call thunk to rasterizer dynamic vertex geometry decal (FUN_0016c090).
 */
void FUN_0017cbd0(void *shader, short p2, int p3, int widget_handle, int p5,
                  int p6, int zbuf_handle, float *position, void *p9)
{
  FUN_0016c090(shader, p2, p3, widget_handle, p5, p6, zbuf_handle, position,
               p9);
}

/* Tail-call thunk to rasterizer_xbox_models end (FUN_0016b1c0).
 * 0x17cbe0: JMP 0x16b1c0 — 0 stack args (callers 0xb1d95 / 0x1246f5 /
 * 0x132c48 / 0x159d9a have no ADD ESP). */
void rasterizer_model_end(void)
{
  FUN_0016b1c0();
}

/* Tail-call thunk to rasterizer_xbox_models (FUN_0016b240).
 * 0x17cbf0: JMP 0x16b240 — 0 stack args. */
void FUN_0017cbf0(void)
{
  FUN_0016b240();
}

/* Tail-call thunk to rasterizer_xbox_environment (FUN_00160c30).
 * 0x17cc00: JMP 0x160c30 — 0 stack args (caller 0x195b5d). */
void FUN_0017cc00(void)
{
  FUN_00160c30();
}

/* Tail-call thunk to rasterizer dynamic vertex geometry decal (FUN_00160dc0).
 */
void FUN_0017cc10(int param_1)
{
  FUN_00160dc0(param_1);
}

/* Tail-call thunk to rasterizer dynamic vertex geometry decal (FUN_00160f50).
 */
void FUN_0017cc20(int param_1, int param_2, int param_3, int param_4,
                  int param_5, int param_6)
{
  FUN_00160f50((void *)param_1, param_2, param_3, param_4, param_5,
               (void *)param_6);
}

/* Tail-call thunk to _rasterizer_environment_lightmaps_end (0x160920).
 * 0x17cc40: JMP 0x160920 — 0 stack args (caller 0x195b90). */
void FUN_0017cc40(void)
{
  _rasterizer_environment_lightmaps_end();
}

/* Tail-call thunk to rasterizer_xbox_environment (FUN_00161f00).
 * 0x17cc50: JMP 0x161f00 — 0 stack args (caller 0x13a429). */
void FUN_0017cc50(void)
{
  FUN_00161f00();
}

/* Tail-call thunk to rasterizer_xbox_environment gel-light setup
 * (FUN_001621c0).
 * 0x17cc60: PUSH EBP; MOV EBP,ESP; POP EBP; JMP 0x1621c0 — ESP is unchanged
 * at the JMP, so the caller's single pushed dword stays in place and becomes
 * FUN_001621c0's first cdecl argument ([ESP+4] / in_stack_00000004). That
 * callee asserts on it with
 * "light_index>=0 && light_index<rasterizer_lights.light_count"
 * (rasterizer_xbox_environment.c:0x2d9) and indexes rasterizer_lights with
 * light_index*0x38, so the forwarded dword is a light index, not a handle.
 * Sole XBE caller: FUN_00196060 @0x196117. */
void FUN_0017cc60(int light_index)
{
  FUN_001621c0(light_index);
}

/* Tail-call thunk to rasterizer dynamic vertex geometry decal (FUN_00162560).
 */
void FUN_0017cc70(int param_1, int param_2, int param_3, int param_4,
                  int param_5, int param_6)
{
  FUN_00162560((void *)param_1, param_2, param_3, param_4, param_5,
               (void *)param_6);
}

/* Tail-call thunk to _rasterizer_environment_diffuse_light_end (0x160930).
 * 0x17cc80: JMP 0x160930 — 0 stack args (caller 0x196141). Target is a
 * lone RET. */
void rasterizer_environment_diffuse_light_end(void)
{
  _rasterizer_environment_diffuse_light_end();
}

/* Tail-call thunk to _rasterizer_hud_begin (0x160940).
 * 0x17cc90: JMP 0x160940 — 0 stack args (caller 0x13a5df). */
void FUN_0017cc90(void)
{
  _rasterizer_hud_begin();
}

/* Tail-call thunk to rasterizer (FUN_00172520).
 * 0x17cca0: JMP 0x172520 — 0 stack args (caller 0x18c48e). */
void FUN_0017cca0(void)
{
  FUN_00172520();
}

/* Tail-call thunk to rasterizer shadow-pass begin (FUN_00172a30).
 * 0x17ccb0: PUSH EBP; MOV EBP,ESP; POP EBP; JMP 0x172a30 — forwards 5 stack
 * args (ADD ESP,0x14 at 0x18b928): [EBP+8]=param_1 (unused), [EBP+C]=shadow
 * matrix ptr (MOV ESI,[EBP+C] at 0x172a81), [EBP+10]=shadow color ptr
 * (MOV EBX,[EBP+10]), [EBP+14]=object_bounding_radius (FLD [EBP+14] at
 * 0x172b88), [EBP+18]=out_radius. Returns FUN_00172a30's char (drawn/visible
 * flag) — the caller chain FUN_0018b830 -> FUN_0018c100 tests AL after the
 * call (implicit-EAX propagation in the original; made explicit here). */
char FUN_0017ccb0(int param_1, const float *shadow_matrix,
                  const float *shadow_color, float object_bounding_radius,
                  float *out_radius)
{
  return FUN_00172a30(param_1, shadow_matrix, shadow_color,
                      object_bounding_radius, out_radius);
}

/* Tail-call thunk to rasterizer decal rendering (FUN_00172590).
 * Inherits the caller's pushed arg and forwards it unchanged (cdecl). */
void FUN_0017ccc0(int param_1)
{
  FUN_00172590(param_1);
}

/* Tail-call thunk to rasterizer decal rendering (FUN_00172de0).
 * 0x17ccd0: PUSH EBP; MOV EBP,ESP; POP EBP; JMP 0x172de0 — forwards 4 stack
 * args (ADD ESP,0x10 at 0x173041): [EBP+8]=decal ptr (MOV ESI,[EBP+8]),
 * [EBP+C]=param_2 (pushed @0x172f20), [EBP+10]=param_3 (MOV EBX,[EBP+10]),
 * [EBP+14]=param_4 (MOV EDI,[EBP+14]). */
void FUN_0017ccd0(void *decal, int param_2, void *param_3, void *param_4)
{
  FUN_00172de0(decal, param_2, param_3, param_4);
}

/* Tail-call thunk to rasterizer (FUN_00172640).
 * 0x17cce0: JMP 0x172640 — 0 stack args (caller 0x1246ee). */
void rasterizer_environment_shadow_model_end(void)
{
  FUN_00172640();
}

/* Tail-call thunk to rasterizer decal rendering (FUN_00173090). */
void FUN_0017ccf0(void *shader, int param_2, int vertices_per_primitive, int a2,
                  int triangle_count, void *vertex_buffer)
{
  FUN_00173090(shader, param_2, vertices_per_primitive, a2, triangle_count,
               vertex_buffer);
}

/* Tail-call thunk to rasterizer (FUN_001726a0).
 * 0x17cd00: JMP 0x1726a0 — 0 stack args (caller 0x18bc4e). */
void FUN_0017cd00(void)
{
  FUN_001726a0();
}

/* Tail-call thunk to rasterizer_window_get_fog (0x172720).
 * 0x17cd10: JMP 0x172720 — 0 stack args (caller 0x18c49f). */
void FUN_0017cd10(void)
{
  rasterizer_window_get_fog();
}

/* Tail-call thunk to rasterizer_xbox_environment (FUN_00162790).
 * 0x17cd20: JMP 0x162790 — 0 stack args (caller 0x195be8). */
void rasterizer_environment_diffuse_textures_begin(void)
{
  FUN_00162790();
}

/* Tail-call thunk to rasterizer dynamic vertex geometry decal (FUN_00162920).
 */
void FUN_0017cd30(int param_1, int param_2, int param_3, int param_4,
                  int param_5, int param_6)
{
  FUN_00162920(param_1, param_2, param_3, param_4, param_5, param_6);
}

/* Tail-call thunk to _rasterizer_environment_diffuse_textures_end (0x160950).
 * 0x17cd40: JMP 0x160950 — 0 stack args (caller 0x195c15). */
void rasterizer_environment_diffuse_textures_end(void)
{
  _rasterizer_environment_diffuse_textures_end();
}

/* Tail-call thunk to rasterizer_xbox_environment (FUN_00162f90).
 * 0x17cd50: JMP 0x162f90 — 0 stack args (caller 0x13a5f9). */
void FUN_0017cd50(void)
{
  FUN_00162f90();
}
