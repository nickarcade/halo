/*
 * bungie_net/64bit_math.c — 64-bit unsigned integer math on 16-bit limbs
 * XBE source: c:\halo\SOURCE\bungie_net\common\64bit_math.c
 *
 * Confirmed TU identity: the assert at 0x7ff5f pushes the __FILE__ string at
 * 0x265a54, "c:\halo\SOURCE\bungie_net\common\64bit_math.c". The neighbouring
 * functions 0x7ffe0 / 0x80070 / 0x800d0 share that string (0x265a81,
 * "a && result", is one of their assert messages), so they belong here too.
 *
 * Operands are 4-element arrays of 16-bit limbs in little-endian order
 * (index 0 = least significant). Every limb access in the disassembly is a
 * MOVZX of a `word ptr` at +0/+2/+4/+6 and every store is a 16-bit
 * `MOV word ptr [dst+N], AX`.
 *
 * Re-implemented functions (by XBE address, ascending):
 *   0x7ff40  math64_add
 *   0x7ffe0  math64_negate
 *   0x80070  math64_subtract
 *   0x800d0  math64_multiply
 *   0x80210  math64_divide
 */

#include "common.h"

/* An 8-byte limb group. math64_divide's working register is two of these laid
 * out contiguously, and the reference copies each group with a single
 * load/load/store/store dword pair rather than four 16-bit moves, so the
 * copies have to be whole-object assignments and not element loops. */
typedef struct {
  uint16_t limb[4];
} math64_half_t;

/* The 128-bit shift-subtract working register: half[0] is the quotient
 * accumulator, half[1] the remainder accumulator, and the shift step walks all
 * eight limbs through `limb`. The union is what lets one object be addressed
 * both ways; the reference's shift loop indexes `word ptr [EBP+ECX*2-0x2c]`
 * for ECX in 0..7, i.e. straight across the two halves. */
typedef union {
  math64_half_t half[2];
  uint16_t limb[8];
} math64_work_t;

/* 64-bit unsigned add: result = a + b, carry propagated across four 16-bit
 * limbs.
 *
 * Confirmed (0x7ff40-0x7ffd5):
 *  - cdecl, three stack pointer args: a @[EBP+8]->ESI, b @[EBP+0xc]->EBX,
 *    result @[EBP+0x10]->EDI. No register args and no return value.
 *  - The null guard tests run in argument order (TEST ESI / TEST EBX /
 *    TEST EDI), so the assert condition is `a && b && result`. Its failure
 *    block at 0x7ff5b is laid out ahead of the body and reached by
 *    fall-through; the body starts at the aligned 0x7ff78.
 *  - The carry is materialised with XOR ECX,ECX / CMP EAX,0xffff / SETG CL.
 *    SETG (not SETA) proves a *signed* accumulator, so the sum is an `int` and
 *    0xffff is a plain int constant.
 *  - Limb 0 has no carry-in and limb 3 computes no carry-out, i.e. a uniform
 *    loop body with carry=0 constant-folded into the first iteration and the
 *    last iteration's dead carry eliminated. VC71 fully unrolls the loop to the
 *    constant +0/+2/+4/+6 displacements the reference uses.
 *
 */
void math64_add(const uint16_t *a, const uint16_t *b, uint16_t *result)
{
  int32_t sum;
  int32_t carry;
  int32_t i;

  assert_halt_at("c:\\halo\\SOURCE\\bungie_net\\common\\64bit_math.c", 0x21,
                 a && b && result);

  carry = 0;
  for (i = 0; i < 4; i++) {
    sum = a[i] + b[i] + carry;
    if (sum > 0xffff)
      carry = 1;
    else
      carry = 0;
    result[i] = (uint16_t)sum;
  }
}

/* 64-bit two's-complement negate: result = -a across four 16-bit limbs.
 * The arithmetic is a correct negate, unlike math64_multiply's seeded
 * accumulator: with borrow = 1 the limb value is -(a[i]+1) == ~a[i] (mod
 * 2^16), and borrow is 1 exactly when a lower limb was non-zero, which is the
 * textbook ripple form of ~a + 1.
 */
