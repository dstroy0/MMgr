#!/usr/bin/env python3
# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Extract the nłeʔkepmxcín of wlwlmelst (Maurice Michell) from ICSNL 59, following that paper's own
# structure and its own glossing categories.
#
#   Usage:  python tools/dev_env/extract_lafontaine_janzen.py
#
# Written for one paper, and it is laid out unlike either of the other two. Each of the four stories is a
# running paragraph with no sentence numbering at all, followed by numbered interlinear blocks. The blocks
# are three lines deep and wrap, so one example carries several transcription, segmentation and gloss
# lines before its translation arrives.
#
# This paper writes ł where the others write ɬ. Carrying only one of those makes every token here
# invisible to the marking and to any check built on it, which is why salish_marking holds both.
#
# Two parts of this paper are the language and are not transcript. Section 2 cites forms directly while
# discussing dialect, and the appendix is a full morpheme inventory with a gloss and a meaning for each
# entry. Both are kept and marked for what they are, because a token of this language that appears in the
# paper and not in the extraction is a hole, and the coverage check counts every one of them.
#
# wlwlmelst transcribed and translated these stories himself, so the translations are his and are marked
# spoken. The segmentation normalizes morphemes to underlying forms and the gloss is written in category
# labels, so neither records anything uttered.
#
# These are words of wisdom passed to wlwlmelst by his mother nxwelinek and his grandmother ʔústko, and he
# shares them freely for people connecting with the language. That is recorded in the output.

import io
import os
import re
import sys

from salish_marking import DERIVED, MARKED, SPOKEN, rendered, switches, tagged_spans

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PAPERS = os.path.join(ROOT, "build", "papers")
CORPORA = os.path.join(ROOT, "build", "corpora")

SOURCE = os.path.join(PAPERS, "ICSNL59_LaFontaine_Janzen_final.txt")

# <original paper and author>_Salish_<language without accents>_<spoken by>_<year>_<mixed>
TARGET = os.path.join(
    CORPORA,
    "FourStoriesByWlwlmelst_LaFontaineJanzen"
    "_Salish_nlekepmxcin_wlwlmelst-MauriceMichell_2024_mixed.txt")

PAGE = re.compile(r"^===== page \d+ =====$")
HEADING = re.compile(r"^(\d+(?:\.\d+)?)\s+(\S.*)$")
NUMBERED_BLOCK = re.compile(r"^\((\d{1,3})\)\s*(.*)$")
APPENDIX = re.compile(r"^Appendix", re.IGNORECASE)
QUOTED = re.compile(r"^['‘“]")

# The category labels this paper defines in its appendix and uses on its gloss line
CATEGORIES = re.compile(
    r"\b(?:ACCM|ACHV|AFF|AGENT|AT|AUG|AUT|AUX|CAUSE|CHR|CTST|DIM|DIR|DRV|DSCR|EMPH|EP|"
    r"EST\.CTX|FMV|FUT|IDF|IM|IMP|INC|INS|INT|LCL|LIG|MDL|NEG|NOM|OBL|PART\.CTX|PER|PTZG|"
    r"QLT|RFL|RFM|RPRT|RSL|SPZG|ST|TR|UNR|1SG|2SG|3SG|1PL|2PL|3PL|1\.SBJ|2\.SBJ|3\.SBJ|"
    r"1\.POSS|3\.POSS|3\.INTR|1PL\.OBJ|1PL\.SBJ|1PL\.POSS|1PL\.INTR|2\.CJV|EMPH\.INT)\b")

STORIES = {
    "3.1": "sptekwlcms l nskixzeʔ, A Story My Mother Told Me",
    "3.2": "kz̓e ʔústko, Grandmother Ustko",
    "3.3": "cúnsm ł nskíxzeʔ, Mom told me",
    "3.4": "nqʷincutn kt, Our language",
}

LAYER = {
    "running speech": SPOKEN,
    "transcription": SPOKEN,
    "translation": SPOKEN,
    "segmentation": DERIVED,
    "gloss": DERIVED,
    "cited form": DERIVED,
    "morpheme entry": DERIVED,
}


def carries_language(text):
    """Whether a line holds any of the marked characters this language is written with."""
    return any(mark in text for mark in MARKED)


def sectioned(lines):
    """The paper's numbered sections and its appendix, as the lines under each."""
    held = {}
    current = None
    for line in lines:
        trimmed = line.strip()
        if PAGE.match(trimmed):
            continue
        if APPENDIX.match(trimmed):
            current = "appendix"
            held.setdefault(current, [])
            continue
        found = HEADING.match(trimmed)
        if found and not NUMBERED_BLOCK.match(trimmed):
            current = found.group(1)
            held.setdefault(current, [])
            continue
        if current is not None:
            held.setdefault(current, []).append(line.rstrip("\n"))
    return held


def running_and_blocks(lines):
    """A story section: the paragraph before the first numbered block, then the blocks."""
    running = []
    blocks = []
    number = None
    building = []
    started = False

    def close():
        if (number is not None) and building:
            blocks.append((number, list(building)))
        building.clear()

    for line in lines:
        trimmed = line.strip()
        if not trimmed:
            continue
        found = NUMBERED_BLOCK.match(trimmed)
        if found:
            close()
            started = True
            number = int(found.group(1))
            rest = " ".join(found.group(2).split())
            if rest:
                building.append(rest)
            continue
        if started:
            building.append(" ".join(trimmed.split()))
        else:
            running.append(" ".join(trimmed.split()))
    close()
    return " ".join(running), blocks


