/* Render camera utilities. */

#include "x87_math.h"

#define MAXIMUM_RENDER_CAMERA_WARNING_CONDITIONS 64

/* Saved copies of the four frustum projection terms hacked by
 * render_camera_hack_frustum_z; stored and restored as raw dwords, exactly as
 * the original does (MOV, never FLD/FSTP). */
static uint32_t render_camera_saved_frustum_z[4]; /* 0x4d0d08 */

static char render_camera_warnings_initialized; /* 0x4d0e18 */
static float render_camera_warning_values
  [MAXIMUM_RENDER_CAMERA_WARNING_CONDITIONS]; /* 0x4d0d18
                                               */

/* render_camera_check_warning_condition - 0x185770
 * Tracks maximum frustum-integrity violation distances per condition ID.
 * Logs when a condition exceeds its previous worst value.
 * render_camera_build_frustum calls this out of line 22 times (id in AX), so
 * it must not be inlined into that caller. */
__declspec(noinline) void render_camera_check_warning_condition(int16_t id,
                                                                float value)
{
  assert_halt(id >= 0 && id < MAXIMUM_RENDER_CAMERA_WARNING_CONDITIONS);

  if (!render_camera_warnings_initialized) {
    csmemset(render_camera_warning_values, 0,
             sizeof(render_camera_warning_values));
    render_camera_warnings_initialized = 1;
  }

  /* 001857c9 FCOMP [0.05f] / TEST AH,0x1 / JNZ skip: only strictly-below
   * skips, so the bound is inclusive. */
  if (value >= 0.05f && value > render_camera_warning_values[id]) {
    error(2,
          "### ERROR cameras: frustum-integrity condition #%d violated by %f",
          (int)id, (double)value);
    render_camera_warning_values[id] = value;
  }
}

/* render_camera_new - 0x185810
 * Zero-initializes a render camera block (camera_t, 0x54 bytes). */
void render_camera_new(camera_t *camera)
{
  csmemset(camera, 0, sizeof(camera_t));
}

/* render_camera_hack_frustum_z - 0x185830
 *
 * Overrides the near/far terms of a frustum's projection matrix.  Two sentinel
 * argument pairs select a save/restore mode instead of a recompute:
 *
 * Evidence (0x185830..0x18594a):
 *   TEST ESI,ESI / JZ + MOV AL,[ESI+0x140] / TEST AL,AL / JNZ
 *     => assert(frustum && frustum->projection_valid) at line 0x10f.
 *   FLD [EBP+0xc] / FCOMP [0x00255e94] / FNSTSW AX / TEST AH,0x44 / JP
 *     (twice)  ; 0x255e94 = -1.0f.  TEST AH,0x44 + JP is the MSVC equality
 *     test, the JP taking the not-equal path.  Both args == -1.0f saves the
 *     four terms into 0x4d0d08..0x4d0d14 as plain dword MOVs.
 *   Same shape against 0x002533c0 (0.0f) restores them, also as dword MOVs.
 *   Otherwise:
 *     FLD [EBP+0x10] / FSUB [EBP+0xc]      => denom = far_z - near_z, held in
 *                                             ST1 across both stores
 *     MOV [ESI+0x14c],0 / MOV [ESI+0x15c],0
 *     FLD [EBP+0xc] / FADD [EBP+0x10] / FDIV ST0,ST1 / FCHS / FSTP [ESI+0x16c]
 *     FLD [EBP+0xc] / FMUL [EBP+0x10] / FMUL [0x0025eeac] / FDIV ST0,ST1 /
 *       FSTP [ESI+0x17c]                   ; 0x25eeac = -2.0f
 *   The trailing FSTP ST0 discards the shared denominator.
 *
 * The frustum has no recovered type at this decl, so the projection terms
 * (stride 0x10 from +0x14c) and the validity byte at +0x140 are dereferenced
 * raw, as in the rest of this file.
 */
