/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file pow5_fuzz.c
 * @brief A libFuzzer harness over the powers of five. The entry each input names runs against five to the power
 *        computed exactly in a wide integer, its top 128 bits truncated. It also runs against the reciprocal, two
 *        to a power floored by five once for each factor, which floors exactly. The result then comes truncated the same way.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-06
 */
#include "pow5/pow5.h"

#include "fuzz_input.h"
#include "fuzz_wide.h"

/** @brief The top 128 bits of @p wide, truncated, as hi and lo, and the exponent of its lowest kept bit. */
static MmgrPow5 wide_top(const Wide *wide, int shift_in)
{
    const unsigned int bits = wide_bits(wide);
    MmgrPow5 top = {0u, 0u, 0};
    for (unsigned int step = 0u; step < 128u; step += 1u)
    {
        const uint64_t set = (step < bits) ? wide_bit(wide, bits - 1u - step) : 0u;
        if (step < 64u)
        {
            top.hi |= set << (63u - step);
        }
        else
        {
            top.lo |= set << (127u - step);
        }
    }
    top.e2 = (embed_iword)((int)bits - 128 + shift_in);
    return top;
}

static MmgrPow5 computed_up(unsigned int index)
{
    Wide wide = wide_of(1u);
    for (unsigned int factor = 0u; factor < (1u << index); factor += 1u)
    {
        wide_times(&wide, 5u);
    }
    return wide_top(&wide, 0);
}

static MmgrPow5 computed_down(unsigned int index)
{
    const unsigned int power = 1300u;
    Wide wide = wide_of(0u);
    wide.limb[power / 32u] = (uint32_t)1u << (power % 32u);
    for (unsigned int factor = 0u; factor < (1u << index); factor += 1u)
    {
        wide_over(&wide, 5u);
    }
    return wide_top(&wide, -(int)power);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzInput input = fuzz_input(data, size);
    while (fuzz_left(&input))
    {
        const uint8_t pick = fuzz_byte(&input);
        const unsigned int index = (unsigned int)(pick >> 1) % (unsigned int)MMGR_POW5_STEPS;
        const MmgrPow5 *const entry = ((pick & 1u) == 0u) ? &mmgr_pow5_up[index] : &mmgr_pow5_down[index];
        const MmgrPow5 computed = ((pick & 1u) == 0u) ? computed_up(index) : computed_down(index);

        fuzz_check((entry->hi >> 63) == 1u, "an entry's significand has its top bit set");
        fuzz_check(entry->hi == computed.hi, "an entry's high half is the computed power's");
        fuzz_check(entry->lo == computed.lo, "an entry's low half is the computed power's, truncated");
        fuzz_check(entry->e2 == computed.e2, "an entry's exponent is the computed power's");
    }
    return 0;
}
