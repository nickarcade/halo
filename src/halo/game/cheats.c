#include "../../x87_math.h"

void FUN_000a54b0(void)
{
  int16_t local_player_index;
  char *player_data;
  int16_t palette_index;
  int particle_system_tag_index;
  void *scenario;
  char *palette_element;

  if (*(char *)0x32574c == '\0' || *(char *)0x2ef7ee == '\0')
    return;

  local_player_index = *(int16_t *)0x506548;
  if (local_player_index == -1)
    return;

  player_data = (char *)weather_particle_system_get(local_player_index);
  *(int16_t *)(player_data + 0x14) = *(int16_t *)0x506784;
  *(int *)(player_data + 0x10) = *(int *)0x506780;

  *(char *)(player_data + 0x1a) = (char)FUN_0018f3e0(
    player_data + 0x10, (void *)0x506550, (int16_t *)(player_data + 0x18));

  palette_index = *(int16_t *)(player_data + 0x18);
  particle_system_tag_index = -1;

  if (palette_index != -1) {
    scenario = scenario_get();
    palette_element = (char *)tag_block_get_element((char *)scenario + 0x1b4,
                                                    (int)palette_index, 0xf0);
    particle_system_tag_index = *(int *)(palette_element + 0x2c);
  }

  if (*(int *)player_data != particle_system_tag_index) {
    if (*(int *)player_data != -1) {
      weather_particle_system_delete(local_player_index);
    }
    if (particle_system_tag_index != -1) {
      weather_particle_system_new(local_player_index, particle_system_tag_index,
                                  1.0f);
    }
  }

  if (*(int *)player_data != -1) {
    weather_particle_system_render(local_player_index);
  }
}

/* FUN_000a5590 (0xa5590)
 *
 * Linear falloff ramp over a float pair. Both parameters are proven floats by
 * the body itself: FLD float ptr [EBP+0x8] and FLD float ptr [EBP+0xc].
 * Confirmed from disassembly (no calls, no globals other than .rdata):
 *   FLD [EBP+0xc]; FMUL [0x253398]; FSTP [EBP-0x4]   -> half = range * 0.5f
 *   FLD [EBP+0x8]; FCOMP [EBP+0xc]; TEST AH,0x1; JNZ -> C0 set means
 *     value < range, so the FALL-THROUGH (value >= range) returns
 *     FLD [0x2533c0] = 0.0f.
 *   FLD [EBP+0x8]; FCOMP [EBP-0x4]; TEST AH,0x41; JP -> JP is taken only when
 *     neither C0 nor C3 is set (value > half), so the FALL-THROUGH
 *     (value < half or value == half) returns FLD [0x2533c8] = 1.0f.
 *   FLD [EBP+0xc]; FSUB [EBP+0x8]; FLD [EBP+0xc]; FSUB [EBP-0x4]; FDIVP
 *     -> ST(1)/ST(0) = (range - value) / (range - half), in that operand
 *     order.
 * 0x253398 = 0.5f, 0x2533c0 = 0.0f, 0x2533c8 = 1.0f are the shared .rdata
 * constants used throughout the binary; read through their addresses so the
 * reference's FLD/FMUL m32 form is preserved (a literal would let the
 * compiler pick FLDZ/FLD1).
 * The semantic meaning of the two parameters is unknown: the only callers are
 * FUN_000a55e0 (which just forwards its own arguments) and the unported
 * FUN_000a5ac0, so the names stay generic.
 */
float FUN_000a5590(float value, float range)
{
  float half;

  half = range * *(float *)0x253398;
  if (value >= range) {
    return *(float *)0x2533c0;
  }
  if (value <= half) {
    return *(float *)0x2533c8;
  }
  return (range - value) / (range - half);
}