void render_camera_hack_frustum_z(void *frustum, float near_z, float far_z)
{
  char *f;
  float denom_held;
  float denom;

  f = (char *)frustum;

  assert_halt_msg_at("frustum && frustum->projection_valid",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x10f,
                     frustum != 0 && *(char *)(f + 0x140) != 0);

  if (near_z == *(float *)0x255e94 && far_z == *(float *)0x255e94) {
    render_camera_saved_frustum_z[0] = *(uint32_t *)(f + 0x14c);
    render_camera_saved_frustum_z[1] = *(uint32_t *)(f + 0x15c);
    render_camera_saved_frustum_z[2] = *(uint32_t *)(f + 0x16c);
    render_camera_saved_frustum_z[3] = *(uint32_t *)(f + 0x17c);
    return;
  }

  if (near_z == *(float *)0x2533c0 && far_z == *(float *)0x2533c0) {
    *(uint32_t *)(f + 0x14c) = render_camera_saved_frustum_z[0];
    *(uint32_t *)(f + 0x15c) = render_camera_saved_frustum_z[1];
    *(uint32_t *)(f + 0x16c) = render_camera_saved_frustum_z[2];
    *(uint32_t *)(f + 0x17c) = render_camera_saved_frustum_z[3];
    return;
  }

  denom = far_z - near_z;
  *(uint32_t *)(f + 0x14c) = 0;
  *(uint32_t *)(f + 0x15c) = 0;
  /* The original leaves the single subtraction result on the x87 stack (ST1)
   * across both divides and pops it once at the end; the second reference to
   * the divisor is written through a second local so the divisor stays held
   * rather than being recomputed/reloaded per divide (+3.1pp VC71). */
  denom_held = denom;
  *(float *)(f + 0x16c) = -((near_z + far_z) / denom);
  *(float *)(f + 0x17c) = (near_z * far_z * *(float *)0x25eeac) / denom_held;
}

/* render_frustum_sphere_diameter_in_pixels - 0x185a70
 *
 * Projects a world-space sphere onto the frustum and returns its diameter in
 * pixels.  The depth term is row 3 of the frustum's world-to-view matrix
 * (frustum + 0x1c / +0x28 / +0x34 dotted with the center, plus the
 * translation at +0x40); it is made positive, clamped up to a floor, then
 * divided into the pixel scale at frustum + 0x188 and scaled by the radius.
 * The doubled result is the diameter.
 *
 * Evidence (0x185a70..0x185ac1):
 *   FLD [ECX+0x34] / FMUL [EAX+8] / FLD [ECX+0x28] / FMUL [EAX+4] / FADDP /
 *   FLD [ECX+0x1c] / FMUL [EAX] / FADDP / FADD [ECX+0x40]
 *     => ((m34*c[2] + m28*c[1]) + m1c*c[0]) + m40, in that x87 order.
 *   FCOM [0x002533c0] / TEST AH,0x1 / JZ +2 / FCHS   ; 0x2533c0 = 0.0f
 *   FCOM [0x0025496c] / TEST AH,0x41 / JZ +8 / FSTP ST0 / FLD [0x0025496c]
 *                                                   ; 0x25496c = 0.1f
 *   FDIVR [ECX+0x188] / FMUL [EBP+0x10] / FADD ST0,ST0
 *
 * The frustum has no recovered type at this decl, so the four matrix fields
 * and the pixel scale are dereferenced raw.  TEST AH,0x1 is C0 alone (strictly
 * less than), TEST AH,0x41 is C0|C3 (less than or equal); both senses are
 * written out as the binary tests them.
 */
float render_frustum_sphere_diameter_in_pixels(void *frustum, float *center,
                                               float radius)
{
  char *f;
  float depth;
  float scaled;

  f = (char *)frustum;
  depth = *(float *)(f + 0x34) * center[2] + *(float *)(f + 0x28) * center[1] +
          *(float *)(f + 0x1c) * center[0] + *(float *)(f + 0x40);

  if (depth < *(float *)0x2533c0) {
    depth = -depth;
  }
  if (depth <= *(float *)0x25496c) {
    depth = *(float *)0x25496c;
  }

  scaled = (*(float *)(f + 0x188) / depth) * radius;
  return scaled + scaled;
}