void math64_negate(const uint16_t *a, uint16_t *result)
{
  uint32_t borrow = 0;

  assert_halt_at("c:\\halo\\SOURCE\\bungie_net\\common\\64bit_math.c", 0x3a,
                 a && result);

  result[0] = (uint16_t)-a[0];
  if (a[0])
    borrow = 1;
  result[1] = (uint16_t)-(a[1] + borrow);
  if (a[1])
    borrow = 1;
  result[2] = (uint16_t)-(a[2] + borrow);
  if (a[2])
    borrow = 1;
  result[3] = (uint16_t)-(a[3] + borrow);
}

/* 64-bit subtract: result = a - b, implemented as a + (-b).
 *
 * Note that because math64_negate is a true two's-complement negate (see
 * above), this is a correct modular subtract — unlike math64_multiply, whose
 * seeded accumulator makes it produce wrong products.
 *
 * Match note: the call into math64_negate is the whole gap. The reference
 * passes its two arguments in ESI/EBX, which is an MSVC LTCG custom calling
 * convention we cannot ask VC71 to reproduce from source (`__fastcall` is
 * ECX/EDX, not ESI/EBX), so our compile necessarily emits a two-push cdecl
 * call plus the register loads the reference folds away. This is the known
 * @<reg>-call-site ceiling, not a structural defect in the lift. Behaviour is
 * proven separately by unicorn equivalence.
 */
void math64_subtract(const uint16_t *a, const uint16_t *b, uint16_t *result)
{
  uint16_t negated_b[4];

  assert_halt_at("c:\\halo\\SOURCE\\bungie_net\\common\\64bit_math.c", 0x4f,
                 a && b && result);

  math64_negate(b, negated_b);
  math64_add(a, negated_b, result);
}

/* 64-bit unsigned multiply: result = a * b, schoolbook over four 16-bit limbs. */
void math64_multiply(const uint16_t *a, const uint16_t *b, uint16_t *result)
{
  uint32_t acc[8];
  uint32_t product;
  uint32_t i;
  uint32_t j;

  acc[0] = 0;
  acc[1] = 1;
  acc[2] = 2;
  acc[3] = 3;
  acc[4] = 4;
  acc[5] = 5;
  acc[6] = 6;

  assert_halt_at("c:\\halo\\SOURCE\\bungie_net\\common\\64bit_math.c", 0x5f,
                 a && b && result);

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      product = a[i] * b[j];
      acc[i + j] += product & 0xffff;
      acc[i + j + 1] += product >> 16;
    }
  }

  result[0] = (uint16_t)acc[0];
  result[1] = (uint16_t)acc[1];
  result[2] = (uint16_t)acc[2];
  result[3] = (uint16_t)acc[3];
}

/* 64-bit division by restoring shift-subtract: 64 iterations over a 128-bit
 * working register. Both outputs are optional.
 */
void math64_divide(const uint16_t *numerator, const uint16_t *denominator,
                   uint16_t *quotient, uint16_t *remainder)
{
  math64_work_t work;
  math64_half_t trial;
  math64_half_t difference;
  uint32_t carry;
  uint32_t i;
  int32_t count;

  assert_halt_at("c:\\halo\\SOURCE\\bungie_net\\common\\64bit_math.c", 0x7c,
                 numerator && denominator);

  /* Seed the low half with the dividend and clear the high half. */
  for (i = 0; i < 4; i++) {
    work.limb[i] = denominator[i];
    work.limb[i + 4] = 0;
  }

  count = 0x40;
  do {
    /* Shift the whole 128-bit register left by one. */
    carry = 0;
    for (i = 0; i < 8; i++) {
      carry = carry + work.limb[i] * 2;
      work.limb[i] = (uint16_t)carry;
      carry >>= 16;
    }

    trial = work.half[1];
    math64_subtract(trial.limb, numerator, difference.limb);

    /* Bit 63 clear = the difference is non-negative, i.e. the divisor fit:
     * keep it and set the quotient bit. See the sign-test bullet above for why
     * this is the mask form and not a signed comparison. */
    if ((difference.limb[3] & 0x8000) == 0) {
      work.limb[0]++;
      work.half[1] = difference;
    }
  } while (--count != 0);

  if (quotient) {
    *(math64_half_t *)quotient = work.half[0];
  }
  if (remainder) {
    *(math64_half_t *)remainder = work.half[1];
  }
}
