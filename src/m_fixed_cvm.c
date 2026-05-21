/* Q16.16 fixed-point for CronoVM — replaces Crispy's src/m_fixed.c.
 *
 * Vanilla FixedMul/FixedDiv use int64_t (a 32x32->64 multiply, and a 64/32
 * divide). The CronoVM translator has no i64 register class and rejects i64
 * SSA, so we reimplement both in pure 32-bit arithmetic:
 *   - FixedMul is the canonical MUL+MULH composition (cvm_qmul_16_16).
 *   - FixedDiv keeps vanilla's overflow guard, then does the (|a|<<16)/|b|
 *     division as a 48-bit-by-32-bit restoring long division and reapplies
 *     the sign. Truncates toward zero, so the result is bit-identical to
 *     vanilla's `((int64_t)a << 16) / b` (demos/savegames stay in sync).
 *
 * This file is compiled INSTEAD OF third_party/crispy-doom/src/m_fixed.c.
 * FixedMul/FixedDiv are the only symbols that file provides. */
#include <stdlib.h>   /* abs */
#include <limits.h>   /* INT_MIN, INT_MAX */
#include <stdint.h>

#include "m_fixed.h"
#include "cvm_intrin.h"   /* cvm_qmul_16_16 (MUL+MULH) */

fixed_t FixedMul(fixed_t a, fixed_t b)
{
    return cvm_qmul_16_16(a, b);
}

/* [cronopio] (int64)(a*b - c*d) / divisor, computed without i64 SSA.
 * Used by r_segs.c's long-wall-wobble fix (R_StoreWallRange), which the
 * vendored code did with int64_t. Forms each 64-bit product via MUL+MULH,
 * subtracts in 64-bit (lo/hi halves), then does a signed 64/32 restoring
 * long division — same technique as FixedDiv above. */
/* [cronopio] Opaque float abs (see m_fixed.h). __attribute__((noinline))
 * keeps clang from inlining and re-recognizing the abs pattern as
 * llvm.fabs.f32, which the translator does not lower. */
__attribute__((noinline)) float CVM_FAbsF(float v)
{
    return v < 0.0f ? -v : v;
}

/* [cronopio] Round-half-away-from-zero, matching lroundf. clang's inline
 * expansion of lroundf emits an unordered float compare (fcmp ult), which the
 * translator rejects (it only supports ordered ==,!=,<,<=,>,>=). This explicit
 * version uses only ordered compares. noinline keeps clang from re-fusing it. */
__attribute__((noinline)) int CVM_LRoundF(float v)
{
    if (v >= 0.0f)
        return (int)(v + 0.5f);
    return -(int)(-v + 0.5f);
}

/* [cronopio] Opaque min/max/abs. clang -O1 folds the ternary min/max idiom and
 * abs() into the llvm.smax/smin/umax/umin/abs intrinsics, which the translator
 * does not lower (i16 errors at translate; i32 slips through as an undefined
 * extern that would trap at run time). Out-of-line calls defeat that fold.
 * Used by the MIN/MAX/BETWEEN macros (crispy.h). noinline is essential. */
__attribute__((noinline)) int CVM_IMax(int a, int b) { return a > b ? a : b; }
__attribute__((noinline)) int CVM_IMin(int a, int b) { return a < b ? a : b; }
__attribute__((noinline)) int CVM_IAbs(int a)        { return a < 0 ? -a : a; }