/* render_camera_screen_to_world - 0x186330
 *
 * Builds a world-space ray for a screen point: the ray origin is the camera
 * position (camera + 0x00) copied verbatim, and the direction is the view-space
 * ray from render_camera_screen_to_view rotated into world space by the
 * frustum's view-to-world matrix (frustum + 0x44).
 *
 * Evidence (0x186330..0x18645f): five null-parameter asserts at source lines
 * 0x41c..0x420, then a frustum->projection_valid byte test at +0x140 (line
 * 0x422).  The position copy is three dword MOVs from [ESI]/[ESI+4]/[ESI+8]
 * into [EDI]/[EDI+4]/[EDI+8] (a struct assignment, not three float loads).
 * Both calls are cdecl; the shared ADD ESP,0x1c at 0x186456 retires 4+3 args.
 */
void render_camera_screen_to_world(camera_t *camera, float *frustum,
                                   float *screen_point, vector3_t *world_point,
                                   float *world_vector)
{
  float view_vector[3];

  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x41c, camera);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x41d, frustum);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x41e,
                 screen_point);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x41f,
                 world_point);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x420,
                 world_vector);
  assert_halt_msg_at("frustum->projection_valid",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x422,
                     *((char *)frustum + 0x140) != 0);

  render_camera_screen_to_view(camera, frustum, screen_point, view_vector);

  *world_point = camera->field_00;

  matrix_scale_transform_vector(frustum + 17, view_vector, world_vector);
}

/* Compute the adjusted FOV tangent for the render camera.
 * Uses FPTAN: tan(fov * half_constant) * aspect_ratio */
double render_camera_get_adjusted_field_of_view_tangent(float fov)
{
#if defined(_MSC_VER) && !defined(__clang__)
  /* VC71 /Oi inlines tan as FPTAN (matches original). */
  return tan(fov * *(float *)0x253398) * *(float *)0x2b1504;
#else
  double result;
  asm volatile("flds %[f]\n\t"
               "fmuls 0x253398\n\t"
               "fptan\n\t"
               "fstp %%st(0)\n\t"
               "fmuls 0x2b1504"
               : "=t"(result)
               : [f] "m"(fov)
               : "memory");
  return result;
#endif
}

/* Frustum-cull a triangle against the clip planes.
 * Builds the outcode flags for each of the three points; a point with no
 * flags is inside every plane, so the triangle is trivially visible.
 * Otherwise the triangle is visible unless all three outcodes share a
 * common plane bit (0x3f mask = the 6 clip planes). */
bool render_frustum_triangle_visible(void *plane_ctx, void *v0, void *v1,
                                     void *v2)
{
  int16_t flags;
  int16_t point_flags;

  point_flags = render_frustum_build_point_flags(plane_ctx, v0);
  if (point_flags == 0) {
    return 1;
  }
  flags = (int16_t)(point_flags & 0x3f);
  point_flags = render_frustum_build_point_flags(plane_ctx, v1);
  if (point_flags == 0) {
    return 1;
  }
  flags &= point_flags;
  point_flags = render_frustum_build_point_flags(plane_ctx, v2);
  if (point_flags == 0) {
    return 1;
  }
  return (int16_t)(point_flags & flags) == 0;
}

/* Project a world-space point into screen space.
 * Transforms the point into view space with the frustum's world-to-view
 * matrix (frustum + 0x10) and defers to render_camera_view_to_screen. */
char render_camera_world_to_screen(void *camera, float *frustum,
                                   float *world_point, float *screen_point)
{
  float view_point[3];

  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3c1, camera);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3c2, frustum);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3c3,
                 world_point);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3c4,
                 screen_point);

  matrix_transform_point(frustum + 4, world_point, view_point);
  return render_camera_view_to_screen((int *)camera, (int *)frustum, view_point,
                                      screen_point);
}

/* Header-inline math helpers used by render_camera_build_frustum.  The
 * original expands these inline (no calls in 0x187250..0x187f7a); bodies follow
 * the PAL 2342 real_math.h definitions.  Suffixed _inline because kb.json
 * already names the out-of-line copies (0x178d0, 0x99490, 0x99500). */
#define global_origin3d (*(vector3_t **)0x31fc1c)

/* The engine builds with -fno-builtin, so clang would lower fabs() to a CRT
 * call; the original (and VC71 /Oi) inlines FABS. */
#if !(defined(_MSC_VER) && !defined(__clang__))
#define fabs(x) __builtin_fabs(x)
#endif

static __inline vector3_t *cross_product3d_inline(const vector3_t *a,
                                           const vector3_t *b,
                                           vector3_t *result)
{
  real k = a->x * b->y - a->y * b->x;
  real j = a->z * b->x - a->x * b->z;
  real i = a->y * b->z - a->z * b->y;

  result->x = i;
  result->y = j;
  result->z = k;
  return result;
}

