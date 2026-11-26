#!/usr/bin/env python3
# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""Emit src/ascii_persona_bitorum/{ascii_persona_bitorum.h,ascii_persona_bitorum.c,CMakeLists.txt}.

Bitmaps and not range chains: one shift and one and answers any class, and every class costs the same.

Sixteen bytes and not two uint64, because this library builds at EMBED_WORD_BITS 16 as well as
64 and a 64-bit shift is a called routine there. A byte index is the same one load, one shift, one
and on every width, and needs no per-width form of the constants.

An index names a class and the .c that holds the masks answers it. The masks and their initializer
macros live in the .c, the only thing that reads them. In the header they would cost a hundred and
sixty bytes in every translation unit that asks one question. They would also put ten names in the
global preprocessor namespace of every consumer, for the benefit of one file.

WHAT THE GENERATOR WRITES AND WHAT IT DOES NOT

The masks come from the predicates below, because they are exactly the constants nobody can check
by eye. A mask written by hand can lose 'Z' from ALPHA, or put space, DEL and 0x1F in PUNCT. The enum
comes out with them. A class can never have a number different from the row it indexes.

Everything else in the two files is prose and structure that describes THIS module. It is here in
full and not left to a hand pass over the output. A generated file with notes added by hand loses
the notes the next time the generator runs.

FORMAT, AND WHY THE TWO FILES DIFFER

ascii_persona_bitorum.h is in .clang-format-ignore. The formatter and this generator disagree
about the mask table, and the generator wins. So the header comes out byte for byte as it must land,
with the column the enum comments sit in.

ascii_persona_bitorum.c is NOT in that list, and so clang-format owns its layout. The generator
emits a plain table and runs the formatter over it. Without that the output differs from what is on
disk until something else runs, and `harness.py generated` reads that as a dirty tree. An absent
clang-format stops the run with an error.
"""

import pathlib
import shutil
import subprocess
import sys

# HERE is mmgr/tools/dev_env, LIB is mmgr. Same convention as readclean.py, and so the generator runs
# from anywhere.
HERE = pathlib.Path(__file__).resolve().parent
LIB = HERE.parent.parent
OUT = LIB / "src" / "ascii_persona_bitorum"
OUT.mkdir(parents=True, exist_ok=True)


def num(c):
    return 0x30 <= c <= 0x39


def upper(c):
    return 0x41 <= c <= 0x5A


def lower(c):
    return 0x61 <= c <= 0x7A


def alpha(c):
    return upper(c) or lower(c)


def alnum(c):
    return alpha(c) or num(c)


def space(c):
    return c == 0x20 or 0x09 <= c <= 0x0D


def punct(c):
    return 0x21 <= c <= 0x7E and not alnum(c)


def hexd(c):
    return num(c) or (0x41 <= c <= 0x46) or (0x61 <= c <= 0x66)


def prnt(c):
    return 0x20 <= c <= 0x7E


def ctrl(c):
    return c < 0x20 or c == 0x7F


# In enum order. These are the values of MmgrAsciiClass, and so this list is API. A new order
# renumbers the classes under every caller that already compiled against them.
#
# The third column is the enumerator's own Doxygen line in the header. It states the code points the
# class covers, in the form a reader checks the mask against.
CLASSES = [
    ("NUM", num, "'0' to '9'."),
    ("ALPHA", alpha, "'A' to 'Z' and 'a' to 'z'."),
    ("ALNUM", alnum, "'0' to '9', 'A' to 'Z' and 'a' to 'z'."),
    ("UPPER", upper, "'A' to 'Z'."),
    ("LOWER", lower, "'a' to 'z'."),
    ("HEX", hexd, "'0' to '9', 'A' to 'F' and 'a' to 'f'."),
    ("PUNCT", punct, "'!' to '/', ':' to '@', '[' to '\\`' and '{' to '~'."),
    ("SPACE", space, "9 to 13, and 32."),
    ("CTRL", ctrl, "0 to 31, and 127."),
    ("PRINT", prnt, "32 to 126."),
]

# The enumerator count closes the enum. It is not a class and as an argument it is out of range.
COUNT = ("CLASSES", "Enumerator count and not a class.")


def bytes_of(pred):
    b = [0] * 16
    for c in range(128):
        if pred(c):
            b[c >> 3] |= 1 << (c & 7)
    return b


# The column the enumerator comments start in, measured from the end of the four space indent.
#
# The header is in .clang-format-ignore, and so the formatter never sees it and this alignment is
# the generator's to hold. The width is two wider than the longest entry plus the one space
# SpacesBeforeTrailingComments asks for. That is the column the header carries, and it stays as
# it is. A narrower gap would rewrite eleven lines of a generated file to no effect anybody reads.
ENUM_COMMENT_WIDTH = 22


def enum_block():
    """The enumerator lines, with every Doxygen comment in one column.

    Refuses and does not run past the column. A single long enumerator would push its own
    comment out and leave the other ten where they are. That reads as ten misaligned lines and not
    one long one.
    """
    entries = [("MMGR_ASCII_%s = 0," % CLASSES[0][0], CLASSES[0][2])]
    entries += [("MMGR_ASCII_%s," % name, doc) for name, _pred, doc in CLASSES[1:]]
    entries.append(("MMGR_ASCII_%s" % COUNT[0], COUNT[1]))
    widest = max(len(text) for text, _doc in entries)
    if widest > ENUM_COMMENT_WIDTH - 1:
        raise SystemExit(
            "gen_ascii_persona_bitorum: %r is %d characters and the comment column is %d, so it "
            "would push its own comment past the others. Widen ENUM_COMMENT_WIDTH."
            % (max(entries, key=lambda one: len(one[0]))[0], widest, ENUM_COMMENT_WIDTH)
        )
    return "\n".join("    %-*s/**< %s */" % (ENUM_COMMENT_WIDTH, text, doc) for text, doc in entries)


def mask_rows():
    """One initializer per class, in enum order, arranged for clang-format to pack.

    The rows carry no array designator. C++ has none, and nvcc compiles the .c as C++. The order of
    CLASSES is the order of the enum, and so the row of each class is its own without one.
    """
    out = []
    for _name, pred, _doc in CLASSES:
        body = ", ".join("0x%02X" % one for one in bytes_of(pred))
        out.append("    {{%s}}," % body)
    return "\n".join(out)


HDR = """/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file ascii_persona_bitorum.h
 * @brief ASCII class membership: mask type, class list, and the ascii dispatch table.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-29
 *
 * @note Class membership as a bitmap lookup and not a chain of range compares. One shift, one
 *       mask and one test answer any class, and the cost does not grow with how many ranges the
 *       class covers.
 * @note Covers code points 0 to 127 only. A byte of 0x80 or above is in no class here, and
 *       that retains the table sixteen bytes and not thirty-two.
 */
