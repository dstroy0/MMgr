#!/usr/bin/env python3
# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Extract the Lushootseed of Vi taqʷšəblu Hilbert from ICSNL 1983, following that paper's own
# structure.
#
#   Usage:  python tools/dev_env/Salishan/corpus_script_extraction/extract_hilbert.py
#
# Written for one paper. Poking Fun in Lushootseed is her essay, in English, about humour her
# students kept missing, with forty numbered examples in Lushootseed set into it. Each example is
# printed twice under the same number: the Lushootseed first, then her English for it. So the
# number is the pairing and the order decides which is which, and nothing else has to.
#
# THE ORTHOGRAPHY IS NOT REPAIRED AND THAT IS DELIBERATE.
#
# This is a 1983 typescript and its scan is badly damaged. ʔ arrives as ?, ə arrives as ~ and J and
# G, ʷ arrives as V and v, and her own name is set as VI [!.aq liS'"} blu] lIil bert. The Lyon
# papers had damage like this and it was repaired, but only because a candidate table could be
# tested: applying it moved attested tokens from 1 of 3599 to 811 against Lyon's later papers on the
# same language.
#
# The same test on this paper says nothing. Six modern Lushootseed papers yield 1068 distinct
# Lushootseed tokens between them, and of the 103 damaged tokens here, 0 are attested before a
# candidate mapping and 0 after. Her vocabulary is Raven and Bear and Marblemount and the names of
# houses; theirs is grammar. The reference does not share enough with her to decide anything, which
# is the case font_substitution.py names as the test having said nothing either way.
#
# So the text comes out as it arrived. A guessed table applied to Vi Hilbert's words would put
# spellings into a corpus that nobody said and nothing downstream would ever question them. Marked
# damaged and left alone, the words are still hers and the repair stays available to whoever finds
# a reference that shares her vocabulary.
#
# She asks in the paper that the moral of a story never be explained. That is not this file's to
# keep or break, but it is the reason her essay is kept whole beside the examples rather than
# thrown away as apparatus: the essay is where she says what she is doing and why.

import io
import os
import re
import sys

from salish_marking import DERIVED, SPOKEN, UNCLASSIFIED, rendered, switches, tagged_spans
from salish_unsorted import UNKNOWN_KIND, covered_tokens, unreached, write_unsorted

ROOT = os.path.abspath(__file__)
while (ROOT != os.path.dirname(ROOT)) and not os.path.isdir(os.path.join(ROOT, "build")):
    ROOT = os.path.dirname(ROOT)
PAPERS = os.path.join(ROOT, "build", "papers")
CORPORA = os.path.join(ROOT, "build", "corpora")

SOURCE = os.path.join(PAPERS, "1983_Hilbert.txt")

# <original paper and author>_Salish_<language without accents>_<spoken by>_<year>_<mixed>
TARGET = os.path.join(
    CORPORA,
    "PokingFunInLushootseed_Hilbert"
    "_Salish_lushootseed_Vitaqwshablu-Hilbert_1983_mixed.txt")

PAGE = re.compile(r"^===== page \d+ =====$")

# A marker anywhere in a line, not only at its start, and tolerating the space this typewriter
# sometimes left inside the bracket. Example 5 ends and its English begins on one line:
# qa~qs. tai ?as~awit. (5 ) And he
# Anchored at the start, that put her English into the Lushootseed stream.
NUMBERED = re.compile(r"\((\d{1,3})\s*\)")

# A page number the typescript left on its own line, between an example and its translation.
PAGE_NUMBER = re.compile(r"^\d{1,4}$")

# What the damaged typescript writes the language with. Not the modern orthography: ? is the
# glottal stop here, ~ and J and G are the schwa, V and v are labialization. A test built on ʔ and
# ə finds nothing in this file at all.
MARKS = "?~JG@V%]!"

LAYER = {
    "transcription": SPOKEN,
    "translation": SPOKEN,
    "essay": DERIVED,
    UNCLASSIFIED: DERIVED,
}


def carries_language(text):
    """Whether a line holds a character this typescript writes the language with."""
    return any(mark in text for mark in MARKS)


