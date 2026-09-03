#!/usr/bin/env python3
# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Extract the Okanagan of three speakers from ICSNL 50, repairing the font substitution first.
#
#   Usage:  python tools/dev_env/extract_lyon_priests.py
#
# Written for one paper, and this is the only one so far whose characters had to be repaired before it
# could be read at all. Its font wrote plain letters in place of the orthography, so the text arrives as
# iP naPì ʼqwQaylqs where it should read iʔ naʔɬ ʼqwʕaylqs.
#
# The mapping was tested, not assumed. font_substitution.py applies a candidate table to the damaged
# tokens and counts how many become forms attested in Lyon's recent papers on the same language, whose
# extraction kept its characters. Before the mapping, 1 token of 3599 was attested. After it, 811. The
# same table tested on the other damaged Lyon paper moved 2 of 4332 to 965. A wrong mapping cannot do
# that, because a wrong substitution produces strings the language does not contain.
#
# What the table does not cover is recorded with it. The caron entries are written by this font as a
# separate character before their letter, so x̌ arrives as ˇx, and the order of replacement matters.
#
# Three speakers, and the permissions are named in the paper. Conversation with the priest was told by
# George Lezard of the Penticton Indian Reserve in 1966 when he was eighty-five, recorded by Randy
# Bouchard and transcribed by Larry Pierre in 1970; Lyon updated that transcription with the permission of
# Arnie Baptiste, Larry Pierre's son. Smokey and the priest is Nellie's, reprinted with the permission of
# her great-granddaughter Lynne Jorgesen of the Upper Nicola Indian Band. The third story's teller is
# named in its own introduction, which this reports so the attribution comes from the paper.

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

SOURCE = os.path.join(PAPERS, "19-Lyon_ICSNL50_final-78.txt")

# <original paper and author>_Salish_<language without accents>_<spoken by>_<year>_<mixed>
TARGET = os.path.join(
    CORPORA,
    "ThreeOkanaganStoriesAboutPriests_Lyon"
    "_Salish_nsyilxcen_GeorgeLezard-NellieGuitterez-AndrewMcGinnis_2015_nomixed.txt")

# Verified by font_substitution.py against LyonICSNL60_Inch-2. The caron pairs are replaced first
# because this font writes the caron as its own character ahead of the letter it belongs to.
REPAIR = (("ˇx", "x̌"), ("ˇc", "č"), ("ˇs", "š"),
          ("@", "ə"), ("P", "ʔ"), ("ì", "ɬ"), ("Q", "ʕ"))

MARKS = MARKED + "ʷ̓’ʼ"

PAGE = re.compile(r"^===== page \d+ =====$")
HEADING = re.compile(r"^(\d(?:\.\d)?)\s+(\S.*)$")
DOTTED = re.compile(r"\.\s*\.\s*\.")
NUMBERED_BLOCK = re.compile(r"^\((\d{1,4})\)\s*(.*)$")
QUOTED = re.compile(r"^['‘“]")

# A segmentation line joins its morphemes with a hyphen or an equals sign, and neither character is
# touched by the font repair below. Testing for one is what keeps a wrapped transcription line or a
# page number from entering the record as segmentation because nothing else matched it.
SEGMENTED = re.compile(r"[-=]")

# The extraction ran the first two columns of a word together wherever the segmentation opens at
# the root, giving ’qwQaylqs√ ’qwQay=lqs on one line. A segmentation of its own can also hold √,
# as in-ks-√ ’ma ’y-ìt-ím does, and what tells the two apart is what stands before the root: a
# merged line has a bare word there, a segmentation has morpheme separators.
MERGED = re.compile(r"^([^-=•√]+?)\s*(√.*)$")

# A page number survives inside a block and would put the four-line cycle out of step by one for
# every word after it. Nothing in this orthography is written with digits alone.
PAGE_NUMBER = re.compile(r"^\d{1,4}$")

CATEGORIES = re.compile(
    r"\b(?:ABS|APPL|AUT|C1C2|C1|C2|CAUS|CHAR|CISL|CONJ|CUST|DEON|DEV|DIM|DIR|DRV|DUB|EMPH|"
    r"EPIS|EVID|INCEPT|INCH|INDEP|INTERJ|INT|LC|LOC|MID|OCC|RED|STAT|UPOSS|"
    r"DET|DEM|ERG|OBJ|POSS|PL|SG|SBJ|NOM|OBL|IPFV|NEG|TR|1SG|2SG|3SG|1PL|2PL|3PL)\b")

