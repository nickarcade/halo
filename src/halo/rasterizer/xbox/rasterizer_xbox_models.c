
/* 40-byte and 16-byte blocks copied wholesale in FUN_0016c090. The reference
 * lowers the first as an inline `rep movsd` (ECX=10) and the second as four
 * dword load/store pairs, which is what a struct assignment of each size
 * produces; no field-level evidence exists for either block's contents, so
 * they stay opaque. */
typedef struct {
  uint32_t d[10];
} rasterizer_model_block40_t;

typedef struct {
  uint32_t d[4];
} rasterizer_model_block16_t;

typedef struct {
  uint32_t d[3];
} rasterizer_model_block12_t;

/* 0x16ab00 — shader-model detail-texture render-state setup.
 *
 * Validates detail_function (0..2, NUMBER_OF_SHADER_MODEL_DETAIL_FUNCTIONS)
 * and detail_mask (0..8, NUMBER_OF_SHADER_MODEL_DETAIL_MASKS) against the
 * asserts at lines 0x6a/0x6b of this TU, then builds a packed render-state
 * word from a 12-entry constant lookup table (indices 0..8 keyed by
 * detail_mask, indices 9..11 keyed by detail_function) and writes a run of
 * fields into the shared 0xf0-byte pixel-shader state block at 0x5a5ac0
 * (documented in rasterizer_xbox_screen_effect.c / rasterizer_xbox_widgets.c).
 * param_3/param_4..param_7 are opaque caller-supplied render-state words —
 * no naming evidence for their meaning beyond how they are stored/consumed
 * here, so they stay as explicit unknowns.
 *
 * If a profile-collection section was requested (DAT_00325173, set by
 * FUN_0016b180 in this TU), the state block is instead finished as a
 * pixel-shader descriptor and handed to rasterizer_set_pixel_shader, and the
 * request flag is cleared. Otherwise the block's fields are pushed to the
 * device one at a time via the ECX/EDX fastcall
 * D3DDevice_SetRenderState_Simple, each followed by a write into its NV2A
 * shadow slot (same interleaved call/shadow-store idiom documented in
 * rasterizer_xbox.c). Finally, in shader-cost accounting mode (DAT_003256ba ==
 * 2) the frame's accumulated pixel-shader cost (DAT_005a555c) is bumped by a
 * fixed cost of 0x2c.
 */
void FUN_0016ab00(int16_t detail_function /* @<ax> */,
                  uint32_t param_3 /* @<ecx> */,
                  int16_t detail_mask /* @<bx> */, uint32_t param_4,
                  uint32_t param_5, uint32_t param_6, uint32_t param_7,
                  char param_8, char param_9)
{
  uint32_t detail_table[12];
  uint32_t combined;
  uint32_t value;

  if (detail_function < 0 || detail_function > 2) {
    display_assert(
      "detail_function>=0 && "
      "detail_function<NUMBER_OF_SHADER_MODEL_DETAIL_FUNCTIONS",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x6a,
      true);
    system_exit(-1);
  }

  if (detail_mask < 0 || detail_mask > 8) {
    display_assert(
      "detail_mask>=0 && detail_mask<NUMBER_OF_SHADER_MODEL_DETAIL_MASKS",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x6b,
      true);
    system_exit(-1);
  }

  detail_table[9] = 0xa0;
  detail_table[11] = 0xa0;
  detail_table[0] = 0x20;
  *(uint32_t *)0x5a5af0 = param_4;
  detail_table[1] = 0x2d;
  detail_table[2] = 0xd;
  detail_table[3] = 0x3c;
  detail_table[4] = 0x1c;
  detail_table[5] = 0x39;
  detail_table[6] = 0x19;
  detail_table[7] = 0x3a;
  detail_table[8] = 0x1a;
  detail_table[10] = 0x20;

  combined = ((detail_table[detail_function + 9] |
               (detail_table[detail_mask] ^ 0x20) << 8)
                << 8 |
              detail_table[detail_mask])
               << 8 |
             9;

  *(uint32_t *)0x5a5aec = param_3;
  *(uint32_t *)0x5a5b54 = combined;

  if (param_8 == 0) {
    detail_table[9] = 0x8090809;
    detail_table[10] = 0x8090000;
    detail_table[11] = 0x8204920;
    *(uint32_t *)0x5a5b60 = detail_table[detail_function + 9];
    *(uint32_t *)0x5a5b8c = 0x800;
    *(uint32_t *)0x5a5b64 = 0x8040b1d;
  } else {
    detail_table[9] = 0xc090c09;
    detail_table[10] = 0xc090000;
    detail_table[11] = 0xc204920;
    *(uint32_t *)0x5a5b64 = detail_table[detail_function + 9];
    *(uint32_t *)0x5a5b60 = 0x8040b1d;
    *(uint32_t *)0x5a5b8c = 0xc00;
  }
  *(uint32_t *)0x5a5b90 = 0xc00;

  if (param_9 == 0) {
    *(uint32_t *)0x5a5afc = param_6;
    *(uint32_t *)0x5a5b1c = param_7;
    *(uint32_t *)0x5a5b6c = param_5;
    *(uint32_t *)0x5a5b70 = param_7;
    *(uint32_t *)0x5a5ae0 = 0x340f010d;
    *(uint32_t *)0x5a5ae4 = 0xc111800;
  } else {
    *(uint32_t *)0x5a5ae0 = 0x330c0300;
    *(uint32_t *)0x5a5ae4 = 0x1800;
  }

  if (*(char *)0x325173 != 0) {
    *(uint32_t *)0x5a5b98 = 0x18421;
    *(uint32_t *)0x5a5b94 = 0x11008;
    *(uint32_t *)0x5a5ae8 = 0xff0000;
    *(uint32_t *)0x5a5b08 = 0xff00;
    *(uint32_t *)0x5a5ac0 = 0xa200000;
    *(uint32_t *)0x5a5b28 = 0xc0;
    *(uint32_t *)0x5a5b48 = 0xa020a01;
    *(uint32_t *)0x5a5b74 = 0x30cd;
    *(uint32_t *)0x5a5ac4 = 0x1c200000;
    *(uint32_t *)0x5a5b2c = 0x90;
    *(uint32_t *)0x5a5b4c = 0x4200c01;
    *(uint32_t *)0x5a5b78 = 0x400;
    *(uint32_t *)0x5a5ac8 = 0xc200d15;
    *(uint32_t *)0x5a5b30 = 0xcd;
    *(uint32_t *)0x5a5b50 = 0x3c201c01;
    *(uint32_t *)0x5a5b7c = 0xc00;
    *(uint32_t *)0x5a5b80 = 0x900;
    *(uint32_t *)0x5a5b58 = 0xb05040c;
    *(uint32_t *)0x5a5b84 = 0xb4;
    *(uint32_t *)0x5a5b5c = 0x22014e1;
    *(uint32_t *)0x5a5b88 = 0xd00;
    rasterizer_set_pixel_shader((void *)0x5a5ac0);
    *(char *)0x325173 = 0;
    return;
  }

  value = param_3;
  D3DDevice_SetRenderState_Simple(0x40a64, value);
  *(uint32_t *)0x1fb6c4 = value;

  value = *(uint32_t *)0x5a5af0;
  D3DDevice_SetRenderState_Simple(0x40a68, value);
  *(uint32_t *)0x1fb6c8 = value;

  value = *(uint32_t *)0x5a5afc;
  D3DDevice_SetRenderState_Simple(0x40a74, value);
  *(uint32_t *)0x1fb6d4 = value;

  value = *(uint32_t *)0x5a5b1c;
  D3DDevice_SetRenderState_Simple(0x40a94, value);
  *(uint32_t *)0x1fb6f4 = value;

  value = *(uint32_t *)0x5a5b54;
  D3DDevice_SetRenderState_Simple(0x40acc, value);
  *(uint32_t *)0x1fb72c = value;

  value = *(uint32_t *)0x5a5b60;
  D3DDevice_SetRenderState_Simple(0x40ad8, value);
  *(uint32_t *)0x1fb738 = value;

  value = *(uint32_t *)0x5a5b8c;
  D3DDevice_SetRenderState_Simple(0x41e58, value);
  *(uint32_t *)0x1fb764 = value;

  value = *(uint32_t *)0x5a5b64;
  D3DDevice_SetRenderState_Simple(0x40adc, value);
  *(uint32_t *)0x1fb73c = value;

  value = *(uint32_t *)0x5a5b90;
  D3DDevice_SetRenderState_Simple(0x41e5c, value);
  *(uint32_t *)0x1fb768 = value;

  value = *(uint32_t *)0x5a5b6c;
  D3DDevice_SetRenderState_Simple(0x41e20, value);
  *(uint32_t *)0x1fb744 = value;

  value = *(uint32_t *)0x5a5b70;
  D3DDevice_SetRenderState_Simple(0x41e24, value);
  *(uint32_t *)0x1fb748 = value;

  value = *(uint32_t *)0x5a5ae0;
  D3DDevice_SetRenderState_Simple(0x40288, value);
  *(uint32_t *)0x1fb6b8 = value;

  value = *(uint32_t *)0x5a5ae4;
  D3DDevice_SetRenderState_Simple(0x4028c, value);
  *(uint32_t *)0x1fb6bc = value;

  if (*(short *)0x3256ba == 2) {
    *(int *)0x5a555c = *(int *)0x5a555c + 0x2c;
  }
}

