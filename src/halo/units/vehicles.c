/* FUN_001b5400 (0x1b5400)
 * Iterates objects with type mask 3. For each object whose +0xcc datum equals
 * vehicle_handle, copies and lowercases tag-block element (+0x2e4) name +4.
 * The current object's datum is iter.last_handle (+0x08), written by
 * object_iterator_next; do not replace it with the object pointer.
 */
uint16_t FUN_001b5400(int vehicle_handle, const char *name_filter)
{
  uint32_t exit_count;
  object_iter_t iter;
  void *unit_tag;
  void *object;
  void *tag_element;
  char name_buf[256];
  int filter_is_empty;

  exit_count = 0;
  if (vehicle_handle != -1) {
    unit_tag = tag_get(0x756e6974,
                       *(int *)object_get_and_verify_type(vehicle_handle, 3));
    filter_is_empty = name_filter == NULL || csstrlen(name_filter) == 0;
    object_iterator_new(&iter, 3, 0);
    object = object_iterator_next(&iter);
    while (object != NULL) {
      if (*(int *)((char *)object + 0xcc) == vehicle_handle) {
        tag_element = tag_block_get_element(
          (char *)unit_tag + 0x2e4, (int)*(int16_t *)((char *)object + 0x2a0),
          0x11c);
        csstrcpy(name_buf, (char *)tag_element + 4);
        csstr_tolower(name_buf);
        if ((filter_is_empty || crt_strstr(name_buf, name_filter) != NULL) &&
            unit_try_and_exit_seat(iter.last_handle) != 0) {
          exit_count++;
        }
      }
      object = object_iterator_next(&iter);
    }
  }

  return (uint16_t)exit_count;
}

/* FUN_001b5500 (0x1b5500) */
void FUN_001b5500(int param_1)
{
  char *unit;

  if (param_1 != -1) {
    unit = (char *)object_get_and_verify_type(param_1, 3);
    if (*(int *)(unit + 0xcc) != -1 && *(int16_t *)(unit + 0x2a0) != -1) {
      unit_try_and_exit_seat(param_1);
    }
  }
}

/* FUN_001b5580 (0x1b5580) */
void vehicle_causes_collision_damage(int param_1, void *param_2)
{
  unit_place(param_1, (char *)param_2 + 0x48);
  object_add_scenario_permutation(param_1, (char *)param_2 + 0x28);
}

/* vehicle_hover (0x1b55c0) — does this vehicle's definition mark it as a
 * hovering vehicle?
 *
 * Confirmed: MOV EAX,[EBP+0x8]; PUSH 0x2; PUSH EAX ->
 * object_get_and_verify_type(vehicle_handle, 2) (type mask 2 = vehicle).
 * Confirmed: MOV ECX,[EAX] -> obj->tag_index (uint32 at object offset 0).
 * Confirmed: PUSH ECX; PUSH 0x76656869 -> tag_get('vehi', tag_index).
 * Confirmed: MOV EAX,[EAX+0x2f0]; SHR EAX,0x7; AND EAX,0x1 -> bit 7 of the
 * 'vehi' tag flags dword at +0x2f0 (the same dword actor_combat.c tests with
 * & 0x100).
 * Confirmed: ADD ESP,0x10 -> both cdecl cleanups (2 calls x 2 args) coalesced.
 * Confirmed: plain RET (cdecl, caller cleans); result returned in EAX.
 * Inferred: name "vehicle_hover" from kb.json; the specific flag bit meaning
 * is unproven beyond "bit 7 of the vehi definition flags".
 */
bool vehicle_hover(int vehicle_handle)
{
  void *vehicle;
  void *vehicle_tag;

  vehicle = object_get_and_verify_type(vehicle_handle, 2);
  vehicle_tag = tag_get(0x76656869, *(uint32_t *)vehicle);
  return (bool)((*(uint32_t *)((char *)vehicle_tag + 0x2f0) >> 7) & 1);
}

/* FUN_001b5610 (0x1b5610) */
void FUN_001b5610(int vehicle_handle, uint8_t param_2)
{
  char *vehicle;

  if (vehicle_handle != -1) {
    vehicle = (char *)object_get_and_verify_type(vehicle_handle, 2);
    if (param_2 != 0) {
      object_get_world_position(vehicle_handle, (vector3_t *)(vehicle + 0x454));
      *(uint8_t *)(vehicle + 0x424) |= 2;
      return;
    }
    *(uint8_t *)(vehicle + 0x424) &= 0xfd;
  }
}

/* vehicle_is_flipped (0x1b5680) — reports whether a vehicle has rolled onto
 * its side or back, based on the float at object+0x38.
 *
 * Confirmed: MOV EAX,[EBP+0x8]; PUSH 0x2; PUSH EAX ->
 * object_get_and_verify_type(vehicle_handle, 2) (type mask 2 = vehicle, the
 * same mask vehicle_hover/vehicle_reset/FUN_001b56b0 use). kb.json's prior
 * "void vehicle_is_flipped(void)" decl was wrong: the function takes one
 * cdecl dword argument (the vehicle handle) and returns a bool in EAX.
 * Confirmed: FLD [EAX+0x38]; FCOMP [0x2549d4] (pooled constant, confirmed
 * 0.2f elsewhere -- see units.c:1768) -> compares the float at vehicle+0x38
 * against 0.2f.
 * Confirmed: FNSTSW AX; TEST AH,0x5; JP -> the parity trick for an x87
 * "less than" test. Per FCOM condition codes, ST0<src sets C0=1/C2=0 (AH&5 =
 * 1, odd parity, PF=0, JP NOT taken); ST0==src, ST0>src, and unordered all
 * set AH&5 to an even-parity value (PF=1, JP taken). So EAX=1 (JP not taken,
 * falls to MOV EAX,1) only when the field is strictly less than 0.2f; EAX=0
 * (JP taken, XOR EAX,EAX) for >=, ==, or NaN.
 * Inferred: +0x38 is the Z component of the vector3 at object+0x30 (types.h
 * unk_48, offset/meaning unproven); a small Z reading "flipped" when
 * < 0.2f is consistent with an up-vector upright test, but the vector's
 * identity is not proven here, so it is left as a raw offset rather than a
 * struct field.
 */
bool vehicle_is_flipped(int vehicle_handle)
{
  int result;
  void *vehicle;

  vehicle = object_get_and_verify_type(vehicle_handle, 2);
  result = *(float *)((char *)vehicle + 0x38) < *(float *)0x2549d4;
  return (bool)result;
}

/* FUN_001b56b0 (0x1b56b0) — per-tick contact bookkeeping for a vehicle,
 * driven by the caller's mass-point state array (passed in EDI).
 *
 * Confirmed: PUSH 0x2; PUSH EAX -> object_get_and_verify_type(handle@<eax>, 2)
 * (type mask 2 = vehicle, the same mask vehicle_hover uses).
 * Confirmed: MOV ECX,[ESI]; PUSH ECX; PUSH 0x76656869 ->
 * tag_get('vehi', obj->tag_index).
 * Confirmed: MOV EDX,[EAX+0x8c]; PUSH EDX; PUSH 0x70687973 ->
 * tag_get('phys', vehi_tag->+0x8c).
 * Confirmed: ADD ESP,0x18 -> the three cdecl cleanups (3 calls x 2 args)
 * coalesced into one 24-byte adjust; this is NOT a 6-argument call.
 * Confirmed: MOV CL,[ESI+0x428]; CMP CL,0xff; JNC -> unsigned saturating
 * increment of the byte counter at obj+0x428.
 * Confirmed: loop counter is 16-bit (INC EDX; MOVSX ECX,DX), element stride is
 * 0x130 (IMUL ECX,ECX,0x130) over the incoming EDI base, and the bound is
 * re-read from phys_tag+0x74 on every iteration (CMP ECX,[EAX+0x74]; JL).
 * Confirmed: TEST CL,0x2 / TEST CL,0x10 -> bits 1 and 4 of the element's first
 * dword; bit 1 clears obj+0x428, saturating-increments obj+0x42b and returns;
 * bit 4 only clears obj+0x428.
 * Confirmed: falling out of the loop stores 0 to obj+0x42b.
 * Unknown: the element type behind EDI (stride 0x130, only its first dword is
 * read here), which 'phys' tag_block owns the count at +0x74, and the meaning
 * of the two saturating byte counters at obj+0x428 / obj+0x42b.
 */
void FUN_001b56b0(int vehicle_handle, void *state_array)
{
  char *vehicle;
  void *vehicle_tag;
  char *physics_tag;
  uint32_t flags;
  int16_t i;

  vehicle = (char *)object_get_and_verify_type(vehicle_handle, 2);
  vehicle_tag = tag_get(0x76656869, *(int32_t *)vehicle);
  physics_tag =
    (char *)tag_get(0x70687973, *(int32_t *)((char *)vehicle_tag + 0x8c));

  if (*(uint8_t *)(vehicle + 0x428) < 0xff) {
    (*(uint8_t *)(vehicle + 0x428))++;
  }

  for (i = 0; i < *(int32_t *)(physics_tag + 0x74); i++) {
    flags = *(uint32_t *)((char *)state_array + i * 0x130);
    if ((flags & 2) != 0) {
      *(uint8_t *)(vehicle + 0x428) = 0;
      if (*(uint8_t *)(vehicle + 0x42b) < 0xff) {
        (*(uint8_t *)(vehicle + 0x42b))++;
      }
      return;
    }
    if ((flags & 0x10) != 0) {
      *(uint8_t *)(vehicle + 0x428) = 0;
    }
  }

  *(uint8_t *)(vehicle + 0x42b) = 0;
}

/*
 * set_real_quaternion (0x1b5750) - store four floats into a real_quaternion.
 *
 * Confirmed: cdecl, EBP frame, no calls. [EBP+0x8] is the destination
 * pointer; the four remaining dword slots are written to +0x0, +0x4, +0x8,
 * +0xc in argument order. Slot [EBP+0xc] is moved with FLD/FSTP, proving the
 * arguments are floats; MSVC bit-copies the other three with MOV because no
 * conversion is needed.
 * Inferred: the component names i/j/k/w follow the real_quaternion field
 * order; the artifact only proves the offsets, not the names. There are no
 * callers in this build, so the argument-to-component mapping is unverified
 * beyond the store offsets.
 */
void set_real_quaternion(float *out, float i, float j, float k, float w)
{
  out[0] = i;
  out[1] = j;
  out[2] = k;
  out[3] = w;
}

