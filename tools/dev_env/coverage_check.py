#!/usr/bin/env python3
# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Check that every token of the language in a paper reached the file extracted from it.
#
#   Usage:  python tools/dev_env/coverage_check.py
#
# A per-paper extractor is written against one paper's layout and can miss a section without saying so.
# Story 3 of the Garcia narratives was numbered differently from stories 1 and 2 and came back empty.
# Section 4 of the Matthewson paper fell from thirty-four blocks to two because a footnote marker matched
# a heading. The Alexander story sits in subsections and reading the bare numbers returned two appendices
# and none of the narrative. Every one of those was silent.
#
# The check that catches them is a diff. Every token in the source carrying a character of the language
# should appear somewhere in the extraction, and the number that do not should be zero.
#
# Both sides have to be put through the same transformation first, which the first version of this file
# did not do. Some extractors repair the source before writing it: the space the PDF inserted after each
# combining mark is closed, and two papers had a font that wrote plain letters in place of the
# orthography. Comparing a repaired extraction against an unrepaired source reports every correctly
# repaired word as missing, which is what produced 173 false holes in one paper. So the repairs are
# applied to the source lines here before either side is tokenized, and it does not matter that a repair
# is lossy as long as both sides receive it.
#
# What this reports that is not an error: a token is counted as covered if it appears anywhere in the
# extraction, so a form living only in a footnote is found wherever the extractor kept it. What it reports
# that is an error: a form cited in prose that no section captures, an appendix nobody read, and a
# sentence a splitter dropped.

import io
import os
import re
import sys
import unicodedata

from salish_marking import MARKED, PRACTICAL

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PAPERS = os.path.join(ROOT, "build", "papers")
CORPORA = os.path.join(ROOT, "build", "corpora")

SPAN = re.compile(r"\{([^}]*)\}")
PAGE = re.compile(r"^===== page (\d+) =====$")
EDGES = ".,!?;:“”‘’\"'()[]…«»"

# A token carrying any of these is the language. The union of every orthography in the set, since a
# checker that knows one paper's alphabet reports the others as empty.
MARKS = MARKED + PRACTICAL + "̓̔̕ʷ˽"

# The font substitution two of these papers needed, verified in font_substitution.py
FONT = (("ˇx", "x̌"), ("ˇc", "č"), ("ˇs", "š"),
        ("@", "ə"), ("P", "ʔ"), ("ì", "ɬ"), ("Q", "ʕ"))

# The marks whose following space the extraction inserted, closed by the Hall and Phillips reader
JOINING = "̴̡̢̧̨̰̱̮̓̕"

# paper, extraction, and which repairs the extractor applied to the source
PAIRS = (
    ("ICSNL59_Garcia_Hannon_Stacey_final",
     "ThreeGlossedNlekepmxcinNarratives_GarciaHannonStacey"
     "_Salish_nlekepmxcin_Kweltezetkwu-BerniceGarcia_2024_mixed.txt", ()),
    ("HallPhillipsICSNL60",
     "WhenOldOneCreatedTheEarth_HallPhillips"
     "_Salish_nlekepmxcin_BevPhillips_2025_nomixed.txt", ("spaces",)),
    ("ICSNL59_LaFontaine_Janzen_final",
     "FourStoriesByWlwlmelst_LaFontaineJanzen"
     "_Salish_nlekepmxcin_wlwlmelst-MauriceMichell_2024_mixed.txt", ()),
    ("Matthewson_Redan_ICSNL61",
     "Cw7aozKati7Lati7KuNaxwit_MatthewsonRedan"
     "_Salish_statimcets_Kweswapaw-LindaRedan_2026_mixed.txt", ()),
    ("AlexanderDavis_ICSNL61",
     "ITsicwasSQwa7yanakAku7GraveyardValley_AlexanderDavis"
     "_Salish_statimcets_Qwa7yanak-CarlAlexander_2026_mixed.txt", ()),
    ("ICSNL56_DavisJ_2_final-1",
     "MaryGeorgePersonalNarratives_JohnHamiltonDavis"
     "_Salish_ayajuthem_MaryGeorge_2021_mixed.txt", ()),
    ("22-Nater-Bella-Coola-tale-10",
     "ABellaCoolaTale_Nater_Salish_nuxalk_MargaretSiwallace_2015_nomixed.txt", ()),
    ("19-Lyon_ICSNL50_final-78",
     "ThreeOkanaganStoriesAboutPriests_Lyon"
     "_Salish_nsyilxcen_GeorgeLezard-NellieGuitterez-AndrewMcGinnis_2015_nomixed.txt", ("font",)),
    ("2013_Lindley_Lyon",
     "TwelveMoreUpperNicolaOkanaganNarratives_LindleyLyon"
     "_Salish_nsyilxcen_LottieLindley_2013_nomixed.txt", ("font",)),
)