/* 0x16b180 — model-rendering profile toggle.
 * Guarded by the profile-collection master switch (DAT_003256c4); when
 * active, records that a models profile section has been requested
 * (DAT_00325173 = 1), latches the requested sub-state (DAT_0047e002), and
 * opens the corresponding profile section via FUN_0016f910 (1 = models with
 * the flag set, 2 = models without). */
void FUN_0016b180(bool flag)
{
  if (*(char *)0x3256c4 != 0) {
    *(char *)0x325173 = 1;
    *(char *)0x47e002 = flag;
    if (flag != 0) {
      FUN_0016f910(1);
      return;
    }
    FUN_0016f910(2);
  }
}

/* 0x16b1c0 - end of the model local-parameters block; the counterpart of
 * FUN_0016bed0 (0x16bed0), which stashes the parameter block pointer in
 * DAT_0047dff8 and its second argument's low byte in DAT_0047e005.
 * Guarded by the profile-collection master switch (DAT_003256c4). Asserts
 * that a parameter block is currently open ("local_parameters", line 0x5d3),
 * flushes the pending sub-pass when DAT_0047e004 is set (FUN_00165fc0), then
 * - only when the block's first byte is negative and DAT_0047e005 is clear,
 * mirroring the open path's entry condition - restores the default projection
 * with FUN_00158ae0(2) and rasterizer_set_frustum_z(0.0f, 0.0f). Finally
 * clears DAT_0047dff8 to mark the block closed. */
void FUN_0016b1c0(void)
{
  if (*(char *)0x3256c4 != 0) {
    if (*(void **)0x47dff8 == NULL) {
      display_assert(
        "local_parameters",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x5d3,
        true);
      system_exit(-1);
    }

    if (*(char *)0x47e004 != 0) {
      FUN_00165fc0();
    }

    if (*(signed char *)*(void **)0x47dff8 < 0 && *(char *)0x47e005 == 0) {
      FUN_00158ae0(2);
      rasterizer_set_frustum_z(0.0f, 0.0f);
    }

    *(void **)0x47dff8 = NULL;
  }
}

/* 0x16b240 — model-rendering profile close; the counterpart of FUN_0016b180.
 * Guarded by the same profile-collection master switch (DAT_003256c4); reads
 * back the sub-state latched by FUN_0016b180 (DAT_0047e002) and closes the
 * matching profile section via FUN_0016fa40 (1 = models with the flag set,
 * 2 = models without). */
void FUN_0016b240(void)
{
  if (*(char *)0x3256c4 != 0) {
    if (*(char *)0x47e002 != 0) {
      FUN_0016fa40(1);
      return;
    }
    FUN_0016fa40(2);
  }
}

/* global_rasterizer_model_ambient_reflection_tint (DAT_0047e4d0): 0x10-byte
 * game-state allocation holding the model ambient reflection tint. Name is
 * taken verbatim from the assert message at 0x17c7aa (#cond string). The
 * pointer is only populated once rasterizer_window_set_fog has run, hence the
 * null check.
 *
 * Declared in kb.json (<common> data, 0x47e4d0) and emitted into
 * build/generated/decl.h. Referencing it by name rather than through an
 * absolute-address cast (`*(void **)0x47e4d0`, still used in
 * src/halo/cutscene/cinematics.c, src/halo/rasterizer/rasterizer.c and
 * src/halo/rasterizer/common/rasterizer_common.c) is what makes cl.exe lower
 * the three float stores below as FLD/FSTP instead of GPR moves, matching the
 * reference exactly. */

/* 0x16b270 — stores the model ambient reflection tint into the 0x10-byte
 * block at global_rasterizer_model_ambient_reflection_tint: an int at +0x0
 * followed by three floats at +0x4/+0x8/+0xc. The whole body is skipped when
 * the block has not been allocated yet. The three float stores are raw
 * FLD/FSTP passthroughs of the incoming stack slots — no numeric conversion
 * takes place, so the parameters must stay typed float. param_1's meaning
 * beyond "the dword at +0x0" has no binary evidence here. */
void FUN_0016b270(int param_1, float param_2, float param_3, float param_4)
{
  if (global_rasterizer_model_ambient_reflection_tint != NULL) {
    *(int *)global_rasterizer_model_ambient_reflection_tint = param_1;
    *(float *)((char *)global_rasterizer_model_ambient_reflection_tint + 4) =
      param_2;
    *(float *)((char *)global_rasterizer_model_ambient_reflection_tint + 8) =
      param_3;
    *(float *)((char *)global_rasterizer_model_ambient_reflection_tint + 0xc) =
      param_4;
  }
}

