/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file ascii_persona_bitorum_fuzz.c
 * @brief A libFuzzer harness over the ASCII classes. Every byte in every class runs against the ranges the header
 *        states for that class, with 0x80 and above in none.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-06
 */
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"

#include "fuzz_input.h"

static int within(unsigned int byte, unsigned int low, unsigned int high)
{
    return (byte >= low) && (byte <= high);
}

static int stated(MmgrAsciiClass kind, unsigned int byte)
{
    const int num = within(byte, '0', '9');
    const int upper = within(byte, 'A', 'Z');
    const int lower = within(byte, 'a', 'z');
    switch (kind)
    {
    case MMGR_ASCII_NUM:
        return num;
    case MMGR_ASCII_ALPHA:
        return upper || lower;
    case MMGR_ASCII_ALNUM:
        return num || upper || lower;
    case MMGR_ASCII_UPPER:
        return upper;
    case MMGR_ASCII_LOWER:
        return lower;
    case MMGR_ASCII_HEX:
        return num || within(byte, 'A', 'F') || within(byte, 'a', 'f');
    case MMGR_ASCII_PUNCT:
        return within(byte, '!', '/') || within(byte, ':', '@') || within(byte, '[', '`') || within(byte, '{', '~');
    case MMGR_ASCII_SPACE:
        return within(byte, 9u, 13u) || (byte == 32u);
    case MMGR_ASCII_CTRL:
        return (byte <= 31u) || (byte == 127u);
    default:
        return within(byte, 32u, 126u);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzInput input = fuzz_input(data, size);
    while (fuzz_left(&input))
    {
        const MmgrAsciiClass kind = (MmgrAsciiClass)(fuzz_byte(&input) % (uint8_t)MMGR_ASCII_CLASSES);
        const uint8_t byte = fuzz_byte(&input);
        fuzz_check((EMBED_CALL(ascii.in, AsciiCfg, .kind = kind, .byte = byte) != 0) == stated(kind, byte),
                   "a byte's class is the class the header states");
    }
    return 0;
}