/*
 * vehicle_reset (0x1b5770) — clear the vehicle-specific state block that
 * lives at object +0x424 .. +0x47b.
 *
 * Confirmed: MOV EAX,[EBP+0x8]; PUSH 0x2; PUSH EAX -> the kb.json decl
 * "void vehicle_reset(void)" was wrong; the function takes one cdecl dword
 * argument, the vehicle handle, and calls
 * object_get_and_verify_type(vehicle_handle, 2) (mask 2 = vehicle, the same
 * mask vehicle_hover and FUN_001b56b0 use). ADD ESP,0x14 covers only the two
 * calls (2 + 3 dwords), so the parameter is caller-cleaned cdecl.
 * Confirmed: XOR EBX,EBX then every store below writes BX/BL/EBX, i.e. the
 * whole block is zeroed with integer zero.
 * Confirmed store widths from the disassembly: word at +0x424 and +0x426,
 * bytes at +0x428..+0x42b, dwords at +0x42c..+0x440, then +0x448 BEFORE
 * +0x444 (MOV [ESI+0x448] at 0x1b57d8 precedes MOV [ESI+0x444] at 0x1b57de);
 * that inverted pair is preserved here.
 * Confirmed: PUSH 0x8; LEA ECX,[ESI+0x44c]; PUSH EBX; PUSH ECX; CALL 0x8db80
 * -> csmemset(vehicle + 0x44c, 0, 8). Only 8 bytes, so +0x454..+0x45f are
 * deliberately left alone (FUN_001b5610 writes a world position at +0x454).
 * Confirmed: dwords +0x460..+0x478 zeroed after the csmemset call.
 * Unknown: the meaning of every field here except +0x428 / +0x42b, which
 * FUN_001b56b0 uses as saturating contact counters.
 */
void vehicle_reset(int vehicle_handle)
{
  char *vehicle;

  vehicle = (char *)object_get_and_verify_type(vehicle_handle, 2);

  *(uint16_t *)(vehicle + 0x424) = 0;
  *(uint16_t *)(vehicle + 0x426) = 0;
  *(uint8_t *)(vehicle + 0x428) = 0;
  *(uint8_t *)(vehicle + 0x429) = 0;
  *(uint8_t *)(vehicle + 0x42a) = 0;
  *(uint8_t *)(vehicle + 0x42b) = 0;
  *(uint32_t *)(vehicle + 0x42c) = 0;
  *(uint32_t *)(vehicle + 0x430) = 0;
  *(uint32_t *)(vehicle + 0x434) = 0;
  *(uint32_t *)(vehicle + 0x438) = 0;
  *(uint32_t *)(vehicle + 0x43c) = 0;
  *(uint32_t *)(vehicle + 0x440) = 0;
  *(uint32_t *)(vehicle + 0x448) = 0;
  *(uint32_t *)(vehicle + 0x444) = 0;

  csmemset(vehicle + 0x44c, 0, 8);

  *(uint32_t *)(vehicle + 0x460) = 0;
  *(uint32_t *)(vehicle + 0x464) = 0;
  *(uint32_t *)(vehicle + 0x468) = 0;
  *(uint32_t *)(vehicle + 0x46c) = 0;
  *(uint32_t *)(vehicle + 0x470) = 0;
  *(uint32_t *)(vehicle + 0x474) = 0;
  *(uint32_t *)(vehicle + 0x478) = 0;
}

/*
 * vehicle_new (0x1b5820) — initialize a freshly created vehicle object.
 *
 * Confirmed: MOV EBX,[EBP+0x8]; PUSH 0x2; PUSH EBX ->
 * object_get_and_verify_type(vehicle_handle, 2) (mask 2 = vehicle, the same
 * mask vehicle_reset / vehicle_hover use). kb.json's prior
 * "void vehicle_new(void)" was wrong; the binary reads one stack parameter.
 * Confirmed: MOV EAX,[ESI]; PUSH EAX; PUSH 0x76656869 ->
 * tag_get('vehi', obj->tag_index).
 * Confirmed: PUSH EBX; CALL 0x001b5770 -> vehicle_reset(vehicle_handle), one
 * stack dword. ADD ESP,0x14 (5 dwords) is the COALESCED cdecl cleanup for all
 * three calls (2 + 2 + 1); it is not a 5-argument call to vehicle_reset.
 * Confirmed: MOV ECX,[EDI+0x8c]; OR EAX,-1; CMP ECX,EAX; JNZ -> the 'vehi'
 * definition's physics tag index at +0x8c compared against -1 (no physics).
 * When absent, bit 0x20 of the object dword at +0x4 is set; when present it is
 * cleared. The compare is then RE-DONE at 0x1b5866 against the same [EDI+0x8c]
 * — two separate tests in the reference, kept as two ifs here.
 * Confirmed: FLD [EDI+0x4]; FMUL [0x253398]; FADD [ESI+0x14]; FSTP [ESI+0x14]
 * -> obj+0x14 += vehi_def+0x4 * 0.5f, in that x87 operand order. 0x253398 is
 * the shared 0.5f constant used across projectiles.c / scenario.c.
 * Confirmed: MOV AL,0x1 with EAX live as 0xffffffff from the earlier OR — a
 * byte-wide write over a live dword is the MSVC bool return ABI, so the
 * function returns true unconditionally.
 * Unknown: the meaning of object+0x14 (a float accumulated by half the vehi
 * definition's +0x4 field) and of flag bit 0x20 at object+0x4.
 */
bool vehicle_new(int vehicle_handle)
{
  char *vehicle;
  char *vehicle_tag;

  vehicle = (char *)object_get_and_verify_type(vehicle_handle, 2);
  vehicle_tag = (char *)tag_get(0x76656869, *(uint32_t *)vehicle);
  vehicle_reset(vehicle_handle);

  if (*(int32_t *)(vehicle_tag + 0x8c) == -1) {
    *(uint32_t *)(vehicle + 0x4) |= 0x20;
  } else {
    *(uint32_t *)(vehicle + 0x4) &= 0xffffffdf;
  }

  if (*(int32_t *)(vehicle_tag + 0x8c) != -1) {
    *(float *)(vehicle + 0x14) =
      *(float *)(vehicle_tag + 0x4) * *(float *)0x253398 +
      *(float *)(vehicle + 0x14);
  }

  return true;
}

/*
 * vehicle_preprocess_node_orientations (0x1b5890)
 *
 * Binary reads two stack params ([EBP+8] handle, [EBP+0xc] node data); the
 * kb decl was (void). Object resolved with type mask 2, then tag_get('vehi')
 * and, when vehi+0x44 != -1, tag_get('antr'). Element 0 of the antr block at
 * +0x24 (size 0x74) holds an int count at +0x5c and a short index array at
 * +0x60; each index selects an antr+0x74 animation (size 0xb4, int16 frame
 * count at +0x22). Index [4] is fetched and discarded. The trailing loop
 * walks the block at elem+0x68 (size 0x14, short at +2) with a signed short
 * counter and scales by the object byte array at +0x44c (0xff -> 1.0f).
 * Constants: 0x2533c0 = 0.0f, 0x2533c8 = 1.0f, 0x253398 = 0.5f; 0x261518 is
 * read from its address. Object/tag fields are unnamed (unproven meaning).
 */
void vehicle_preprocess_node_orientations(int vehicle_handle, void *node_data)
{
  char *object;
  char *vehi;
  char *antr;
  char *elem;
  char *anim;
  char *entry;
  short *indices;
  short i;
  unsigned char scale_byte;
  float t;
  float v;

  object = (char *)object_get_and_verify_type(vehicle_handle, 2);
  vehi = (char *)tag_get(0x76656869 /* 'vehi' */, *(int *)object);
  if (*(int *)(vehi + 0x44) == -1) {
    return;
  }
  antr = (char *)tag_get(0x616e7472 /* 'antr' */, *(int *)(vehi + 0x44));
  if (*(int *)(antr + 0x24) == 0) {
    return;
  }
  elem = (char *)tag_block_get_element(antr + 0x24, 0, 0x74);
  if (elem == (char *)0) {
    return;
  }

  if (*(int *)(elem + 0x5c) > 0) {
    indices = *(short **)(elem + 0x60);
    if (indices[0] != -1) {
      aiming_screen_apply(
        (int)tag_block_get_element(antr + 0x74, indices[0], 0xb4),
        (float *)elem, *(float *)(object + 0x434), 0.0f, (int)node_data);
    }
  }

  if (*(int *)(elem + 0x5c) > 1) {
    indices = *(short **)(elem + 0x60);
    if (indices[1] != -1) {
      anim = (char *)tag_block_get_element(antr + 0x74, indices[1], 0xb4);
      t = (triple_product3d((float *)(object + 0x30), (float *)(object + 0x24),
                            (float *)(object + 0x18)) /
             *(float *)(vehi + 0x2f8) +
           1.0f) *
          0.5f;
      if (t < 0.0f) {
        t = 0.0f;
      } else if (t > 1.0f) {
        t = 1.0f;
      }
      FUN_00122690(anim, (float)(*(short *)(anim + 0x22) - 1) * t, node_data);
    }
  }

  if (*(int *)(elem + 0x5c) > 2) {
    indices = *(short **)(elem + 0x60);
    if (indices[2] != -1) {
      anim = (char *)tag_block_get_element(antr + 0x74, indices[2], 0xb4);
      v = *(float *)(object + 0x42c);
      if (v < 0.0f) {
        t = 0.5f - v / *(float *)(vehi + 0x2fc) * 0.5f;
      } else {
        t = (v / *(float *)(vehi + 0x2f8) + 1.0f) * 0.5f;
      }
      FUN_00122690(anim, (float)(*(short *)(anim + 0x22) - 1) * t, node_data);
    }
  }

  if (*(int *)(elem + 0x5c) > 3) {
    indices = *(short **)(elem + 0x60);
    if (indices[3] != -1) {
      anim = (char *)tag_block_get_element(antr + 0x74, indices[3], 0xb4);
      t = *(float *)(object + 0x20) * *(float *)(object + 0x2c) +
          *(float *)(object + 0x1c) * *(float *)(object + 0x28) +
          *(float *)(object + 0x18) * *(float *)(object + 0x24);
      if (t < 0.0f) {
        t = 0.0f;
      } else if (t > 1.0f) {
        t = 1.0f;
      }
      t = t / (float)fabs(*(float *)(vehi + 0x2f8));
      if (t < 0.0f) {
        t = 0.0f;
      } else if (t > 1.0f) {
        t = 1.0f;
      }
      FUN_00122690(anim, (float)(*(short *)(anim + 0x22) - 1) * t, node_data);
    }
  }

  if (*(int *)(elem + 0x5c) > 4) {
    indices = *(short **)(elem + 0x60);
    if (indices[4] != -1) {
      tag_block_get_element(antr + 0x74, indices[4], 0xb4);
    }
  }

  if (*(int *)(elem + 0x5c) > 5) {
    indices = *(short **)(elem + 0x60);
    if (indices[5] != -1) {
      anim = (char *)tag_block_get_element(antr + 0x74, indices[5], 0xb4);
      if (*(float *)(vehi + 0x310) > 0.0f) {
        t = *(float *)(object + 0x438) / *(float *)(vehi + 0x310);
      } else {
        t = 0.0f;
      }
      FUN_00122690(anim, (float)(int)*(short *)(anim + 0x22) * t, node_data);
    }
  }

  for (i = 0; i < *(int *)(elem + 0x68); i++) {
    entry = (char *)tag_block_get_element(elem + 0x68, i, 0x14);
    if (*(short *)(entry + 2) != -1) {
      anim =
        (char *)tag_block_get_element(antr + 0x74, *(short *)(entry + 2), 0xb4);
      scale_byte = *(unsigned char *)(object + 0x44c + i);
      if (scale_byte != 0xff) {
        t = (float)scale_byte * *(float *)0x261518;
      } else {
        t = 1.0f;
      }
      FUN_00122690(anim, (float)(*(short *)(anim + 0x22) - 1) * t, node_data);
    }
  }
}