def close_spaces(line):
    """Take out the space the extraction inserted between a consonant's mark and the rest of it."""
    out = []
    for symbol in line:
        if (symbol == " ") and out and (out[-1] in JOINING):
            continue
        out.append(symbol)
    return "".join(out)


def font_repaired(line):
    """Apply the verified substitution for a paper whose font wrote plain letters."""
    for was, becomes in FONT:
        line = line.replace(was, becomes)
    return line


def prepared(line, repairs):
    """One source line put through the same transformation its extractor applied."""
    if "font" in repairs:
        line = font_repaired(line)
    if "spaces" in repairs:
        line = close_spaces(line)
    return line


def marked_tokens(text):
    """Every token holding a character of the language, stripped of surrounding punctuation."""
    held = {}
    for token in text.split():
        plain = token.strip(EDGES)
        if not plain or not any(mark in plain for mark in MARKS):
            continue
        # A token needs a letter in it. St'át'imcets writes the glottal stop as 7, which puts the
        # digit in the marks above, and without this every year, page number and timestamp holding
        # a 7 was counted as a word of the language and then reported missing.
        if not any(symbol.isalpha() for symbol in plain):
            continue
        held[plain] = held.get(plain, 0) + 1
    return held


def source_tokens(path, repairs):
    """The language tokens of a paper, with the page each was first seen on."""
    held = {}
    where = {}
    page = 0
    with open(path, encoding="utf-8", errors="replace") as handle:
        for line in handle:
            found = PAGE.match(line.strip())
            if found:
                page = int(found.group(1))
                continue
            for token, times in marked_tokens(prepared(line, repairs)).items():
                held[token] = held.get(token, 0) + times
                where.setdefault(token, page)
    return held, where


def extracted_tokens(path):
    """The language tokens present anywhere in an extracted file."""
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
    out.write("  %-36s %-9s %-9s %-9s %s\n"
              % ("paper", "in paper", "extracted", "missing", "covered"))

    worst = []
    for stem, name, repairs in PAIRS:
        source = os.path.join(PAPERS, "%s.txt" % stem)
        target = os.path.join(CORPORA, name)
        if not os.path.isfile(source) or not os.path.isfile(target):
            out.write("  %-36s missing a file\n" % stem[:36])
            continue

        held, where = source_tokens(source, repairs)
        got = extracted_tokens(target)
        missing = {token: count for token, count in held.items() if token not in got}
        covered = (100.0 * (len(held) - len(missing)) / len(held)) if held else 0.0
        out.write("  %-36s %-9d %-9d %-9d %.1f%%\n"
                  % (stem[:36], len(held), len(got), len(missing), covered))
        worst.append((len(missing), stem, missing, where))

    out.write("\n  where the missing tokens sit, for the papers that have any\n")
    for count, stem, missing, where in sorted(worst, reverse=True):
        if not count:
            continue
        by_page = {}
        for token in missing:
            by_page.setdefault(where.get(token, 0), []).append(token)
        out.write("\n  %s, %d missing on %d page(s)\n" % (stem, count, len(by_page)))
        for page in sorted(by_page)[:6]:
            shown = by_page[page][:5]
            out.write("    page %-4d %-3d  %s\n"
                      % (page, len(by_page[page]), "  ".join(shown)))
        if len(by_page) > 6:
            out.write("    and %d more page(s)\n" % (len(by_page) - 6))

    clean = [one for one in worst if not one[0]]
    out.write("\n  %d of %d papers have every token of the language accounted for\n"
              % (len(clean), len(worst)))
    out.write("  both sides are put through the same repair first, so a correctly repaired\n")
    out.write("  word is not reported as a hole\n")

    out.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
