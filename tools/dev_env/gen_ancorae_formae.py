#!/usr/bin/env python3
# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""Generate the anchor cost profiles.

Cost is "how common", so the picker takes the minimum and a byte that cannot occur in the profile's
grammar scores best - if a needle contains one, it is the most selective anchor available.

Sources:
  english   practicalcryptography.com monograms, 4.5e9 characters from Wortschatz. Space is taken
            at twice E per en.wikipedia.org/wiki/Letter_frequency.
  uri       RFC 3986 character classes: unreserved, gen-delims, sub-delims, weighted by which parts
            of a URI they appear in.
  inet      RFC 4291 IPv6 text form and dotted-quad: the alphabet is 0-9 a-f A-F : . / % and
            nothing else.
  route     RFC 3986 path-abempty plus the template syntax routers actually use.
  generic   no grammar assumed, only the shape of byte data: NUL never, high bytes rare, ASCII
            printable common.

WHAT EACH PROFILE CARRIES BESIDES ITS FREQUENCIES

The generated file is ordinary documented source, so every profile also carries the prose that
describes ITS table: the @brief, the notes about which bytes were pinned, and the range the entry
point returns. Those are observations about one table and cannot be derived from another, so they
sit beside the frequency function rather than in the template.

The two numbers in the @return line ARE derived, from the table that was just computed, so that
line cannot come to disagree with the data above it.

FORMATTING

