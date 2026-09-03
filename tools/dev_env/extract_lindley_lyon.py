#!/usr/bin/env python3
# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Extract the Upper Nicola Okanagan of twelve narratives from ICSNL 48, repairing the font substitution
# first.
#
#   Usage:  python tools/dev_env/extract_lindley_lyon.py
#
# Written for one paper. Twelve stories, and unlike the others in this set its subsections are named
# rather than positional: Okanagan, Interlinear gloss, Free translation, Commentary. Three of the twelve
# are three tellings of one story and three more are three tellings of another, each with its own
# commentary, so the section numbers do not line up with the story count and reading them positionally
# would misfile the versions against each other. The subsections are matched on their titles instead.
#
# This paper carries the same font substitution as the other Lyon volume, and the same table was tested
# against it independently: before the mapping, 2 tokens of 4332 were attested in Lyon's later papers on
# this language; after it, 965. That table is applied here and recorded in the output, so a reader knows
# the text passed through a transformation and can check the mapping rather than trust it.
#
# The appendix holds a note on transcription and glossing practice and two pronominal paradigm tables.
# Those are the language and they are not narrative, so they are kept and marked derived.

import io
import os
import re
import sys

from salish_marking import (DERIVED, MARKED, SPOKEN, UNCLASSIFIED, is_mixed, rendered,
                            switches, tagged_spans)
from salish_unsorted import UNKNOWN_KIND, covered_tokens, unreached, write_unsorted

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PAPERS = os.path.join(ROOT, "build", "papers")
CORPORA = os.path.join(ROOT, "build", "corpora")

SOURCE = os.path.join(PAPERS, "2013_Lindley_Lyon.txt")

# <original paper and author>_Salish_<language without accents>_<spoken by>_<year>_<mixed>
TARGET = os.path.join(
    CORPORA,
    "TwelveMoreUpperNicolaOkanaganNarratives_LindleyLyon"
    "_Salish_nsyilxcen_LottieLindley_2013_nomixed.txt")

# Verified by font_substitution.py against LyonICSNL60_Inch-2 on this file: 2 of 4332 attested
# before, 965 after. The caron pairs go first, since this font writes the caron as its own
# character ahead of the letter it belongs to.
REPAIR = (("ˇx", "x̌"), ("ˇc", "č"), ("ˇs", "š"),
          ("@", "ə"), ("P", "ʔ"), ("ì", "ɬ"), ("Q", "ʕ"))

MARKS = MARKED + "ʷ̓’ʼ"

PAGE = re.compile(r"^===== page \d+ =====$")
HEADING = re.compile(r"^(\d{1,2}(?:\.\d+)*)\s+(\S.*)$")
DOTTED = re.compile(r"\.\s*\.\s*\.")
NUMBERED_BLOCK = re.compile(r"^\((\d{1,4})\)\s*(.*)$")
QUOTED = re.compile(r"^['‘“]")

# A segmentation line joins its morphemes with a hyphen or an equals sign, and neither character is
# touched by the font repair below. Testing for one is what keeps a wrapped transcription line from
# entering the record as segmentation because nothing else matched it.
SEGMENTED = re.compile(r"[-=]")

CATEGORIES = re.compile(
    r"\b(?:ABS|APPL|AUT|C1C2|C1|C2|CAUS|CHAR|CISL|CONJ|CUST|DEON|DEV|DIM|DIR|DRV|DUB|EMPH|"
    r"EPIS|EVID|INCEPT|INCH|INDEP|INTERJ|INT|LC|LOC|MID|OCC|RED|STAT|UPOSS|FUT|IMP|"
    r"DET|DEM|ERG|OBJ|POSS|PL|SG|SBJ|NOM|OBL|IPFV|NEG|TR|1SG|2SG|3SG|1PL|2PL|3PL)\b")

