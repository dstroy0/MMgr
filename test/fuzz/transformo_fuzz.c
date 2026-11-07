/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file transformo_fuzz.c
 * @brief A libFuzzer harness over the decimal conversions: take against the multiply and add it states, scale
 *        against the C library's correctly rounded strtod, scale with dropped digits between the two neighbors
 *        it lies between, and scale_to_u64 against the exact rational rounded half up in a wide integer.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-06
 */
#include "transformo/transformo.h"

#include "fuzz_input.h"
#include "fuzz_wide.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static embed_u64 drawn_mant(FuzzInput *input)
{
    const uint8_t kind = fuzz_byte(input);
    switch (kind & 3u)
    {
    case 0:
        return (embed_u64)(kind >> 2);
    case 1:
        return MMGR_MUTO_MANT_MAX - 2u + (embed_u64)(kind >> 6);
    case 2:
        return fuzz_word(input) >> (fuzz_byte(input) % 64u);
    default:
        return fuzz_word(input);
    }
}

static double library_scaled(embed_u64 mant, int ex, int neg)
{
    char text[48];
    snprintf(text, sizeof text, "%s%llue%d", neg ? "-" : "", (unsigned long long)mant, ex);
    return strtod(text, NULL);
}

static void taken(FuzzInput *input)
{
    embed_u64 mant = drawn_mant(input);
    const embed_u64 before = mant;
    const char digit = (char)('0' + (fuzz_byte(input) % 10u));
    const embed_bool appended = EMBED_CALL(muto.take, TransformoCfg, .mant = &mant, .digit = digit);
    if (before > MMGR_MUTO_MANT_MAX)
    {
        fuzz_check(!appended && (mant == before), "a mantissa past the most refuses the digit and stands");
    }
    else
    {
        fuzz_check(appended && (mant == before * 10u + (embed_u64)(digit - '0')), "take appends the digit");
    }
}

static void scaled(FuzzInput *input)
{
    embed_u64 mant = drawn_mant(input);
    const int ex = (int)(fuzz_below(input, 1400u)) - 700;
    const int neg = (fuzz_byte(input) & 1u) != 0u;
    const double exact =
        EMBED_CALL(muto.scale, TransformoCfg, .mant = &mant, .ex = (embed_iword)ex, .neg = (embed_bool)neg);

    if (mant == 0u)
    {
        fuzz_check((exact == 0.0) && ((signbit(exact) != 0) == neg), "a zero mantissa scales to a signed zero");
        return;
    }
    if (ex > MMGR_POW5_MAX)
    {
        fuzz_check(isinf(exact) && ((signbit(exact) != 0) == neg), "an exponent past the tables is a signed infinity");
        return;
    }
    if (ex < -MMGR_POW5_MAX)
    {
        fuzz_check((exact == 0.0) && ((signbit(exact) != 0) == neg), "an exponent under the tables is a signed zero");
        return;
    }
    const double library = library_scaled(mant, ex, neg);
    if (exact != library)
    {
        fprintf(stderr, "scale %llue%d gave %a, strtod %a\n", (unsigned long long)mant, ex, exact, library);
    }
    fuzz_check(exact == library, "scale is the nearest double, as strtod gives");

    if (mant < ~(embed_u64)0)
    {
        const double dropped = EMBED_CALL(muto.scale, TransformoCfg, .mant = &mant, .ex = (embed_iword)ex, .rest = 1,
                                          .neg = (embed_bool)neg);
        const double above = library_scaled(mant + 1u, ex, neg);
        const double low = neg ? above : library;
        const double high = neg ? library : above;
        fuzz_check((dropped >= low) && (dropped <= high), "dropped digits scale between the mantissa and the next one");
    }
}

static void scaled_to_u64(FuzzInput *input)
{
    embed_u64 mant = drawn_mant(input);
    const int e2 = (int)fuzz_below(input, 200u) - 140;
    const int ex = (int)fuzz_below(input, 80u) - 40;
    const embed_u64 rounded =
        EMBED_CALL(muto.scale_to_u64, TransformoCfg, .mant = &mant, .e2 = (embed_iword)e2, .ex = (embed_iword)ex);

    Wide numerator = wide_of(mant);
    Wide denominator = wide_of(1u);
    wide_shift_up((e2 >= 0) ? &numerator : &denominator, (unsigned int)((e2 >= 0) ? e2 : -e2));
    for (int step = 0; step < ((ex >= 0) ? ex : -ex); step += 1)
    {
        wide_times((ex >= 0) ? &numerator : &denominator, 10u);
    }
    wide_times(&numerator, 2u);
    wide_plus(&numerator, &denominator);
    wide_times(&denominator, 2u);
    const Wide quotient = wide_quotient(&numerator, &denominator);
    const embed_u64 expected = (wide_bits(&quotient) > 64u)
                                   ? ~(embed_u64)0
                                   : ((embed_u64)quotient.limb[0] | ((embed_u64)quotient.limb[1] << 32));
    if (rounded != expected)
    {
        fprintf(stderr, "scale_to_u64 %llu*2^%d*10^%d gave %llu, exact %llu\n", (unsigned long long)mant, e2, ex,
                (unsigned long long)rounded, (unsigned long long)expected);
    }
    fuzz_check(rounded == expected, "scale_to_u64 is the exact value rounded half up, saturated");
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzInput input = fuzz_input(data, size);
    while (fuzz_left(&input))
    {
        switch (fuzz_byte(&input) % 3u)
        {
        case 0:
            taken(&input);
            break;
        case 1:
            scaled(&input);
            break;
        default:
            scaled_to_u64(&input);
            break;
        }
    }
    return 0;
}