/* 0x16bed0 — per-model profiling dispatch: LOD/skinning-lighting cost
 * accounting plus cull-plane visibility flag for the model's next draw.
 *
 * Guarded by the profile-collection master switch (DAT_003256c4, see
 * FUN_0016b180); returns immediately when profiling is inactive. Asserts
 * param_1 (the model's render parameter block) is non-null.
 *
 * When the block's flags byte is negative (bit 0x80 set) and param_2's low
 * byte is 0, forces a near/far-frustum override before anything else:
 * FUN_00158ae0(1) then rasterizer_set_frustum_z(DAT_0032569c, DAT_003256a0).
 *
 * Stashes the parameter block pointer (DAT_0047dff8), clears DAT_0047dffc,
 * and records param_2's low byte into DAT_0047e005.
 *
 * Picks a render/profiling path into DAT_0047e000 (0/1/2) from the block's
 * word field at +0x8c and float field at +0x90 (an LOD-tier / min-distance
 * pair) gated by the sub-flags DAT_003256f9 and DAT_005a5bc0:
 *   - tier==1 with distance>0 and sub-flags satisfied: DAT_0047e000=1
 *     (already-cached path skinning/lighting is skipped).
 *   - tier==2: DAT_0047e000=2 (also skipped).
 *   - otherwise: programs skinning (param_1+8) and lighting (param_1+0x10),
 *     and accumulates the draw-call/triangle-count deltas those two calls
 *     produced into DAT_005a5560/DAT_005a5564; DAT_0047e000=0.
 *
 * Computes a cull-plane visibility flag (DAT_0047e003) from the block's
 * flag bits 0x4/0x40 and, when bit 0x40 is set, a plane test — the dot
 * product of a plane normal (DAT_005a5dc8..d0) against a reference point
 * (DAT_005a5bc8..d0) minus the plane distance (DAT_005a5dd4) — gated by the
 * cull-enable word DAT_005a5dc4.
 *
 * If the profiling sub-state DAT_0047e002 is 0, calls FUN_00167ee0(param_1)
 * (returns bool in AL, per-model getter/predicate — not yet ported) and
 * latches its result into DAT_0047e004; otherwise DAT_0047e004 is cleared.
 *
 * In shader-cost accounting mode (DAT_003256ba == 2) bumps the per-frame
 * counter DAT_005a54d4.
 */
void FUN_0016bed0(void *param_1, int param_2)
{
  int draw_calls_before;
  int draw_calls_delta;
  int tri_before;
  bool profile_getter_result;

  if (*(char *)0x3256c4 == 0) {
    return;
  }

  if (param_1 == 0) {
    display_assert(
      "parameters",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x24b,
      true);
    system_exit(-1);
  }

  if (*(signed char *)param_1 < 0 && (char)param_2 == 0) {
    FUN_00158ae0(1);
    rasterizer_set_frustum_z(*(float *)0x32569c, *(float *)0x3256a0);
  }

  *(void **)0x47dff8 = param_1;
  *(char *)0x47dffc = 0;
  *(char *)0x47e005 = (char)param_2;

  if (*(char *)0x3256f9 == 0 || *(short *)0x5a5bc0 != 0 ||
      *(short *)((char *)param_1 + 0x8c) != 1 ||
      *(float *)((char *)param_1 + 0x90) <= *(float *)0x2533c0) {
    if (*(short *)((char *)param_1 + 0x8c) == 2) {
      *(short *)0x47e000 = 2;
    } else {
      draw_calls_before = *(int *)0x5a5550;
      rasterizer_set_model_skinning((char *)param_1 + 8);
      draw_calls_delta = *(int *)0x5a5550 - draw_calls_before;
      tri_before = *(int *)0x5a5554;
      rasterizer_set_model_lighting((char *)param_1 + 0x10);
      *(int *)0x5a5560 = *(int *)0x5a5560 + draw_calls_delta;
      *(int *)0x5a5564 = *(int *)0x5a5564 + (*(int *)0x5a5554 - tri_before);
      *(short *)0x47e000 = 0;
    }
  } else {
    *(short *)0x47e000 = 1;
  }

  if (*(short *)0x5a5dc4 == 0 || (*(int *)param_1 & 4) != 0 ||
      ((*(int *)param_1 & 0x40) != 0 &&
       *(float *)0x2533c0 <= (*(float *)0x5a5dc8 * *(float *)0x5a5bc8 +
                              *(float *)0x5a5dcc * *(float *)0x5a5bcc +
                              *(float *)0x5a5dd0 * *(float *)0x5a5bd0) -
                               *(float *)0x5a5dd4)) {
    *(char *)0x47e003 = 0;
  } else {
    *(char *)0x47e003 = 1;
  }

  if (*(char *)0x47e002 == 0) {
    profile_getter_result = FUN_00167ee0(param_1);
    *(char *)0x47e004 = 1;
    if (profile_getter_result != 0) {
      goto profile_done;
    }
  }
  *(char *)0x47e004 = 0;

profile_done:
  if (*(short *)0x3256ba == 2) {
    *(int *)0x5a54d4 = *(int *)0x5a54d4 + 1;
  }
}

/* 0x16c090 — submit one model part as a transparent geometry group.
 *
 * Runs only while profile collection is active (DAT_003256c4 and its second
 * gate DAT_003256c5); otherwise returns NULL without touching anything.
 *
 * Two shader predicates are computed up front from the shader's base.type
 * (the int16 at +0x24; type 4 is the one that owns the extra data block
 * fetched by FUN_001906b0):
 *   - is_decal: type-4 shader whose byte at +0x28 has bit 0x8 set. When set,
 *     the part is dropped entirely: the caller's out block is reset to
 *     {NULL, NULL, 0xffff} and the function returns NULL.
 *   - use_pool: false only when the model path selector DAT_0047e000 is 1 AND
 *     the shader is not a type-4 shader with a non-zero int16 at +0x28.
 *
 * Then asserts shader / shader_type_is_valid_for_model / centroid /
 * local_parameters (lines 0x515-0x518) and picks the destination group:
 *   - use_pool path with bit 1 of local_parameters->flags set (forced on for
 *     decal shaders, which OR in 3): the shared static group at 0x47df58,
 *     with the pending-index global 0x47dfe8 reset to -1.
 *   - otherwise a freshly allocated group — the secondary pool when
 *     DAT_0047e000 == 1 and the shader is not type 4, else the primary
 *     transparent-geometry pool. Only the primary allocation is handed back to
 *     the caller as the return value; the secondary and static-group paths
 *     return NULL. Allocation failure reports "too many transparent geometry
 *     groups" once (latched in DAT_0047e006) and returns.
 *
 * When the caller supplied an out block it receives the group's presorted
 * index plus pointers to the group's two int16 sort keys at +0x94/+0x96.
 *
 * The group is then filled from local_parameters and the stack arguments; the
 * centroid is taken from the caller unless local_parameters has an effect
 * (int16 at +0x8c non-zero), in which case the effect's source object index
 * (+0x98) and its own centroid (+0x9c..+0xa4) are used instead. The sort key
 * at +0x70 is minus the dot product of the group's centroid, relative to the
 * reference point at 0x5a5bc8..0x5a5bd0, with the direction at
 * 0x5a5bd4..0x5a5bdc.
 *
 * Finally the geometry source fields (+0x60..+0x6c) are wired either straight
 * at local_parameters (static-group path, which draws immediately and requests
 * a profile section via DAT_00325173) or at one-shot copies of the vertex/
 * lighting/skinning blocks staged into the rasterizer memory pool and cached in
 * 0x47df48..0x47df54 behind the DAT_0047dffc latch.
 *
 * In shader-cost accounting mode (DAT_003256ba == 2) the part count, total and
 * peak of param_5, and the triangle cost reported by FUN_0017ed90(param_3,
 * param_6) are accumulated into 0x5a54e4..0x5a54f0.
 *
 * param_3..param_7 are opaque caller-supplied geometry words beyond how they
 * are stored and forwarded here, so they keep mechanical names. */