# What each named subsection holds. Matched on the title because the numbering does not line up
# with the story count once the multiple versions and their commentaries are counted.
def kind_of(title):
    """What a subsection holds, from its own heading."""
    lowered = title.lower()
    if lowered.startswith("okanagan"):
        return "running speech"
    if "interlinear" in lowered:
        return "interlinear"
    if "free translation" in lowered:
        return "free translation"
    if "commentary" in lowered:
        return "commentary"
    if "paradigm" in lowered or "transcription" in lowered or "abbreviation" in lowered:
        return "appendix"
    return None


LAYER = {
    "running speech": SPOKEN,
    "transcription": SPOKEN,
    "translation": SPOKEN,
    "free translation": SPOKEN,
    "segmentation": DERIVED,
    "gloss": DERIVED,
    "commentary": DERIVED,
    "appendix": DERIVED,
    # Kept and marked, held out of the ingestion stream until someone has classified it.
    UNCLASSIFIED: DERIVED,
}


def repaired(text):
    """One line with the verified font substitution applied."""
    for was, becomes in REPAIR:
        text = text.replace(was, becomes)
    return text


def carries_language(text):
    """Whether a line holds any character this paper writes the language with."""
    return any(mark in text for mark in MARKS)


def looks_heading(trimmed):
    """A numbered heading, told apart from a contents entry and from a numbered example."""
    if NUMBERED_BLOCK.match(trimmed) or DOTTED.search(trimmed):
        return None
    found = HEADING.match(trimmed)
    if not found:
        return None
    if len(trimmed) > 78 or trimmed.endswith("."):
        return None
    return found.group(1), found.group(2).strip()