# Taken from each story's own introduction, which names its teller and the recording
SPEAKER = {
    "1": "George Lezard, Penticton Indian Reserve, told 1966",
    "2": "Nellie Guitterez, Upper Nicola Indian Band, told 1978 or 1979",
    "3": "Kiláwnaʔ (Andrew McGinnis), Penticton Indian Reserve, told 9 October 2014",
}

LAYER = {
    "running speech": SPOKEN,
    "transcription": SPOKEN,
    "translation": SPOKEN,
    "segmentation": DERIVED,
    "gloss": DERIVED,
    "word gloss": DERIVED,
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


def four_line_words(block):
    """One numbered block of the interlinear, as the four lines the paper gives for each word.

    The PDF handed this paper's interlinear over a column at a time, so a word arrives as four
    consecutive lines: the word as spoken, its segmentation, its gloss, and an English word for it.
    Position in that cycle is what says which line is which. Content cannot say it, because a short
    word written in plain letters is spelled the same on the first two lines and carries nothing to
    test: block 1 opens with p, then p again, and only the order separates the two.

    Two things break the cycle and both are handled. Where the extraction ran the first two columns
    together the line fills both positions at once, and a page number sitting inside a block is
    passed over rather than counted, since counting it puts every word after it out of step.

    Returns the words, the free translation that closes the block, and any line left over.
    """
    words = []
    translation = None
    leftover = []
    slot = 0
    holding = [None, None, None, None]

    def close():
        if any(one is not None for one in holding):
            words.append(tuple(holding))
        holding[0] = holding[1] = holding[2] = holding[3] = None

    for line in block:
        if PAGE_NUMBER.match(line):
            continue
        if QUOTED.match(line):
            close()
            slot = 0
            translation = line
            continue
        if translation is not None:
            leftover.append(line)
            continue
        if slot == 0:
            found = MERGED.match(line)
            if found:
                holding[0] = found.group(1)
                holding[1] = found.group(2)
                slot = 2
                continue
        holding[slot] = line
        slot += 1
        if slot == 4:
            close()
            slot = 0
    close()
    return words, translation, leftover


def looks_heading(trimmed):
    """A numbered heading, told apart from the contents listing and from a numbered example."""
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
    blocks = []
    intros = {}
    section = None
    story = None
    number = None
    seen_heading = set()

    for line in lines:
        trimmed = " ".join(line.split())
        if PAGE.match(trimmed) or not trimmed:
            continue

        opened = looks_heading(trimmed)
        if opened:
            number_of, title = opened
            # The contents list repeats every heading before the body, so a top-level heading
            # opens its section on its second appearance. Subsection entries in that list are
            # padded with dot leaders and are already refused above, so they never register as
            # seen, and skipping their first appearance discarded every one of them.
            if "." not in number_of:
                if number_of not in seen_heading:
                    seen_heading.add(number_of)
                    continue
            section = number_of
            story = number_of.split(".")[0]
            number = None
            continue

        if section is None:
            continue

        who = SPEAKER.get(story, "")

        if "." not in section:
            # Prose introducing a story, which is where the paper names its teller
            intros.setdefault(section, []).append(trimmed)
            continue

        fixed = repaired(trimmed)

        if section.endswith(".1"):
            if carries_language(fixed):
                rows.append(("T", 0, section, "running speech", who, fixed))
            continue

        # A block is gathered whole and read afterward. Its lines cannot be typed one at a time,
        # because which column a line belongs to is fixed by its position in the block and not by
        # anything the line itself holds.
        found = NUMBERED_BLOCK.match(fixed)
        if found:
            number = int(found.group(1))
            rest = found.group(2).strip()
            blocks.append((section, number, who, [rest] if rest else []))
            continue
        if number is None:
            continue
        blocks[-1][3].append(fixed)

    # Each block read back as words. The sentence is the first column joined in order, and it is
    # the only part of a block that was spoken: the other three columns are Lyon's analysis of it.
    for section, number, who, block in blocks:
        words, translation, leftover = four_line_words(block)
        said = " ".join(one[0] for one in words if one[0])
        if said:
            rows.append(("T", number, section, "transcription", who, said))
        for one in words:
            if one[1]:
                rows.append(("T", number, section, "segmentation", who, one[1]))
            if one[2]:
                rows.append(("N", number, section, "gloss", who, one[2]))
            if one[3]:
                rows.append(("N", number, section, "word gloss", who, one[3]))
        if translation:
            rows.append(("N", number, section, "translation", who, translation))
        for one in leftover:
            # What sits after the free translation is a footnote or a page artifact. Only a line
            # carrying the language with no running English in it is worth flagging.
            if carries_language(one) and not is_mixed(one, MARKS):
                rows.append(("T", number, section, UNCLASSIFIED, who, one))

    with open(TARGET, "w", encoding="utf-8", newline="") as handle:
        handle.write("# Three Okanagan stories about priests. John Lyon, Simon Fraser University.\n")
        handle.write("# Okanagan, also called Nsyílxcən, Colville-Okanagan and Nqílxwcən, a\n")
        handle.write("# southern Interior Salish language. Three different fluent speakers.\n")
        handle.write("# Papers for the International Conference on Salish and Neighbouring\n")
        handle.write("# Languages 50, UBCWPL 40, 2015.\n")
        handle.write("# Conversation with the priest: George Lezard, Penticton Indian Reserve,\n")
        handle.write("# told 1966 at eighty-five, recorded by Randy Bouchard, transcribed by Larry\n")
        handle.write("# Pierre 1970, updated by permission of Arnie Baptiste, his son.\n")
        handle.write("# Smokey and the priest: Nellie, reprinted by permission of her\n")
        handle.write("# great-granddaughter Lynne Jorgesen, Upper Nicola Indian Band.\n")
        handle.write("#\n")
        handle.write("# FONT REPAIRED. This paper's PDF substituted plain letters for the\n")
        handle.write("# orthography. The mapping below was verified against Lyon's later papers on\n")
        handle.write("# the same language, moving attested tokens from 1 of 3599 to 811.\n")
        for was, becomes in REPAIR:
            handle.write("#   %s -> %s\n" % (was, becomes))
        handle.write("#\n")
        handle.write("# Mark is language.layer.kind. T is Okanagan, N is anything else.\n")
        handle.write("# Gloss categories are the paper's own, from its first footnote, unchanged.\n")
        handle.write("line\tsection\tkind\tspeaker\tswitches\tcontent\n")
        for mark, count, sect, kind, who, text in rows:
            layer = LAYER[kind]
            if mark == "T":
                content = rendered(text, layer, kind, MARKS)
                crossings = switches(text)
            else:
                content = "N.%s.%s:{%s}" % (layer, kind, text)
                crossings = 0
            handle.write("line#${%d}\t%s\t%s\t%s\t%d\t%s\n"
                         % (count, sect, kind, who, crossings, content))

    pure = TARGET[:-4] + ".pure.txt"
    kept = 0
    repeated = 0
    already = set()
    with open(pure, "w", encoding="utf-8", newline="") as handle:
        for mark, count, sect, kind, who, text in rows:
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

    # A file of its own for what the tool could not sort: a line inside a story that none of the
    # tests typed, and a line no section reached, which here is the front matter and the prose
    # introducing each story. The source is put through the same font repair before comparing, so
    # a correctly repaired word is not reported. What that repair does to English is reported: it
    # turns Pierre into ʔierre and Quilchena into ʕuilchena, and those arrive here as unreached.
    stuck = TARGET[:-4] + ".unclassifiable.tsv"
    flagged = [(0, "%s block %d" % (sect, count), UNKNOWN_KIND, "", text)
               for mark, count, sect, kind, who, text in rows if kind == UNCLASSIFIED]
    flagged.extend(unreached(lines, covered_tokens(one[5] for one in rows),
                             repair=repaired, marks=MARKS))
    stuck_count = write_unsorted(stuck, "Three Okanagan stories about priests", flagged)

    out.write("  %d lines written to\n  %s\n" % (len(rows), os.path.basename(TARGET)))
    out.write("  %d target-language spans written to\n  %s\n" % (kept, os.path.basename(pure)))
    out.write("  %d spans skipped as already written\n" % repeated)
    out.write("  %d lines the tool could not sort written to\n  %s\n"
              % (stuck_count, os.path.basename(stuck)))

    counted = {}
    for mark, count, sect, kind, who, text in rows:
        counted[(sect, kind)] = counted.get((sect, kind), 0) + 1
    out.write("\n  %-8s %-18s %s\n" % ("section", "kind", "lines"))
    for key in sorted(counted):
        out.write("  %-8s %-18s %d\n" % (key[0], key[1], counted[key]))

    out.write("\n  the opening prose of each story, which is where the teller is named\n")
    for section in sorted(intros):
        text = " ".join(intros[section])
        out.write("    section %s: %s\n" % (section, text[:190]))

    out.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