void *FUN_0016c090(void *shader, short param_2, int param_3, int param_4,
                   int param_5, int param_6, int param_7, float *centroid,
                   void *out)
{
  volatile rasterizer_model_block16_t zero;
  uint32_t flags;
  void *result;
  bool use_pool;
  bool is_decal;
  char *group;
  float *group_centroid;
  float dx;
  float dy;
  float dz;
  int *total_ptr;

  result = NULL;
  if (*(char *)0x3256c4 == 0 || *(char *)0x3256c5 == 0) {
    return result;
  }

  if (shader != NULL && *(short *)((char *)shader + 0x24) == 4 &&
      (*(char *)((char *)FUN_001906b0(shader, 4) + 0x28) & 8) != 0) {
    is_decal = true;
  } else {
    is_decal = false;
  }

  if (*(short *)0x47e000 != 1 ||
      (shader != NULL && *(short *)((char *)shader + 0x24) == 4 &&
       *(short *)((char *)FUN_001906b0(shader, 4) + 0x28) != 0)) {
    use_pool = true;
  } else {
    use_pool = false;
  }

  if (!is_decal) {
    if (shader == NULL) {
      display_assert(
        "shader",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x515,
        true);
      system_exit(-1);
    }

    if (shader_type_is_valid_for_model(*(uint16_t *)((char *)shader + 0x24)) ==
        0) {
      display_assert(
        "shader_type_is_valid_for_model(shader->base.type)",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x516,
        true);
      system_exit(-1);
    }

    if (centroid == NULL) {
      display_assert(
        "centroid",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x517,
        true);
      system_exit(-1);
    }

    if (*(void **)0x47dff8 == NULL) {
      display_assert(
        "local_parameters",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x518,
        true);
      system_exit(-1);
    }

    flags = **(uint32_t **)0x47dff8;
    if (use_pool) {
      if (shader_is_decal(shader) != 0) {
        flags = flags | 3;
      }
      if ((flags & 2) != 0) {
        group = (char *)0x47df58;
        *(uint32_t *)0x47dfe8 = 0xffffffff;
        goto have_group;
      }
    }

    if (*(short *)0x47e000 == 1 && *(short *)((char *)shader + 0x24) != 4) {
      group = (char *)rasterizer_secondary_geometry_group_new();
    } else {
      group = (char *)rasterizer_transparent_geometry_group_new();
      result = group;
    }

    if (out != NULL) {
      *(short *)((char *)out + 8) =
        rasterizer_transparent_geometry_group_to_presorted_index(
          (unsigned int)group);
      *(void **)out = group + 0x94;
      *(void **)((char *)out + 4) = group + 0x96;
    }

    if (group != NULL) {
    have_group:
      *(uint32_t *)group = flags;
      *(uint32_t *)(group + 4) = *(uint32_t *)(*(char **)0x47dff8 + 4);
      zero.d[0] = 0;
      zero.d[3] = (zero.d[2] = (zero.d[1] = 0));

      group_centroid = (float *)(group + 0x74);
      if (*(short *)((*(char **)0x47dff8) + 0x8c) == 0) {
        *(uint32_t *)(group + 8) = 0;
        *(rasterizer_model_block12_t *)group_centroid =
          *(rasterizer_model_block12_t *)centroid;
      } else {
        if (*(uint32_t *)((*(char **)0x47dff8) + 0x98) == 0) {
          display_assert(
            "local_parameters->effect.source_object_index!=0",
            "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c",
            0x54c, true);
          system_exit(-1);
        }
        *(uint32_t *)(group + 8) = *(uint32_t *)((*(char **)0x47dff8) + 0x98);
        *(rasterizer_model_block12_t *)group_centroid =
          *(rasterizer_model_block12_t *)((*(char **)0x47dff8) + 0x9c);
      }

      *(short *)(group + 0x10) = param_2;
      *(void **)(group + 0xc) = shader;
      *(rasterizer_model_block40_t *)(group + 0x14) =
        *(rasterizer_model_block40_t *)((*(char **)0x47dff8) + 0x8c);
      *(int *)(group + 0x44) = param_4;
      *(int *)(group + 0x48) = param_3;
      *(int *)(group + 0x50) = param_5;
      *(int *)(group + 0x54) = param_7;
      *(int *)(group + 0x58) = param_6;
      *(int *)(group + 0x4c) = 0;
      *(int *)(group + 0x5c) = 0;

      dx = group_centroid[0] - *(float *)0x5a5bc8;
      dy = group_centroid[1] - *(float *)0x5a5bcc;
      dz = group_centroid[2] - *(float *)0x5a5bd0;
      *(rasterizer_model_block16_t *)(group + 0x80) = zero;
      *(float *)(group + 0x70) =
        -(*(float *)0x5a5bdc * dz + *(float *)0x5a5bd8 * dy +
          *(float *)0x5a5bd4 * dx);

      *(uint32_t *)(group + 0x3c) = *(uint32_t *)((*(char **)0x47dff8) + 0xc4);
      *(uint32_t *)(group + 0x40) = *(uint32_t *)((*(char **)0x47dff8) + 0xc8);
      *(short *)(group + 0x94) = -1;
      *(short *)(group + 0x96) = -1;

      if (*(short *)0x47e000 == 1 && *(short *)((char *)shader + 0x24) != 4) {
        *(uint32_t *)(group + 0x98) =
          *(uint32_t *)((*(char **)0x47dff8) + 0x98);
        if (*(uint32_t *)(group + 0x98) == 0) {
          display_assert(
            "group->active_camouflage_transparent_source_object_index",
            "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c",
            0x56d, true);
          system_exit(-1);
        }
      } else {
        *(uint32_t *)(group + 0x98) = 0;
      }

      *(char *)(group + 0x9d) = *(char *)0x5a5570;

      if ((flags & 2) != 0) {
        {
          char *lp = *(char **)0x47dff8;
          *(uint32_t *)(group + 0x60) = *(uint32_t *)(lp + 8);
          *(short *)(group + 0x64) = *(short *)(lp + 0xc);
          *(char **)(group + 0x68) = lp + 0x10;
          *(char **)(group + 0x6c) = lp + 0x84;
        }
        FUN_00174ce0();
        rasterizer_transparent_geometry_group_draw(group, 0);
        FUN_001749b0();
        *(char *)0x325173 = 1;
      } else {
        if (*(char *)0x47dffc == 0) {
          char *lp = *(char **)0x47dff8;
          *(int *)0x47df54 = rasterizer_memory_pool_copy(
            *(int *)(lp + 8), *(short *)(lp + 0xc) * 0x34);
          *(short *)0x47df50 = *(short *)(lp + 0xc);
          *(int *)0x47df4c =
            rasterizer_memory_pool_copy((int)(lp + 0x10), 0x74);
          *(int *)0x47df48 = rasterizer_memory_pool_copy((int)(lp + 0x84), 8);
          *(char *)0x47dffc = 1;
        }
        *(uint32_t *)(group + 0x60) = *(uint32_t *)0x47df54;
        *(short *)(group + 0x64) = *(short *)0x47df50;
        *(uint32_t *)(group + 0x68) = *(uint32_t *)0x47df4c;
        *(uint32_t *)(group + 0x6c) = *(uint32_t *)0x47df48;
      }

      if (*(short *)0x3256ba == 2) {
        *(int *)0x5a54f0 = *(int *)0x5a54f0 + 1;
        total_ptr = (int *)0x5a54e8;
        *total_ptr = *total_ptr + param_5;
        if (param_5 > *(int *)0x5a54ec) {
          *(int *)0x5a54ec = param_5;
        }
        *(int *)0x5a54e4 =
          *(int *)0x5a54e4 + FUN_0017ed90((void *)param_3, (void *)param_6);
      }

    } else if (*(char *)0x47e006 == 0) {
      error(2, "### ERROR too many transparent geometry groups");
      *(char *)0x47e006 = 1;
    }

  } else if (out != NULL) {
    *(short *)((char *)out + 8) = (short)0xffff;
    *(void **)out = NULL;
    *(void **)((char *)out + 4) = NULL;
  }
  return result;
}