/*
 * vehicle_render_debug (0x1b5d90) — debug walk over a vehicle's physics
 * powered-mass-point block.
 *
 * Confirmed from disassembly at 0x1b5d90:
 *   MOV EAX,[EBP+8]; PUSH 0x2; PUSH EAX -> object_get_and_verify_type(handle,
 * 2) (the kb decl said (void); the binary reads one stack parameter). MOV
 * ECX,[EAX]; PUSH ECX; PUSH 0x76656869 -> tag_get('vehi', obj->tag_index). MOV
 * EAX,[EAX+0x8c]; CMP EAX,-1; JZ exit -> 'vehi' tag physics tag index. PUSH
 * EAX; PUSH 0x70687973 -> tag_get('phys', physics_index). MOV CL,[0x005054f4];
 * TEST CL,CL; JZ exit -> unnamed debug byte gate. MOV EAX,[EAX+0x74]; XOR
 * ECX,ECX; INC ECX; MOVSX EDX,CX; CMP EDX,EAX; JL
 *     -> signed short counter over the dword count at phys+0x74.
 * The loop body is empty in this build: nothing is emitted between INC ECX and
 * the compare, so whatever debug drawing it contained produced no code here.
 */
void vehicle_render_debug(int vehicle_handle)
{
  char *vehicle_tag;
  char *physics_tag;
  int physics_index;
  short i;

  vehicle_tag = (char *)tag_get(
    0x76656869, *(int *)object_get_and_verify_type(vehicle_handle, 2));
  physics_index = *(int *)(vehicle_tag + 0x8c);
  if (physics_index != -1) {
    physics_tag = (char *)tag_get(0x70687973, physics_index);
    if (*(char *)0x5054f4 != 0) {
      for (i = 0; (int)i < *(int *)(physics_tag + 0x74); i++) {
      }
    }
  }
}

/*
 * vehicle_get_estimated_position (0x1b5df0) — predict vehicle contact point.
 *
 * For vehicle types that support ground contact estimation (types 0, 1, 4, 6),
 * casts a ray downward from above the vehicle's current position to find the
 * BSP surface beneath it. The estimated position is:
 *   out[i] = dir_vec[i] + fwd_vec_doubled[i] * ray_t
 * where dir_vec = object_pos + up_vec * 0.4 (start above vehicle) and
 * fwd_vec_doubled = fwd_vec * 2 (ray direction/scale).
 *
 * For types 2, 3, 5 (and types >6): returns -1 immediately.
 * Returns result_buf[2] (EAX on success path, opaque hit info) or -1.
 * Callers only check != -1 to know whether the position was estimated.
 *
 * Confirmed: SUB ESP,0x434 — large stack frame.
 * Confirmed: PUSH 0x2, PUSH ESI -> object_get_and_verify_type(handle, 2).
 * Confirmed: MOV EAX,[EAX] -> obj->tag_index (uint32 at offset 0).
 * Confirmed: PUSH EAX, PUSH 0x76656869 -> tag_get('vehi', tag_index).
 * Confirmed: MOVSX EAX,word ptr [EDI+0x2f4] ->
 * (int16_t)vehicle_tag->type_field. Confirmed: CMP EAX,0x6; JA -> default
 * return -1 for types > 6. Confirmed: switch byte table at 0x1b5f18: types
 * 0,1,4,6 -> case body; 2,3,5 -> default. Confirmed: CALL 0x18e3f0
 * (global_collision_bsp_get) with no args. Confirmed: LEA EDX,[EBP-0xc]; PUSH
 * EDX; PUSH ESI -> object_get_world_position(handle, &adj_pos). Confirmed: MOV
 * EAX,[0x31fc44] -> up_vec_ptr = *(float**)0x31fc44 (global_up_vector_ptr).
 * Confirmed: FMUL float[0x253524] -> 0.4f (constant at 0x253524 = 0x3ECCCCCD).
 * Confirmed: adj_pos[i] = up_vec[i] * 0.4 + world_pos[i] (FSTP to
 * EBP-0xc,-0x8,-0x4). Confirmed: MOV EAX,[0x31fc50] -> fwd_vec_ptr =
 * *(float**)0x31fc50 (global_forward_vector_ptr). Confirmed: FADD ST0,ST0 ->
 * fwd_vec[i] * 2; FSTP to EBP-0x18,-0x14,-0x10. Confirmed: PUSH
 * EAX(result_buf), PUSH 0x7f7fffff(FLT_MAX), PUSH ECX(&fwd_doubled), PUSH
 * EDX(&adj_pos), PUSH 0, PUSH 0, PUSH EDI(bsp), PUSH 1 ->
 * collision_bsp_test_vector. Confirmed: TEST AL,AL; JZ -> if ray misses, fall
 * to default return -1. Confirmed: MOV EAX,[EBP-0x42c] -> result_buf[2] loaded
 * as return value. Confirmed: FMUL [EBP-0x434] -> multiply by result_buf[0]
 * (ray t param). Confirmed: out_pos[i] = fwd_doubled[i] * result_buf[0] +
 * adj_pos[i].
 */
int vehicle_get_estimated_position(int vehicle_handle, vector3_t *out_position)
{
  void *bsp;
  float adj_pos[3]; /* object_pos + up_vec * 0.4, at EBP-0xc */
  float fwd_doubled[3]; /* fwd_vec * 2, at EBP-0x18 */
  float result_buf[0x10d]; /* 0x434/4 floats; ray hit result buffer, at
                              EBP-0x434; only [0] and [2] used */
  int default_ret;
  int16_t vtype;

  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(vehicle_handle, 2);
  void *vehicle_tag = tag_get(0x76656869, *(uint32_t *)obj);
  default_ret = -1;

  /* First call: store current position into out_position (fills in initial
   * value). */
  object_get_world_position(vehicle_handle, out_position);

  /* Switch on vehicle type at tag+0x2f4. */
  vtype = *(int16_t *)((char *)vehicle_tag + 0x2f4);
  switch (vtype) {
  case 0:
  case 1:
  case 4:
  case 6:
    break;
  default:
    return default_ret;
  }

  /* Get BSP for ray cast. */
  bsp = global_collision_bsp_get();

  /* Get vehicle world position into adj_pos. */
  object_get_world_position(vehicle_handle, (vector3_t *)adj_pos);

  /* Compute adjusted start: pos + up_vec * 0.4 */
  {
    float *up = *(float **)0x31fc44;
    adj_pos[0] = up[0] * 0.4f + adj_pos[0];
    adj_pos[1] = up[1] * 0.4f + adj_pos[1];
    adj_pos[2] = up[2] * 0.4f + adj_pos[2];
  }

  /* Compute direction vector: fwd_vec * 2 */
  {
    float *fwd = *(float **)0x31fc50;
    fwd_doubled[0] = fwd[0] + fwd[0];
    fwd_doubled[1] = fwd[1] + fwd[1];
    fwd_doubled[2] = fwd[2] + fwd[2];
  }

  /* Cast ray. result_buf[0] = t, result_buf[2] = hit object (returned as EAX).
   */
  if (!((char (*)(int, void *, int16_t, int, float *, float *, float,
                  float *))0x149480)(1, bsp, 0, 0, adj_pos, fwd_doubled,
                                     3.4028235e+38f, result_buf)) {
    return default_ret;
  }

  /* Estimated position: adj_pos + fwd_doubled * t */
  out_position->x = fwd_doubled[0] * result_buf[0] + adj_pos[0];
  out_position->y = fwd_doubled[1] * result_buf[0] + adj_pos[1];
  out_position->z = fwd_doubled[2] * result_buf[0] + adj_pos[2];

  /* Return result_buf[2] as EAX (success indicator; caller checks != -1). */
  return *(int *)&result_buf[2];
}

/*
 * create_pelican_effect (0x1b6e20)
 *
 * Spawns the vehicle tag's thruster-wash effect ('vehi' tag +0x3ec) under
 * every "hover thrusters" and "jet thrusters" marker of the vehicle.
 *
 * Confirmed from disassembly at 0x1b6e20:
 *   MOV EBX,[EBP+0x8] -> one cdecl stack argument (the vehicle datum handle);
 *   kb.json declared it (void), corrected here. Both xrefs (0x1b8239 and
 *   0x1b855d, inside FUN_001b81d0) are unconditional calls.
 *   PUSH 0x2; PUSH EBX; CALL 0x13d680 -> object_get_and_verify_type(h, 2).
 *   MOV EAX,[EAX]; PUSH 0x76656869 -> tag_get('vehi', obj->tag_index).
 *   MOV EAX,[EDI+0x3ec]; CMP EAX,-0x1; JZ exit -> nothing to do when the
 *   effect tag reference is NONE.
 *   Marker buffer is at EBP-0x78c and is exactly 16 * 0x6c = 0x6c0 bytes; the
 *   first query is capped at 0xf and the second at 0x10 minus the first
 *   result, appended at (first_count * 0x6c).
 *   Per marker, ESI = &markers[i]; ESI+0x3c is the world-transform forward
 *   vector and ESI+0x60 the world-transform position (marker record =
 *   {int16 node; local matrix4x3 @0x04; world matrix4x3 @0x38}, and a
 *   matrix4x3 is {scale, forward[3], left[3], up[3], position[3]}).
 *   PUSH &dir; PUSH 0x3e860a92; PUSH 0x0; PUSH ESI+0x3c; CALL 0x10b120;
 *   PUSH EAX; CALL 0x10b4c0; ADD ESP,0x14 -> random_direction3d(seed,
 *   marker_forward, 0.0f, 0.2617994f, dir) with the seed fetched last
 *   (cdecl right-to-left), matching the call in C source order.
 *   CMP BX,word [EBP-0x1c] selects vehi+0x444 for hover markers and vehi+0x448
 *   for jet markers; FMUL [0x254640] (6.0f); FADD [0x253f40] (2.0f).
 *   PUSH ECX(&collision); PUSH EDX(handle); PUSH EAX(&velocity);
 *   PUSH ESI+0x60; PUSH 0x61; CALL 0x14df70; ADD ESP,0x14.
 *   On a hit, three effect markers are built: "incident" forward = -dir,
 *   "normal" forward = collision+0x24, "reflected" forward = 0x10c8e0(dir,
 *   collision+0x24); all three points are the hit position collision+0x18.
 *   FLD [0x2533c8] (1.0f); FSUB [collision+0x14] -> fade = 1 - hit fraction,
 *   passed as both scale arguments.
 *   The final call pushes 15 dwords (ADD ESP,0x3c) for the 12 declared
 *   parameters of effect_new_unattached_from_markers (3 of them are floats).
 * Inferred: name from the 2276 symbol dump; the marker-name strings are the
 *   literals at 0x2b7d18/0x2b7d08 and 0x28ab18/0x26b188/0x2b7cfc.
 * Unknown: the meaning of vehi+0x444 / vehi+0x448 (per-thruster-class speed
 *   scalars), collision flag set 0x61, and the trailing 0.0f/0.0f/1 effect
 *   arguments.
 */
