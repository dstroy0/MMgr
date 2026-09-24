#!/usr/bin/env python3
# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""Emit src/ascii_persona_bitorum/{ascii_persona_bitorum.h,ascii_persona_bitorum.c,CMakeLists.txt}.

Bitmaps, not range chains: one shift and one and answer any class, and every class costs the same.

Sixteen bytes rather than two uint64, because this library builds at EMBED_WORD_BITS 16 as well as
64 and a 64-bit shift is a called routine there. Indexing bytes is the same one load, one shift, one
and on every width, and needs no per-width spelling of the constants.

A class is named by an index and answered by the .c that holds the masks. The masks used to be ten
static const objects in the header, which is a hundred and sixty bytes of them in every translation
unit that wanted to ask one question, and their initializer macros stayed in the header after the
storage moved out - ten names in the global preprocessor namespace of every consumer, for the
benefit of one file. Both now live in the .c, which is the only thing that reads them.

WHAT IS GENERATED AND WHAT IS NOT

The masks are generated from the predicates below, because they are exactly the constants nobody
can check by eye: the first three written out by hand had 'Z' missing from ALPHA, and space, DEL
and 0x1F wrongly in PUNCT. The enum is generated with them. A class can never be numbered
differently from the row it indexes.

Everything else in the two files is prose and structure that describes THIS module, and it is
written out here in full rather than left to a hand pass over the output. A generated file that is
then documented by hand loses the documentation the next time the generator runs, which is what
happened to all three of this tree's generators.

FORMATTING, AND WHY THE TWO FILES ARE TREATED DIFFERENTLY

ascii_persona_bitorum.h is listed in .clang-format-ignore: the formatter and this generator disagree
about the mask table, and the generator wins. So the header is emitted byte for byte as it must land,
including the column the enum comments sit in.

ascii_persona_bitorum.c is NOT ignored, so clang-format owns its layout and the generator emits a
plain table and runs the formatter over it. Without that the output differs from what is on disk
until something else has run, and `harness.py generated` reads that as a dirty tree. A missing
clang-format is a refusal rather than a warning.
"""

import pathlib
import shutil
import subprocess
import sys

# HERE is mmgr/tools/dev_env, LIB is mmgr. Same convention as readclean.py, so the generator runs
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


# In enum order. These are the values of MmgrAsciiClass, so this list is API: reordering it
# renumbers the classes under every caller that already compiled against them.
#
# The third column is the enumerator's own Doxygen line in the header. It states the code points the
# class covers, in the spelling a reader checks the mask against.
CLASSES = [
    ("NUM", num, "'0' to '9'."),
    ("ALPHA", alpha, "'A' to 'Z' and 'a' to 'z'."),
    ("ALNUM", alnum, "'0' to '9', 'A' to 'Z' and 'a' to 'z'."),
    ("UPPER", upper, "'A' to 'Z'."),
    ("LOWER", lower, "'a' to 'z'."),
    ("HEX", hexd, "'0' to '9', 'A' to 'F' and 'a' to 'f'."),
    ("PUNCT", punct, "'!' to '/', ':' to '@', '[' to '`' and '{' to '~'."),
    ("SPACE", space, "9 to 13, and 32."),
    ("CTRL", ctrl, "0 to 31, and 127."),
    ("PRINT", prnt, "32 to 126."),
]

# The enumerator count closes the enum. It is not a class and passing it is out of range.
COUNT = ("CLASSES", "Enumerator count, not a class.")


def bytes_of(pred):
    b = [0] * 16
    for c in range(128):
        if pred(c):
            b[c >> 3] |= 1 << (c & 7)
    return b


# The column the enumerator comments start in, measured from the end of the four space indent.
#
# The header is listed in .clang-format-ignore, so the formatter never sees it and this alignment is
# the generator's to hold. The width is two wider than the longest entry plus the one space
# SpacesBeforeTrailingComments asks for, which is the column the header has carried since before the
# masks moved out of it. It is reproduced rather than normalised: closing the gap would rewrite
# eleven lines of a generated file to no effect anybody reads.
ENUM_COMMENT_WIDTH = 22


def enum_block():
    """The enumerator lines, with every Doxygen comment in one column.

    Refuses rather than overflowing the column, because a single long enumerator would push its own
    comment out and leave the other ten where they are, which reads as ten misaligned lines instead
    of one long one.
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
    """One designated initializer per class, in enum order, laid out for clang-format to pack."""
    out = []
    for name, pred, _doc in CLASSES:
        body = ", ".join("0x%02X" % one for one in bytes_of(pred))
        out.append("    [MMGR_ASCII_%s] = {{%s}}," % (name, body))
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
 * @note Class membership as a bitmap lookup rather than a chain of range compares. One shift, one
 *       mask and one test answer any class, and the cost does not grow with how many ranges the
 *       class covers.
 * @note Covers code points 0 to 127 only. A byte of 0x80 or above is in no class here, which is
 *       what keeps the table sixteen bytes rather than thirty-two.
 */