def main():
    out = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace", newline="")
    os.makedirs(CORPORA, exist_ok=True)
    if not os.path.isfile(SOURCE):
        out.write("  no %s\n" % SOURCE)
        out.flush()
        return 1

    with open(SOURCE, encoding="utf-8", errors="replace") as handle:
        lines = [one.rstrip("\n") for one in handle]

    rows = []
    stories = {}
    section = None
    story = None
    holds = None
    number = None
    opening = []

    for line in lines:
        trimmed = " ".join(line.split())
        if PAGE.match(trimmed) or not trimmed:
            continue
        if trimmed.startswith("References"):
            section = None
            holds = None
            continue

        opened = looks_heading(trimmed)
        if opened:
            section, title = opened
            story = section.split(".")[0]
            number = None
            if "." not in section:
                stories[story] = repaired(title)
                holds = None
            else:
                holds = kind_of(title)
            continue

        if section is None:
            if not stories:
                opening.append(trimmed)
            continue

        if holds is None:
            continue

        fixed = repaired(trimmed)
        name = stories.get(story, "")

        if holds == "running speech":
            if carries_language(fixed):
                rows.append(("T", 0, section, name, "running speech", fixed))
            continue

        if holds in ("free translation", "commentary", "appendix"):
            mark = "T" if (holds == "appendix" and carries_language(fixed)) else "N"
            rows.append((mark, 0, section, name, holds, fixed))
            continue

        found = NUMBERED_BLOCK.match(fixed)
        if found:
            number = int(found.group(1))
            rest = found.group(2).strip()
            if rest:
                rows.append(("T", number, section, name, "transcription", rest))
            continue
        if number is None:
            continue
        if QUOTED.match(fixed):
            rows.append(("N", number, section, name, "translation", fixed))
        elif CATEGORIES.search(fixed):
            rows.append(("N", number, section, name, "gloss", fixed))
        elif carries_language(fixed) and SEGMENTED.search(fixed):
            rows.append(("T", number, section, name, "segmentation", fixed))
        elif carries_language(fixed) and not is_mixed(fixed, MARKS):
            # Carries the language, holds no running English, and nothing typed it. This branch
            # used to be absent, so such a line left without a word. The English test is here
            # because the repair below turns Pierre into ʔierre and Quilchena into ʕuilchena, so a
            # test on the marked characters alone reads a line of Lyon's prose as the language.
            rows.append(("T", number, section, name, UNCLASSIFIED, fixed))

    with open(TARGET, "w", encoding="utf-8", newline="") as handle:
        handle.write("# 12 more Upper Nicola Okanagan narratives.\n")
        handle.write("# Nsyilxcən, Upper Nicola. Lottie Lindley and John Lyon.\n")
        handle.write("# Papers for the International Conference on Salish and Neighbouring\n")
        handle.write("# Languages, UBCWPL, 2013.\n")
        handle.write("# Three of the twelve are three tellings of one story and three more are\n")
        handle.write("# three tellings of another, each with its own commentary.\n")
        handle.write("#\n")
        handle.write("# FONT REPAIRED. This paper's PDF substituted plain letters for the\n")
        handle.write("# orthography. The mapping below was verified against Lyon's later papers on\n")
        handle.write("# the same language, moving attested tokens from 2 of 4332 to 965.\n")
        for was, becomes in REPAIR:
            handle.write("#   %s -> %s\n" % (was, becomes))
        handle.write("#\n")
        handle.write("# Mark is language.layer.kind. T is Nsyilxcən, N is anything else.\n")
        handle.write("# Subsections are matched on their titles, not their numbers, because the\n")
        handle.write("# numbering does not line up with the story count once the versions and\n")
        handle.write("# their commentaries are counted.\n")
        handle.write("line\tsection\tstory\tkind\tswitches\tcontent\n")
        for mark, count, sect, name, kind, text in rows:
            layer = LAYER[kind]
            if mark == "T":
                content = rendered(text, layer, kind, MARKS)
                crossings = switches(text)
            else:
                content = "N.%s.%s:{%s}" % (layer, kind, text)
                crossings = 0
            handle.write("line#${%d}\t%s\t%s\t%s\t%d\t%s\n"
                         % (count, sect, name[:40], kind, crossings, content))

    pure = TARGET[:-4] + ".pure.txt"
    kept = 0
    repeated = 0
    already = set()
    with open(pure, "w", encoding="utf-8", newline="") as handle:
        for mark, count, sect, name, kind, text in rows:
            if (mark != "T") or (LAYER[kind] != SPOKEN):
                continue
            for span, run in tagged_spans(text, MARKS):
                if (span != "T") or (not run.strip()):
                    continue
                key = " ".join(run.split())
                if key in already:
                    repeated += 1
                    continue
                already.add(key)
                handle.write("%s\n" % key)
                kept += 1

    # A file of its own for what the tool could not sort: an interlinear line none of the tests
    # typed, and a line no subsection reached. The source is put through the same font repair
    # before comparing, so a correctly repaired word is not reported as a hole.
    stuck = TARGET[:-4] + ".unclassifiable.tsv"
    flagged = [(0, "%s block %d" % (sect, count), UNKNOWN_KIND, "", text)
               for mark, count, sect, name, kind, text in rows if kind == UNCLASSIFIED]
    flagged.extend(unreached(lines, covered_tokens(one[5] for one in rows),
                             repair=repaired, marks=MARKS))
    stuck_count = write_unsorted(stuck, "12 more Upper Nicola Okanagan narratives", flagged)

    out.write("  %d lines written to\n  %s\n" % (len(rows), os.path.basename(TARGET)))
    out.write("  %d target-language spans written to\n  %s\n" % (kept, os.path.basename(pure)))
    out.write("  %d spans skipped as already written\n" % repeated)
    out.write("  %d lines the tool could not sort written to\n  %s\n"
              % (stuck_count, os.path.basename(stuck)))

    out.write("\n  %-40s %s\n" % ("story", "transcriptions"))
    counted = {}
    for mark, count, sect, name, kind, text in rows:
        if kind == "transcription":
            counted[name] = counted.get(name, 0) + 1
    for name in sorted(counted):
        out.write("  %-40s %d\n" % (name[:40], counted[name]))

    kinds = {}
    for mark, count, sect, name, kind, text in rows:
        kinds[kind] = kinds.get(kind, 0) + 1
    out.write("\n  by kind: %s\n" % ", ".join("%s %d" % (one, kinds[one]) for one in sorted(kinds)))
    out.write("  %d stories found\n" % len(stories))
    if opening:
        out.write("\n  the paper's opening, which names its speaker\n    %s\n"
                  % " ".join(opening)[:220])

    out.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
