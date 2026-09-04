#!/usr/bin/env python3
# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Close the space a PDF leaves after a stacked diacritic.
#
#   Usage:  from inserted_space import closed_spaces
#
# Every one of these PDFs sets a combining mark by leaving room after it, and the extraction reads
# that room as a space. One word arrives as two tokens. Six of the eleven papers with readers carry
# it, and the counts are not small: 996 in Hall and Phillips, 943 in Garcia, 478 in LaFontaine and
# Janzen, 298 in Mellesmoen and Kye, 169 in Matthewson and Redan, 159 in Mary George.
#
# THE COVERAGE CHECK CANNOT SEE THIS
#
# coverage_check.py puts the source and the extraction through the same repair before comparing. A
# word broken in the source and broken in the extraction matches itself, and the paper reports 100
# percent while its corpus holds K̓ and weswapáw̓ as two words of St'át'imcets.
#
# What found it was the hand extraction. A person read Cw7aoz káti7 láti7 ku naxwít off the page,
# wrote K̓weswapáw̓ down as the one word it is, and reader_check.py then reported 91 of that paper's
# 102 forms as forms the reader does not produce.
#
# ʷ IS NOT A STACKED MARK
#
# It is a spacing modifier letter on the baseline, and the typesetter never has to make room after
# one. Every space following a ʷ is a real word boundary: bəlkʷ ‘return’, wix̌ʷ x̌il, sčədadxʷ
# sʔuladxʷ, tíləxʷ ʔəsxʷák̓ʷilbids. Closing those welded thirteen pairs of words in one paper alone,
# among them the first two words of Annie Jack's opening sentence.
#
# TWO SPACES ARE A COLUMN BOUNDARY
#
# Where a real boundary follows a stacked mark the extraction prints two spaces. That is what keeps
# the two columns of Table A1 apart at č̓ ə́šay̓  č̓ əsáy̓. Only a lone space is closed, and one of the
# two survives.

import unicodedata


def closed_spaces(line):
    """One line with the space the PDF left after a stacked diacritic taken out.

    Uses the Unicode combining class, not a list of marks. The papers between them leave a space
    after the comma above, the comma above right, the caron, the acute, the grave, the dot below
    and the hook above, and a hand-kept list of those went stale the first time a new paper arrived.
    """
    out = []
    at = 0
    while at < len(line):
        symbol = line[at]
        if ((symbol == " ") and out and unicodedata.combining(out[-1])
                and (((at + 1) >= len(line)) or (line[at + 1] != " "))):
            at += 1
            continue
        out.append(symbol)
        at += 1
    return "".join(out)