/* FUN_000a55e0 (0xa55e0)
 *
 * Multiplies the results of two calls to FUN_000a5590 (0xa5590, cdecl,
 * 2 float args -> float in ST0; the callee loads both stack slots with
 * FLD float ptr, so these forwarded arguments are floats, not ints).
 * Confirmed from disassembly:
 *   000a55ec CALL FUN_000a5590(arg1, arg2) -> FSTP [EBP-4] (saved)
 *   000a55fc CALL FUN_000a5590(arg3, arg4) -> FMUL [EBP-4] (result * saved)
 * Argument push order at each call site is the standard cdecl
 * right-to-left push (second param pushed first), so callee arg order is
 * NOT swapped: call 1 is FUN_000a5590(arg1, arg2), call 2 is
 * FUN_000a5590(arg3, arg4). No evidence of the semantic meaning of arg1-4
 * beyond their float type -- kept as generic names, and no callers found in
 * this artifact (xrefs_to: none).
 */
float FUN_000a55e0(float arg1, float arg2, float arg3, float arg4)
{
  float saved;

  saved = FUN_000a5590(arg1, arg2);
  return FUN_000a5590(arg3, arg4) * saved;
}

/* compare_targets (0xa5700)
 *
 * qsort comparator over the 0x38-byte candidate-target records built by
 * FUN_000a5f00 and sorted at 0xa60a8 (the only reference to this address is
 * that DATA xref, i.e. the qsort function-pointer argument).
 *
 * Confirmed from the disassembly at 0xa5700 (ECX = param a = [EBP+8],
 * EDX = param b = [EBP+0xc]; each FLD/FCOMP pair loads a's field and compares
 * it against b's, so FNSTSW C0 means a < b and C3 means a == b):
 *   0xa5709 TEST AH,0x41 / JZ   -> a[0x30] >  b[0x30] -> -1
 *   0xa5716 TEST AH,0x05 / JNP  -> a[0x30] <  b[0x30] -> +1
 *   0xa5723 TEST AH,0x41 / JZ   -> a[0x34] >  b[0x34] -> -1
 *   0xa5730 TEST AH,0x05 / JNP  -> a[0x34] <  b[0x34] -> +1
 *   0xa573d TEST AH,0x05 / JNP  -> a[0x28] <  b[0x28] -> -1
 *   0xa574a TEST AH,0x41 / JZ   -> a[0x28] >  b[0x28] -> +1
 *   0xa5757 TEST AH,0x05 / JP   -> fall through when a[0x2c] < b[0x2c] -> -1
 *   0xa576b TEST AH,0x41 / JNZ  -> equal -> tie-break; else +1
 * Two of the four keys therefore sort descending (0x30, 0x34) and two
 * ascending (0x28, 0x2c).  The tie-break at 0xa577f loads the full dword at
 * offset 0 of each record and masks it with 0xffff before subtracting
 * (MOV/AND/AND/SUB, not MOVZX), i.e. the low 16 bits of the datum handle the
 * record carries at offset 0 -- that same dword is passed as the handle
 * argument to FUN_000a5830 at 0xa60ea.  The semantic meaning of the four
 * float keys is unknown from this evidence, so they stay raw offsets.
 */
int compare_targets(const void *a, const void *b)
{
  const char *ra;
  const char *rb;

  ra = (const char *)a;
  rb = (const char *)b;

  if (*(const float *)(ra + 0x30) > *(const float *)(rb + 0x30))
    return -1;
  if (*(const float *)(ra + 0x30) < *(const float *)(rb + 0x30))
    return 1;

  if (*(const float *)(ra + 0x34) > *(const float *)(rb + 0x34))
    return -1;
  if (*(const float *)(ra + 0x34) < *(const float *)(rb + 0x34))
    return 1;

  if (*(const float *)(ra + 0x28) < *(const float *)(rb + 0x28))
    return -1;
  if (*(const float *)(ra + 0x28) > *(const float *)(rb + 0x28))
    return 1;

  if (*(const float *)(ra + 0x2c) < *(const float *)(rb + 0x2c))
    return -1;
  if (*(const float *)(ra + 0x2c) > *(const float *)(rb + 0x2c))
    return 1;

  return (int)(*(const uint32_t *)ra & 0xffff) -
         (int)(*(const uint32_t *)rb & 0xffff);
}