The tables are laid out by clang-format, which aligns each column to the widest entry in it. That
alignment is not reproducible by hand in any way worth maintaining, so the generator emits a plain
sixteen-per-row table and then runs the formatter over it. Without that step the output differs
from what is on disk until something else has run, and `harness.py generated` reads that as a dirty
tree. A missing clang-format is a refusal rather than a warning.
"""

import math
import pathlib
import shutil
import subprocess
import sys

# HERE is mmgr/tools/dev_env, LIB is mmgr. Same convention as readclean.py, so the generator runs
# from anywhere.
HERE = pathlib.Path(__file__).resolve().parent
LIB = HERE.parent.parent
OUT = LIB / "src" / "impensa_ancorae_acus"
OUT.mkdir(parents=True, exist_ok=True)

MONO = {
    "a": 8.55,
    "b": 1.60,
    "c": 3.16,
    "d": 3.87,
    "e": 12.10,
    "f": 2.18,
    "g": 2.09,
    "h": 4.96,
    "i": 7.33,
    "j": 0.22,
    "k": 0.81,
    "l": 4.21,
    "m": 2.53,
    "n": 7.17,
    "o": 7.47,
    "p": 2.07,
    "q": 0.10,
    "r": 6.33,
    "s": 6.73,
    "t": 8.94,
    "u": 2.68,
    "v": 1.06,
    "w": 1.83,
    "x": 0.19,
    "y": 1.72,
    "z": 0.11,
}


def scale(freq, floor=0.001):
    """Map a frequency dict onto 1 through 255 on a log scale. An absent byte gets the floor."""
    vals = [max(f, floor) for f in freq.values() if f > 0]
    lo, hi = math.log(floor), math.log(max(vals))
    out = []
    for b in range(256):
        f = max(freq.get(b, 0.0), floor)
        v = (math.log(f) - lo) / (hi - lo)
        out.append(max(1, min(255, int(round(1 + v * 254)))))
    out[0] = 255  # NUL terminates; never anchor on it
    return out


def english():
    f = {}
    f[ord(" ")] = 24.0  # about twice E
    for c, p in MONO.items():
        f[ord(c)] = p * 0.75  # letters share the stream with space and punctuation
        f[ord(c.upper())] = p * 0.75 / 20.0
    for c, p in {
        "\n": 1.9,
        ".": 1.2,
        ",": 1.1,
        "'": 0.5,
        '"': 0.3,
        "-": 0.3,
        "\r": 0.3,
        "\t": 0.2,
        "/": 0.15,
        ":": 0.12,
        ")": 0.10,
        "(": 0.10,
        ";": 0.07,
        "?": 0.06,
        "!": 0.05,
        "_": 0.05,
    }.items():
        f[ord(c)] = p
    for d in "0123456789":
        f[ord(d)] = 0.30
    return f


def uri():
    f = {}
    for c, p in {
        "/": 12.0,
        ".": 8.0,
        "-": 4.0,
        ":": 3.0,
        "?": 1.2,
        "=": 2.0,
        "&": 1.5,
        "%": 1.0,
        "_": 1.0,
        "#": 0.3,
        "~": 0.15,
        "@": 0.2,
        "+": 0.4,
        ",": 0.2,
        ";": 0.15,
        "!": 0.05,
        "$": 0.05,
        "'": 0.05,
        "(": 0.05,
        ")": 0.05,
        "*": 0.05,
        "[": 0.03,
        "]": 0.03,
    }.items():
        f[ord(c)] = p
    for c, p in MONO.items():
        f[ord(c)] = p * 0.55
        f[ord(c.upper())] = p * 0.55 / 12.0
    for d in "0123456789":
        f[ord(d)] = 1.6
    return f


def inet():
    """IPv6 text form and dotted quad. The alphabet is tiny; everything outside it is a gift."""
    f = {}
    f[ord(":")] = 20.0
    f[ord(".")] = 12.0
    for d in "0123456789":
        f[ord(d)] = 7.0
    f[ord("0")] = 11.0  # leading zeros and :: runs
    f[ord("1")] = 9.0
    f[ord("2")] = 8.0
    for c in "abcdef":
        f[ord(c)] = 3.0
        f[ord(c.upper())] = 1.0
    f[ord("/")] = 1.0  # prefix length
    f[ord("%")] = 0.2  # zone id
    f[ord("[")] = 0.5
    f[ord("]")] = 0.5
    return f


def route():
    f = {}
    f[ord("/")] = 22.0
    for c, p in {
        "-": 3.0,
        "_": 2.0,
        "{": 1.5,
        "}": 1.5,
        ":": 1.2,
        ".": 1.0,
        "*": 0.3,
        "?": 0.2,
        "<": 0.15,
        ">": 0.15,
        "(": 0.1,
        ")": 0.1,
    }.items():
        f[ord(c)] = p
    for c, p in MONO.items():
        f[ord(c)] = p * 0.60
        f[ord(c.upper())] = p * 0.60 / 25.0
    for d in "0123456789":
        f[ord(d)] = 1.0
    return f


def generic():
    """No grammar. Only the shape of byte data: printable ASCII is what strings are made of."""
    f = {}
    f[ord(" ")] = 12.0
    for c, p in MONO.items():
        f[ord(c)] = p * 0.55
        f[ord(c.upper())] = p * 0.55 / 8.0
    for d in "0123456789":
        f[ord(d)] = 1.2
    for b in range(0x21, 0x7F):
        f.setdefault(b, 0.5)
    for b in list(range(1, 0x20)) + [0x7F]:
        f[b] = 0.15
    f[ord("\n")] = 1.5
    f[ord("\t")] = 0.4
    for b in range(0x80, 0x100):
        f[b] = 0.05
    return f


# name, the @brief for the file, the notes on the table, the @return phrasing, the frequency source.
#
# `notes` are observations about the table this profile produces - which bytes were pinned to the
# ceiling, where the floor sits, which bytes are unexpectedly above it. They are per profile because
# they are true of one table and false of the next.
#
# `returns` takes the lowest and highest entry of the computed table, so the range it states is read
# off the data rather than remembered.
PROFILES = [
    (
        "generic",
        "Byte cost table with no entry at the floor of 1, because every byte value carries a cost.",
        [
            "Lower means rarer, and cellul_pick_rows keeps the lowest cost it finds.",
            "255 marks the NUL and the space, so neither is ever chosen as a sieve offset.",
            "Bytes 128 through 255 all carry 107, so no high byte is preferred over another.",
            "The entries carry no U suffix. Each is within 0 to 255, so the initializer stores it as a\n"
            " *       uint8_t unchanged.",
        ],
        "The cost, %d through %d in this table.",
        generic,
    ),
    (
        "english",
        "Byte cost table weighted for English text.",
        [
            "Lower means rarer in this corpus, and cellul_pick_rows keeps the lowest cost it finds.",
            "The floor is 1. The ceiling 255 sits on the NUL and the space, so neither is ever chosen as a\n"
            " *       sieve offset.",
        ],
        "The cost, %d through %d.",
        english,
    ),
    (
        "uri",
        "Byte cost table scoring letters, digits and URI punctuation.",
        [
            "Lower means rarer, and cellul_pick_rows keeps the lowest cost it finds.",
            "255 marks the NUL and the slash, so neither is ever chosen as a sieve offset.",
            "The space sits at 1 here, where the two text tables give it 255.",
            "Every initializer is a plain int constant from 1 to 255, so narrowing to the uint8_t element"
            " keeps its value.",
        ],
        "The cost, %d through %d.",
        uri,
    ),
    (
        "inet",
        "Byte cost table scoring only the characters an address is built from.",
        [
            "Lower means rarer, and cellul_pick_rows keeps the lowest cost it finds.",
            "255 marks the NUL and the colon, so neither is ever chosen as a sieve offset.",
            "Also above 1: the digits, 'a' to 'f', 'A' to 'F', and 37 '%', 46 '.', 47 '/', 91 '[', 93 ']'.",
        ],
        "The cost, %d through %d.",
        inet,
    ),
    (
        "route",
        "Byte cost table scoring letters, digits and path punctuation.",
        [
            "Lower means rarer, and cellul_pick_rows keeps the lowest cost it finds.",
            "255 marks the NUL and the slash, so neither is ever chosen as a sieve offset.",
            "The braces at 123 and 125 carry a cost, where most punctuation sits at 1.",
        ],
        "The cost, %d through %d.",
        route,
    ),
]

SRC = """/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file impensa_ancorae_acus_%(name)s.c
 * @brief %(brief)s
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-29
 *
 * @note One of five files defining mmgr_ancorae_impensa. A build links exactly one of them.
 */