static __inline real dot_product3d_inline(const vector3_t *a, const vector3_t *b)
{
  return a->x * b->x + a->y * b->y + a->z * b->z;
}

static __inline real_plane3d *plane3d_from_point_and_normal_inline(
  real_plane3d *plane, const vector3_t *point, const vector3_t *normal)
{
  *(vector3_t *)plane->normal = *normal;
  plane->d = dot_product3d_inline(point, (const vector3_t *)plane->normal);
  return plane;
}

static __inline real plane3d_distance_to_point_inline(const real_plane3d *plane,
                                               const vector3_t *point)
{
  return dot_product3d_inline(point, (const vector3_t *)plane->normal) - plane->d;
}

/* render_camera_build_frustum - 0x187250
 *
 * Builds the view frustum from a camera, optional frustum bounds, and a
 * projection flag: bounds, world_to_view / view_to_world matrices, the six
 * clip planes (left, right, bottom, top, near, far), z_near / z_far copies, the
 * four far-plane corners plus the camera position, the midpoint, the world
 * AABB of the five vertices, the optional projection matrix, and 22
 * frustum-integrity warning checks.
 *
 * Structure follows the PAL 2342 reconstruction, re-verified against
 * 0x187250..0x187f7a: assert lines 0x1ae..0x1b4 and 0x1ca..0x1cb, the bounds
 * struct copy (dword MOVs), per-vertex MIN/MAX stores (FCOMP / TEST AH,0x41 /
 * unconditional FSTP), and out-of-line warning calls with the id in EAX.
 */