#ifndef MMGR_ASCII_PERSONA_BITORUM_H
#define MMGR_ASCII_PERSONA_BITORUM_H

/** @addtogroup mod_ascii
 *  @{ */

#include "mmgr.h"

EMBED_BEGIN_DECLS

/**
 * @brief Sixteen bytes with one bit for each of the code points 0 to 127.
 *
 * @note A shift and a mask locate a code point, not a search. Code point n is bit
 *       (n & 7) of bits[n >> 3]. Any class answers in the same three operations.
 */
typedef struct
{
    uint8_t bits[16]; /**< The bitmap, with code point 0 at the low bit of bits[0]. */
} MmgrAsciiMask;

/**
 * @brief Asserts an MmgrAsciiMask is exactly sixteen bytes.
 *
 * @note mmgr_ascii_in reads bits[byte >> 3] for every byte below 0x80. All sixteen have to be
 *       there.
 * @note Sixteen reach code point 127 and no further, as leaves a byte of 0x80 or above in
 *       no class at all.
 */
EMBED_STATIC_ASSERT(sizeof(MmgrAsciiMask) == 16u, "an ASCII class mask is exactly 128 bits");

/**
 * @brief Character class selector, numbered from 0.
 *
 * @note MMGR_ASCII_CLASSES is the enumerator count and not a class. As an argument it is out of range.
 */
typedef enum
{
%(enum)s
} MmgrAsciiClass;

/**
 * @brief Arguments to mmgr_ascii_in: the class and the byte to test.
 */
typedef struct
{
    const MmgrAsciiClass type; /**< Class to test against, below MMGR_ASCII_CLASSES. */
    const uint8_t byte;        /**< Code point to look up; 0x80 and above are in no class. */
} AsciiCfg;

/**
 * @brief Type of the ascii dispatch table.
 *
 * @note EMBED_TABLE_LAYOUT asserts the three members sit at consecutive EMBED_FUNCTION_POINTER_BYTES offsets, with
 *       nothing else.
 * @note lower and upper read only the byte.
 */
typedef struct
{
    embed_bool (*in)(const AsciiCfg *args); /**< Whether a byte belongs to a class. */
    uint8_t (*lower)(const AsciiCfg *args); /**< The byte with 'A' to 'Z' folded to 'a' to 'z'. */
    uint8_t (*upper)(const AsciiCfg *args); /**< The byte with 'a' to 'z' folded to 'A' to 'Z'. */
} AsciiPersonaBitorumNs;
EMBED_TABLE_LAYOUT(AsciiPersonaBitorumNs, in, lower, upper);