int CVM_CrossDiv(int a, int b, int c, int d, int divisor)
{
    /* product1 = a*b, product2 = c*d as signed 64-bit (lo:hi) */
    uint32_t p1_lo = (uint32_t)(a * b);
    int32_t  p1_hi = cvm_mulh(a, b);
    uint32_t p2_lo = (uint32_t)(c * d);
    int32_t  p2_hi = cvm_mulh(c, d);

    /* num = p1 - p2 (64-bit subtract with borrow) */
    uint32_t num_lo = p1_lo - p2_lo;
    uint32_t borrow = (p1_lo < p2_lo) ? 1u : 0u;
    int32_t  num_hi = (int32_t)((uint32_t)p1_hi - (uint32_t)p2_hi - borrow);

    if (divisor == 0)
        return 0;

    /* sign of result = sign(num) ^ sign(divisor); work in magnitudes */
    int neg = 0;
    if (num_hi < 0)
    {
        neg = !neg;
        /* negate 64-bit num */
        num_lo = ~num_lo + 1u;
        num_hi = (int32_t)(~(uint32_t)num_hi + (num_lo == 0u ? 1u : 0u));
    }
    uint32_t dv = (uint32_t)divisor;
    if (divisor < 0) { neg = !neg; dv = (uint32_t)(-divisor); }

    /* restoring division of the 64-bit magnitude (num_hi:num_lo) by dv */
    uint32_t uhi = (uint32_t)num_hi;
    uint32_t q = 0, rem = 0;
    for (int i = 63; i >= 0; --i)
    {
        uint32_t bit = (i >= 32) ? ((uhi >> (i - 32)) & 1u)
                                 : ((num_lo >> i) & 1u);
        rem = (rem << 1) | bit;
        q <<= 1;
        if (rem >= dv) { rem -= dv; q |= 1u; }
    }

    return neg ? -(int32_t)q : (int32_t)q;
}

/* [cronopio] (int64)(a1*b1 + a2*b2 + a3*b3) / s without i64 SSA.
 * Used by p_setup.c P_RemoveSlimeTrails (slime-trail removal projection). */
static void add64(uint32_t *lo, int32_t *hi, int a, int b)
{
    uint32_t plo = (uint32_t)(a * b);
    int32_t  phi = cvm_mulh(a, b);
    uint32_t nlo = *lo + plo;
    uint32_t carry = (nlo < *lo) ? 1u : 0u;
    *lo = nlo;
    *hi = (int32_t)((uint32_t)*hi + (uint32_t)phi + carry);
}

int CVM_ProjDiv(int a1, int b1, int a2, int b2, int a3, int b3, int s)
{
    uint32_t lo = 0;
    int32_t  hi = 0;
    add64(&lo, &hi, a1, b1);
    add64(&lo, &hi, a2, b2);
    add64(&lo, &hi, a3, b3);

    if (s == 0)
        return 0;

    int neg = 0;
    if (hi < 0)
    {
        neg = !neg;
        lo = ~lo + 1u;
        hi = (int32_t)(~(uint32_t)hi + (lo == 0u ? 1u : 0u));
    }
    uint32_t dv = (uint32_t)s;
    if (s < 0) { neg = !neg; dv = (uint32_t)(-s); }

    uint32_t uhi = (uint32_t)hi;
    uint32_t q = 0, rem = 0;
    for (int i = 63; i >= 0; --i)
    {
        uint32_t bit = (i >= 32) ? ((uhi >> (i - 32)) & 1u)
                                 : ((lo >> i) & 1u);
        rem = (rem << 1) | bit;
        q <<= 1;
        if (rem >= dv) { rem -= dv; q |= 1u; }
    }
    return neg ? -(int32_t)q : (int32_t)q;
}

fixed_t FixedDiv(fixed_t a, fixed_t b)
{
    if ((abs(a) >> 14) >= abs(b))
    {
        return (a ^ b) < 0 ? INT_MIN : INT_MAX;
    }

    /* Past the guard the quotient fits in 32 bits. Divide the 48-bit
     * magnitude (|a| << 16) by |b| bit by bit, then reapply the sign. */
    int      neg    = (a ^ b) < 0;
    uint32_t ua     = (a < 0) ? (uint32_t)(-a) : (uint32_t)a;
    uint32_t ub     = (b < 0) ? (uint32_t)(-b) : (uint32_t)b;
    uint32_t num_lo = ua << 16;
    uint32_t num_hi = ua >> 16;          /* numerator = (num_hi : num_lo) */
    uint32_t q = 0, rem = 0;

    for (int i = 47; i >= 0; --i)
    {
        uint32_t bit = (i >= 32) ? ((num_hi >> (i - 32)) & 1u)
                                 : ((num_lo >> i) & 1u);
        rem = (rem << 1) | bit;
        q <<= 1;
        if (rem >= ub) { rem -= ub; q |= 1u; }
    }

    return neg ? -(int32_t)q : (int32_t)q;
}
