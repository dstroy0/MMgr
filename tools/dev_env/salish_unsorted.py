#!/usr/bin/env python3
# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# The flag file every Salish extractor writes beside its output.
#
#   Usage:  from salish_unsorted import unreached, write_unsorted
#
# An extractor is written against one paper's layout and meets lines that layout does not account for.
# There are two ways that happens. A line reaches the classifier and none of its tests fire, which is what
# a wrapped phonetic line or a form cited inline in a note does. Or a line is never reached at all, because
# it sits in front matter, in an appendix, or in a section the extractor was not told to read.
#
# Neither is a line to guess about and neither is a line to drop. The rule is the same for both: flag it,
# name why, and hold it out of the ingestion stream until a person has said what it is. A guess costs more
# than a gap, because a gap is visible and a wrong guess is not.
#
# The second kind is found by comparing rather than by parsing. Every token in the source carrying a
# character the language is written with should appear somewhere in the extraction, so a source line
# holding words that no extracted row holds was not reached. That is the same test coverage_check.py runs,
# and the number it reports does not change: these lines are flagged, not classified, so they still count
# against coverage. What changes is that the gap now has a file naming every line in it.
#
# A paper whose extractor repairs its source has to be compared after the same repair. Comparing a repaired
# extraction against an unrepaired source reports every correctly repaired word as unreached, so the repair
# is passed in and applied to the source line first.

import re

from salish_marking import MARKED, PRACTICAL

PAGE = re.compile(r"^===== page (\d+) =====$")

EDGES = ".,!?;:“”‘’\"'()[]…«»"

# The union of every orthography in the set. A finder that knows one paper's alphabet reports the
# others as holding no language at all, and then reports nothing missing from them either.
MARKS = MARKED + PRACTICAL + "̓̔̕ʷ˽"

# What the reason column holds. The first is a line the classifier reached and could not type. The
# second is a line no section of the extractor ever looked at.
UNKNOWN_KIND = "kind unknown"
NOT_REACHED = "not reached"


def language_tokens(text, marks=MARKS):
    """Every token of a line holding a character the language is written with."""
    held = []
    for token in text.split():
        plain = token.strip(EDGES)
        if not plain or not any(mark in plain for mark in marks):
            continue
        # A token needs a letter in it. St'át'imcets writes the glottal stop as the digit 7, which
        # puts a digit in the marks above, and without this every year and page number holding a 7
        # counts as a word of the language.
        if not any(symbol.isalpha() for symbol in plain):
            continue
        held.append(plain)
    return held


def covered_tokens(texts, marks=MARKS):
    """Every language token an extraction holds, taken from the text of its rows."""
    held = set()
    for text in texts:
        held.update(language_tokens(text, marks))
    return held


def unreached(source_lines, kept, repair=None, marks=MARKS):
    """Source lines holding language the extraction does not, each with the page it sits on.

    A line counts as reached when every one of its language tokens appears somewhere in the
    extraction, wherever the extractor happened to keep it. That is deliberately generous: the
    question here is whether the words got out of the paper, not whether they were filed tidily.
    """
    found = []
    page = 0
    for line in source_lines:
        trimmed = " ".join(line.split())
        seen = PAGE.match(trimmed)
        if seen:
            page = int(seen.group(1))
            continue
        if repair is not None:
            trimmed = repair(trimmed)
        missing = [one for one in language_tokens(trimmed, marks) if one not in kept]
        if missing:
            found.append((page, "", NOT_REACHED, " ".join(missing), trimmed))
    return found


def write_unsorted(path, paper, rows):
    """The flag file for one paper, in the column order every paper's file uses.

    Each row is page, where, reason, missing, text. The where column carries whatever locator the
    paper has, a section and block number for a line the classifier reached and nothing for a line
    found by comparison, which carries its page instead.
    """
    ordered = sorted(rows, key=lambda one: (one[0], one[1]))
    with open(path, "w", encoding="utf-8", newline="") as handle:
        handle.write("# Lines of %s carrying the language that this extractor could not sort.\n"
                     % paper)
        handle.write("# They are held out of the pure stream until someone says what they are,\n")
        handle.write("# and they still count against coverage, because a flag is not a fix.\n")
        handle.write("#\n")
        handle.write("# reason %s: the classifier read the line and none of its tests fired.\n"
                     % UNKNOWN_KIND)
        handle.write("# reason %s: no section of the extractor ever looked at the line, which\n"
                     % NOT_REACHED)
        handle.write("# the missing column shows by naming the words no extracted row holds.\n")
        handle.write("page\twhere\treason\tmissing\ttext\n")
        for page, where, reason, missing, text in ordered:
            handle.write("%s\t%s\t%s\t%s\t%s\n" % (page, where, reason, missing, text))
    return len(ordered)