/**
 * @brief Returns whether args->byte has its bit set in the type bitmap.
 *
 * @param[in] args Class and byte to test [BORROWS].
 * @return         EMBED_TRUE when the bit holds, EMBED_FALSE otherwise.
 * @note Bytes 0x80 and above return EMBED_FALSE.
 * @warning args->type must be below MMGR_ASCII_CLASSES, and nothing holds it there outside a
 *          MMGR_DEBUG_CHECKS build. It indexes the bitmap, and a byte under 0x80 then reads
 *          past the table.
 */
EMBED_HOST_DEVICE embed_bool mmgr_ascii_in(const AsciiCfg *args);

/**
 * @brief Returns args->byte with an upper case letter folded to its lower case, as tolower folds it.
 *
 * @param[in] args The byte, and args->type, which the call does not read [BORROWS].
 * @return         'a' to 'z' for 'A' to 'Z', and any other byte as it is.
 */
EMBED_HOST_DEVICE uint8_t mmgr_ascii_lower(const AsciiCfg *args);

/**
 * @brief Returns args->byte with a lower case letter folded to its upper case, as toupper folds it.
 *
 * @param[in] args The byte, and args->type, which the call does not read [BORROWS].
 * @return         'A' to 'Z' for 'a' to 'z', and any other byte as it is.
 */
EMBED_HOST_DEVICE uint8_t mmgr_ascii_upper(const AsciiCfg *args);

/**
 * @brief Dispatch table instance named ascii, with each member set to its mmgr_ascii_ function.
 */
EMBED_TABLE_STORAGE AsciiPersonaBitorumNs ascii EMBED_UNUSED = {
    .in = mmgr_ascii_in,
    .lower = mmgr_ascii_lower,
    .upper = mmgr_ascii_upper,
};

EMBED_END_DECLS

/** @} */

#endif
"""

SRC = """/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file ascii_persona_bitorum.c
 * @brief ASCII class membership, read from the 128-bit s_class bitmaps.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-29
 *
 * @note The whole module is one table lookup. The bitmaps are constant data. So nothing runs
 *       before main, and a membership test costs a shift, a mask and a compare whatever
 *       the class covers.
 * @note Reaches nothing outside config.
 */
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"

/**
 * @brief One 128-bit membership bitmap per MmgrAsciiClass value.
 *
 * @note Indexed by MmgrAsciiClass. Code point n is bit (n & 7) of byte (n >> 3).
 * @note Worked through, MMGR_ASCII_NUM holds 0xFF at byte 6 and 0x03 at byte 7. Byte 6 carries code
 *       points 48 through 55, which is '0' to '7', and the low two bits of byte 7 carry 56 and 57,
 *       which is '8' and '9'. Every row below reads the same way, and so none of them asks for
 *       trust.
 * @note The rows come in the order of MmgrAsciiClass, from MMGR_ASCII_NUM to MMGR_ASCII_PRINT. C++ has no array
 *       designator, and nvcc compiles this file as C++.
 * @note EMBED_PASS_MEMORY gives each pass of nvcc its own copy, and so a kernel reads the table from device memory.
 * @note Sixteen bytes reach code point 127 and no further, which leaves a byte at 0x80 or
 *       above with no row to hold it.
 */
static EMBED_PASS_MEMORY const MmgrAsciiMask s_class[MMGR_ASCII_CLASSES] = {
%(rows)s
};

/**
 * @brief Argument type built by EMBED_CALL in mmgr_ascii_in.
 *
 * @note Fields match AsciiCfg, without its const qualifiers.
 */
typedef struct
{
    MmgrAsciiClass type; /**< Class whose bitmap the test reads. */
    uint8_t byte;        /**< Code point to look up. */
} AsciiCtx;

/**
 * @brief Returns whether args->byte has its bit set in s_class[args->type].
 *
 * @param[in] args Class and byte to test [BORROWS].
 * @return         EMBED_TRUE when the bit holds, EMBED_FALSE otherwise.
 * @note Bytes 0x80 and above return EMBED_FALSE without reading s_class.
 * @warning args->type must be below MMGR_ASCII_CLASSES, and nothing holds it there outside a
 *          MMGR_DEBUG_CHECKS build: a byte under 0x80 then reads past s_class.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_bool ascii_in(const AsciiCtx *args)
{
    MMGR_ASSERT(args->type < MMGR_ASCII_CLASSES, "no such character class");

    const MmgrAsciiMask *const entry = &s_class[args->type];

    // The byte test comes first and && stops there. A byte of 0x80 or above would index bits[16] or
    // past it, outside the sixteen the mask holds. Explicit cast narrows the int result of && to
    // the embed_bool container
    return (embed_bool)((args->byte < 0x80u) && (((entry->bits[args->byte >> 3] >> (args->byte & 7u)) & 1u) != 0u));
}

/**
 * @brief Returns args->byte with 'A' to 'Z' folded to 'a' to 'z'.
 * @param[in] args The byte [BORROWS].
 * @return         The folded byte, or the byte as it is where it is no upper case letter.
 * @note A letter of either case differs from the other case in the bit of 0x20 alone.
 */