def examples(lines):
    """The forty numbered examples, each as its Lushootseed and her English for it.

    A number appears twice. The first time it opens the Lushootseed, the second time it opens her
    translation, and each runs on until the next number or a page number interrupts it. Content
    cannot tell the two apart here, because the damage leaves both looking like neither.
    """
    held = {}
    order = []
    taken = set()
    number = None
    which = None
    # Set when a marker opened a Lushootseed slot and carried no text with it. The typescript puts
    # such a marker in the left margin, part way down the previous example's English, and the OCR
    # flattened that into its own line. Example 3's English runs on for two lines after the bare
    # (4) that interrupts it, and those two lines are hers in English, not the start of example 4.
    waiting = False
    previous = None

    def open_number(one):
        if one not in held:
            held[one] = {"said": [], "english": []}
            order.append(one)
            return "said"
        return "english"

    for at, line in enumerate(lines):
        trimmed = " ".join(line.split())
        if not trimmed or PAGE.match(trimmed) or PAGE_NUMBER.match(trimmed):
            continue

        marks = list(NUMBERED.finditer(trimmed))
        if not marks:
            if (number is None) or (which is None):
                continue
            taken.add(at)
            # A line with none of the typescript's marks, while a Lushootseed slot is still
            # waiting for its first line, is the previous example's English running past the
            # marker in the margin.
            if waiting and not carries_language(trimmed) and (previous is not None):
                held[previous]["english"].append(trimmed)
                continue
            if waiting:
                waiting = False
            held[number][which].append(trimmed)
            continue

        # Text before the first marker belongs to whatever was open.
        taken.add(at)
        lead = trimmed[:marks[0].start()].strip()
        if lead and (number is not None) and (which is not None):
            held[number][which].append(lead)
        for index, mark in enumerate(marks):
            if (number is not None) and (which == "english"):
                previous = number
            number = int(mark.group(1))
            which = open_number(number)
            stop = marks[index + 1].start() if (index + 1) < len(marks) else len(trimmed)
            rest = trimmed[mark.end():stop].strip()
            if rest:
                held[number][which].append(rest)
                waiting = False
            else:
                waiting = (which == "said")
    return [(one, held[one]) for one in order], taken


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
    found, taken = examples(lines)
    for number, parts in found:
        said = " ".join(parts["said"])
        english = " ".join(parts["english"])
        if said:
            rows.append(("T", number, "transcription", said))
        if english:
            rows.append(("N", number, "translation", english))

    # Her essay. It is English and it is hers, and it is what the examples are set into, so it is
    # kept and marked derived rather than dropped as apparatus. Told from the examples by which
    # lines they took, not by comparing text: a line matched against a bag of words matches
    # nothing, and every continuation line went into the essay a second time.
    for at, line in enumerate(lines):
        trimmed = " ".join(line.split())
        if not trimmed or PAGE.match(trimmed) or PAGE_NUMBER.match(trimmed) or (at in taken):
            continue
        rows.append(("N", 0, "essay", trimmed))

    missed = unreached(lines, covered_tokens(one[3] for one in rows))
    for page, where, reason, missing, text in missed:
        rows.append(("T", 0, UNCLASSIFIED, text))

    with open(TARGET, "w", encoding="utf-8", newline="") as handle:
        handle.write("# Poking Fun in Lushootseed.\n")
        handle.write("# Vi taqʷšəblu Hilbert, University of Washington. Papers for the\n")
        handle.write("# International Conference on Salish and Neighbouring Languages, 1983.\n")
        handle.write("# Her essay on humour her students kept missing, with forty numbered\n")
        handle.write("# Lushootseed examples and her own English for each.\n")
        handle.write("#\n")
        handle.write("# ORTHOGRAPHY NOT REPAIRED. This is a 1983 typescript and its scan is\n")
        handle.write("# damaged: ? stands for the glottal stop, ~ and J and G for the schwa,\n")
        handle.write("# V and v for labialization. A repair table was tried and could not be\n")
        handle.write("# tested: of 103 damaged tokens, 0 are attested in six modern Lushootseed\n")
        handle.write("# papers before a mapping and 0 after, because her story vocabulary and\n")
        handle.write("# their grammar vocabulary do not meet. An untested table applied to her\n")
        handle.write("# words would put spellings in that nobody said, so none was applied.\n")
        handle.write("#\n")
        handle.write("# Mark is language.layer.kind. T is Lushootseed, N is anything else.\n")
        handle.write("# An example is printed twice under one number, the Lushootseed first and\n")
        handle.write("# her English second, so the number pairs them and the order names them.\n")
        handle.write("line\tkind\tswitches\tcontent\n")
        for mark, number, kind, text in rows:
            # Not span-marked. The damage leaves this paper's Lushootseed in plain ASCII, so the
            # span test reads huy, six, tud and Zilid as English and cuts them out of her own
            # sentence. She does not switch languages inside an example: her English is the second
            # block under the same number, so an example line is one language from end to end.
            content = "%s.%s.%s:{%s}" % (mark, LAYER[kind], kind, text)
            handle.write("line#${%d}\t%s\t0\t%s\n" % (number, kind, content))

    pure = TARGET[:-4] + ".pure.txt"
    kept = 0
    already = set()
    with open(pure, "w", encoding="utf-8", newline="") as handle:
        for mark, number, kind, text in rows:
            if (mark != "T") or (kind != "transcription"):
                continue
            key = " ".join(text.split())
            if not key or (key in already):
                continue
            already.add(key)
            handle.write("%s\n" % key)
            kept += 1

    stuck = TARGET[:-4] + ".unclassifiable.tsv"
    flagged = [(0, "example %d" % number, UNKNOWN_KIND, "", text)
               for mark, number, kind, text in rows if kind == UNCLASSIFIED]
    flagged.extend(missed)
    stuck_count = write_unsorted(stuck, "Poking Fun in Lushootseed", flagged)

    counted = {}
    for mark, number, kind, text in rows:
        counted[kind] = counted.get(kind, 0) + 1
    out.write("  %d lines written to\n  %s\n" % (len(rows), os.path.basename(TARGET)))
    out.write("  %d target-language spans written to\n  %s\n" % (kept, os.path.basename(pure)))
    out.write("  %d lines the tool could not sort written to\n  %s\n"
              % (stuck_count, os.path.basename(stuck)))
    out.write("\n  by kind: %s\n"
              % ", ".join("%s %d" % (one, counted[one]) for one in sorted(counted)))
    out.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