#ifndef MMGR_ASCII_PERSONA_BITORUM_H
#define MMGR_ASCII_PERSONA_BITORUM_H

#include "mmgr.h"

EMBED_BEGIN_DECLS

/**
 * @brief Sixteen bytes holding one bit for each of the code points 0 to 127.
 *
 * @note A code point is located by shift and mask rather than by search. Code point n is bit
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
 * @note Sixteen bytes reach code point 127 and no further, which is what leaves a byte of 0x80 or
 *       above in no class at all.
 */
EMBED_STATIC_ASSERT(sizeof(MmgrAsciiMask) == 16u, "an ASCII class mask is exactly 128 bits");

/**
 * @brief Character class selector, numbered from 0.
 *
 * @note MMGR_ASCII_CLASSES is the enumerator count, not a class. Passing it is out of range.
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
    const MmgrAsciiClass kind; /**< Class to test against, below MMGR_ASCII_CLASSES. */
    const uint8_t byte;        /**< Code point to look up; 0x80 and above are in no class. */
} AsciiCfg;

/**
 * @brief Type of the ascii dispatch table.
 *
 * @note EMBED_TABLE_LAYOUT asserts that the in member is at offset 0 and that the struct holds
 *       nothing else.
 */
typedef struct
{
    embed_bool (*in)(const AsciiCfg *args); /**< Whether a byte belongs to a class. */
} AsciiPersonaBitorumNs;
EMBED_TABLE_LAYOUT(AsciiPersonaBitorumNs, in);

/**
 * @brief Returns whether args->byte has its bit set in the kind bitmap.
 *
 * @param[in] args Class and byte to test [BORROWS].
 * @return         EMBED_TRUE when the bit is set, EMBED_FALSE otherwise.
 * @note Bytes 0x80 and above return EMBED_FALSE.
 * @warning args->kind must be below MMGR_ASCII_CLASSES, and nothing holds it there outside a
 *          MMGR_DEBUG_CHECKS build: the bitmap is indexed by it. A byte under 0x80 then reads
 *          past the table.
 */
embed_bool mmgr_ascii_in(const AsciiCfg *args);

/**
 * @brief Dispatch table instance named ascii, whose in member is set to mmgr_ascii_in.
 */
EMBED_TABLE_STORAGE AsciiPersonaBitorumNs ascii EMBED_UNUSED = {
    .in = mmgr_ascii_in,
};

EMBED_END_DECLS

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
 * @note The whole module is one table lookup. The bitmaps are emitted as initialized data, so
 *       nothing runs before main and a membership test costs a shift, a mask and a compare whatever
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
 *       which is '8' and '9'. Every row below reads the same way, so none of them has to be taken
 *       on trust.
 * @note Sixteen bytes reach code point 127 and no further, which is what leaves a byte at 0x80 or
 *       above with no row it could be found in.
 */
static const MmgrAsciiMask s_class[MMGR_ASCII_CLASSES] = {
%(rows)s
};

/**
 * @brief Argument type built by EMBED_CALL in mmgr_ascii_in.
 *
 * @note Fields match AsciiCfg, without its const qualifiers.
 */
typedef struct
{
    MmgrAsciiClass kind; /**< Class whose bitmap is read. */
    uint8_t byte;        /**< Code point to look up. */
} AsciiCtx;