void create_pelican_effect(int vehicle_handle)
{
  char markers[16 * 0x6c]; /* EBP-0x78c */
  int16_t collision_result[40]; /* EBP-0xcc, 80-byte raycast result */
  float marker_forwards[9]; /* EBP-0x7c: 3 marker forward vectors */
  float marker_points[9]; /* EBP-0x58: 3 marker positions */
  const char *marker_names[3]; /* EBP-0x34 */
  float velocity[3]; /* EBP-0x28 */
  float fade; /* EBP-0x14 */
  float direction[3]; /* EBP-0x10 */
  char *vehicle;
  char *vehicle_tag;
  char *marker;
  int16_t hover_count;
  int16_t jet_count;
  int marker_count;
  int16_t i;
  float speed;

  vehicle = (char *)object_get_and_verify_type(vehicle_handle, 2);
  vehicle_tag = (char *)tag_get(0x76656869, *(int *)vehicle);
  if (*(int *)(vehicle_tag + 0x3ec) == -1) {
    return;
  }

  hover_count = object_get_markers_by_string_id(
    vehicle_handle, (void *)"hover thrusters", markers, 0xf);
  jet_count = object_get_markers_by_string_id(
    vehicle_handle, (void *)"jet thrusters", markers + (int)hover_count * 0x6c,
    0x10 - (int)hover_count);
  marker_count = (int)hover_count + (int)jet_count;

  i = 0;
  if (marker_count <= 0) {
    return;
  }

  do {
    marker = markers + (int)i * 0x6c;
    seed_random_vector_in_cone3d((int *)random_math_get_local_seed_address(),
                                 (float *)(marker + 0x3c), 0.0f, 0.2617994f,
                                 direction);

    if (i < hover_count) {
      speed = *(float *)(vehicle + 0x444);
    } else {
      speed = *(float *)(vehicle + 0x448);
    }
    speed = speed * *(float *)0x254640 + *(float *)0x253f40;
    velocity[0] = direction[0] * speed;
    velocity[1] = direction[1] * speed;
    velocity[2] = direction[2] * speed;

    if (FUN_0014df70(0x61, (float *)(marker + 0x60), velocity, vehicle_handle,
                     collision_result)) {
      marker_forwards[0] = -direction[0];
      marker_forwards[1] = -direction[1];
      marker_forwards[2] = -direction[2];
      marker_forwards[3] = *(float *)((char *)collision_result + 0x24);
      marker_forwards[4] = *(float *)((char *)collision_result + 0x28);
      marker_forwards[5] = *(float *)((char *)collision_result + 0x2c);

      marker_points[0] = *(float *)((char *)collision_result + 0x18);
      marker_points[1] = *(float *)((char *)collision_result + 0x1c);
      marker_points[2] = *(float *)((char *)collision_result + 0x20);
      marker_points[3] = marker_points[0];
      marker_points[4] = marker_points[1];
      marker_points[5] = marker_points[2];
      marker_points[6] = marker_points[0];
      marker_points[7] = marker_points[1];
      marker_points[8] = marker_points[2];

      marker_names[0] = "incident";
      marker_names[1] = "normal";
      marker_names[2] = "reflected";

      FUN_0010c8e0(direction, (float *)((char *)collision_result + 0x24),
                   &marker_forwards[6]);

      fade = *(float *)0x2533c8 - *(float *)((char *)collision_result + 0x14);
      effect_new_unattached_from_markers(
        *(int *)(vehicle_tag + 0x3ec), -1, (float *)0, 3, marker_names,
        marker_points, marker_forwards, fade, fade, 0.0f, 0.0f, 1);
    }
    i = i + 1;
  } while ((int)i < marker_count);
}

/*
 * vehicle_moving_near_any_player (0x1b7ee0)
 *
 * Scans all local players. For each local player whose unit is on foot (not
 * currently inside a vehicle, checked via object_data+0xcc == NONE), collects
 * up to MAXIMUM_NUMBER_OF_LOCAL_PLAYERS unit handles and their bounding-sphere
 * centres. Then iterates every vehicle object in the world. For each vehicle,
 * checks each on-foot player:
 *   1. The player's unit is not already sitting inside this vehicle
 *      (object_data[unit]+0xcc != vehicle_datum_handle).
 *   2. Squared distance between vehicle position (+0x50) and the cached
 *      player bounding-sphere centre is < 100.0 (within 10 world units).
 *   3. Squared velocity magnitude (+0x18) is >= ~1/900 (speed >= ~1/30 u/tick).
 * Returns true if any qualifying vehicle is found.
 *
 * Called from game_safe_to_save (0xa7530) to block saving while a moving
 * vehicle is close to a player on foot.
 */

#include "../../common.h"
#include "../../x87_math.h"

/* Squared proximity threshold: 10.0^2 = 100.0 world units. */
#define VEHICLE_NEAR_PLAYER_DIST_SQ 100.0f
/* Squared velocity threshold: (1/30)^2 = 1/900. Vehicle must exceed this to
 * be considered "moving". Value from binary at 0x25620c. */
#define VEHICLE_MIN_SPEED_SQ 0.001111111138f

bool vehicle_moving_near_any_player(void)
{
  /* Object iterator buffer: 0x10-byte struct identical in layout to
   * data_iter_t. object_iterator_next writes the current datum handle at
   * byte offset 0x08 (iter_buf[2] as an int array). */
  int iter_buf[4];
  int unit_handles[4]; /* handles of on-foot player units */
  float player_pos[12]; /* 3-float bounding-sphere centre per player */
  float radius_scratch; /* radius out-param, not used here */
  int16_t lpi; /* current local_player_index */
  int16_t n; /* count of on-foot player units collected */
  int16_t i;
  int player_handle;
  char *player;
  int unit_handle;
  void *unit_obj;
  void *veh_obj;
  int vehicle_datum_handle;
  float dx, dy, dz;
  float vx, vy, vz;
  char found; /* 1 = no vehicle found yet, 0 = found; matches local_5 */

  n = 0;
  found = 1;

  /* Phase 1: collect on-foot local player units and their positions.
   * local_player_get_next(-1) returns the first valid local_player_index. */
  lpi = ((int16_t(*)(int16_t))0xba4c0)((int16_t)-1);
  if (lpi == (int16_t)-1)
    goto done;

  /* Push ESI before inner loop (matches disasm 001b7f07: PUSH ESI). */
  do {
    /* First call: validate player index. */
    player_handle = ((int (*)(int16_t))0xba3c0)(lpi);
    if (player_handle != -1) {
      /* Second call: get handle for datum_get (matches disasm 001b7f16). */
      player_handle = ((int (*)(int16_t))0xba3c0)(lpi);
      player = (char *)((void *(*)(void *, int))0x119320)(*(void **)0x5aa6d4,
                                                          player_handle);
      unit_handle = *(int *)(player + 0x34);
      if (unit_handle != -1) {
        /* object_get_and_verify_type(handle, 3): accepts biped or vehicle. */
        unit_obj = ((void *(*)(int, int))0x13d680)(unit_handle, 3);
        /* +0xcc = parent_object_index; NONE (-1) means unit is on foot. */
        if (*(int *)((char *)unit_obj + 0xcc) == -1) {
          unit_handles[n] = unit_handle;
          /* object_get_bounding_sphere: writes centre to &player_pos[n*3],
           * radius to &radius_scratch. Centre is at object_data+0x50. */
          ((void (*)(int, float *, float *))0x1aae0)(
            unit_handle, &player_pos[n * 3], &radius_scratch);
          n++;
        }
      }
    }
    lpi = ((int16_t(*)(int16_t))0xba4c0)(lpi);
  } while (lpi != (int16_t)-1);

  if (n == 0)
    goto done;

  /* Phase 2: iterate all vehicle objects (type_mask=2 = bit 1 = vehicle). */
  object_iterator_new(iter_buf, 2, 0);

  while ((veh_obj = object_iterator_next(iter_buf)) != NULL) {
    /* iter_buf[2] holds the datum handle of the current vehicle object,
     * written by object_iterator_next at offset 0x08 in the iter buffer. */
    vehicle_datum_handle = iter_buf[2];

    i = 0;
    if (n <= 0)
      continue;

    do {
      unit_obj = ((void *(*)(int, int))0x13d680)(unit_handles[i], 3);
      /* Skip player whose unit is already inside this vehicle. */
      if (*(int *)((char *)unit_obj + 0xcc) == vehicle_datum_handle) {
        i++;
        continue;
      }

      /* Squared distance: vehicle pos (+0x50) vs player bounding centre. */
      dx = *(float *)((char *)veh_obj + 0x50) - player_pos[i * 3];
      dy = *(float *)((char *)veh_obj + 0x54) - player_pos[i * 3 + 1];
      dz = *(float *)((char *)veh_obj + 0x58) - player_pos[i * 3 + 2];
      if (dx * dx + dy * dy + dz * dz >= VEHICLE_NEAR_PLAYER_DIST_SQ) {
        i++;
        continue;
      }

      /* Squared velocity: vehicle velocity (+0x18) must exceed threshold. */
      vx = *(float *)((char *)veh_obj + 0x18);
      vy = *(float *)((char *)veh_obj + 0x1c);
      vz = *(float *)((char *)veh_obj + 0x20);
      if (vx * vx + vy * vy + vz * vz >= VEHICLE_MIN_SPEED_SQ) {
        found = 0;
        goto done;
      }
      i++;
    } while (i < n);
  }

done:
  return found == 0;
}

