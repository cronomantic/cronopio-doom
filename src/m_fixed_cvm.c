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