/* reciprocal_square_root (0xa57a0)
 *
 * Whole function, verbatim from the disassembly (7 instructions, no calls):
 *   000a57a0 PUSH EBP
 *   000a57a1 MOV  EBP,ESP
 *   000a57a3 FLD  float ptr [EBP+0x8]   -> the single argument is a float
 *   000a57a6 FSQRT                      -> ST(0) = sqrt(value)
 *   000a57a8 FDIVR float ptr [0x2533c8] -> ST(0) = [0x2533c8] / sqrt(value)
 *   000a57ae POP  EBP
 *   000a57af RET                        -> result returned in ST(0)
 *
 * FDIVR is the reversed form, so the .rdata constant is the dividend, not the
 * divisor.  0x2533c8 is the same .rdata slot FUN_000a5590 (0xa5590) loads as
 * its 1.0f return value, so the constant is 1.0f and the function is
 * 1.0f / sqrt(value).
 *
 * No domain guard: a negative or zero argument is passed straight to FSQRT,
 * matching the original.  The Ghidra decompile for this address is stale (an
 * empty `void __cdecl FUN_000a57a0(void)`); the disassembly above is the
 * evidence used here, and it also proves the float parameter and float return
 * that the kb.json decl previously spelled `void (void)`.
 */
real reciprocal_square_root(real value)
{
  return 1.0f / sqrtf(value);
}

/* set_real_euler_angles2d (0xa5810)
 *
 * Two-store setter, straight from the disassembly:
 *   0xa5813 MOV EAX,[EBP+0x8]     -> destination pointer (arg 1)
 *   0xa5816 FLD  float ptr [EBP+0xc]  -> arg 2 loaded as a float
 *   0xa5819 MOV ECX,[EBP+0x10]    -> arg 3 copied as a plain dword
 *   0xa581c FSTP float ptr [EAX+0x4]  -> arg 2 stored at destination +0x04
 *   0xa581f MOV  [EAX],ECX        -> arg 3 stored at destination +0x00
 *
 * The argument slots are fixed, so the mapping is unambiguous: arg 2 lands at
 * +0x04 and arg 3 at +0x00.  Which component is yaw and which is pitch is NOT
 * proven -- the function has no callers in this build and no assert string --
 * so the parameters are named after the offsets they write.  Arg 2 is a float
 * by the FLD; arg 3's form (a dword copy) does not prove its type, but the
 * symbol name and the adjacent float make `real` the recovered spelling.
 */
void set_real_euler_angles2d(real *angles, real angle_04, real angle_00)
{
  angles[1] = angle_04;
  angles[0] = angle_00;
}

/* FUN_000a5d70 (0xa5d70)
 *
 * Recursive per-cluster worker behind FUN_000a5f00 (0xa5f00, unported): walks
 * the linked list of objects rooted at `object_handle` (object field +0xc4 =
 * "next object in this cluster"), filters each object by type mask, deletion
 * flag, unit health fraction, cone containment (FUN_00110210), team
 * allegiance and a "bipd"-tag flag, appends any matching candidate's 0x38-byte
 * record (built by FUN_000a5ac0, unported) into `out_buffer`, then recurses
 * into the object's linked cluster (field +0xc8 = "next cluster", -1
 * terminated) before continuing the object-list walk.
 *
 * Confirmed from disassembly at 0xa5d70:
 *   - The decompiler's `in_stack_XXXXXXXX` stack-slot names are offset -4
 *     from the true [EBP+N] disassembly locations in this function (e.g. its
 *     `in_stack_0000000c` is really [EBP+0x10], `in_stack_00000020` is really
 *     [EBP+0x24]). Every parameter below was derived from the raw [EBP+N]
 *     operands and the recursive self-call's argument marshalling at
 *     0xa5e93-0xa5ecb (which forwards params 3-9 unchanged and only threads
 *     the next handle / remaining capacity / advanced buffer pointer), not
 *     from the decompiler's mislabeled variable names.
 *   - param_1 ([EBP+8]) is never read in this function; it is only forwarded
 *     unchanged as the first pushed arg to FUN_000a5ac0 (0xa5e5c) and to the
 *     recursive self-call (0xa5eca).
 *   - The function returns its running match count in AX only (0xa5eec
 *     `MOV AX,BX`); the caller's `ADD EBX,EAX` (0xa5ed3) only ever executes
 *     right after the recursive CALL itself, so the upper 16 bits of EAX are
 *     always freshly the callee's own (equally AX-only) return and never
 *     carry stale garbage into a live comparison -- every consumer of the
 *     count reads BX/AX, never the high word. int16_t is exact.
 */
