#!/usr/bin/env python3
# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Check a hand extraction against the paper it was read off.
#
#   Usage:  python tools/dev_env/Salishan/hand_extraction/oracle_check.py
#
# The hand extraction is the control every reader is graded against, which puts the whole weight of
# the corpus on it being right. A person reading a thirty-seven page paper into a table skips rows
# and mistypes marks, and neither shows up later as anything but a corpus that quietly disagrees
# with the paper.
#
# So the table is checked against the paper both ways. Every form written down has to be findable in
# the source, which catches a mistyped mark and an invented row. Every word in the source has to
# appear in some form written down, which catches a skipped row and a table read only halfway. The
# second direction is the one that finds omissions, and omissions are what a person reading by hand
# actually produces.
#
# Both sides go through the paper's repairs first, for the reason coverage_check.py states: comparing
# a repaired hand extraction against an unrepaired source reports every correctly repaired word as
# missing.

import io
import os
import sys
import unicodedata

ROOT = os.path.abspath(__file__)
while (ROOT != os.path.dirname(ROOT)) and not os.path.isdir(os.path.join(ROOT, "build")):
    ROOT = os.path.dirname(ROOT)
PAPERS = os.path.join(ROOT, "build", "papers")
HERE = os.path.dirname(os.path.abspath(__file__))

sys.path.insert(0, os.path.join(os.path.dirname(HERE), "corpus_script_extraction"))

from salish_unsorted import is_language_token  # noqa: E402

from papers import EVERY  # noqa: E402

EDGES = ".,!?;:“”‘’\"'()[]…«»{}/*•→≤≥"

# What a form may be built out of besides its letters. A morpheme boundary, a clitic boundary, a
# reduplication tilde and the parentheses around a deleted segment are all part of how the paper
# writes a form, and splitting on them would compare pieces the paper never printed apart.
INSIDE = "-=~()"


def pieces(form):
    """One written form as the strings a source line could hold it as.

    Composed the same way the source is. A hand extraction is typed at a keyboard that composes á as
    one character while the paper sometimes writes it as two, and without this every accented form
    written by hand is reported as one the paper does not hold.
    """
    held = set()
    for token in unicodedata.normalize("NFC", form).split():
        plain = token.strip(EDGES)
        if plain:
            held.add(plain)
    return held


def oracle_rows(path):
    """Every row of a hand extraction, as where, dialect, kind, form, gloss."""
    held = []
    with open(path, encoding="utf-8") as handle:
        for line in handle:
            if line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            if (len(fields) < 4) or (fields[0] == "where"):
                continue
            held.append((fields[0], fields[1], fields[2], fields[3],
                         fields[4] if len(fields) > 4 else ""))
    return held


def source_forms(path, repair=None):
    """Every string of a paper a written form could be looking for, with the line it sits on.

    Three things beyond splitting on spaces. A cell can hold two alternants divided by a slash, as
    Table A2 does at dᶻəlč̓/ǰəlč̓. Each half of a slashed token is offered as well as the whole of
    it, which keeps the constraint name *P/ə findable too. A form can be broken across a line break,
    as sčəbíd-ac is in both of the Figure 7 captions, and a line ending in a hyphen is also offered
    joined to the line after it. All of this widens what a lookup finds and none of it builds a
    corpus. An over-join here costs nothing but a question not asked.
    """
    held = {}
    previous = ""
    with open(path, encoding="utf-8", errors="replace") as handle:
        for number, line in enumerate(handle, 1):
            if line.startswith("====="):
                continue
            # NFC on both sides, always. It is not one of the paper's repairs, it is the definition
            # of two strings being the same string, and a comparison that skips it reports ǰ typed
            # at a keyboard as absent from a paper that prints ǰ on ten lines. A repair ends in NFC
            # itself, and has to, because composing a with a combining acute first would take that
            # acute out of the set of marks whose following space gets closed.
            line = repair(line.rstrip()) if repair else unicodedata.normalize("NFC", line.rstrip())
            reach = [line]
            if previous.endswith("-"):
                reach.append("%s%s" % (previous.split()[-1], line.lstrip()))
            previous = line
            for at, one in enumerate(reach):
                tokens = one.split()
                for where, token in enumerate(tokens):
                    # The half of a wrapped form left at a line end is not a form to ask about; the
                    # join built from it above is. p̓il- ‘flat’ ends in a hyphen too and is a real
                    # prefix, which is why only the last token on a line is dropped.
                    if ((at == 0) and (where == (len(tokens) - 1)) and token.endswith("-")):
                        continue
                    for part in [token] + token.split("/"):
                        plain = part.strip(EDGES)
                        if plain:
                            held.setdefault(plain, number)
    return held


def main():
    out = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace", newline="")
    failed = 0
    waiting = []
    for name, stem, record, repair, marks in EVERY:
        table = os.path.join(HERE, name)
        source = os.path.join(PAPERS, "%s.txt" % stem)
        if not os.path.isfile(table):
            waiting.append(stem)
            continue
        if not os.path.isfile(source):
            out.write("  no paper on disk for %s\n" % stem)
            failed += 1
            continue

        rows = oracle_rows(table)
        held = source_forms(source, repair)
        raw = source_forms(source)
        out.write("  %s\n" % name)
        out.write("    %d rows read by hand, %d distinct tokens in the paper\n"
                  % (len(rows), len(held)))

        # Direction one. A form written down that the paper does not hold is a typing slip or an
        # invented row, and it would put a word nobody printed into the corpus.
        #
        # Asked against the unrepaired paper as well. A form the repair took out is a different
        # thing from a form nobody wrote. The person read it off the page correctly and the repair
        # then destroyed it. That is a fact about the repair and is reported as one.
        unfound = []
        cost = []
        written = set()
        for where, dialect, kind, form, gloss in rows:
            for piece in pieces(form):
                written.add(piece)
                if piece in held:
                    continue
                (cost if piece in raw else unfound).append((where, kind, form, piece))
        out.write("    %d forms the repair took out of the paper\n" % len(cost))
        for where, kind, form, piece in cost:
            out.write("      %-12s %-11s %-28s %s\n" % (where, kind, form, piece))
        out.write("    %d written forms the paper does not hold\n" % len(unfound))
        for where, kind, form, piece in unfound:
            out.write("      %-12s %-11s %-28s %s\n" % (where, kind, form, piece))

        # Direction two. A word in the paper that no row holds is a row the reader skipped. Only
        # tokens carrying a character of the language are asked about; the paper is otherwise
        # English prose and the hand extraction is not a transcription of that.
        missed = []
        for token, number in sorted(held.items(), key=lambda one: one[1]):
            if not is_language_token(token, marks):
                continue
            # A slashed token is two forms printed in one cell, dᶻəlč̓/ǰəlč̓, and the hand
            # extraction gives each of them its own row. Asking for the whole string back would
            # make a row per printing accident.
            if all((one in written) for one in token.split("/")
                   if is_language_token(one, marks)):
                continue
            missed.append((number, token))
        out.write("    %d language tokens in the paper that no row holds\n" % len(missed))
        for number, token in missed:
            out.write("      line %-6d %s\n" % (number, token))

        failed += len(unfound) + len(missed)

    out.write("\n  %d of %d papers have a hand extraction\n"
              % (len(EVERY) - len(waiting), len(EVERY)))
    if waiting:
        out.write("  still to be read by hand:\n")
        for stem in waiting:
            out.write("    %s\n" % stem)
    out.write("\n  %s\n" % ("every hand extraction agrees with its paper" if not failed
                            else "%d disagreements to work through" % failed))
    out.flush()
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