/* 0x16c5a0 — opaque model draw (PAL 2342 names it _rasterizer_model_draw).
 *
 * local_parameters (0x47dff8) fields read here, from the 2276 disassembly:
 *   +0x00 geometry flags (bit 2 no-fog, bit 3 no-zbuffer), +0x04 unique id
 *   (self-illumination random-phase seed), +0x0c int16 node matrix count,
 *   +0x50 int16 point light count, +0x5c..+0x68 reflection tint (argb),
 *   +0x84 animation (colors pointer at +0x84), +0xa8 effect shader,
 *   +0xac effect animation (values pointer at +0xb0), +0xb4 centroid,
 *   +0xc4/+0xc8 base map scale.
 * Shader-model flags byte +0x28: bit 0 detail after reflection, bit 1 two
 * sided, bit 2 not alpha tested, bit 3 alpha blended decal, bit 4 true
 * atmospheric fog. Asserts carry the 2276 line numbers of this TU. */
void FUN_0016c5a0(int param_1, int param_2, int param_3, int param_4,
                  int param_5, int param_6, int param_7)
{
  void *shader;
  char *local_parameters;
  char *shader_model;
  void *effect_shader;
  void *group;
  float *animation_values;
  float *external_color;
  float *tint;
  short intensity_exponent_source;
  short color_source;
  short vertex_type;
  short local_model_effect_type;
  bool alpha_blended_decal;
  uint32_t alpha_test_enable;
  uint32_t zbias;
  int vertex_shader_permutation;
  int vertex_shader_work;
  unsigned int seed;
  float camera_distance;
  float reflection_fraction;
  float self_illumination_phase;
  float self_illumination_fraction;
  float planar_fog_fraction;
  float fog_density;
  float one_minus_planar;
  float value;
  float color_delta[3];
  float self_illumination_color[3];
  float diffuse_change_color[3];
  float perpendicular[4];
  float parallel[4];
  float vertex_constants[12];
  float specular_constants[8];
  float cc0[4];
  float cc0_error[3];
  float cc1[3];
  uint32_t cc0_pixel;
  uint32_t cc0_error_pixel;
  uint32_t cc1_pixel;
  uint32_t diffuse_change_pixel;
  uint32_t self_illumination_pixel;

  shader = (void *)param_1;

  if (*(void **)0x476ab0 == NULL) {
    display_assert("global_d3d_device",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c",
                   0x29f, true);
    system_exit(-1);
  }

  if (*(char *)0x3256c4 == 0) {
    return;
  }

  if (*(void **)0x47dff8 == NULL) {
    display_assert("local_parameters",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c",
                   0x2a3, true);
    system_exit(-1);
  }

  if (shader == NULL) {
    display_assert("shader",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c",
                   0x2a4, true);
    system_exit(-1);
  }

  effect_shader = *(void **)(*(char **)0x47dff8 + 0xa8);
  if (effect_shader != NULL) {
    intensity_exponent_source = -1;
    if (*(short *)((char *)effect_shader + 0x24) == 10) {
      intensity_exponent_source =
        *(short *)((char *)FUN_001906b0(effect_shader, 10) + 0x2c);
    }

    animation_values = *(float **)(*(char **)0x47dff8 + 0xb0);
    if (intensity_exponent_source < 1 || intensity_exponent_source > 4 ||
        animation_values == NULL ||
        animation_values[intensity_exponent_source - 1] != 0.0f) {
      local_parameters = *(char **)0x47dff8;
      group = FUN_0016c090(*(void **)(local_parameters + 0xa8), (short)param_2,
                           param_3, param_4, param_5, param_6, param_7,
                           (float *)(local_parameters + 0xb4), NULL);
      if (group != NULL) {
        *(int *)((char *)group + 0x6c) =
          rasterizer_memory_pool_alloc((int)(*(char **)0x47dff8 + 0xac), 8);
      }
    }
  }

  local_model_effect_type = *(short *)0x47e000;
  if (local_model_effect_type == 1) {
    if (*(short *)((char *)shader + 0x24) != 4) {
      display_assert("shader->base.type==_shader_type_model",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c",
                     0x2d9, true);
      system_exit(-1);
    }
    FUN_0017cbd0(shader, (short)param_2, param_3, param_4, param_5, param_6,
                 param_7, (float *)(*(char **)0x47dff8 + 0xb4), NULL);
    FUN_001592e0(1);
    return;
  }

  if (local_model_effect_type != 0) {
    error(2, "### ERROR model effect type #%d can't render opaque shader",
          (int)local_model_effect_type);
    return;
  }

  if (*(short *)((char *)shader + 0x24) == 3) {
    rasterizer_model_draw_environment_shader(shader, (short)param_2,
                                             (void *)param_3, param_4, param_5,
                                             (void *)param_6, param_7);
  } else {
    shader_model = (char *)FUN_001906b0(shader, 4);
    alpha_blended_decal = ((*(unsigned char *)(shader_model + 0x28) >> 3) & 1) != 0;

    if (*(short *)0x47e000 != 0) {
      display_assert("local_model_effect_type==_render_model_effect_type_none",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c",
                     0x303, true);
      system_exit(-1);
    }

    local_parameters = *(char **)0x47dff8;
    camera_distance =
      (*(float *)(local_parameters + 0xb4) - *(float *)0x5a5bc8) *
        *(float *)0x5a5bd4 +
      (*(float *)(local_parameters + 0xb8) - *(float *)0x5a5bcc) *
        *(float *)0x5a5bd8 +
      *(float *)0x5a5bdc *
        (*(float *)(local_parameters + 0xbc) - *(float *)0x5a5bd0);

    if (*(float *)(shader_model + 0x140) != 0.0f) {
      reflection_fraction =
        (camera_distance - *(float *)(shader_model + 0x140)) /
        (*(float *)(shader_model + 0x13c) - *(float *)(shader_model + 0x140));
      if (reflection_fraction < 0.0f) {
        reflection_fraction = 0.0f;
      } else if (reflection_fraction > 1.0f) {
        reflection_fraction = 1.0f;
      }
    } else {
      reflection_fraction = 1.0f;
    }

    if ((*(unsigned char *)local_parameters & 8) != 0) {
      D3DDevice_SetRenderState_ZEnable(0);
      zbias = 0;
    } else {
      D3DDevice_SetRenderState_ZEnable(1);
      D3DDevice_SetRenderState_Simple(0x4035c, !alpha_blended_decal);
      *(uint32_t *)0x1fb798 = !alpha_blended_decal;
      D3DDevice_SetRenderState_Simple(0x40354, 0x203);
      *(uint32_t *)0x1fb77c = 0x203;
      zbias = alpha_blended_decal ? *(uint32_t *)0x32570c : 0;
    }
    D3DDevice_SetRenderState_ZBias(zbias);
    D3DDevice_SetRenderState_CullMode(0x901);
    D3DDevice_SetRenderState_Simple(0x40358, 0x10101);
    *(uint32_t *)0x1fb7a4 = 0x10101;
    D3DDevice_SetRenderState_Simple(0x40304, alpha_blended_decal);
    *(uint32_t *)0x1fb784 = alpha_blended_decal;
    D3DDevice_SetRenderState_Simple(0x40344, 0x302);
    *(uint32_t *)0x1fb790 = 0x302;
    D3DDevice_SetRenderState_Simple(0x40348, 0x303);
    *(uint32_t *)0x1fb794 = 0x303;
    D3DDevice_SetRenderState_Simple(0x40350, 0x8006);
    *(uint32_t *)0x1fb7c0 = 0x8006;
    if (!alpha_blended_decal &&
        (*(unsigned char *)(shader_model + 0x28) & 4) == 0) {
      alpha_test_enable = 1;
    } else {
      alpha_test_enable = 0;
    }
    D3DDevice_SetRenderState_Simple(0x40300, alpha_test_enable);
    *(uint32_t *)0x1fb788 = alpha_test_enable;
    D3DDevice_SetRenderState_Simple(0x40340, 0x7f);
    *(uint32_t *)0x1fb78c = 0x7f;

    rasterizer_set_texture(0, 0, 1, *(int *)(shader_model + 0xb0),
                           (short)param_2);
    D3DDevice_SetTextureStageState(0, 10, 1);
    D3DDevice_SetTextureStageState(0, 0xb, 1);
    D3DDevice_SetTextureStageState(0, 0xd, 2);
    D3DDevice_SetTextureStageState(0, 0xe, 2);
    D3DDevice_SetTextureStageState(0, 0xf, 2);
    rasterizer_set_texture(1, 0, 2, *(int *)(shader_model + 0xe8),
                           (short)param_2);
    D3DDevice_SetTextureStageState(1, 10, 1);
    D3DDevice_SetTextureStageState(1, 0xb, 1);
    D3DDevice_SetTextureStageState(1, 0xd, 2);
    D3DDevice_SetTextureStageState(1, 0xe, 2);
    D3DDevice_SetTextureStageState(1, 0xf, 2);
    rasterizer_set_texture(2, 0, 1, *(int *)(shader_model + 0xc8),
                           (short)param_2);
    SetTextureStageStateSmart(2, 10, 1);
    SetTextureStageStateSmart(2, 0xb, 1);
    SetTextureStageStateSmart(2, 0xd, 2);
    SetTextureStageStateSmart(2, 0xe, 2);
    SetTextureStageStateSmart(2, 0xf, 2);
    rasterizer_set_texture(3, 2, 0, *(int *)(shader_model + 0x170),
                           (short)param_2);
    SetTextureStageStateSmart(3, 10, 3);
    SetTextureStageStateSmart(3, 0xb, 3);
    SetTextureStageStateSmart(3, 0xc, 3);
    SetTextureStageStateSmart(3, 0xd, 2);
    SetTextureStageStateSmart(3, 0xe, 2);
    SetTextureStageStateSmart(3, 0xf, 2);

    seed = *(unsigned int *)(*(char **)0x47dff8 + 4);
    if ((*(unsigned char *)(shader_model + 0x6c) & 1) != 0) {
      self_illumination_phase = 0.0f;
    } else {
      self_illumination_phase = random_math_real(&seed);
    }

    if (*(float *)(shader_model + 0x74) == 0.0f) {
      display_assert(
        "shader_model->model.self_illumination_animation_period!=0.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x360,
        true);
      system_exit(-1);
    }

    FUN_00013090((float *)(shader_model + 0x84), (float *)(shader_model + 0x78),
                 color_delta);
    self_illumination_fraction = FUN_0010a5e0(
      *(int16_t *)(shader_model + 0x72),
      *(float *)0x5a5e18 / *(float *)(shader_model + 0x74) +
        self_illumination_phase);
    vector3d_scale_add((float *)(shader_model + 0x78), color_delta,
                       self_illumination_fraction, self_illumination_color);

    if (!(self_illumination_color[0] >= 0.0f &&
          self_illumination_color[0] <= 1.0f)) {
      display_assert(
        "self_illumination_color.red >=0.0f && "
        "self_illumination_color.red <=1.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x36c,
        true);
      system_exit(-1);
    }
    if (!(self_illumination_color[1] >= 0.0f &&
          self_illumination_color[1] <= 1.0f)) {
      display_assert(
        "self_illumination_color.green>=0.0f && "
        "self_illumination_color.green<=1.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x36d,
        true);
      system_exit(-1);
    }
    if (!(self_illumination_color[2] >= 0.0f &&
          self_illumination_color[2] <= 1.0f)) {
      display_assert(
        "self_illumination_color.blue >=0.0f && "
        "self_illumination_color.blue <=1.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x36e,
        true);
      system_exit(-1);
    }

    color_source = *(short *)(shader_model + 0x70);
    if (color_source > 0 && color_source < 5) {
      external_color = (float *)(*(char **)(*(char **)0x47dff8 + 0x84) +
                                 (color_source - 1) * 0xc);
      if (!(external_color[0] >= 0.0f && external_color[0] <= 1.0f)) {
        display_assert(
          "external_color->red >=0.0f && external_color->red <=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x377,
          true);
        system_exit(-1);
      }
      if (!(external_color[1] >= 0.0f && external_color[1] <= 1.0f)) {
        display_assert(
          "external_color->green>=0.0f && external_color->green<=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x378,
          true);
        system_exit(-1);
      }
      if (!(external_color[2] >= 0.0f && external_color[2] <= 1.0f)) {
        display_assert(
          "external_color->blue >=0.0f && external_color->blue <=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x379,
          true);
        system_exit(-1);
      }
      self_illumination_color[0] = self_illumination_color[0] * external_color[0];
      self_illumination_color[1] = self_illumination_color[1] * external_color[1];
      self_illumination_color[2] = self_illumination_color[2] * external_color[2];
    }

    color_source = *(short *)(shader_model + 0x4c);
    if (color_source > 0 && color_source < 5) {
      external_color = (float *)(*(char **)(*(char **)0x47dff8 + 0x84) +
                                 (color_source - 1) * 0xc);
      diffuse_change_color[0] = external_color[0];
      diffuse_change_color[1] = external_color[1];
      diffuse_change_color[2] = external_color[2];
      if (!(diffuse_change_color[0] >= 0.0f &&
            diffuse_change_color[0] <= 1.0f)) {
        display_assert(
          "diffuse_change_color.red >=0.0f && diffuse_change_color.red <=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x388,
          true);
        system_exit(-1);
      }
      if (!(diffuse_change_color[1] >= 0.0f &&
            diffuse_change_color[1] <= 1.0f)) {
        display_assert(
          "diffuse_change_color.green>=0.0f && "
          "diffuse_change_color.green<=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x389,
          true);
        system_exit(-1);
      }
      if (!(diffuse_change_color[2] >= 0.0f &&
            diffuse_change_color[2] <= 1.0f)) {
        display_assert(
          "diffuse_change_color.blue >=0.0f && "
          "diffuse_change_color.blue <=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x38a,
          true);
        system_exit(-1);
      }
    } else {
      diffuse_change_color[0] = (*(float **)0x2ee708)[0];
      diffuse_change_color[1] = (*(float **)0x2ee708)[1];
      diffuse_change_color[2] = (*(float **)0x2ee708)[2];
    }

    vertex_shader_work = *(int *)0x5a5558;
    local_parameters = *(char **)0x47dff8;
    if ((*(unsigned char *)(shader_model + 0x28) & 0x10) != 0) {
      vertex_shader_permutation = 2;
    } else if (*(char *)0x47e003 != 0) {
      vertex_shader_permutation = 0;
    } else if (*(short *)(local_parameters + 0x50) > 0) {
      vertex_shader_permutation = 1;
    } else if (*(short *)(local_parameters + 0xc) <= 1 &&
               (*(int *)(shader_model + 0xc8) == -1 ||
                (*(short *)(shader_model + 0xd6) == 0 &&
                 self_illumination_color[0] == 0.0f &&
                 self_illumination_color[1] == 0.0f &&
                 self_illumination_color[2] == 0.0f &&
                 diffuse_change_color[0] == 1.0f &&
                 diffuse_change_color[1] == 1.0f &&
                 diffuse_change_color[2] == 1.0f)) &&
               (*(int *)(shader_model + 0x170) == -1 ||
                !(reflection_fraction > 0.0f))) {
      vertex_shader_permutation = 3;
    } else {
      vertex_shader_permutation = 2;
    }

    if (param_6 != 0) {
      vertex_type = *(short *)param_6;
    } else {
      vertex_type = rasterizer_dynamic_vertices_get_type(param_7);
    }
    FUN_00178b40(10, vertex_type, vertex_shader_permutation);

    if (*(short *)0x3256ba >= 1) {
      *(int *)0x5a5568 =
        *(int *)0x5a5568 + (*(int *)0x5a5558 - vertex_shader_work);
      ((int *)0x5a5414)[(short)vertex_shader_permutation] =
        ((int *)0x5a5414)[(short)vertex_shader_permutation] +
        *(int *)(param_6 + 4);
    }

    local_parameters = *(char **)0x47dff8;
    perpendicular[0] = reflection_fraction * *(float *)(shader_model + 0x144) *
                       *(float *)(local_parameters + 0x5c);
    perpendicular[1] =
      *(float *)(shader_model + 0x148) * *(float *)(local_parameters + 0x60);
    perpendicular[2] =
      *(float *)(shader_model + 0x14c) * *(float *)(local_parameters + 0x64);
    perpendicular[3] =
      *(float *)(shader_model + 0x150) * *(float *)(local_parameters + 0x68);
    parallel[0] = reflection_fraction * *(float *)(shader_model + 0x154) *
                  *(float *)(local_parameters + 0x5c);
    parallel[1] =
      *(float *)(shader_model + 0x158) * *(float *)(local_parameters + 0x60);
    parallel[2] =
      *(float *)(shader_model + 0x15c) * *(float *)(local_parameters + 0x64);
    parallel[3] =
      *(float *)(shader_model + 0x160) * *(float *)(local_parameters + 0x68);

    vertex_constants[0] = *(float *)(shader_model + 0xd8);
    vertex_constants[1] =
      *(float *)(shader_model + 0xd8) * *(float *)(shader_model + 0xec);
    vertex_constants[2] = 1.0f;
    vertex_constants[3] = 1.0f;
    vertex_constants[4] = 1.0f;
    vertex_constants[5] = 0.0f;
    vertex_constants[6] = 0.0f;
    vertex_constants[7] = 0.0f;
    vertex_constants[8] = 0.0f;
    vertex_constants[9] = 1.0f;
    vertex_constants[10] = 0.0f;
    vertex_constants[11] = 0.0f;
    specular_constants[0] = perpendicular[1] - parallel[1];
    specular_constants[1] = perpendicular[2] - parallel[2];
    specular_constants[2] = perpendicular[3] - parallel[3];
    specular_constants[3] = perpendicular[0] - parallel[0];
    specular_constants[4] = parallel[1];
    specular_constants[5] = parallel[2];
    specular_constants[6] = parallel[3];
    specular_constants[7] = parallel[0];

    FUN_00190e10(shader_model + 0xfc, local_parameters + 0x84,
                 *(float *)(local_parameters + 0xc4) *
                   *(float *)(shader_model + 0x9c),
                 *(float *)(local_parameters + 0xc8) *
                   *(float *)(shader_model + 0xa0),
                 0.0f, 0.0f, 0.0f, *(float *)0x5a5e18, &vertex_constants[4],
                 &vertex_constants[8]);
    vertex_constants[10] = *(float *)(shader_model + 0x38);
    D3DDevice_SetVertexShaderConstant(-0x54, vertex_constants, 3);
    D3DDevice_SetVertexShaderConstant(-0x51, specular_constants, 2);

    tint = *(float **)0x47e4d0;
    if (tint != NULL &&
        (tint[0] > 0.0f || tint[1] > 0.0f || tint[2] > 0.0f ||
         tint[3] > 0.0f)) {
      specular_constants[0] = 0.0f;
      specular_constants[1] = 0.0f;
      specular_constants[2] = 0.0f;
      specular_constants[3] = 0.0f;
      specular_constants[4] = tint[0];
      specular_constants[5] = tint[1];
      specular_constants[6] = tint[2];
      specular_constants[7] = tint[3];
      D3DDevice_SetVertexShaderConstant(-0x51, specular_constants, 2);
    }

    if (!(*(float *)0x5a5dc0 > *(float *)0x5a5dbc)) {
      display_assert(
        "global_window_parameters.fog.atmospheric_maximum_distance>"
        "global_window_parameters.fog.atmospheric_minimum_distance",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x432,
        true);
      system_exit(-1);
    }
    if (!(*(float *)0x5a5dc0 > 0.0f)) {
      display_assert(
        "global_window_parameters.fog.atmospheric_maximum_distance>0.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x433,
        true);
      system_exit(-1);
    }
    if (!(*(float *)0x5a5de8 > 0.0f)) {
      display_assert(
        "global_window_parameters.fog.planar_maximum_distance>0.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x434,
        true);
      system_exit(-1);
    }
    if (!(*(float *)0x5a5dec > 0.0f)) {
      display_assert(
        "global_window_parameters.fog.planar_maximum_depth>0.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x435,
        true);
      system_exit(-1);
    }

    if (*(char *)0x3256d4 != 0 &&
        (**(unsigned char **)0x47dff8 & 4) == 0) {
      planar_fog_fraction =
        plane3d_distance_to_point((float *)0x5a5dc8, (float *)0x5a5bc8) /
        *(float *)0x5a5dc0;
      if (planar_fog_fraction < 0.0f) {
        planar_fog_fraction = 0.0f;
      } else if (planar_fog_fraction > 1.0f) {
        planar_fog_fraction = 1.0f;
      }
      fog_density = (camera_distance - *(float *)0x5a5dbc) /
                    (*(float *)0x5a5dc0 - *(float *)0x5a5dbc);
      if (fog_density < 0.0f) {
        fog_density = 0.0f;
      } else if (fog_density > 1.0f) {
        fog_density = 1.0f;
      }
      fog_density = fog_density * *(float *)0x5a5db8;
      if ((*(unsigned char *)0x5a5da8 & 2) != 0) {
        planar_fog_fraction = 1.0f;
      }

      cc0[0] = 1.0f - fog_density;
      one_minus_planar = 1.0f - planar_fog_fraction;
      cc0[1] = *(float *)0x5a5dd8 -
               (planar_fog_fraction * *(float *)0x5a5dd8 +
                one_minus_planar * *(float *)0x5a5dac) *
                 fog_density;
      cc0[2] = *(float *)0x5a5ddc -
               (*(float *)0x5a5db0 * one_minus_planar +
                *(float *)0x5a5ddc * planar_fog_fraction) *
                 fog_density;
      cc0[3] = *(float *)0x5a5de0 -
               (one_minus_planar * *(float *)0x5a5db4 +
                *(float *)0x5a5de0 * planar_fog_fraction) *
                 fog_density;

      value = -cc0[1];
      cc0_error[0] = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
      value = -cc0[2];
      cc0_error[1] = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
      value = -cc0[3];
      cc0_error[2] = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
      if (cc0[1] < 0.0f) {
        cc0[1] = 0.0f;
      } else if (cc0[1] > 1.0f) {
        cc0[1] = 1.0f;
      }
      if (cc0[2] < 0.0f) {
        cc0[2] = 0.0f;
      } else if (cc0[2] > 1.0f) {
        cc0[2] = 1.0f;
      }
      if (cc0[3] < 0.0f) {
        cc0[3] = 0.0f;
      } else if (cc0[3] > 1.0f) {
        cc0[3] = 1.0f;
      }
      cc1[0] = *(float *)0x5a5dac * fog_density;
      cc1[1] = *(float *)0x5a5db0 * fog_density;
      cc1[2] = *(float *)0x5a5db4 * fog_density;

      if (!(cc0[1] >= 0.0f && cc0[1] <= 1.0f)) {
        display_assert(
          "cc0.red >=0.0f && cc0.red <=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x45b,
          true);
        system_exit(-1);
      }
      if (!(cc0[2] >= 0.0f && cc0[2] <= 1.0f)) {
        display_assert(
          "cc0.green>=0.0f && cc0.green<=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x45c,
          true);
        system_exit(-1);
      }
      if (!(cc0[3] >= 0.0f && cc0[3] <= 1.0f)) {
        display_assert(
          "cc0.blue >=0.0f && cc0.blue <=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x45d,
          true);
        system_exit(-1);
      }
      if (!(cc1[0] >= 0.0f && cc1[0] <= 1.0f)) {
        display_assert(
          "cc1.red >=0.0f && cc1.red <=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x45e,
          true);
        system_exit(-1);
      }
      if (!(cc1[1] >= 0.0f && cc1[1] <= 1.0f)) {
        display_assert(
          "cc1.green>=0.0f && cc1.green<=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x45f,
          true);
        system_exit(-1);
      }
      if (!(cc1[2] >= 0.0f && cc1[2] <= 1.0f)) {
        display_assert(
          "cc1.blue >=0.0f && cc1.blue <=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x460,
          true);
        system_exit(-1);
      }
      if (!(cc0_error[0] >= 0.0f && cc0_error[0] <= 1.0f)) {
        display_assert(
          "cc0_error.red >=0.0f && cc0_error.red <=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x461,
          true);
        system_exit(-1);
      }
      if (!(cc0_error[1] >= 0.0f && cc0_error[1] <= 1.0f)) {
        display_assert(
          "cc0_error.green>=0.0f && cc0_error.green<=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x462,
          true);
        system_exit(-1);
      }
      if (!(cc0_error[2] >= 0.0f && cc0_error[2] <= 1.0f)) {
        display_assert(
          "cc0_error.blue >=0.0f && cc0_error.blue <=1.0f",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_models.c", 0x463,
          true);
        system_exit(-1);
      }

      cc0_pixel = real_argb_color_to_pixel32(cc0);
      cc0_error_pixel = real_rgb_color_to_pixel32(cc0_error);
      cc1_pixel = real_rgb_color_to_pixel32(cc1);
    } else {
      cc1_pixel = 0xff000000;
      cc0_error_pixel = 0xff000000;
      cc0_pixel = 0xff000000;
    }

    diffuse_change_pixel = real_rgb_color_to_pixel32(diffuse_change_color);
    self_illumination_pixel = real_rgb_color_to_pixel32(self_illumination_color);
    FUN_0016ab00(*(int16_t *)(shader_model + 0xd4), self_illumination_pixel,
                 *(int16_t *)(shader_model + 0xd6), diffuse_change_pixel,
                 cc0_pixel, cc0_error_pixel, cc1_pixel,
                 (char)(*(unsigned char *)(shader_model + 0x28) & 1),
                 (char)((*(unsigned char *)(shader_model + 0x28) >> 4) & 1));
    D3DDevice_SetRenderState_CullMode(0x901);
    rasterizer_draw((const triangle_buffer *)param_3, param_4, 0, param_5,
                    (const vertex_buffer *)param_6, param_7);
    if (*(short *)0x3256ba == 2) {
      *(int *)0x5a54dc = *(int *)0x5a54dc + param_5;
      *(int *)0x5a54e0 = *(int *)0x5a54e0 + 1;
      *(int *)0x5a54d8 =
        *(int *)0x5a54d8 + FUN_0017ed90((void *)param_3, (void *)param_6);
    }

    if ((*(unsigned char *)(shader_model + 0x28) & 2) != 0) {
      vertex_constants[0] = *(float *)(shader_model + 0xd8);
      vertex_constants[1] =
        *(float *)(shader_model + 0xd8) * *(float *)(shader_model + 0xec);
      vertex_constants[2] = 1.0f;
      vertex_constants[3] = -1.0f;
      vertex_constants[4] = 1.0f;
      vertex_constants[5] = 0.0f;
      vertex_constants[6] = 0.0f;
      vertex_constants[7] = 0.0f;
      vertex_constants[8] = 0.0f;
      vertex_constants[9] = 1.0f;
      vertex_constants[10] = 0.0f;
      vertex_constants[11] = 0.0f;
      local_parameters = *(char **)0x47dff8;
      FUN_00190e10(shader_model + 0xfc, local_parameters + 0x84,
                   *(float *)(local_parameters + 0xc4) *
                     *(float *)(shader_model + 0x9c),
                   *(float *)(local_parameters + 0xc8) *
                     *(float *)(shader_model + 0xa0),
                   0.0f, 0.0f, 0.0f, *(float *)0x5a5e18, &vertex_constants[4],
                   &vertex_constants[8]);
      vertex_constants[10] = *(float *)(shader_model + 0x38);
      D3DDevice_SetVertexShaderConstant(-0x54, vertex_constants, 3);
      SetRenderStateSmart(0x7f, 0x900);
      rasterizer_draw((const triangle_buffer *)param_3, param_4, 0, param_5,
                      (const vertex_buffer *)param_6, param_7);
      if (*(short *)0x3256ba == 2) {
        *(int *)0x5a54e0 = *(int *)0x5a54e0 + 1;
        *(int *)0x5a54dc = *(int *)0x5a54dc + param_5;
        *(int *)0x5a54d8 =
          *(int *)0x5a54d8 + FUN_0017ed90((void *)param_3, (void *)param_6);
      }
    }
  }

  if (*(char *)0x47e004 != 0) {
    rasterizer_environment_fog_screen_model_submit(
      shader, (int16_t)param_2, (void *)param_3, param_4, param_5,
      (void *)param_6, param_7);
  }
}