/*
 * update_alien_fighter_physics (0x1b8f10) — select which alien-fighter
 * (Banshee) physics update to run for this vehicle, then run the ghost
 * effect update.
 *
 * Confirmed from disassembly at 0x1b8f10:
 *   PUSH EDI (callee save, POP EDI at both exits); PUSH 0x2; PUSH ESI;
 *   MOV EDI,EAX -> object_get_and_verify_type(vehicle_handle@<esi>, 2), with
 *   the incoming EAX stashed in EDI. So this function takes three register
 *   arguments: ESI, EAX and EBX (EBX is PUSHed at 0x1b8f44 as a call argument
 *   and only ever reclaimed by ADD ESP — never POPped — so it is an incoming
 *   argument, not a save).
 *   MOV EAX,[EAX]; PUSH EAX; PUSH 0x76656869 -> tag_get('vehi',
 * obj->tag_index). MOV ECX,[EAX+0x8c]; PUSH ECX; PUSH 0x70687973 ->
 *     tag_get('phys', vehi_tag->physics_tag_index at +0x8c).
 *   ADD ESP,0x18 -> all three cdecl cleanups (3 calls x 2 args) coalesced.
 *   FLD [EAX]; FCOMP [0x002533c0]; FNSTSW AX; TEST AH,0x41; JNZ 0x1b8f60.
 *     TEST AH,0x41 masks C0|C3, so the jump is taken when phys[0] <= 0.0f and
 *     the FALL-THROUGH is the phys[0] > 0.0f case. 0x2533c0 is the shared 0.0f
 *     constant. Fall-through runs 0x1b69a0 (the "_old" variant).
 *   Fall-through: PUSH ESI; CALL 0x1b69a0; ADD ESP,0x8 -> two stack args, the
 *     EBX pushed at 0x1b8f44 plus ESI: update_alien_fighter_physics_old(esi,
 * ebx). Taken: PUSH EDI; PUSH ESI; CALL 0x1b6560; ADD ESP,0xc -> three stack
 * args: update_alien_fighter_physics_new(esi, edi(=incoming eax), ebx). Both
 * paths end PUSH ESI; CALL 0x1b7020; ADD ESP,0x4 -> create_ghost_effect(esi).
 * Inferred: names from kb.json symbol dump.
 * Unknown: the meaning of the EAX and EBX arguments (they are only forwarded,
 *   never inspected here), and which 'phys' field lives at offset 0.
 */
void update_alien_fighter_physics(int vehicle_handle, int param_2, int param_3)
{
  void *vehicle;
  char *vehicle_tag;
  float *physics_tag;

  vehicle = object_get_and_verify_type(vehicle_handle, 2);
  vehicle_tag = (char *)tag_get(0x76656869, *(uint32_t *)vehicle);
  physics_tag = (float *)tag_get(0x70687973, *(int32_t *)(vehicle_tag + 0x8c));

  if (*physics_tag > *(float *)0x2533c0) {
    update_alien_fighter_physics_old(vehicle_handle, param_3);
    create_ghost_effect(vehicle_handle);
    return;
  }

  update_alien_fighter_physics_new(vehicle_handle, param_2, param_3);
}

/* 0x1b5c90: vehicle_accelerate
 * Applies a linear acceleration vector to the vehicle's velocity,
 * computes an angular torque impulse about the up axis crossed with
 * acceleration, and wakes the vehicle up if sleeping (clears flag bit 0x20).
 */
void vehicle_accelerate(int vehicle_handle, float *velocity)
{
  char *vehicle;
  uint32_t tag_index;
  char *vehicle_tag;
  int32_t physics_tag_index;
  float cross[3];
  float *up;
  float len;

  vehicle = (char *)object_get_and_verify_type(vehicle_handle, 2);
  tag_index = *(uint32_t *)vehicle;
  vehicle_tag = (char *)tag_get(0x76656869, tag_index);
  physics_tag_index = *(int32_t *)(vehicle_tag + 0x8c);
  if (physics_tag_index != -1) {
    tag_get(0x70687973, physics_tag_index);

    *(float *)(vehicle + 0x18) += velocity[0];
    *(float *)(vehicle + 0x1c) += velocity[1];
    *(float *)(vehicle + 0x20) += velocity[2];

    up = *(float **)0x31fc44;
    cross[0] = velocity[2] * up[1] - velocity[1] * up[2];
    cross[1] = up[2] * velocity[0] - velocity[2] * up[0];
    cross[2] = velocity[1] * up[0] - velocity[0] * up[1];

    len = normalize3d(cross);
    if (len > *(float *)0x2533c0) {
      len *= *(float *)0x256980;
      *(float *)(vehicle + 0x3c) += cross[0] * len;
      *(float *)(vehicle + 0x40) += cross[1] * len;
      *(float *)(vehicle + 0x44) += cross[2] * len;
      *(uint32_t *)(vehicle + 4) &= ~0x20;
      return;
    }
    *(uint32_t *)(vehicle + 4) &= ~0x20;
  }
}

/* 0x1b5f20: compute_acceleration
 * Computes acceleration vector required to reach desired_vel from current_vel,
 * adjusting for global gravity, and limits the resulting acceleration vector
 * according to the angle between desired_vel and acceleration.
 */
float *compute_acceleration(float *out_accel, const float *current_vel, const float *desired_vel, float max_accel, float min_accel)
{
  float dot;
  float delta_len_sq;
  float desired_len_sq;
  float cos_sq;
  float accel_limit;

  out_accel[0] = desired_vel[0] - current_vel[0];
  out_accel[1] = desired_vel[1] - current_vel[1];
  out_accel[2] = (desired_vel[2] - current_vel[2]) + *(float *)0x32512c;

  dot = desired_vel[0] * out_accel[0] + desired_vel[2] * out_accel[2] + desired_vel[1] * out_accel[1];
  if (dot > *(float *)0x253f44) {
    delta_len_sq = out_accel[0] * out_accel[0] + out_accel[1] * out_accel[1] + out_accel[2] * out_accel[2];
    desired_len_sq = desired_vel[0] * desired_vel[0] + desired_vel[1] * desired_vel[1] + desired_vel[2] * desired_vel[2];
    cos_sq = (dot * dot) / (delta_len_sq * desired_len_sq);
    accel_limit = (max_accel - min_accel) * cos_sq + min_accel;
    FUN_000a57b0(out_accel, accel_limit);
    return out_accel;
  }

  FUN_000a57b0(out_accel, min_accel);
  return out_accel;
}

/* 0x1b6ca0: slowly_stop_vehicle
 * Decrements the stopping countdown timer (+0x426) and dampens linear/angular
 * velocities by 0.835 (*(float *)0x2b7cf8). Rotates forward/up vectors by angular velocity,
 * integrates position, and zeros velocities when the timer reaches 0.
 */
void slowly_stop_vehicle(int vehicle_handle)
{
  char *vehicle;
  float damp;
  float new_pos[3];
  float ang_vel[3];
  float new_forward[3];
  float new_up[3];
  float angle;
  float rot_mat[12];

  vehicle = (char *)object_get_and_verify_type(vehicle_handle, 2);
  *(int16_t *)(vehicle + 0x426) -= 1;

  damp = *(const float *)0x2b7cf8;
  *(float *)(vehicle + 0x18) *= damp;
  *(float *)(vehicle + 0x1c) *= damp;
  *(float *)(vehicle + 0x20) *= damp;
  *(float *)(vehicle + 0x3c) *= damp;
  *(float *)(vehicle + 0x40) *= damp;
  *(float *)(vehicle + 0x44) *= damp;

  new_pos[0] = *(float *)(vehicle + 0x18) + *(float *)(vehicle + 0xc);
  new_pos[1] = *(float *)(vehicle + 0x10) + *(float *)(vehicle + 0x1c);
  new_pos[2] = *(float *)(vehicle + 0x14) + *(float *)(vehicle + 0x20);

  ang_vel[0] = *(float *)(vehicle + 0x3c);
  ang_vel[1] = *(float *)(vehicle + 0x40);
  ang_vel[2] = *(float *)(vehicle + 0x44);

  angle = normalize3d(ang_vel);
  if (angle != *(float *)0x2533c0) {
    float sin_val = x87_fsin(angle);
    float cos_val = x87_fcos(angle);
    FUN_001092d0(rot_mat, ang_vel, sin_val, cos_val);
    matrix_scale_transform_vector(rot_mat, (float *)(vehicle + 0x24), new_forward);
    matrix_scale_transform_vector(rot_mat, (float *)(vehicle + 0x30), new_up);
  } else {
    new_forward[0] = *(float *)(vehicle + 0x24);
    new_forward[1] = *(float *)(vehicle + 0x28);
    new_forward[2] = *(float *)(vehicle + 0x2c);
    new_up[0] = *(float *)(vehicle + 0x30);
    new_up[1] = *(float *)(vehicle + 0x34);
    new_up[2] = *(float *)(vehicle + 0x38);
  }

  if (*(int16_t *)(vehicle + 0x426) == 0) {
    float *zero = *(float **)0x31fc38;
    *(float *)(vehicle + 0x18) = zero[0];
    *(float *)(vehicle + 0x1c) = zero[1];
    *(float *)(vehicle + 0x20) = zero[2];
    *(float *)(vehicle + 0x3c) = zero[0];
    *(float *)(vehicle + 0x40) = zero[1];
    *(float *)(vehicle + 0x44) = zero[2];
  }

  object_set_position(vehicle_handle, new_pos, new_forward, new_up);
}

/* 0x1b8060: vehicle_stuck
 * Checks whether any wheel/suspension contact point is flagged in the
 * vehicle's stuck contact bitmask (+0x478). If so, computes the centroid of
 * flagged contact points transformed to world space, outputs the normalized
 * direction from vehicle position to centroid, and returns 1. Otherwise returns 0.
 */
