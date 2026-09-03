#!/usr/bin/env python3
# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Check that every token of the language in a paper reached the file extracted from it.
#
#   Usage:  python tools/dev_env/coverage_check.py
#
# A per-paper extractor is written against one paper's layout and can miss a section without saying so.
# Story 3 of the Garcia narratives was numbered differently from stories 1 and 2 and came back empty, and
# the only sign of it was a total. A sentence lost inside a paragraph is worse, because the total is only
# one short.
#
# The check that catches both is a diff. Every token in the source that carries a marked character of the
# language should appear somewhere in the extracted file. There is no judgement in it: a token in the
# paper and not in the extraction is a hole, and the number of holes should be zero.
#
# What it will report that is not an error: this test counts a token as covered if it appears anywhere in
# the extraction, so a form appearing only in a footnote, a reference or a section heading is still found
# where the extractor kept it. What it will report that is an error: a form cited in the prose that no
# section captures, an appendix nobody read, and a sentence a splitter dropped.
#
# Nothing is repaired here. This says where to look.

import io
import os
import re
import sys

from salish_marking import MARKED

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PAPERS = os.path.join(ROOT, "build", "papers")
CORPORA = os.path.join(ROOT, "build", "corpora")

# The extracted file writes its text inside braces after a mark
SPAN = re.compile(r"\{([^}]*)\}")

# Punctuation that sits around a token without being part of it
EDGES = ".,!?;:“”‘’\"'()[]…«»"

PAIRS = (
    ("ICSNL59_Garcia_Hannon_Stacey_final",
     "ThreeGlossedNlekepmxcinNarratives_GarciaHannonStacey"
     "_Salish_nlekepmxcin_Kweltezetkwu-BerniceGarcia_2024_mixed.txt"),
    ("HallPhillipsICSNL60",
     "WhenOldOneCreatedTheEarth_HallPhillips"
     "_Salish_nlekepmxcin_BevPhillips_2025_nomixed.txt"),
    ("ICSNL59_LaFontaine_Janzen_final",
     "FourStoriesByWlwlmelst_LaFontaineJanzen"
     "_Salish_nlekepmxcin_wlwlmelst-MauriceMichell_2024_mixed.txt"),
)


def marked_tokens(text):
    """Every token holding a marked character, stripped of the punctuation around it."""
    held = {}
    for token in text.split():
        plain = token.strip(EDGES)
        if not plain:
            continue
        if not any(mark in plain for mark in MARKED):
            continue
        held[plain] = held.get(plain, 0) + 1
    return held


def source_tokens(path):
    """The marked tokens of a paper, with the page each was first seen on."""
    held = {}
    where = {}
    page = 0
    with open(path, encoding="utf-8", errors="replace") as handle:
        for line in handle:
            trimmed = line.strip()
            found = re.match(r"^===== page (\d+) =====$", trimmed)
            if found:
                page = int(found.group(1))
                continue
            for token, times in marked_tokens(trimmed).items():
                held[token] = held.get(token, 0) + times
                where.setdefault(token, page)
    return held, where


def extracted_tokens(path):
    """The marked tokens present anywhere in an extracted file."""
    held = {}
    with open(path, encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if line.startswith("#"):
                continue
            for span in SPAN.findall(line):
                for token, times in marked_tokens(span).items():
                    held[token] = held.get(token, 0) + times
    return held


def main():
    out = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace", newline="")

    for stem, name in PAIRS:
        source = os.path.join(PAPERS, "%s.txt" % stem)
        target = os.path.join(CORPORA, name)
        out.write("\n  %s\n" % stem)
        if not os.path.isfile(source):
            out.write("    no source at %s\n" % source)
            continue
        if not os.path.isfile(target):
            out.write("    no extraction at %s\n" % target)
            continue

        held, where = source_tokens(source)
        got = extracted_tokens(target)
        missing = {token: count for token, count in held.items() if token not in got}

        out.write("    %d distinct marked tokens in the paper, %d in the extraction\n"
                  % (len(held), len(got)))
        out.write("    %d distinct tokens missing, %d occurrences\n"
                  % (len(missing), sum(missing.values())))

        if missing:
            by_page = {}
            for token in missing:
                by_page.setdefault(where.get(token, 0), []).append(token)
            out.write("    where they sit, by page\n")
            for page in sorted(by_page)[:12]:
                shown = by_page[page][:6]
                out.write("      page %-4d %d missing, e.g. %s\n"
                          % (page, len(by_page[page]), "  ".join(shown)))
            if len(by_page) > 12:
                out.write("      and %d more pages\n" % (len(by_page) - 12))

    out.write("\n  a token in the paper and not in the extraction is a hole. Zero is the\n")
    out.write("  only passing number, and a page with many of them is a section nobody read\n")
    out.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
