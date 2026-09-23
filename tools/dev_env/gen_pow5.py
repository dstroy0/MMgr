#!/usr/bin/env python3
# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""Write src/pow5/pow5.h: the powers of five a decimal conversion needs, and nothing else.

A decimal value is mant * 10^ex, and ten to the ex is five to the ex times two to the ex. The twos
never need arithmetic - they are the exponent field of the result, and putting one there is an add.
The fives are what has to be carried, and they are the same handful of numbers on every call, so
they belong in a table rather than in a loop that rebuilds them.

Each entry is five to a power of two, held as a 128 bit fraction with the top bit set and its own
binary exponent. Any exponent up to 511 is the product of at most nine of them. The reciprocals
are here too, which is what keeps a negative exponent a multiply instead of a division.

Truncated rather than rounded, on purpose. A truncated entry is never larger than the true power,
so the product is never larger than the true product, and the sticky bit that decides a tie is
never wrongly clear. Rounding the entries would put an error either side of the true value and the
tie would stop being decidable from the bits that are there.

WHAT THIS FILE OWES THE HEADER IT WRITES

The output is ordinary documented source: the licence block, an @file block, and a Doxygen comment
on every macro, the struct and both tables. It is written here rather than by hand because the
significands cannot be checked by eye, and the prose around them describes THIS configuration.

That prose names nine steps, 128 bit significands, an exponent reach of 511 and an e2 of -722 at
the widest. Those are statements about STEPS and BITS, so the two are asserted below rather than
left to be quietly contradicted. A generator that emits prose describing a shape it no longer
produces is the defect this file was rewritten to remove.

The layout matches what clang-format produces. A formatter run over the output is a no-op and a
header that comes back from the formatter changed reads as one somebody edited by hand.
"""

import pathlib

STEPS = 9  # 5^1, 5^2, 5^4 ... 5^256, enough for any exponent below 512
BITS = 128
ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "src" / "pow5" / "pow5.h"

# The prose below states nine entries, 128 bit significands, a reach of 511 and a widest e2 of -722.
# Every one of those is a claim about these two numbers, so changing either makes the generated
# documentation false. Refusing here is cheaper than shipping a header that describes itself wrongly.
if STEPS != 9 or BITS != 128:
    raise SystemExit(
        "gen_pow5: the generated prose describes STEPS 9 and BITS %d. STEPS is %d and BITS is %d, so "
        "the @file block, both table comments and the static assert would all be false. Update the "
        "prose in this file alongside the numbers." % (128, STEPS, BITS)
    )


def norm(num, den):
    """The 128 bit fraction just below num/den, with the binary exponent that goes with it."""
    s = BITS - 1 - (num.bit_length() - den.bit_length())

    def at(shift):
        return (num << shift) // den if shift >= 0 else num // (den << -shift)

    while at(s) < (1 << (BITS - 1)):
        s += 1
    while at(s) >= (1 << BITS):
        s -= 1
    return at(s), -s


def entries(recip):
    """Every `{hi, lo, e2}` initializer for one table, as text, in index order."""
    out = []
    for i in range(STEPS):
        p = 5 ** (1 << i)
        v, e = norm(1, p) if recip else norm(p, 1)
        out.append("{0x%016XULL, 0x%016XULL, %d}" % (v >> 64, v & ((1 << 64) - 1), e))
    return out


def rows(items):
    """`items` laid out two per line, padded the way clang-format aligns a braced initializer list.

    Two per line because that is what fits the column limit, and the first of each pair is padded so
    the second starts at the same column on every row. Emitting one per line and leaving the
    formatter to do this works, but then the generator's own output is not what is on disk until
    something else has run, and `harness.py generated` reads that as a dirty tree.
    """
    width = max(len(one) for one in items)
    out = []
    for i in range(0, len(items), 2):
        pair = items[i : i + 2]
        if len(pair) == 1:
            out.append("    %s," % pair[0])
        else:
            out.append("    %-*s %s," % (width + 1, pair[0] + ",", pair[1]))
    return "\n".join(out)


HEADER = """/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file pow5.h
 * @brief Powers of five as 128-bit significands, for scaling a decimal mantissa into a binary one.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-29
 *
 * @note transformo walks the bits of the decimal exponent and multiplies in one entry per set bit, so nine
 *       entries reach 511.
 * @note Declares no function. Both tables are static const data that outlive every call. A pointer into
 *       one stays good for the whole program [BORROWS].
 */
#ifndef MMGR_POW5_H
#define MMGR_POW5_H

#include "mmgr.h"

EMBED_BEGIN_DECLS