EMBED_HOST_DEVICE EMBED_INLINE uint8_t ascii_lower(const AsciiCtx *args)
{
    // Explicit cast narrows the int of the bit operation back to the byte it came from
    return ((args->byte >= (uint8_t)'A') && (args->byte <= (uint8_t)'Z')) ? (uint8_t)(args->byte | 0x20u) : args->byte;
}

/**
 * @brief Returns args->byte with 'a' to 'z' folded to 'A' to 'Z'.
 * @param[in] args The byte [BORROWS].
 * @return         The folded byte, or the byte as it is where it is no lower case letter.
 * @note A letter of either case differs from the other case in the bit of 0x20 alone.
 */
EMBED_HOST_DEVICE EMBED_INLINE uint8_t ascii_upper(const AsciiCtx *args)
{
    // Explicit cast narrows the int of the bit operation back to the byte it came from
    return ((args->byte >= (uint8_t)'a') && (args->byte <= (uint8_t)'z')) ? (uint8_t)(args->byte & 0xDFu) : args->byte;
}

/**
 * @brief Binds this module's four fixed arguments to EMBED_ENTRY.
 *
 * @param[in] ReturnType_ Return type of the entry point.
 * @param[in] name_       Name after the mmgr_ascii_ and ascii_ prefixes, which the two share.
 * @param[in] ...         Initializers for the AsciiCtx literal, written through args.
 * @note Four of EMBED_ENTRY's six arguments are the same at every entry in this module. So they
 *       bind once here and each entry below states only what differs.
 */
#define ASCII_ENTRY(ReturnType_, name_, ...)                                                                           \\
    EMBED_ENTRY(mmgr_ascii_, ascii_, AsciiCtx, AsciiCfg, ReturnType_, name_, __VA_ARGS__)

/**
 * @brief The public surface, one line per entry point.
 *
 * @note ascii_persona_bitorum.h describes each at its declaration.
 */
ASCII_ENTRY(embed_bool, in, .type = args->type, .byte = args->byte)
ASCII_ENTRY(uint8_t, lower, .byte = args->byte)
ASCII_ENTRY(uint8_t, upper, .byte = args->byte)
"""

CMAKE = """# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
# negotiated commercial licensing contract or an educator's license issued to you personally.
#
# Generated by tools/dev_env/gen_ascii_persona_bitorum.py.

# The masks are file local. A header full of static const tables is a copy of all of them in
# every translation unit that wanted to ask one question.
mmgr_add_module(ascii_persona_bitorum
  SOURCES ascii_persona_bitorum.c
)
"""


def formatter():
    """The clang-format to run over the .c, or a refusal naming why one is needed."""
    found = shutil.which("clang-format")
    if found:
        return found
    raise SystemExit(
        "gen_ascii_persona_bitorum: clang-format is not on PATH. The mask table in the .c is positioned "
        "out by the formatter, so without it this writes a file that differs from the one on disk "
        "and `harness.py generated` reports a dirty tree. Install it or put it on PATH."
    )


def main():
    clang = formatter()

    header = OUT / "ascii_persona_bitorum.h"
    source = OUT / "ascii_persona_bitorum.c"
    cmake = OUT / "CMakeLists.txt"

    header.write_text(HDR % {"enum": enum_block()}, encoding="utf-8", newline="\n")
    source.write_text(SRC % {"rows": mask_rows()}, encoding="utf-8", newline="\n")
    cmake.write_text(CMAKE, encoding="utf-8", newline="\n")

    # The header is in .clang-format-ignore and comes out as it must land. Only the .c goes through the formatter.
    subprocess.run([clang, "-i", str(source)], check=True)

    for path in (header, source, cmake):
        print("wrote %s" % path.relative_to(LIB).as_posix())

    for name, pred, _doc in CLASSES:
        b = bytes_of(pred)
        lo = int.from_bytes(bytes(b[:8]), "little")
        hi = int.from_bytes(bytes(b[8:]), "little")
        print("  %-6s low=0x%016X high=0x%016X" % (name, lo, hi))
    return 0


if __name__ == "__main__":
    sys.exit(main())
