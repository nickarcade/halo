/* path_obstacle_avoidance.c — AI path obstacle-avoidance helpers.
 *
 * Corresponds to path_obstacle_avoidance.obj.
 *
 * Recovered by lifting from cachebeta.xbe (v01.10.12.2276).
 */
#include "../../common.h"

/*
 * FUN_00060c40 -- valid_real_point2d: returns true when both components of a
 * real_point2d (x, y) are finite (neither NaN nor +/-Inf).
 *
 * A float is non-finite iff its IEEE-754 exponent field is all ones, i.e.
 * (bits & 0x7f800000) == 0x7f800000. The original materializes the boolean
 * in full EAX (MOV EAX,1 / XOR EAX,EAX); Ghidra collapsed the two returns to
 * void because callers discarded the result. Same 0x7f800000 mask idiom as
 * valid_real_rgb_color.
 *
 * ABI: cdecl, one stack pointer arg (real_point2d*), pure integer leaf.
 *
 * Shape (delinked 00060c40.obj): each component is copied into a float local
 * first, then bit-tested through the local — VC71 spills the local into the
 * dead param home slot ([EBP+8], MOV [EBP+8],ECX / MOV [EBP+8],EAX), keeping
 * the frame at zero locals. Testing point[N]'s bits directly loses those
 * stores (59.5%); the local recovers them. The tests are spelled as a nested
 * valid-chain (`!= mask` guarding inward, shared `return 0` tail) so both
 * branches compile to JE into the trailing XOR EAX block — goto/early-return
 * spellings made VC71 flip the second branch (85.7%). 100.0% VC71.
 */
int valid_real_point2d(float *point)
{
  float v;

  v = point[0];
  if ((*(uint32_t *)&v & 0x7f800000) != 0x7f800000) {
    v = point[1];
    if ((*(uint32_t *)&v & 0x7f800000) != 0x7f800000)
      return 1;
  }
  return 0;
}

/*
 * FUN_000616e0 -- path_find (name: 2276 symbol dump, T1).
 *
 * ABI (binary evidence, caller FUN_00061750 @ 0x619d3..0x619f1 and this
 * function's prologue): avoidance_record arrives in ESI; three more values
 * arrive in EDX/ECX/EAX and are pushed through unchanged as FUN_00060ea0's
 * last three stack args (PUSH EAX / PUSH ECX / PUSH EDX before any other
 * push). Six cdecl stack params (caller ADD ESP,0x18). The caller loads EDX
 * from a dword local, ECX from a byte (XOR ECX,ECX / MOV CL), and AL=1, and
 * tests only AL of the result. Register params are declared first
 * (EAX, ECX, EDX, then ESI) so the reverse thunk stages them in scratch
 * slots; the C parameter order is not source-order evidence. Return is
 * materialized in full EAX (XOR EAX,EAX / SETNZ AL).
 *
 * FUN_00060ea0 argument map (ADD ESP,0x24 = 9 stack args):
 *   ECX=avoidance_record, EAX=[EBP+0x1c], then [EBP+0xc], scenario_get(),
 *   [EBP+0x8], [EBP+0x10], [EBP+0x14], [EBP+0x18], EDX, ECX, EAX.
 * FUN_000615b0 is repeated while it returns nonzero in AL.
 * +0x1e/+0x20 are int16 fields (compared against -1); +0x28 is a byte flag.
 */
int path_find(unsigned char param_10, unsigned char param_9, float param_8,
              void *avoidance_record, unsigned char param_4, void *param_2,
              float radius, float *start_point, int param_7,
              float *end_point)
{
  char *record;

  record = (char *)avoidance_record;
  FUN_00060ea0(avoidance_record, end_point, param_2, scenario_get(), param_4,
               radius, start_point, param_7, param_8, param_9, param_10);
  do {
  } while ((char)FUN_000615b0(avoidance_record) != 0);
  if (*(int16_t *)(record + 0x1e) != -1) {
    *(unsigned char *)(record + 0x28) = 1;
  } else if (*(int16_t *)(record + 0x20) != -1) {
    *(int16_t *)(record + 0x1e) = *(int16_t *)(record + 0x20);
  }
  return *(int16_t *)(record + 0x1e) != -1;
}