/**
 * @brief Expands to %(steps)d, the number of entries in each table.
 *
 * @note Entry i holds five raised to two to the i, so the nine entries run from 5^1 to 5^%(reach)d.
 * @note Carries no suffix: it sizes both tables and bounds the walk, where it converts to the signed
 *       embed_iword the loop counts in.
 */
#define MMGR_POW5_STEPS %(steps)d

/**
 * @brief Expands to ((1 << %(steps)d) - 1), which is %(max)d, the largest decimal exponent the tables reach.
 *
 * @note Every one of the nine entries multiplied together gives 5^%(max)d.
 * @note The expansion is a plain int constant expression, which widens to the embed_iword an exponent is
 *       carried in wherever the two are compared.
 * @warning muto_scale bounds against this and returns infinity above it and zero below its negative;
 *          muto_scale_to_u64 does not. What keeps either off the end of the tables is the walk itself,
 *          which takes only MMGR_POW5_STEPS steps. An exponent past this loses its higher bits.
 */
#define MMGR_POW5_MAX ((1 << MMGR_POW5_STEPS) - 1)

/**
 * @brief One power of five, as a 128-bit significand with a binary exponent.
 *
 * @note The value is (hi * 2^64 + lo) * 2^e2, and hi always has its top bit set.
 */
typedef struct
{
    embed_u64 hi;   /**< High 64 bits of the significand, with its top bit set. */
    embed_u64 lo;   /**< Low 64 bits of the significand. */
    embed_iword e2; /**< Binary exponent the significand is scaled by; %(widest)d at the widest fits a 16-bit embed_iword. */
} MmgrPow5;

/**
 * @brief Five raised to each power of two, from 5^1 at index 0 to 5^%(reach)d at index %(last)d.
 *
 * @note Entry i is the multiplier for bit i of the exponent magnitude, and the walk in muto_apply_pow10
 *       takes this table over mmgr_pow5_down whenever the decimal exponent is not negative.
 * @note Index 0 through 5 are exact. 5^64 and up need more than 128 bits, so the last three entries are
 *       truncated toward zero and read a little low.
 * @note Every significand literal carries ULL to match the embed_u64 it is stored in. Each e2 is a bare int
 *       that converts to embed_iword.
 * @note static const at header scope, so each translation unit gets its own copy, EMBED_UNUSED keeps a unit
 *       that never reads it quiet, and an entry's address stays good for the whole program [BORROWS].
 * @warning transformo settles a positive exponent from its exact powers of ten and returns before the walk,
 *          so the only exponent reaching this table there is zero, which sets no bit.
 */
static const MmgrPow5 mmgr_pow5_up[MMGR_POW5_STEPS] EMBED_UNUSED = {
%(up)s
};

/**
 * @brief The reciprocal of each mmgr_pow5_up entry, from 5^-1 at index 0 to 5^-%(reach)d at index %(last)d.
 *
 * @note Entry i is the multiplier for bit i of the exponent magnitude, and the walk in muto_apply_pow10
 *       takes this table when the decimal exponent is negative.
 * @note No negative power of five ends in binary. All nine are the exact value truncated toward zero.
 *       Every entry reads a little low and none of them round up: 5^-1 is the repeating 0xCCCC..., not the
 *       0xCCCD... that rounding to nearest would give.
 * @note Every significand literal carries ULL to match the embed_u64 it is stored in. Each e2 is a bare int
 *       that converts to embed_iword.
 * @note static const at header scope, so each translation unit gets its own copy, EMBED_UNUSED keeps a unit
 *       that never reads it quiet, and an entry's address stays good for the whole program [BORROWS].
 */
static const MmgrPow5 mmgr_pow5_down[MMGR_POW5_STEPS] EMBED_UNUSED = {
%(down)s
};

/**
 * @brief Asserts MMGR_POW5_MAX is at least %(max)d.
 *
 * @note A double's decimal exponent stays well inside %(max)d, so nine steps cover every value one can hold.
 */
EMBED_STATIC_ASSERT(MMGR_POW5_MAX >= %(max)d, "the tables do not reach the exponents a double can carry");

EMBED_END_DECLS

#endif
"""

up = entries(False)
down = entries(True)

# The widest e2 in either table, which the struct comment cites as what an embed_iword has to hold.
widest = min(int(one.rsplit(",", 1)[1].rstrip("}")) for one in up + down)

text = HEADER % {
    "steps": STEPS,
    "reach": 1 << (STEPS - 1),
    "last": STEPS - 1,
    "max": (1 << STEPS) - 1,
    "widest": widest,
    "up": rows(up),
    "down": rows(down),
}

OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text(text, encoding="utf-8", newline="\n")
print("wrote %s" % OUT.relative_to(ROOT).as_posix())