char vehicle_stuck(int vehicle_handle, float *vec)
{
  char *vehicle;
  uint32_t stuck_mask;
  char instance_buf[0x64];
  void *physics_tag;
  void *points_block;
  int count;
  int match_count;
  int i;
  float *origin;
  float sum[3];
  float world_center[3];
  vector3_t vehicle_pos;
  float mag;

  vehicle = (char *)object_get_and_verify_type(vehicle_handle, 2);
  stuck_mask = *(uint32_t *)(vehicle + 0x478);
  if (stuck_mask == 0) {
    return 0;
  }

  if (!FUN_001509c0(instance_buf, vehicle_handle)) {
    return 0;
  }

  origin = *(float **)0x31fc1c;
  sum[0] = origin[0];
  sum[1] = origin[1];
  sum[2] = origin[2];

  physics_tag = *(void **)(instance_buf + 4);
  points_block = (char *)physics_tag + 0x74;
  count = *(int *)points_block;

  match_count = 0;
  for (i = 0; i < count; i++) {
    if (stuck_mask & (1U << (i & 0x1f))) {
      char *elem = (char *)tag_block_get_element(points_block, i, 0x80);
      sum[0] += *(float *)(elem + 0x38);
      sum[1] += *(float *)(elem + 0x3c);
      sum[2] += *(float *)(elem + 0x40);
      match_count++;
    }
  }

  if (match_count <= 0) {
    return 0;
  }

  {
    float inv_count = *(float *)0x2533c8 / (float)match_count;
    sum[0] *= inv_count;
    sum[1] *= inv_count;
    sum[2] *= inv_count;
  }

  matrix_transform_point((float *)(instance_buf + 8), sum, world_center);
  object_get_world_position(vehicle_handle, &vehicle_pos);

  vec[0] = world_center[0] - vehicle_pos.x;
  vec[1] = world_center[1] - vehicle_pos.y;
  vec[2] = world_center[2] - vehicle_pos.z;

  mag = normalize3d(vec);
  if (mag != *(float *)0x2533c0) {
    return 1;
  }

  return 0;
}

/* 0x1b5ff0: update_human_tank_physics
 * Computes tank tread velocities from forward throttle and steering inputs,
 * updates cyclical tread texture scroll parameters (+0x43c and +0x440),
 * and feeds powered mass points (in EDI) to physics_update.
 */
void update_human_tank_physics(int vehicle_handle, void *mass_points)
{
  char *vehicle;
  char *vehicle_tag;
  char *physics_tag;
  float throttle;
  float steering;
  float diff;
  float sum;
  float period;
  float scroll_left;
  float scroll_right;
  int32_t physics_model_type;
  void *powered_mass_points;

#ifdef _MSC_VER
  __asm { mov powered_mass_points, edi }
#else
  __asm__ __volatile__("movl %%edi, %0" : "=r"(powered_mass_points));
#endif

  vehicle = (char *)object_get_and_verify_type(vehicle_handle, 2);
  vehicle_tag = (char *)tag_get(0x76656869, *(uint32_t *)vehicle);
  physics_tag = (char *)tag_get(0x70687973, *(uint32_t *)(vehicle_tag + 0x8c));

  throttle = *(float *)(vehicle + 0x42c);
  steering = *(float *)(vehicle + 0x434);
  diff = throttle - steering;
  sum = steering + throttle;

  period = *(float *)(vehicle_tag + 0x310);

  scroll_left = diff + *(float *)(vehicle + 0x43c);
  scroll_left = x87_fmod(scroll_left, period);
  if (scroll_left < *(float *)0x2533c0) {
    scroll_left += period;
  }
  *(float *)(vehicle + 0x43c) = scroll_left;

  scroll_right = sum + *(float *)(vehicle + 0x440);
  scroll_right = x87_fmod(scroll_right, period);
  if (scroll_right < *(float *)0x2533c0) {
    scroll_right += period;
  }
  *(float *)(vehicle + 0x440) = scroll_right;

  physics_model_type = *(int32_t *)(physics_tag + 0x68);
  if (physics_model_type == 2) {
    char *pmp = (char *)powered_mass_points;
    *(float *)(pmp + 0x00) = diff;
    *(uint32_t *)(pmp + 0x1c) = 0;
    *(uint32_t *)(pmp + 0x20) = 0;
    *(uint32_t *)(pmp + 0x24) = 0;
    *(float *)(pmp + 0x28) = 1.0f;

    *(float *)(pmp + 0x60) = sum;
    *(uint32_t *)(pmp + 0x7c) = 0;
    *(uint32_t *)(pmp + 0x80) = 0;
    *(uint32_t *)(pmp + 0x84) = 0;
    *(float *)(pmp + 0x88) = 1.0f;

    physics_update(vehicle_handle, powered_mass_points, mass_points, NULL, NULL);
  } else {
    physics_update(vehicle_handle, NULL, mass_points, NULL, NULL);
  }
}

/* 0x1b6140: update_human_jeep_physics
 * Computes Warthog front wheel steering angles from steering yaw,
 * updates wheel scroll parameters (+0x438), and feeds powered mass
 * points (in EDI) with steering orientation vectors to physics_update.
 */
void update_human_jeep_physics(int vehicle_handle, void *mass_points)
{
  char *vehicle;
  char *vehicle_tag;
  char *physics_tag;
  float throttle;
  float steering;
  float half_steer;
  float period;
  float scroll;
  int32_t physics_model_type;
  void *powered_mass_points;

#ifdef _MSC_VER
  __asm { mov powered_mass_points, edi }
#else
  __asm__ __volatile__("movl %%edi, %0" : "=r"(powered_mass_points));
#endif

  vehicle = (char *)object_get_and_verify_type(vehicle_handle, 2);
  vehicle_tag = (char *)tag_get(0x76656869, *(uint32_t *)vehicle);
  physics_tag = (char *)tag_get(0x70687973, *(uint32_t *)(vehicle_tag + 0x8c));

  throttle = *(float *)(vehicle + 0x42c);
  period = *(float *)(vehicle_tag + 0x310);

  scroll = throttle + *(float *)(vehicle + 0x438);
  scroll = x87_fmod(scroll, period);
  if (scroll < *(float *)0x2533c0) {
    scroll += period;
  }
  *(float *)(vehicle + 0x438) = scroll;

  physics_model_type = *(int32_t *)(physics_tag + 0x68);
  if (physics_model_type == 2) {
    char *pmp;
    float sin_steer;
    float cos_steer;

    steering = *(float *)(vehicle + 0x434);
    half_steer = steering * *(float *)0x253398;
    cos_steer = x87_fcos(half_steer);
    sin_steer = x87_fsin(half_steer);

    pmp = (char *)powered_mass_points;
    *(float *)(pmp + 0x00) = throttle;
    *(uint32_t *)(pmp + 0x1c) = 0;
    *(uint32_t *)(pmp + 0x20) = 0;
    *(float *)(pmp + 0x24) = sin_steer;
    *(float *)(pmp + 0x28) = cos_steer;

    *(float *)(pmp + 0x60) = throttle;
    *(uint32_t *)(pmp + 0x7c) = 0;
    *(uint32_t *)(pmp + 0x80) = 0;
    *(float *)(pmp + 0x84) = -sin_steer;
    *(float *)(pmp + 0x88) = cos_steer;

    physics_update(vehicle_handle, powered_mass_points, mass_points, NULL, NULL);
  } else {
    physics_update(vehicle_handle, NULL, mass_points, NULL, NULL);
  }
}

/* 0x1b7020: create_ghost_effect
 * Raycasts ground probes from hover thruster markers (tag string "hover thrusters", max 15 markers)
 * and spawns ground-contact dust/thruster particle effects via effect_new_unattached_from_markers.
 */
void create_ghost_effect(int vehicle_handle)
{
  char *vehicle;
  char *vehicle_tag;
  int effect_tag_index;
  float hover_vitality;
  int16_t marker_count;
  int i;
  char markers[15 * 0x6c];
  float dir[3];
  char collision_result[0x34];
  const char *marker_names[4];
  float marker_points[12];
  float marker_forwards[12];

  vehicle = (char *)object_get_and_verify_type(vehicle_handle, 2);
  vehicle_tag = (char *)tag_get(0x76656869, *(uint32_t *)vehicle);

  effect_tag_index = *(int32_t *)(vehicle_tag + 0x3ec);
  if (effect_tag_index == -1) {
    return;
  }

  hover_vitality = *(float *)(vehicle + 0x2e8);
  if (hover_vitality <= *(float *)0x2533c0) {
    return;
  }

  marker_count = object_get_markers_by_string_id(vehicle_handle, (void *)0x2b7d18, markers, 15);
  if (marker_count <= 0) {
    return;
  }

  marker_names[0] = (const char *)0x28ab18; /* "incident" */
  marker_names[1] = (const char *)0x26b188; /* "normal" */
  marker_names[2] = (const char *)0x2b7cfc; /* "reflected" */
  marker_names[3] = (const char *)0x2b7d28; /* "midpoint" */

  for (i = 0; i < (int)marker_count; i++) {
    char *marker = markers + i * 0x6c;
    float *marker_forward = (float *)(marker + 0x3c);
    float *marker_pos = (float *)(marker + 0x60);
    seed_random_vector_in_cone3d((int *)random_math_get_local_seed_address(), marker_forward, 0.0f, 15.0f, dir);

    if (FUN_0014df70(0x61, marker_pos, dir, vehicle_handle, (int16_t *)collision_result)) {
      float t = *(float *)(collision_result + 0x14);
      float normal_z = *(float *)(marker + 0x44);
      float scale = (*(float *)0x2533c8 - t) * (-normal_z) * hover_vitality;

      if (scale > *(float *)0x2533c0) {
        float hit_pos[3];
        float midpoint[3];
        float normal[3];
        float reflected[3];
        float neg_dir[3];
        float scale_b;

        if (scale > *(float *)0x2533c8) {
          scale = *(float *)0x2533c8;
        }
        scale_b = scale;

        hit_pos[0] = *(float *)(collision_result + 0x18);
        hit_pos[1] = *(float *)(collision_result + 0x1c);
        hit_pos[2] = *(float *)(collision_result + 0x20);

        normal[0] = *(float *)(collision_result + 0x24);
        normal[1] = *(float *)(collision_result + 0x28);
        normal[2] = *(float *)(collision_result + 0x2c);

        midpoint[0] = (hit_pos[0] + marker_pos[0]) * *(float *)0x253398;
        midpoint[1] = (hit_pos[1] + marker_pos[1]) * *(float *)0x253398;
        midpoint[2] = (hit_pos[2] + marker_pos[2]) * *(float *)0x253398;

        neg_dir[0] = -dir[0];
        neg_dir[1] = -dir[1];
        neg_dir[2] = -dir[2];

        FUN_0010c8e0(dir, normal, reflected);

        marker_points[0] = hit_pos[0];
        marker_points[1] = hit_pos[1];
        marker_points[2] = hit_pos[2];

        marker_points[3] = hit_pos[0];
        marker_points[4] = hit_pos[1];
        marker_points[5] = hit_pos[2];

        marker_points[6] = hit_pos[0];
        marker_points[7] = hit_pos[1];
        marker_points[8] = hit_pos[2];

        marker_points[9] = midpoint[0];
        marker_points[10] = midpoint[1];
        marker_points[11] = midpoint[2];

        marker_forwards[0] = neg_dir[0];
        marker_forwards[1] = neg_dir[1];
        marker_forwards[2] = neg_dir[2];

        marker_forwards[3] = normal[0];
        marker_forwards[4] = normal[1];
        marker_forwards[5] = normal[2];

        marker_forwards[6] = reflected[0];
        marker_forwards[7] = reflected[1];
        marker_forwards[8] = reflected[2];

        marker_forwards[9] = reflected[0];
        marker_forwards[10] = reflected[1];
        marker_forwards[11] = reflected[2];

        effect_new_unattached_from_markers(
          effect_tag_index,
          0xffffffff,
          0,
          4,
          (void *)marker_names,
          marker_points,
          marker_forwards,
          scale,
          scale_b,
          0.0f,
          0.0f,
          1
        );
      }
    }
  }
}