def main():
    out = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace", newline="")
    os.makedirs(CORPORA, exist_ok=True)
    if not os.path.isfile(SOURCE):
        out.write("  no %s\n" % SOURCE)
        out.flush()
        return 1

    with open(SOURCE, encoding="utf-8", errors="replace") as handle:
        lines = handle.read().splitlines()

    held = sectioned(lines)
    rows = []

    for number in sorted(STORIES):
        under = held.get(number)
        if not under:
            continue
        story = STORIES[number]
        running, blocks = running_and_blocks(under)
        if running:
            rows.append(("T", 0, story, number, "running speech", running))
        for count, block in blocks:
            first = True
            for one in block:
                if QUOTED.match(one):
                    rows.append(("N", count, story, number, "translation", one))
                elif CATEGORIES.search(one):
                    rows.append(("N", count, story, number, "gloss", one))
                elif first and carries_language(one):
                    rows.append(("T", count, story, number, "transcription", one))
                    first = False
                elif carries_language(one):
                    rows.append(("T", count, story, number, "segmentation", one))
                else:
                    rows.append(("N", count, story, number, "gloss", one))

    # Section 2 cites forms while discussing dialect. They are this language and belong in the file.
    for one in held.get("2", []):
        trimmed = " ".join(one.split())
        if trimmed and carries_language(trimmed) and not CATEGORIES.search(trimmed):
            rows.append(("T", 0, "story traits", "2", "cited form", trimmed))

    # The appendix is a morpheme inventory: one morpheme, its gloss, and its meaning per row.
    for one in held.get("appendix", []):
        trimmed = " ".join(one.split())
        if not trimmed:
            continue
        if carries_language(trimmed) or re.match(r"^-?[A-Za-zʔə]{1,8}-?\s+[A-Z]", trimmed):
            rows.append(("T", 0, "glossing terms", "appendix", "morpheme entry", trimmed))

    with open(TARGET, "w", encoding="utf-8", newline="") as handle:
        handle.write("# Four Stories by wlwlmelst.\n")
        handle.write("# Written, transcribed and translated by wlwlmelst (Maurice Michell), a\n")
        handle.write("# speaker of the Southern yutémkt dialect of nłeʔkepmxcín. With Jade\n")
        handle.write("# LaFontaine and Jonathan Janzen. Papers for the International Conference\n")
        handle.write("# on Salish and Neighbouring Languages 59, UBCWPL, 2024.\n")
        handle.write("# These stories were passed to wlwlmelst by his mother nxwelinek and his\n")
        handle.write("# grandmother ʔústko, and he shares them freely for those connecting with\n")
        handle.write("# the language.\n")
        handle.write("#\n")
        handle.write("# Mark is language.layer.kind. T is the target language, N is anything else.\n")
        handle.write("# spoken is what was said or written by him, including his own translations.\n")
        handle.write("# derived is worked out from it: segmentation normalizes to underlying forms\n")
        handle.write("# and the gloss is category labels, so neither records anything uttered.\n")
        handle.write("# Gloss categories are the paper's own, from its appendix, unchanged.\n")
        handle.write("line\tstory\tsection\tswitches\tcontent\n")
        for mark, count, story, number, kind, text in rows:
            layer = LAYER[kind]
            if mark == "T":
                content = rendered(text, layer, kind)
                crossings = switches(text)
            else:
                content = "N.%s.%s:{%s}" % (layer, kind, text)
                crossings = 0
            handle.write("line#${%d}\t%s\t%s\t%d\t%s\n"
                         % (count, story, number, crossings, content))

    pure = TARGET[:-4] + ".pure.txt"
    kept = 0
    repeated = 0
    already = set()
    with open(pure, "w", encoding="utf-8", newline="") as handle:
        for mark, count, story, number, kind, text in rows:
            if (mark != "T") or (LAYER[kind] != SPOKEN):
                continue
            for span, run in tagged_spans(text):
                if (span != "T") or (not run.strip()):
                    continue
                key = " ".join(run.split())
                if key in already:
                    repeated += 1
                    continue
                already.add(key)
                handle.write("%s\n" % run)
                kept += 1

    out.write("  %d lines written to\n  %s\n" % (len(rows), os.path.basename(TARGET)))
    out.write("  %d target-language spans written to\n  %s\n" % (kept, os.path.basename(pure)))
    out.write("  %d spans skipped as already written\n" % repeated)

    out.write("\n  %-38s %-10s %-16s %s\n" % ("story", "section", "kind", "lines"))
    counted = {}
    for mark, count, story, number, kind, text in rows:
        counted[(story, number, kind)] = counted.get((story, number, kind), 0) + 1
    for key in sorted(counted):
        out.write("  %-38s %-10s %-16s %d\n" % (key[0][:38], key[1], key[2], counted[key]))

    marks = {}
    for mark, count, story, number, kind, text in rows:
        marks[mark] = marks.get(mark, 0) + 1
    mixed = sum(1 for row in rows
                if (row[0] == "T") and (LAYER[row[4]] == SPOKEN)
                and any(one == "N" for one, run in tagged_spans(row[5])))
    out.write("\n  T lines %d, N lines %d, spoken lines he mixed %d\n"
              % (marks.get("T", 0), marks.get("N", 0), mixed))
    missing = [one for one in sorted(STORIES) if not held.get(one)]
    out.write("  stories the paper has that came back empty: %s\n"
              % (", ".join(missing) if missing else "none"))

    out.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