int16_t FUN_000a5d70(void *param_1, int object_handle, float *arg_p3,
                     float *arg_p4, float arg_p5, float arg_sine,
                     float arg_cosine, int exclude_handle, int16_t query_team,
                     int16_t max_count, void *out_buffer)
{
  char *obj;
  char *unit_obj;
  char *tag_data;
  char cone_match;
  char record_built;
  int record_buf[14];
  int datum_handle;
  int next_cluster;
  int type_byte;
  int16_t total_count;

  datum_handle = object_handle;
  total_count = 0;

  do {
    obj = (char *)object_get_and_verify_type(datum_handle, -1);
    type_byte = *(unsigned char *)(obj + 0x64) & 0x1f;

    if (((1 << type_byte) & 3) != 0 && (*(unsigned char *)(obj + 4) & 1) == 0) {
      unit_obj = (char *)object_get_and_verify_type(datum_handle, 3);

      if (*(float *)(unit_obj + 0x32c) < *(float *)0x2533c8) {
        cone_match = FUN_00110210((float *)(obj + 0x50), *(float *)(obj + 0x5c),
                                  arg_p3, arg_p4, arg_p5, arg_sine, arg_cosine);

        if (cone_match != 0) {
          if (((1 << type_byte) & 1) != 0 &&
              (*(unsigned char *)(obj + 0xb6) & 4) == 0 &&
              datum_handle != exclude_handle) {
            if (game_allegiance_get_team_is_friendly(
                  query_team, *(int16_t *)(obj + 0x68))) {
              tag_data = (char *)tag_get(0x62697064, *(int *)obj);

              if ((*(unsigned int *)(tag_data + 0x17c) & 0x200000) == 0) {
                record_built = FUN_000a5ac0(param_1, datum_handle, arg_p3,
                                            arg_p4, record_buf);

                if (record_built != 0 && total_count < max_count) {
                  csmemcpy((char *)out_buffer + (int)total_count * 0x38,
                           record_buf, 0x38);
                  total_count = total_count + 1;
                }
              }
            }
          }

          next_cluster = *(int *)(obj + 0xc8);
          if (next_cluster != -1 && total_count < max_count) {
            total_count =
              (int16_t)(total_count +
                        FUN_000a5d70(
                          param_1, next_cluster, arg_p3, arg_p4, arg_p5,
                          arg_sine, arg_cosine, exclude_handle, query_team,
                          (int16_t)(max_count - total_count),
                          (char *)out_buffer + (int)total_count * 0x38));
          }
        }
      }
    }

    datum_handle = *(int *)(obj + 0xc4);
  } while (datum_handle != -1 && total_count < max_count);

  return total_count;
}

/* FUN_000a6030 (0xa6030)
 *
 * Locate the best candidate record inside the cone described by `cone_spec`,
 * starting from the structure cluster that contains `point`.
 *
 * Confirmed from the disassembly at 0xa6030:
 *   - param2 ([EBP+0xc], held in EBX) is the point: it is the sole argument to
 *     bsp3d_find_leaf_point (FUN_0018e720) at 0xa603f/0xa6053, and is
 *     forwarded unchanged to FUN_000a5f00 (0xa6097) and FUN_000a5830
 *     (0xa60ea) -- so FUN_000a5830's first parameter is that same point, not
 *     an object handle.
 *   - the FIRST stack argument to FUN_000a5f00 is EAX at 0xa6098, i.e. the
 *     cluster index loaded from the bsp leaf element at 0xa6072
 *     (MOV AX, word ptr [EAX+8]) and range-checked against -1 at 0xa6079.
 *     It is NOT param1.
 *   - param1 ([EBP+8]) is loaded into EDI at 0xa6082 and stays live across the
 *     CALL at 0xa6099: it is FUN_000a5f00's implicit @<edi> argument.  That
 *     callee reads four floats from it ([EDI+0]/[EDI+8] = angle,
 *     [EDI+4]/[EDI+0xc] = distance) and derives the cone length/sine/cosine it
 *     hands to structure_clusters_in_cone (0x198ad0).  EDI is reloaded with
 *     the return count at 0xa60a1, which is why the original's live range ends
 *     at the call.
 */
