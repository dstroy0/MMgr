/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file fractio_fuzz.c
 * @brief A libFuzzer harness over the binary64 fields: every pattern split into its sign, exponent and mantissa
 *        against shifts and masks, merged back to itself, merge of wide fields against each field masked, and the
 *        two reinterpretations against memcpy.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-06
 */
#include "fractio/fractio.h"

#include "fuzz_input.h"

#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzInput input = fuzz_input(data, size);
    while (fuzz_left(&input))
    {
        const embed_u64 bits = fuzz_edge_word(&input);
        const embed_u64 sign = EMBED_CALL(fract.sign, FractioCfg, .bits = bits);
        const embed_u64 exp = EMBED_CALL(fract.exp, FractioCfg, .bits = bits);
        const embed_u64 mant = EMBED_CALL(fract.mant, FractioCfg, .bits = bits);

        fuzz_check(sign == (bits >> 63), "sign is the top bit");
        fuzz_check(exp == ((bits >> 52) & 0x7FFu), "exp is the eleven bits under the sign");
        fuzz_check(mant == (bits & 0x000FFFFFFFFFFFFFull), "mant is the low fifty-two bits");
        fuzz_check(EMBED_CALL(fract.merge, FractioCfg, .sign = sign, .exp = exp, .mant = mant) == bits,
                   "the three fields merge back to the pattern");

        const embed_u64 wide_sign = fuzz_edge_word(&input);
        const embed_u64 wide_exp = fuzz_edge_word(&input);
        const embed_u64 wide_mant = fuzz_edge_word(&input);
        const embed_u64 masked = ((wide_sign & 1u) << 63) | ((wide_exp & 0x7FFu) << 52) | (wide_mant & 0x000FFFFFFFFFFFFFull);
        fuzz_check(EMBED_CALL(fract.merge, FractioCfg, .sign = wide_sign, .exp = wide_exp, .mant = wide_mant) == masked,
                   "merge keeps each field to its own width");

        double expected_val;
        memcpy(&expected_val, &bits, sizeof expected_val);
        const double val = EMBED_CALL(fract.from_bits, FractioCfg, .bits = bits);
        fuzz_check(memcmp(&val, &expected_val, sizeof val) == 0, "from_bits is the same bytes read as a double");
        fuzz_check(EMBED_CALL(fract.to_bits, FractioCfg, .val = expected_val) == bits,
                   "to_bits is the same bytes read as a pattern");
    }
    return 0;
}