/**
 * @brief Returns whether args->byte has its bit set in s_class[args->kind].
 *
 * @param[in] args Class and byte to test [BORROWS].
 * @return         EMBED_TRUE when the bit is set, EMBED_FALSE otherwise.
 * @note Bytes 0x80 and above return EMBED_FALSE without reading s_class.
 * @warning args->kind must be below MMGR_ASCII_CLASSES, and nothing holds it there outside a
 *          MMGR_DEBUG_CHECKS build: a byte under 0x80 then reads past s_class.
 */
EMBED_INLINE embed_bool ascii_in(const AsciiCtx *args)
{
    MMGR_ASSERT(args->kind < MMGR_ASCII_CLASSES, "no such character class");

    const MmgrAsciiMask *const entry = &s_class[args->kind];

    // The byte test comes first and && stops there. A byte of 0x80 or above would index bits[16] or
    // past it, outside the sixteen the mask holds. Explicit cast narrows the int result of && to
    // the embed_bool container.
    return (embed_bool)((args->byte < 0x80u) && (((entry->bits[args->byte >> 3] >> (args->byte & 7u)) & 1u) != 0u));
}

/**
 * @brief Binds this module's four fixed arguments to EMBED_ENTRY.
 *
 * @param[in] ReturnType_ Return type of the entry point.
 * @param[in] name_       Name after the mmgr_ascii_ and ascii_ prefixes, which the two share.
 * @param[in] ...         Initializers for the AsciiCtx literal, written in terms of args.
 * @note Four of EMBED_ENTRY's six arguments are the same at every entry in this module, so they
 *       are bound once here and each entry below states only what differs.
 */
#define ASCII_ENTRY(ReturnType_, name_, ...)                                                                           \\
    EMBED_ENTRY(mmgr_ascii_, ascii_, AsciiCtx, AsciiCfg, ReturnType_, name_, __VA_ARGS__)

/**
 * @brief The public surface, one line per entry point.
 *
 * @note Each is documented at its declaration in ascii_persona_bitorum.h.
 */
ASCII_ENTRY(embed_bool, in, .kind = args->kind, .byte = args->byte)
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
    if not found:
        raise SystemExit(
            "gen_ascii_persona_bitorum: clang-format is not on PATH. The mask table in the .c is "
            "laid out by the formatter, so without it this writes a file that differs from the one "
            "on disk and `harness.py generated` reports a dirty tree. Install it or put it on PATH."
        )
    # Being on PATH is not enough. .clang-format uses keys that only exist from clang-format 20, and
    # an older binary refuses the whole file rather than passing over the one key it does not know.
    # It is asked here, over a throwaway buffer, because finding out during the -i below leaves the
    # generated files already written and unformatted, with a traceback in place of this message.
    probe = subprocess.run(
        [found, "--style=file", "--assume-filename=%s" % (OUT / "probe.c")],
        input="int a;\n",
        capture_output=True,
        text=True,
    )
    if probe.returncode != 0:
        version = subprocess.run([found, "--version"], capture_output=True, text=True).stdout.strip()
        raise SystemExit(
            "%s: %s cannot read %s.\n%s\n"
            "%s, so a run with this binary would leave files that differ from the ones on disk. "
            "Use the version .github/workflows/format-code.yml pins."
            % (
                "gen_ascii_persona_bitorum",
                version or found,
                LIB / ".clang-format",
                probe.stderr.strip(),
                "The mask table in the .c is laid out by the formatter",
            )
        )
    return found


def main():
    clang = formatter()

    header = OUT / "ascii_persona_bitorum.h"
    source = OUT / "ascii_persona_bitorum.c"
    cmake = OUT / "CMakeLists.txt"

    header.write_text(HDR % {"enum": enum_block()}, encoding="utf-8", newline="\n")
    source.write_text(SRC % {"rows": mask_rows()}, encoding="utf-8", newline="\n")
    cmake.write_text(CMAKE, encoding="utf-8", newline="\n")

    # The header is in .clang-format-ignore and is emitted as it must land. Only the .c is formatted.
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