char FUN_000a6030(float *cone_spec, float *point, float *direction, float *arg4,
                  float *arg5, void *out_struct)
{
  void *scenario;
  void *leaf_element;
  int leaf_index;
  int16_t cluster_index;
  int16_t count;
  int16_t i;
  char local_buffer[0xe00];
  char *elem;

  if (FUN_0018e720((int)point) == -1)
    return 0;

  leaf_index = FUN_0018e720((int)point) & 0x7fffffff;
  scenario = scenario_get();
  leaf_element =
    tag_block_get_element((char *)scenario + 0xe0, leaf_index, 0x10);
  cluster_index = *(int16_t *)((char *)leaf_element + 8);

  if (cluster_index == -1)
    return 0;

  count = (int16_t)FUN_000a5f00(cone_spec, cluster_index, point, direction,
                                arg4, arg5, 0x40, local_buffer);

  if (count <= 0)
    return 0;

  qsort(local_buffer, (size_t)count, 0x38, (qsort_compar_proc)compare_targets);

  for (i = 0; i < count; i++) {
    elem = local_buffer + (int)i * 0x38;
    if (FUN_000a5830(point, elem + 4, arg4, *(int *)elem)) {
      qmemcpy(out_struct, elem, 0x38);
      return 1;
    }
  }

  return 0;
}

void cheats_initialize(void)
{
  csmemset(cheats_globals, 0, sizeof(cheats_globals));
}

void cheats_dispose(void)
{
}

void cheats_dispose_from_old_map(void)
{
}

void cheats_update(void)
{
  int16_t player_index;
  void *gamepad;
  char *cheat;
  char *btn;
  int cnt;

  if (!cheat_controller)
    return;

  player_index = (int16_t)local_player_get_next(-1);
  while (player_index != -1) {
    gamepad = input_get_gamepad_state(player_index);
    if (gamepad != NULL && *(char *)((char *)gamepad + 0x1d)) {
      cheat = cheats_globals;
      btn = (char *)gamepad + 0x10;
      cnt = 0x10;
      do {
        if (*cheat != '\0' && *btn != '\0') {
          director_set_local_player_context(player_index);
          if (*btn == '\x01') {
            console_printf(0, cheat);
            if (!hs_console_evaluate(cheat))
              *cheat = '\0';
          }
        }
        btn++;
        cheat += 0xc8;
        cnt--;
      } while (cnt != 0);
    }
    player_index = (int16_t)local_player_get_next(player_index);
  }
}

void cheats_load_from_file(void)
{
  void *stream;
  int16_t slot;
  char *entry;

  stream = crt_fopen("d:\\cheats.txt", "r");
  if (stream == NULL)
    return;

  for (slot = 0; slot < 16; slot++) {
    entry = cheats_globals + (int)slot * 200;
    if (crt_fgets(entry, 199, stream) == NULL)
      break;
    csstrtok(entry, "\r\n\t;");
    if ((slot == 12 || slot == 13) && *entry != '\0') {
      /* Second textual use of the address expression: with only one use cl.exe
       * sinks the add into the scaled-index register (`add esi,0`); a second
       * use makes it CSE the value and emit the reference's `lea esi,(esi)`. */
      *(cheats_globals + (int)slot * 200) = '\0';
      error(2, "Cannot execute cheats attached to the back or start button");
    }
  }
  crt_fclose(stream);
}

/* cheat_active_camouflage_local_player — give weapon infinite ammo cheat for
 * one local player. Finds the player's primary weapon, sets its vitality
 * to 1.0f (full), and sets bit 4 (+optionally bit 5) in the weapon's flags at
 * +0x1b4. Source: cheats.c, local_player_index in [0,3].
 */