#include "impensa_ancorae_acus/impensa_ancorae_acus.h"

/**
 * @brief Cost of each byte value, indexed by the byte itself.
 *
%(notes)s
 */
static const uint8_t s_impensa[256] = {
%(rows)s
};

/**
 * @brief Argument type built by EMBED_CALL in mmgr_ancorae_impensa.
 *
 * @note Mirrors AncoraeCfg without its const qualifier.
 */
typedef struct
{
    uint8_t byte; /**< Byte value to look up. */
} AncoraeCtx;

/**
 * @brief Returns the table entry for args->byte.
 *
 * @param[in] args Byte to look up [BORROWS].
 * @return         %(returns)s
 * @note The table holds 256 entries, so every uint8_t value indexes it in range.
 */
EMBED_INLINE uint8_t ancorae_impensa(const AncoraeCtx *args)
{
    return s_impensa[args->byte];
}

/**
 * @brief Copies args->byte into an AncoraeCtx and returns the table entry.
 *
 * @note Documented at the declaration in impensa_ancorae_acus.h.
 */
uint8_t mmgr_ancorae_impensa(const AncoraeCfg *args)
{
    return EMBED_CALL(ancorae_impensa, AncoraeCtx, .byte = args->byte);
}
"""


def formatter():
    """The clang-format to run over the emitted tables, or a refusal naming why one is needed."""
    found = shutil.which("clang-format")
    if not found:
        raise SystemExit(
            "gen_ancorae_formae: clang-format is not on PATH. The cost tables are laid out by the "
            "formatter, so without it this writes files that differ from the ones on disk and "
            "`harness.py generated` reports a dirty tree. Install it or put it on PATH."
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
                "gen_ancorae_formae",
                version or found,
                LIB / ".clang-format",
                probe.stderr.strip(),
                "The cost tables are laid out by the formatter",
            )
        )
    return found


def main():
    clang = formatter()
    written = []
    for name, brief, notes, returns, source in PROFILES:
        cost = scale(source())
        rows = "\n".join(
            "    " + ", ".join("%d" % c for c in cost[i : i + 16]) + ("," if i < 240 else "") for i in range(0, 256, 16)
        )
        text = SRC % {
            "name": name,
            "brief": brief,
            "notes": "\n".join(" * @note %s" % one for one in notes),
            "rows": rows,
            "returns": returns % (min(cost), max(cost)),
        }
        path = OUT / ("impensa_ancorae_acus_%s.c" % name)
        path.write_text(text, encoding="utf-8", newline="\n")
        written.append(path)

    # One invocation for all five: the formatter reads .clang-format from the tree above them.
    subprocess.run([clang, "-i"] + [str(one) for one in written], check=True)

    for path in written:
        cost = [line for line in path.read_text(encoding="utf-8").splitlines()]
        print("%-8s %-34s %d lines" % (path.stem.rsplit("_", 1)[-1], path.name, len(cost)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