/* 0x1b72b0: create_crashing_effects
 * Computes vehicle crash impact damage and plays collision impact sounds
 * if velocity change exceeds threshold and mass points made ground collision.
 */
void create_crashing_effects(int vehicle_handle, const float *prev_velocity, void *mass_points)
{
  char *vehicle;
  char *vehicle_tag;
  char *physics_tag;
  char *game_globals;
  char *havok_cleanup;
  int crash_damage_tag_index;
  int crash_sound_tag_index;
  float delta_vel_x;
  float delta_vel_y;
  float delta_vel_z;
  float speed_change;
  int mass_point_count;
  int i;
  int collided;

  vehicle = (char *)object_get_and_verify_type(vehicle_handle, 2);
  vehicle_tag = (char *)tag_get(0x76656869, *(uint32_t *)vehicle);
  physics_tag = (char *)tag_get(0x70687973, *(uint32_t *)(vehicle_tag + 0x8c));

  game_globals = (char *)game_globals_get();
  havok_cleanup = (char *)tag_block_get_element(game_globals + 0x188, 0, 0x98);

  crash_damage_tag_index = *(int32_t *)(havok_cleanup + 0x48);
  crash_sound_tag_index = *(int32_t *)(vehicle_tag + 0x3cc);

  if (crash_damage_tag_index == -1 && crash_sound_tag_index == -1) {
    return;
  }

  delta_vel_x = *(float *)(vehicle + 0x18) - prev_velocity[0];
  delta_vel_y = *(float *)(vehicle + 0x1c) - prev_velocity[1];
  delta_vel_z = *(float *)(vehicle + 0x20) - prev_velocity[2];

  speed_change = (float)x87_sqrt(delta_vel_x * delta_vel_x + delta_vel_y * delta_vel_y + delta_vel_z * delta_vel_z);
  if (speed_change <= *(float *)0x255ca0) {
    return;
  }

  mass_point_count = *(int32_t *)(physics_tag + 0x74);
  if (mass_point_count <= 0) {
    return;
  }

  collided = 0;
  for (i = 0; i < mass_point_count; i++) {
    char *mp_state;
    tag_block_get_element(physics_tag + 0x74, i, 0x80);
    mp_state = (char *)mass_points + i * 0x130;
    if (*(uint8_t *)mp_state & 2) {
      collided = 1;
      break;
    }
  }

  if (collided) {
    float scale = (speed_change - *(float *)0x255ca0) * *(float *)0x2b7d34;

    if (crash_damage_tag_index != -1) {
      char damage_params[0x70];
      float clamped_scale = scale;
      if (clamped_scale < *(float *)0x2533c0) {
        clamped_scale = *(float *)0x2533c0;
      } else if (clamped_scale > *(float *)0x2533c8) {
        clamped_scale = *(float *)0x2533c8;
      }

      damage_data_new(damage_params, crash_damage_tag_index);

      *(float *)(damage_params + 0x1c) = *(float *)(vehicle + 0x50);
      *(float *)(damage_params + 0x20) = *(float *)(vehicle + 0x54);
      *(float *)(damage_params + 0x24) = *(float *)(vehicle + 0x58);

      *(float *)(damage_params + 0x34) = delta_vel_x;
      *(float *)(damage_params + 0x38) = delta_vel_y;
      *(float *)(damage_params + 0x3c) = delta_vel_z;

      *(float *)(damage_params + 0x40) = clamped_scale;

      object_cause_damage(damage_params, vehicle_handle, -1, -1, -1, 0);
    }

    if (crash_sound_tag_index != -1) {
      float sound_scale = scale;
      if (sound_scale < *(float *)0x2533c0) {
        sound_scale = *(float *)0x2533c0;
      } else if (sound_scale > *(float *)0x2533c8) {
        sound_scale = *(float *)0x2533c8;
      }

      object_impulse_sound_new(
        vehicle_handle,
        crash_sound_tag_index,
        -1,
        *(float **)0x31fc1c,
        *(float **)0x31fc3c,
        sound_scale
      );
    }
  }
}

/* 0x1b74d0: update_suspension
 * Wheel collision ray testing, animation track displacement, and bottoming-out impulse sounds.
 * Returns true if bottoming-out impulse sound was triggered, false otherwise.
 */
bool update_suspension(int vehicle_handle)
{
  char *vehicle;
  char *vehicle_tag;
  char *physics_tag;
  char *anim_tag;
  char *mode;
  char *suspension_block;
  uint32_t anim_tag_index;
  float transform[12];
  float max_displacement;
  int suspension_count;
  int sound_tag_index;
  int i;

  vehicle = (char *)object_get_and_verify_type(vehicle_handle, 2);
  vehicle_tag = (char *)tag_get(0x76656869, *(uint32_t *)vehicle);

  anim_tag_index = *(uint32_t *)(vehicle_tag + 0x44);
  if (anim_tag_index == 0xffffffff) {
    return false;
  }

  anim_tag = (char *)tag_get(0x616e7472, anim_tag_index);
  if (*(int32_t *)(anim_tag + 0x24) == 0) {
    return false;
  }

  mode = (char *)tag_block_get_element(anim_tag + 0x24, 0, 0x74);
  if (mode == NULL) {
    return false;
  }

  physics_tag = (char *)tag_get(0x70687973, *(uint32_t *)(vehicle_tag + 0x8c));

  matrix4x3_from_forward_up_position(
    transform,
    (float *)(vehicle + 0xc),
    (float *)(vehicle + 0x24),
    (float *)(vehicle + 0x30)
  );

  suspension_count = *(int32_t *)(mode + 0x68);
  suspension_block = mode + 0x68;
  max_displacement = 0.0f;

  if (suspension_count > 0) {
    for (i = 0; i < suspension_count; i++) {
      char *suspension = (char *)tag_block_get_element(suspension_block, i, 0x14);
      int16_t mass_point_index = *(int16_t *)suspension;
      int16_t anim_track_index;
      char *mass_point;
      uint8_t prev_byte;
      float prev_val;
      float transformed_pt[3];
      float transformed_norm[3];
      float diff;
      float dist;
      float ray_start[3];
      float ray_dir[3];
      char collision_result[0x34];
      float cur_val;
      float displacement;
      float new_val;
      uint8_t quantized;

      if (mass_point_index < 0 || mass_point_index >= *(int32_t *)(physics_tag + 0x74)) {
        continue;
      }

      anim_track_index = *(int16_t *)(suspension + 2);
      if (anim_track_index == -1) {
        continue;
      }

      tag_block_get_element(anim_tag + 0x74, (int)anim_track_index, 0xb4);
      mass_point = (char *)tag_block_get_element(physics_tag + 0x74, (int)mass_point_index, 0x80);

      prev_byte = *(uint8_t *)(vehicle + 0x44c + i);
      if (prev_byte == 0xff) {
        prev_val = *(float *)0x2533c8;
      } else {
        prev_val = (float)prev_byte * *(float *)0x261518;
      }

      matrix_transform_point(transform, (float *)(mass_point + 0x38), transformed_pt);
      matrix_transform_vector(transform, (float *)(mass_point + 0x50), transformed_norm);

      diff = *(float *)(suspension + 4) - *(float *)(suspension + 8);
      dist = (*(float *)(suspension + 8) - *(float *)(physics_tag + 0x14)) - diff;

      ray_start[0] = transformed_norm[0] * dist + transformed_pt[0];
      ray_start[1] = transformed_norm[1] * dist + transformed_pt[1];
      ray_start[2] = transformed_norm[2] * dist + transformed_pt[2];

      ray_dir[0] = transformed_norm[0] * (diff + diff);
      ray_dir[1] = transformed_norm[1] * (diff + diff);
      ray_dir[2] = transformed_norm[2] * (diff + diff);

      FUN_0014df70(0xc0a0, ray_start, ray_dir, vehicle_handle, (int16_t *)collision_result);

      cur_val = (*(float *)0x2533c8 - *(float *)(collision_result + 0x14)) * 2.0f;
      if (cur_val < *(float *)0x2533c0) {
        cur_val = *(float *)0x2533c0;
      } else if (cur_val > *(float *)0x2533c8) {
        cur_val = *(float *)0x2533c8;
      }

      displacement = cur_val - prev_val;
      if (displacement > max_displacement) {
        max_displacement = displacement;
      }

      new_val = (cur_val + prev_val) * *(float *)0x253398;
      quantized = quantize_real_to_byte_lower_bound(0.0f, 1.0f, new_val);
      *(uint8_t *)(vehicle + 0x44c + i) = quantized;
    }
  }

  sound_tag_index = *(int32_t *)(vehicle_tag + 0x3bc);
  if (sound_tag_index != -1 && max_displacement >= *(float *)0x2533e4) {
    float sound_scale = (max_displacement - *(float *)0x2533e4) * *(float *)0x2b7d38;
    if (sound_scale < *(float *)0x2533c0) {
      sound_scale = *(float *)0x2533c0;
    } else if (sound_scale > *(float *)0x2533c8) {
      sound_scale = *(float *)0x2533c8;
    }

    object_impulse_sound_new(
      vehicle_handle,
      sound_tag_index,
      -1,
      *(float **)0x31fc1c,
      *(float **)0x31fc3c,
      sound_scale
    );
    return true;
  }

  return false;
}

/* 0x1b77f0: create_slipping_effects
 * Skid/drift tire slipping material particles.
 * Takes vehicle_handle in EAX, unused first stack parameter, and mass_points in second stack parameter.
 */