void render_camera_build_frustum(camera_t *camera, float *bounds,
                                 float *frustum, bool do_projection)
{
  render_frustum_t *fr = (render_frustum_t *)frustum;
  int viewport_width_integer =
    camera->viewport_bounds.x1 - camera->viewport_bounds.x0;
  int viewport_height_integer =
    camera->viewport_bounds.y1 - camera->viewport_bounds.y0;
  real viewport_width = (real)viewport_width_integer;
  real viewport_height = (real)viewport_height_integer;
  real half_bounds_width;
  real half_bounds_height;
  real bounds_center_x;
  real bounds_center_y;
  real field_of_view_tangent;
  real projection_x_scale;
  real projection_y_scale;
  vector3_t view_left;
  vector3_t view_up;
  vector3_t view_backward;
  vector3_t plane_normal;
  real_plane3d view_plane;
  real left_plane_z;
  real bottom_plane_z;
  real inverse_projection_x_scale;
  real inverse_projection_y_scale;
  real half_z;
  real far_left;
  real far_right;
  real far_bottom;
  real far_top;
  vector3_t view_point;
  int vertex_index;

  if (bounds) {
    fr->field_00 = *(real_rectangle2d *)bounds;
  } else {
    fr->field_00.y0 = -1.0f;
    fr->field_00.x0 = -1.0f;
    fr->field_00.y1 = 1.0f;
    fr->field_00.x1 = 1.0f;
  }

  half_bounds_width = (fr->field_00.x1 - fr->field_00.x0) * 0.5f;
  half_bounds_height = (fr->field_00.y1 - fr->field_00.y0) * 0.5f;
  bounds_center_x =
    (fr->field_00.x0 + fr->field_00.x1) / half_bounds_width * -0.5f;
  bounds_center_y =
    (fr->field_00.y0 + fr->field_00.y1) / half_bounds_height * -0.5f;
#if defined(_MSC_VER) && !defined(__clang__)
  /* VC71 /Oi inlines tan as FPTAN (matches original). */
  field_of_view_tangent = (real)tan(camera->vertical_field_of_view * 0.5f);
#else
  field_of_view_tangent = x87_fptan(camera->vertical_field_of_view * 0.5f);
#endif
  projection_x_scale = 1.0f / (half_bounds_width / viewport_height *
                               viewport_width * field_of_view_tangent);
  projection_y_scale = 1.0f / (field_of_view_tangent * half_bounds_height);

  assert_halt_msg_at("camera->vertical_field_of_view<_pi - _real_epsilon",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x1ae,
                     camera->vertical_field_of_view < *(float *)0x2b16f4);
  if (camera->vertical_field_of_view <= *(float *)0x253f44) {
    display_assert(csprintf((char *)0x5ab100,
                            "### FATAL ERROR: field of view set to %f (0x%x)",
                            (double)camera->vertical_field_of_view,
                            *(int *)&camera->vertical_field_of_view),
                   "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x1b0, true);
    system_exit(-1);
  }
  assert_halt_msg_at("camera->z_near>=0.0f",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x1b1,
                     camera->z_near >= 0.0f);
  assert_halt_msg_at("camera->z_far>camera->z_near",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x1b2,
                     camera->z_far > camera->z_near);
  assert_halt_msg_at("camera->viewport_bounds.x0<camera->viewport_bounds.x1",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x1b3,
                     camera->viewport_bounds.x0 < camera->viewport_bounds.x1);
  assert_halt_msg_at("camera->viewport_bounds.y0<camera->viewport_bounds.y1",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x1b4,
                     camera->viewport_bounds.y0 < camera->viewport_bounds.y1);

  cross_product3d_inline(&camera->field_0c, &camera->field_18, &view_left);
  cross_product3d_inline(&view_left, &camera->field_0c, &view_up);
  view_backward.x = -camera->field_0c.x;
  view_backward.y = -camera->field_0c.y;
  view_backward.z = -camera->field_0c.z;
  normalize3d(&view_left.x);
  normalize3d(&view_up.x);
  normalize3d(&view_backward.x);
  fr->field_44.forward = view_left;
  fr->field_44.left = view_up;
  fr->field_44.up = view_backward;
  fr->field_44.position = camera->field_00;
  fr->field_44.scale = 1.0f;
  matrix_inverse(&fr->field_44.scale, &fr->field_10.scale);
  assert_halt_msg_at("valid_real_matrix4x3(&frustum->world_to_view)",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x1ca,
                     valid_real_matrix4x3(&fr->field_10.scale));
  assert_halt_msg_at("valid_real_matrix4x3(&frustum->view_to_world)",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x1cb,
                     valid_real_matrix4x3(&fr->field_44.scale));

  plane_normal.x = -projection_x_scale;
  plane_normal.y = 0.0f;
  left_plane_z = bounds_center_x + 1.0f;
  plane_normal.z = left_plane_z;
  normalize3d(&plane_normal.x);
  plane3d_from_point_and_normal_inline(&view_plane, global_origin3d, &plane_normal);
  FUN_0010a1c0(&fr->field_44.scale, view_plane.normal,
               fr->field_78[0].normal);

  plane_normal.x = projection_x_scale;
  plane_normal.y = 0.0f;
  plane_normal.z = 1.0f - bounds_center_x;
  normalize3d(&plane_normal.x);
  plane3d_from_point_and_normal_inline(&view_plane, global_origin3d, &plane_normal);
  FUN_0010a1c0(&fr->field_44.scale, view_plane.normal,
               fr->field_78[1].normal);

  plane_normal.x = 0.0f;
  plane_normal.y = -projection_y_scale;
  bottom_plane_z = bounds_center_y + 1.0f;
  plane_normal.z = bottom_plane_z;
  normalize3d(&plane_normal.x);
  plane3d_from_point_and_normal_inline(&view_plane, global_origin3d, &plane_normal);
  FUN_0010a1c0(&fr->field_44.scale, view_plane.normal,
               fr->field_78[2].normal);

  plane_normal.x = 0.0f;
  plane_normal.y = projection_y_scale;
  plane_normal.z = 1.0f - bounds_center_y;
  normalize3d(&plane_normal.x);
  plane3d_from_point_and_normal_inline(&view_plane, global_origin3d, &plane_normal);
  FUN_0010a1c0(&fr->field_44.scale, view_plane.normal,
               fr->field_78[3].normal);

  view_plane.normal[0] = 0.0f;
  view_plane.normal[1] = 0.0f;
  view_plane.normal[2] = 1.0f;
  view_plane.d = -camera->z_near;
  FUN_0010a1c0(&fr->field_44.scale, view_plane.normal,
               fr->field_78[4].normal);

  view_plane.normal[0] = 0.0f;
  view_plane.normal[1] = 0.0f;
  view_plane.normal[2] = -1.0f;
  view_plane.d = camera->z_far;
  FUN_0010a1c0(&fr->field_44.scale, view_plane.normal,
               fr->field_78[5].normal);

  fr->field_d8 = camera->z_near;
  fr->field_dc = camera->z_far;
  inverse_projection_x_scale = 1.0f / projection_x_scale;
  inverse_projection_y_scale = 1.0f / projection_y_scale;
  half_z = (camera->z_far + camera->z_near) * 0.5f;
  far_left = left_plane_z * -(inverse_projection_x_scale * camera->z_far);
  far_right =
    (bounds_center_x - 1.0f) * -(inverse_projection_x_scale * camera->z_far);
  far_bottom = bottom_plane_z * -(inverse_projection_y_scale * camera->z_far);
  far_top =
    (bounds_center_y - 1.0f) * -(inverse_projection_y_scale * camera->z_far);

  view_point.x = far_left;
  view_point.y = far_bottom;
  view_point.z = -camera->z_far;
  matrix_transform_point(&fr->field_44.scale, &view_point.x,
                         &fr->field_e0[0].x);
  view_point.x = far_right;
  view_point.y = far_bottom;
  view_point.z = -camera->z_far;
  matrix_transform_point(&fr->field_44.scale, &view_point.x,
                         &fr->field_e0[1].x);
  view_point.x = far_left;
  view_point.y = far_top;
  view_point.z = -camera->z_far;
  matrix_transform_point(&fr->field_44.scale, &view_point.x,
                         &fr->field_e0[2].x);
  view_point.x = far_right;
  view_point.y = far_top;
  view_point.z = -camera->z_far;
  matrix_transform_point(&fr->field_44.scale, &view_point.x,
                         &fr->field_e0[3].x);

  fr->field_e0[4] = camera->field_00;
  view_point.x = -(inverse_projection_x_scale * half_z * bounds_center_x);
  view_point.y = -(inverse_projection_y_scale * half_z * bounds_center_y);
  view_point.z = -half_z;
  matrix_transform_point(&fr->field_44.scale, &view_point.x,
                         &fr->field_11c.x);

  /* World AABB: x0,x1,y0,y1,z0,z1. */
  fr->field_128[0] = fr->field_128[1] = fr->field_e0[0].x;
  fr->field_128[2] = fr->field_128[3] = fr->field_e0[0].y;
  fr->field_128[4] = fr->field_128[5] = fr->field_e0[0].z;
  for (vertex_index = 1; vertex_index < 5; vertex_index++) {
    const vector3_t *vertex = &fr->field_e0[vertex_index];

    fr->field_128[0] =
      fr->field_128[0] > vertex->x ? vertex->x : fr->field_128[0];
    fr->field_128[2] =
      fr->field_128[2] > vertex->y ? vertex->y : fr->field_128[2];
    fr->field_128[4] =
      fr->field_128[4] > vertex->z ? vertex->z : fr->field_128[4];
    fr->field_128[1] =
      fr->field_128[1] > vertex->x ? fr->field_128[1] : vertex->x;
    fr->field_128[3] =
      fr->field_128[3] > vertex->y ? fr->field_128[3] : vertex->y;
    fr->field_128[5] =
      fr->field_128[5] > vertex->z ? fr->field_128[5] : vertex->z;
  }

  if (do_projection) {
    real inverse_plane_z;
    real clip_offset;
    real projection_scale;

    if (camera->z_near == 0.0f) {
      FUN_0010a1c0(&fr->field_10.scale, camera->field_44, view_plane.normal);
    } else {
      view_plane.normal[0] = 0.0f;
      view_plane.normal[1] = 0.0f;
      view_plane.normal[2] = 1.0f;
      view_plane.d = -camera->z_near;
    }

    inverse_plane_z = 1.0f / view_plane.normal[2];
    clip_offset = -(view_plane.d * inverse_plane_z);
    projection_scale =
      (real)(camera->z_far / ((camera->z_far - clip_offset) *
                              (fabs(inverse_plane_z * view_plane.normal[0]) +
                               fabs(inverse_plane_z * view_plane.normal[1]) +
                               1.0)));
    view_plane.normal[0] *= inverse_plane_z * projection_scale;
    view_plane.normal[1] *= inverse_plane_z * projection_scale;
    view_plane.normal[2] = projection_scale;
    view_plane.d = -(projection_scale * clip_offset);
    if (view_plane.d > 0.0f && camera->z_near == 0.0f) {
      view_plane.normal[0] = -view_plane.normal[0];
      view_plane.normal[1] = -view_plane.normal[1];
      view_plane.normal[2] = -view_plane.normal[2];
      view_plane.d = -view_plane.d;
    }

    /* 4x4 projection matrix, row-major [row * 4 + column]. */
    csmemset(fr->field_144, 0, sizeof(fr->field_144));
    fr->field_144[0] = projection_x_scale;
    fr->field_144[2] = -view_plane.normal[0];
    fr->field_144[5] = projection_y_scale;
    fr->field_144[6] = -view_plane.normal[1];
    fr->field_144[8] = -bounds_center_x;
    fr->field_144[9] = -bounds_center_y;
    fr->field_144[10] = -view_plane.normal[2];
    fr->field_144[11] = -1.0f;
    fr->field_144[14] = view_plane.d;
    fr->field_140 = 1;
    fr->field_184[0] = projection_x_scale * viewport_width * 0.5f;
    fr->field_184[1] = projection_y_scale * viewport_height * 0.5f;
  } else {
    csmemset(fr->field_144, 0, sizeof(fr->field_144));
    csmemset(fr->field_184, 0, sizeof(fr->field_184));
    fr->field_140 = 0;
  }

  /* Planes: 0 left, 1 right, 2 bottom, 3 top, 4 near, 5 far.
   * Vertices: 0 bottom-left, 1 bottom-right, 2 top-left, 3 top-right,
   * 4 apex (camera position). */
  render_camera_check_warning_condition(
    0, (real)fabs(plane3d_distance_to_point_inline(&fr->field_78[0],
                                            &fr->field_e0[0])));
  render_camera_check_warning_condition(
    1, (real)fabs(plane3d_distance_to_point_inline(&fr->field_78[0],
                                            &fr->field_e0[2])));
  render_camera_check_warning_condition(
    2, (real)fabs(plane3d_distance_to_point_inline(&fr->field_78[0],
                                            &fr->field_e0[4])));
  render_camera_check_warning_condition(
    3, (real)fabs(plane3d_distance_to_point_inline(&fr->field_78[1],
                                            &fr->field_e0[1])));
  render_camera_check_warning_condition(
    4, (real)fabs(plane3d_distance_to_point_inline(&fr->field_78[1],
                                            &fr->field_e0[3])));
  render_camera_check_warning_condition(
    5, (real)fabs(plane3d_distance_to_point_inline(&fr->field_78[1],
                                            &fr->field_e0[4])));
  render_camera_check_warning_condition(
    6, (real)fabs(plane3d_distance_to_point_inline(&fr->field_78[2],
                                            &fr->field_e0[0])));
  render_camera_check_warning_condition(
    7, (real)fabs(plane3d_distance_to_point_inline(&fr->field_78[2],
                                            &fr->field_e0[1])));
  render_camera_check_warning_condition(
    8, (real)fabs(plane3d_distance_to_point_inline(&fr->field_78[2],
                                            &fr->field_e0[4])));
  render_camera_check_warning_condition(
    9, (real)fabs(plane3d_distance_to_point_inline(&fr->field_78[3],
                                            &fr->field_e0[2])));
  render_camera_check_warning_condition(
    10, (real)fabs(plane3d_distance_to_point_inline(&fr->field_78[3],
                                             &fr->field_e0[3])));
  render_camera_check_warning_condition(
    11, (real)fabs(plane3d_distance_to_point_inline(&fr->field_78[3],
                                             &fr->field_e0[4])));
  render_camera_check_warning_condition(
    12, (real)fabs(plane3d_distance_to_point_inline(&fr->field_78[5],
                                             &fr->field_e0[0])));
  render_camera_check_warning_condition(
    13, (real)fabs(plane3d_distance_to_point_inline(&fr->field_78[5],
                                             &fr->field_e0[1])));
  render_camera_check_warning_condition(
    14, (real)fabs(plane3d_distance_to_point_inline(&fr->field_78[5],
                                             &fr->field_e0[2])));
  render_camera_check_warning_condition(
    15, (real)fabs(plane3d_distance_to_point_inline(&fr->field_78[5],
                                             &fr->field_e0[3])));
  render_camera_check_warning_condition(
    16, plane3d_distance_to_point_inline(&fr->field_78[0], &fr->field_11c));
  render_camera_check_warning_condition(
    17, plane3d_distance_to_point_inline(&fr->field_78[1], &fr->field_11c));
  render_camera_check_warning_condition(
    18, plane3d_distance_to_point_inline(&fr->field_78[2], &fr->field_11c));
  render_camera_check_warning_condition(
    19, plane3d_distance_to_point_inline(&fr->field_78[3], &fr->field_11c));
  render_camera_check_warning_condition(
    20, plane3d_distance_to_point_inline(&fr->field_78[4], &fr->field_11c));
  render_camera_check_warning_condition(
    21, plane3d_distance_to_point_inline(&fr->field_78[5], &fr->field_11c));
}

/* render_contrails - 0x1887b0
 *
 * Walks the live contrail pool and draws each contrail sub-trail whose
 * definition selects one of the render passes named by the incoming mask.
 *
 * The gate byte at 0x32574a is one of the four effect-enable bytes that
 * render_effects (0x184b60) writes together.
 *
 * Evidence (0x1887b0..0x18885f):
 *   - 001887b6 MOV AL,[0x0032574a] / TEST AL,AL / JZ end.
 *   - 001887c3/001887e7/00188842 MOV from [0x005aa8c0] = contrail_data.
 *   - 001887f6 MOV EDX,[EDI+4] = contrail definition_index, passed to
 *     tag_get with group 'cont' (0x636f6e74).
 *   - 0018880c LEA EBX,[EDI+0x2c] = contrail_point_counts[4]; the loop
 *     counter is 16-bit (00188839 CMP SI,0x4 / JL).
 *   - 00188810 MOV CL,[EAX+0x18] is reloaded from [EBP-8] every iteration
 *     (0018882f), so the shift is recomputed inside the inner loop.
 *   - 00188827 PUSH ESI / PUSH EAX / PUSH EDI / CALL 0x00188010 /
 *     ADD ESP,0xc => render_contrail(contrail, definition, index) cdecl.
 *
 * The contrail element type lives in effects/contrails.c and is not visible
 * here, so the two touched offsets are dereferenced raw; the definition tag
 * has no recovered type at all (+0x18 is the only field observed).
 */
void render_contrails(uint32_t render_pass_mask)
{
  int contrail_index;
  char *contrail;
  char *definition;
  int16_t *point_counts;
  int16_t index;

  if (*(char *)0x32574a == 0) {
    return;
  }

  for (contrail_index = data_next_index(contrail_data, -1);
       contrail_index != -1;
       contrail_index = data_next_index(contrail_data, contrail_index)) {
    contrail = (char *)datum_get(contrail_data, contrail_index);
    definition = (char *)tag_get(0x636f6e74, *(int *)(contrail + 4));

    point_counts = (int16_t *)(contrail + 0x2c);
    index = 0;
    do {
      /* 00188818 SHL EDX,CL with no preceding AND: the reference relies on
       * the x86 implicit shift-count mask, so no & 0x1f is written here. */
      if ((render_pass_mask &
           ((uint32_t)1 << *(uint8_t *)(definition + 0x18))) != 0 &&
          *point_counts > 1) {
        render_contrail(contrail, definition, index);
      }
      index++;
      point_counts++;
    } while (index < 4);
  }
}

/* render_contrails_normal - 0x188880
 *
 * Trivial wrapper: selects the "normal" contrail render passes and tail-calls
 * render_contrails.
 *
 * Evidence (0x188880..0x188888):
 *   00188880 PUSH -0xd       ; render_pass_mask = 0xfffffff3
 *   00188882 CALL 0x001887b0 ; render_contrails
 *   00188887 POP ECX         ; cdecl cleanup of the single dword arg
 *   00188888 RET
 *
 * The mask is the one's complement of 0x0000000c, i.e. every pass except the
 * two selected by bits 2 and 3. The pass-bit meanings are not recovered, so
 * the constant is written as the literal the binary pushes.
 */
void render_contrails_normal(void)
{
  render_contrails(0xfffffff3);
}