void cheat_active_camouflage_local_player(int local_player_index)
{
  int player_handle;
  char *player;
  char *weapon;
  unsigned int flags;

  if ((short)local_player_index < 0 || (short)local_player_index >= 4)
    return;
  player_handle = local_player_get_player_index((int16_t)local_player_index);
  if (player_handle == -1)
    return;
  player = (char *)datum_get(player_data, player_handle);
  weapon = (char *)object_get_and_verify_type(*(int *)(player + 0x34), 3);
  *(unsigned int *)(weapon + 0x32c) = 0x3f800000;
  flags = *(unsigned int *)(weapon + 0x1b4);
  if (flags & 0x10)
    *(unsigned int *)(weapon + 0x1b4) = flags | 0x20;
  *(unsigned int *)(weapon + 0x1b4) |= 0x10;
}

/* FUN_000a67c0 — find the first player datum that has a weapon equipped.
 * Returns the datum handle of the player, or -1 if none found.
 * The datum handle is stored in the iterator at offset 8 (data_iter_t.datum).
 */
int FUN_000a67c0(void)
{
  data_iter_t iter;
  char *player;

  data_iterator_new(&iter, player_data);
  player = (char *)data_iterator_next(&iter);
  while (player != NULL) {
    if (*(int *)(player + 0x34) != -1)
      return *(int *)((char *)&iter + 8);
    player = (char *)data_iterator_next(&iter);
  }
  return -1;
}

void cheats_initialize_for_new_map(void)
{
  cheats_load_from_file();
}

/* cheat_teleport_to_camera — teleport cheat: move a player's vehicle/weapon to
 * the camera. Finds a player with a weapon (via FUN_000a67c0), then teleports
 * the vehicle (or weapon if not in a vehicle) to the camera position using
 * object_set_position. Frameless in the original binary.
 */
typedef void (*terminal_output_2_t)(void *, const char *);

void cheat_teleport_to_camera(void)
{
  int player_handle;
  char *player;
  short local_player_idx;
  char *camera;
  char *weapon_obj;
  int object_handle;

  player_handle = FUN_000a67c0();
  if (player_handle == -1)
    return;
  player = (char *)datum_get(player_data, player_handle);
  local_player_idx = *(short *)(player + 2);
  if (local_player_idx == (short)-1)
    return;
  camera = (char *)observer_get_camera((unsigned short)local_player_idx);
  if (!camera) {
    display_assert("result", "c:\\halo\\SOURCE\\game\\cheats.c", 0x100, 1);
    system_exit(-1);
  }
  if (*(short *)(camera + 0x10) != (short)-1) {
    weapon_obj = (char *)object_get_and_verify_type(*(int *)(player + 0x34), 3);
    object_handle = *(int *)(weapon_obj + 0xcc);
    if (object_handle == -1)
      object_handle = *(int *)(player + 0x34);
    object_set_position(object_handle, (float *)camera, NULL, NULL);
    return;
  }
  ((terminal_output_2_t)terminal_printf)(
    *(void **)0x2ee6f0,
    "Camera is outside BSP... cannot initiate teleportation...");
}

/* cheat_all_powerups — give weapon infinite ammo for the first armed player.
 * Same weapon modification logic as cheat_active_camouflage_local_player but
 * targets the first player that has a weapon equipped (via FUN_000a67c0) rather
 * than a specific local index. Frameless in the original binary.
 */
void cheat_all_powerups(void)
{
  int player_handle;
  char *player;
  char *weapon;
  unsigned int flags;

  player_handle = FUN_000a67c0();
  if (player_handle == -1)
    return;
  player = (char *)datum_get(player_data, player_handle);
  weapon = (char *)object_get_and_verify_type(*(int *)(player + 0x34), 3);
  *(unsigned int *)(weapon + 0x32c) = 0x3f800000;
  flags = *(unsigned int *)(weapon + 0x1b4);
  if (flags & 0x10)
    *(unsigned int *)(weapon + 0x1b4) = flags | 0x20;
  *(unsigned int *)(weapon + 0x1b4) |= 0x10;
}

