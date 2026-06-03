#include "FLOAT.h"
#include <stdint.h>
#include <assert.h>

FLOAT F_mul_F(FLOAT a, FLOAT b) {
  return (FLOAT)(((int64_t)a * b) >> 16);
}

FLOAT F_div_F(FLOAT a, FLOAT b) {
  return (FLOAT)(((int64_t)a << 16) / b);
}

FLOAT f2F(float a) {
  /* Read float bits as integer via union — no arithmetic on float, no FPU ops. */
  union { float f; uint32_t i; } u;
  u.f = a;
  uint32_t bits = u.i;
  uint32_t sign = bits >> 31;
  int32_t  exp  = (int32_t)((bits >> 23) & 0xff);
  uint32_t frac = bits & 0x7fffff;
  if (exp == 0 && frac == 0) return 0;          /* ±0 */
  uint32_t sig  = frac | (1u << 23);            /* implicit leading 1 */
  int32_t  shift = exp - 134;                   /* A = sig * 2^(exp-134), want A = a*2^16 */
  FLOAT r = (shift >= 0) ? (FLOAT)(sig << shift) : (FLOAT)(sig >> (-shift));
  return sign ? -r : r;
}

FLOAT Fabs(FLOAT a) {
  return a < 0 ? -a : a;
}

/* Functions below are already implemented */

FLOAT Fsqrt(FLOAT x) {
  FLOAT dt, t = int2F(2);

  do {
    dt = F_div_int((F_div_F(x, t) - t), 2);
    t += dt;
  } while(Fabs(dt) > f2F(1e-4));

  return t;
}

FLOAT Fpow(FLOAT x, FLOAT y) {
  /* we only compute x^0.333 */
  FLOAT t2, dt, t = int2F(2);

  do {
    t2 = F_mul_F(t, t);
    dt = (F_div_F(x, t2) - t) / 3;
    t += dt;
  } while(Fabs(dt) > f2F(1e-4));

  return t;
}
