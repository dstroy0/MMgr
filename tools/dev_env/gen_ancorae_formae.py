#!/usr/bin/env python3
# memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later
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
"""

import math
import pathlib

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
    """map a frequency dict to 1..255 on a log scale; absent bytes get the floor"""
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


PROFILES = [
    (
        "generic",
        "MMGR_IMPENSA_ANCORAE_ACUS_GENERIC",
        generic,
        "No grammar assumed. Printable ASCII is common, control bytes and the high half are not.\n"
        " * This is the default and it is a weak prior on purpose - it should never be badly wrong.",
    ),
    (
        "english",
        "MMGR_IMPENSA_ANCORAE_ACUS_ENGLISH",
        english,
        "English prose. Monogram frequencies from 4.5e9 characters of Wortschatz text\n"
        " * (practicalcryptography.com); space taken at twice E per Wikipedia's letter frequency page.",
    ),
    (
        "uri",
        "MMGR_IMPENSA_ANCORAE_ACUS_URI",
        uri,
        "URIs and URLs. Character classes from RFC 3986, weighted by where they occur: the path\n"
        " * separator dominates, then the host dots, then the query delimiters.",
    ),
    (
        "inet",
        "MMGR_IMPENSA_ANCORAE_ACUS_INET",
        inet,
        "IPv6 text form and dotted quad, RFC 4291. The alphabet is 0-9 a-f A-F : . / % [ ] and\n"
        " * nothing else, so any needle byte outside it is the most selective anchor there is.",
    ),
    (
        "route",
        "MMGR_IMPENSA_ANCORAE_ACUS_ROUTE",
        route,
        "HTTP route patterns. RFC 3986 path-abempty plus the {param} and :param template syntax\n"
        " * routers use. The slash is over half of all structural bytes.",
    ),
]

HDR = """// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef {guard}
#define {guard}

/**
 * @file
 * @brief Anchor cost profile.
 *
 * {doc}
 *
 * Cost is how common a byte is, so the picker takes the minimum. A byte that cannot occur under
 * this profile scores best, because a needle containing one is filtered on the first row.
 *
 * Generated, not hand tuned. See tools/dev_env/gen_ancorae_formae.py.
 */
#define {macro}                                                                                    \\
{rows}

#endif
"""

for name, macro, fn, doc in PROFILES:
    cost = scale(fn())
    rows = []
    for i in range(0, 256, 16):
        line = "    " + ", ".join(f"{c:3d}" for c in cost[i : i + 16]) + ","
        rows.append(line.ljust(97) + "\\")
    rows[-1] = rows[-1].rstrip("\\").rstrip().rstrip(",")
    body = "\n".join(rows)
    guard = f"MMGR_IMPENSA_ANCORAE_ACUS_{name.upper()}_H"
    p = OUT / f"impensa_ancorae_acus_{name}.h"
    p.write_text(HDR.format(guard=guard, doc=doc, macro=macro, rows=body), encoding="utf-8")
    sample = {c: cost[ord(c)] for c in " ./:eqzx"}
    print(f"{name:8} {p.name:26} {sample}  hi=0x{cost[0xE9]:02x}({cost[0xE9]})")