/* FUN_000a6930 -- spawn one object per non-NONE tag index in a 0x10-byte
 * stride record array, arranged on a circle around the first armed player.
 *
 * records (param_1) -- base of the record array; each 0x10-byte element holds
 *                      a tag index at +0xC (NONE (-1) skips that slot).
 * count   (param_2) -- element count, re-read as a signed 16-bit word from the
 *                      stack slot on every iteration (MOV AX,[EBP+0xC]).
 *
 * Bails out when no player has a weapon equipped (FUN_000a67c0 == -1).
 * Otherwise takes that player's object (player+0x34) world position and
 * orientation, then for each populated slot places an object at
 *   spacing = min(*(float *)0x255a54 / count, *(float *)0x26b164)
 *   angle   = (i - count/2) * spacing + atan2(forward.x, forward.y)
 *   x = cos(angle) * *(float *)0x2533ec + position.x
 *   y = sin(angle) * *(float *)0x2533ec + position.y
 *   z = position.z + *(float *)0x2533f0
 * with the player's forward/up copied into the placement descriptor.
 * The constants keep their raw addresses because their values are not proven
 * here beyond 0x255a54 = 6.2831855f (2*pi, named in structures.c).
 *
 * Call-site verification (raw disassembly at 0xa6930):
 *   0xa694f datum_get: PUSH EAX(*(data_t **)0x5aa6d4), PUSH EAX(handle)
 *   0xa695c object_get_and_verify_type: PUSH 3, PUSH [ESI+0x34]; result unused
 *   0xa6969 object_get_world_position: PUSH EDX(&position), PUSH [ESI+0x34]
 *   0xa697a object_get_orientation: PUSH ECX(&up), PUSH EDX(&forward),
 *           PUSH EAX([ESI+0x34]).  ADD ESP,0x24 merges the cleanup of all
 *           four calls, so the ARG_COUNT=9 hazard on this site is that merge
 *           and not a 9-argument call.
 *   0xa69fb object_placement_data_new: PUSH -1, PUSH ECX(*record),
 *           PUSH EAX(&placement)
 *   0xa6a62 object_new: PUSH ECX(&placement); ADD ESP,0x10 covers both.
 * FPATAN operand order: FLD [EBP-0x10] (forward.x) then FLD [EBP-0x0c]
 * (forward.y), so ST(1) = forward.x is the atan2 numerator.
 * The placement buffer is 0x88 bytes ([EBP-0xB0 .. EBP-0x29]), matching the
 * 0x88 bytes object_placement_data_new writes.
 */
void FUN_000a6930(int records, unsigned short count)
{
  object_placement_data placement;
  vector3_t position;
  vector3_t forward;
  vector3_t up;
  int player_handle;
  char *player;
  int i;
  int *record;
  unsigned int remaining;
  float spacing;
  float angle;

  player_handle = FUN_000a67c0();
  if (player_handle == -1)
    return;
  player = (char *)datum_get(player_data, player_handle);
  object_get_and_verify_type(*(int *)(player + 0x34), 3);
  object_get_world_position(*(int *)(player + 0x34), &position);
  object_get_orientation(*(int *)(player + 0x34), (float *)&forward,
                         (float *)&up);
  if ((short)count <= 0)
    return;
  i = 0;
  record = (int *)((char *)records + 0xc);
  remaining = count;
  do {
    if (*record != -1) {
      spacing = *(float *)0x00255a54 / (float)(short)count;
      if (spacing > *(float *)0x0026b164)
        spacing = *(float *)0x0026b164;
      angle = (float)(i - (short)count / 2) * spacing +
              (float)atan2((double)forward.x, (double)forward.y);
      object_placement_data_new(&placement, *record, -1);
      placement.forward = forward;
      placement.up = up;
      placement.position_x =
        x87_fcos(angle) * *(float *)0x002533ec + position.x;
      placement.position_y =
        x87_fsin(angle) * *(float *)0x002533ec + position.y;
      placement.position_z = position.z + *(float *)0x002533f0;
      object_new(&placement);
    }
    i++;
    record += 4;
    remaining--;
  } while (remaining != 0);
}
