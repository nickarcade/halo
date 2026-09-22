/* Store a 2D point (0x1089d0).
 * point layout: {x, y} as int16_t[2]. */
void set_point2d(int16_t *point, int16_t x, int16_t y)
{
  point[0] = x;
  point[1] = y;
}

/* Offset a 2D point by (dx, dy) (0x1089f0).
 * point layout: {x, y} as int16_t[2]. */
void offset_point2d(int16_t *point, int16_t dx, int16_t dy)
{
  point[0] += dx;
  point[1] += dy;
}

/* Width of a 2D rectangle (0x108a10).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * Binary reads `right` zero-extended (xor eax,eax; mov ax,[ecx+6]) and
 * `left` sign-extended (movsx ecx,[ecx+2]); both extensions preserved. */
int rect2d_width(const int16_t *rect)
{
  return (int)(uint16_t)rect[3] - (int)rect[1];
}

/* Height of a 2D rectangle (0x108a30).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * Binary reads `bottom` zero-extended (xor eax,eax; mov ax,[ecx+4]) and
 * `top` sign-extended (movsx ecx,[ecx]); both extensions preserved. */
int rect2d_height(const int16_t *rect)
{
  return (int)(uint16_t)rect[2] - (int)rect[0];
}

/* Inset a 2D rectangle by (dx, dy) (0x108a50).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * Store order follows the binary: left, right, top, bottom. */
void inset_rectangle2d(int16_t *rect, int16_t dx, int16_t dy)
{
  rect[1] += dx;
  rect[3] -= dx;
  rect[0] += dy;
  rect[2] -= dy;
}

/* Offset a 2D rectangle by (dx, dy) (0x108a70).
 * rect layout: {top, left, bottom, right} as int16_t[4]. */
void rect2d_offset(int16_t *rect, int16_t dx, int16_t dy)
{
  rect[1] += dx;
  rect[3] += dx;
  rect[0] += dy;
  rect[2] += dy;
}

/* Test whether a 2D point lies inside a 2D rectangle (0x108cd0).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * point layout: {x, y} as int16_t[2].
 * Binary compares point[0] against left/right and point[1] against
 * top/bottom, all as signed 16-bit compares (JL/JGE); the range is
 * half-open at right/bottom. Result is returned in AL. */
boolean point2d_in_rectangle2d(const int16_t *rect, const int16_t *point)
{
  return (boolean)(point[0] >= rect[1] && point[0] < rect[3] &&
                   point[1] >= rect[0] && point[1] < rect[2]);
}

/* Test whether `interior` lies entirely inside `rect` (0x108d00).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * Binary compare order is left, right, top, bottom; all signed 16-bit
 * (JL/JG), and the bounds are inclusive on every edge.
 * Returns a full 32-bit 0/1 in EAX (mov eax,1 / xor eax,eax), not a byte
 * boolean, so the return type is int32_t. */
int32_t interior_rectangle2d(const int16_t *rect, const int16_t *interior)
{
  return (int32_t)(interior[1] >= rect[1] && interior[3] <= rect[3] &&
                   interior[0] >= rect[0] && interior[2] <= rect[2]);
}

/* Test whether two 2D rectangles are identical (0x108d40).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * Binary compare order is left, right, top, bottom, all 16-bit equality
 * compares short-circuiting to the common failure exit.
 * Returns a full 32-bit 0/1 in EAX (mov eax,1 / xor eax,eax), not a byte
 * boolean, so the return type is int32_t. */
int32_t equal_rectangle2d(const int16_t *rect_a, const int16_t *rect_b)
{
  return (int32_t)(rect_a[1] == rect_b[1] && rect_a[3] == rect_b[3] &&
                   rect_a[0] == rect_b[0] && rect_a[2] == rect_b[2]);
}

/* Test whether two 2D points are identical (0x108d80).
 * point layout: {x, y} as int16_t[2].
 * Binary compares x then y as 16-bit equality compares, both short-circuiting
 * to the common failure exit.
 * Returns a full 32-bit 0/1 in EAX (mov eax,1 / xor eax,eax), not a byte
 * boolean, so the return type is int32_t. */
int32_t equal_point2d(const int16_t *point_a, const int16_t *point_b)
{
  return (int32_t)(point_a[0] == point_b[0] && point_a[1] == point_b[1]);
}

/* Compute floor(log2(value)) (0x108db0).
 * Returns 0 for value <= 1. */
int16_t FUN_00108db0(unsigned int value)
{
  int result = 0;
  if (value > 0) {
    while (value != 1) {
      value >>= 1;
      result++;
    }
  }
  return (int16_t)result;
}