void create_slipping_effects(void *unused, void *mass_points)
{
  int vehicle_handle;
  char *vehicle;
  char *vehicle_tag;
  char *physics_tag;
  int material_effect_tag_index;
  int mass_point_count;
  int i;

#ifdef _MSC_VER
  __asm { mov vehicle_handle, eax }
#else
  __asm__ __volatile__("movl %%eax, %0" : "=r"(vehicle_handle));
#endif

  (void)unused;

  vehicle = (char *)object_get_and_verify_type(vehicle_handle, 2);
  vehicle_tag = (char *)tag_get(0x76656869, *(uint32_t *)vehicle);
  physics_tag = (char *)tag_get(0x70687973, *(uint32_t *)(vehicle_tag + 0x8c));

  material_effect_tag_index = *(int32_t *)(vehicle_tag + 0x3dc);
  if (material_effect_tag_index == -1) {
    return;
  }

  mass_point_count = *(int32_t *)(physics_tag + 0x74);
  if (mass_point_count <= 0) {
    return;
  }

  for (i = 0; i < mass_point_count; i++) {
    char *mp_state = (char *)mass_points + i * 0x130;
    char *mp_def = (char *)tag_block_get_element(physics_tag + 0x74, i, 0x80);

    if (*(uint8_t *)mp_state & 2) {
      float slip_vx = *(float *)(mp_state + 0x54);
      float slip_vy = *(float *)(mp_state + 0x58);
      float slip_vz = *(float *)(mp_state + 0x5c);
      float slip_speed = (float)x87_sqrt(slip_vx * slip_vx + slip_vy * slip_vy + slip_vz * slip_vz);

      if (slip_speed > *(float *)0x25bc08) {
        float scale = (slip_speed - *(float *)0x25bc08) * *(float *)0x2b7d3c;
        float diff = (*(float *)(mp_state + 0x74) - *(float *)(mp_def + 0x68)) + *(float *)0x2b2264;
        float pos[3];
        float inv_speed = *(float *)0x2533dc / slip_speed;
        float dir_x = inv_speed * slip_vx;
        float dir_y = inv_speed * slip_vy;
        float dir_z = inv_speed * slip_vz;
        float fwd[3];
        short effect_type;
        uint16_t material_index;

        pos[0] = diff * *(float *)(mp_state + 0x60) + *(float *)(mp_state + 4);
        pos[1] = diff * *(float *)(mp_state + 0x64) + *(float *)(mp_state + 8);
        pos[2] = diff * *(float *)(mp_state + 0x68) + *(float *)(mp_state + 0xc);

        fwd[0] = *(float *)(mp_state + 0x60) * *(float *)0x253398 + dir_x;
        fwd[1] = *(float *)(mp_state + 0x64) * *(float *)0x253398 + dir_y;
        fwd[2] = *(float *)(mp_state + 0x68) * *(float *)0x253398 + dir_z;

        if (scale < *(float *)0x2533c0) {
          scale = *(float *)0x2533c0;
        } else if (scale > *(float *)0x2533c8) {
          scale = *(float *)0x2533c8;
        }

        effect_type = (short)(9 + ((*(uint32_t *)(mp_def + 0x24) & 1) ? 1 : 0));
        material_index = *(uint16_t *)(mp_state + 0x70);

        material_effect_new(
          material_effect_tag_index,
          effect_type,
          (short)material_index,
          pos,
          fwd,
          (void *)(vehicle + 0x48),
          scale
        );
      }
    }
  }
}

/* 0x1b79c0: vehicle_export_function_values
 * Computes vehicle export function outputs (speed, steering, throttle,
 * slip, RPM, etc.) for up to 4 functions defined in the vehicle tag (+0x31c),
 * clamping each result to [0.0, 1.0] and storing at vehicle + 0xd4.
 */
void vehicle_export_function_values(int vehicle_handle)
{
  char *vehicle;
  char *vehicle_tag;
  float max_fwd_rev;
  float max_side;
  float max_yaw;
  float abs_fwd;
  float abs_rev;
  float abs_side_l;
  float abs_side_r;
  float abs_yaw_l;
  float abs_yaw_r;
  int i;

  vehicle = (char *)object_get_and_verify_type(vehicle_handle, 2);
  vehicle_tag = (char *)tag_get(0x76656869, *(uint32_t *)vehicle);

  abs_fwd = x87_fabs(*(float *)(vehicle_tag + 0x2f8));
  abs_rev = x87_fabs(*(float *)(vehicle_tag + 0x2fc));
  max_fwd_rev = (abs_fwd > abs_rev) ? abs_fwd : abs_rev;

  abs_side_l = x87_fabs(*(float *)(vehicle_tag + 0x330));
  abs_side_r = x87_fabs(*(float *)(vehicle_tag + 0x334));
  max_side = (abs_side_l > abs_side_r) ? abs_side_l : abs_side_r;

  abs_yaw_l = x87_fabs(*(float *)(vehicle_tag + 0x308));
  abs_yaw_r = x87_fabs(*(float *)(vehicle_tag + 0x30c));
  max_yaw = (abs_yaw_l > abs_yaw_r) ? abs_yaw_l : abs_yaw_r;

  for (i = 0; i < 4; i++) {
    int16_t func_type = *(int16_t *)(vehicle_tag + 0x31c + i * 2);
    float val = *(float *)0x2533c0;

    if (func_type == 0) {
      continue;
    }

    switch (func_type) {
    case 1:
    case 0x1c:
    case 0x1d:
    case 0x1e:
    case 0x1f:
      val = x87_fabs(*(float *)(vehicle + 0x42c)) / max_fwd_rev;
      break;

    case 2:
      if (*(float *)(vehicle + 0x42c) >= *(float *)0x2533c0) {
        val = *(float *)(vehicle + 0x42c) / abs_fwd;
      }
      break;

    case 3:
      if (*(float *)(vehicle + 0x42c) <= *(float *)0x2533c0) {
        val = x87_fabs(*(float *)(vehicle + 0x42c)) / abs_rev;
      }
      break;

    case 4:
      val = x87_fabs(*(float *)(vehicle + 0x430)) / max_side;
      break;

    case 5:
      val = x87_fabs(*(float *)(vehicle + 0x430)) / abs_side_l;
      break;

    case 6:
      val = x87_fabs(*(float *)(vehicle + 0x430)) / abs_side_r;
      break;

    case 7: {
      float fwd_val = x87_fabs(*(float *)(vehicle + 0x42c)) / max_fwd_rev;
      float side_val = x87_fabs(*(float *)(vehicle + 0x430)) / max_side;
      val = (fwd_val > side_val) ? fwd_val : side_val;
      break;
    }

    case 8:
      val = x87_fabs(*(float *)(vehicle + 0x434)) / max_yaw;
      break;

    case 9:
      val = x87_fabs(*(float *)(vehicle + 0x434)) / abs_yaw_l;
      break;

    case 10:
      val = x87_fabs(*(float *)(vehicle + 0x434)) / abs_yaw_r;
      break;

    case 0xb:
      if (*(uint8_t *)(vehicle + 0x425) & 4) {
        val = *(float *)0x2533c8;
      }
      break;

    case 0xc:
      if (*(uint8_t *)(vehicle + 0x425) & 8) {
        val = *(float *)0x2533c8;
      }
      break;

    case 0xe:
      val = FUN_00012fe0((float *)(vehicle + 0x18)) / max_fwd_rev;
      break;

    case 0xf:
      if (*(uint8_t *)(vehicle + 5) & 0x1c) {
        val = FUN_00012fe0((float *)(vehicle + 0x18)) / max_fwd_rev;
      }
      break;

    case 0x10:
      if (*(uint8_t *)(vehicle + 5) & 2) {
        val = FUN_00012fe0((float *)(vehicle + 0x18)) / max_fwd_rev;
      }
      break;

    case 0x11: {
      float *vel = (float *)(vehicle + 0x18);
      float *fwd = (float *)(vehicle + 0x24);
      float dot = vel[0] * fwd[0] + vel[1] * fwd[1] + vel[2] * fwd[2];
      val = x87_fabs(dot) / max_fwd_rev;
      break;
    }

    case 0x12:
    case 0x13: {
      float *vel = (float *)(vehicle + 0x18);
      float *up = (float *)(vehicle + 0x30);
      float dot = vel[0] * up[0] + vel[1] * up[1] + vel[2] * up[2];
      val = x87_fabs(dot) / max_fwd_rev;
      break;
    }

    case 0x14:
      val = *(float *)(vehicle + 0x43c) / *(float *)(vehicle_tag + 0x310);
      break;

    case 0x15:
      val = *(float *)(vehicle + 0x440) / *(float *)(vehicle_tag + 0x310);
      break;

    case 0x16:
      val = x87_fabs(*(float *)(vehicle + 0x42c) - *(float *)(vehicle + 0x434)) / max_fwd_rev;
      break;

    case 0x17:
      val = x87_fabs(*(float *)(vehicle + 0x434) + *(float *)(vehicle + 0x42c)) / max_fwd_rev;
      break;

    case 0x18:
    case 0x19:
    case 0x1a:
    case 0x1b:
      val = *(float *)(vehicle + 0x438) / *(float *)(vehicle_tag + 0x310);
      break;

    case 0x20: {
      float perp[3];
      float par[3];
      float mag;
      FUN_0010b8a0((float *)(vehicle + 0x18), (float *)(vehicle + 0x24), par, perp);
      mag = FUN_00012fe0(perp) * *(float *)0x254e6c;
      val = mag * mag;
      break;
    }

    case 0x21:
      val = *(float *)(vehicle + 0x444);
      break;

    case 0x22:
      val = *(float *)(vehicle + 0x448);
      break;

    case 0x23: {
      float *vel = (float *)(vehicle + 0x18);
      float *fwd = (float *)(vehicle + 0x24);
      float dot = vel[0] * fwd[0] + vel[1] * fwd[1] + vel[2] * fwd[2];
      float factor = ((float)*(uint8_t *)(vehicle + 0x428) * *(float *)0x2549d4 + *(float *)0x2533c8) * *(float *)0x253398;
      if (factor < *(float *)0x2533c0) {
        factor = *(float *)0x2533c0;
      } else if (factor > *(float *)0x2533c8) {
        factor = *(float *)0x2533c8;
      }
      val = factor * (x87_fabs(*(float *)(vehicle + 0x42c)) / abs_fwd) +
            (*(float *)0x2533c8 - factor) * (x87_fabs(dot) / max_fwd_rev);
      break;
    }

    case 0x24: {
      float speed = FUN_00012fe0((float *)(vehicle + 0x18));
      val = ((speed / *(float *)(vehicle_tag + 0x2f8)) * *(float *)(vehicle + 0x448) - *(float *)0x2533e8) * *(float *)0x2b7d40;
      break;
    }

    default:
      break;
    }

    if (val < *(float *)0x2533c0) {
      val = *(float *)0x2533c0;
    } else if (val > *(float *)0x2533c8) {
      val = *(float *)0x2533c8;
    }

    *(float *)(vehicle + 0xd4 + i * 4) = val;
  }
}
