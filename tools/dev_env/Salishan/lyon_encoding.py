#!/usr/bin/env python3
# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Turn the TeX font's encoding back into the orthography, as far as it goes.
#
#   Usage:  from lyon_encoding import drafted
#
# 19-Lyon_ICSNL50_final-78 and 2013_Lindley_Lyon are set in NimbusRomNo9L and TeX-xipa with a custom
# encoding and no ToUnicode map, so what pypdf hands back is glyph codes read as ASCII. Page 1 of
# Lindley prints q̓sápi ɬaʔ ct̓ʕapənwíxʷ and the text holds ’qsápi ìaP c’tQap@nwíxw.
#
# WHAT THIS IS AND IS NOT
#
# It is a draft. Every rule below was read off a rendered page and holds on the pages checked, and
# the output still has to be verified against the page before it counts as the paper. It exists
# because retyping 148 pages by hand introduces its own errors, and a draft a person corrects is
# more accurate than a page a person types from nothing.
#
# WHAT IT CANNOT DO
#
# w is two letters. Page kʷukʷ and page wist both arrive as w, and nothing in the text separates
# them. The rule below labializes a w that follows one of the consonants that take it, which is
# right for kʷ, qʷ, xʷ, x̌ʷ and wrong wherever a real w follows a consonant.
#
# Word boundaries are the other one. The PDF puts a space in front of a letter carrying a mark.
# s ’plá ’ks@lx is one word, iP ’kl is two, and both of them are a space in front of a marked
# letter. Page 25 settles the first as sp̓lák̓səlx and page 24 the second as iʔ k̓l. wa’y and Lyon’s
# are the same case with the space missing instead of inserted.
#
# Those are the sites to read first on any page, and they are why this file is not a repair in
# papers.py: a repair is applied and trusted, and this has to be checked.

import re
import unicodedata

# The mark the extraction prints in front of the letter it belongs over.
EJECTIVE = "̓"

# What the glyph codes stand for, one for one, wherever they appear.
LETTERS = (
    ("@", "ə"),
    ("ì", "ɬ"),
    ("Q", "ʕ"),
    ("ň", "ƛ"),
    (";", "·"),
)

# The wedge arrives before its letter as well, and only ever sits on x here.
WEDGE = (("ˇx", "x̌"),)

# The consonants a following w labializes. A w after anything else, or at the front of a word, is
# the letter w: wist, wa’y, nwíwpəm.
LABIALIZED = "kqxgɣʕǰč"


def moved_marks(token):
    """One token with each ’ carried onto the letter it was printed in front of.

    Called only for a token salish() answered for. In the English the same character is an
    apostrophe and follows its letter, and moving it turns Lyon’s into Lyons̓.
    """
    out = []
    at = 0
    while at < len(token):
        symbol = token[at]
        if (symbol == "’") and ((at + 1) < len(token)) and token[at + 1].isalpha():
            out.append(token[at + 1])
            out.append(EJECTIVE)
            at += 2
            continue
        out.append(symbol)
        at += 1
    return "".join(out)


# The characters of the extraction that only the language is written with, so a token holding one of
# them is Salish. These are the codes as they arrive, not what they become: an earlier version of
# this listed ə ɬ ʕ ƛ, which the test never sees, and s’tmQa’lt came through with its ejective marks
# still standing in front of their letters.
#
# The first two characters are the wedge. drafted() runs WEDGE over the whole line before any of
# this, so a wedge that stood on an x is a combining caron by the time the test reads it, and the
# standalone ˇ catches one that stood anywhere else. Writing the pair as the string "x̌" put a bare x
# in the set and made every English word holding one Salish.
MARKS = "̌ˇ@ìQňáéíóú"

# A gloss token is plain ASCII, apart from the ligatures the PDF sets its f-words with. The whole
# gloss of one morpheme is one token, so go-n-dip.ﬂuid-MID-3SG.POSS is a single string and the one ﬂ
# in it kept it out of this class. It was then read as Salish and its POSS came out as ʔOSS.
GLOSS = re.compile(r"^[A-Za-z0-9.()\[\]/,;:'ﬁﬂﬀﬃﬄ-]+$")
LABEL = re.compile(r"[A-Z]{2}")


def a_gloss(token):
    """Whether a token belongs to a gloss line, where its P is the P of RECIP and PASS.

    A gloss is plain ASCII and carries a run of two or more capitals: RECIP, 3PL.ABS, NOM-son-RED.
    One capital on its own is not enough, because that is also iP and Philosophical.
    """
    return bool(GLOSS.match(token)) and bool(LABEL.search(token))


def salish(token):
    """Whether a token is set in the language's font, so its P is a glottal stop and its ’ a mark.

    P and ’ are the two the encoding cannot decide by itself. P is ʔ throughout the Salish and a
    capital P throughout the English, and both sit on one line: COMP and RECIP head the gloss lines
    while Philosophical and Penticton run through the notes. ’ is the ejective mark in the Salish
    and the apostrophe in Lyon’s and Society’s.

    A token carrying any other mark of the orthography is Salish, and so is one holding a P that is
    not the first letter, which is what iP, smsámaP and nPaysənúlaPxw are and what Philosophical is
    not.

    Two things this misses. A P-initial Salish word with no other mark on it, as Pitx, Pamn and
    Pasil are, comes through as English. So does a word whose ’ the extraction did not put a space
    in front of, because wa’y and Lyon’s are then the same shape and only the page tells them apart.
    Both are among the first things to look for on a page.
    """
    if a_gloss(token):
        return False
    # Qu opening a token is the English digraph, as Quilchena and Queen are. Q is the pharyngeal
    # everywhere else, at the front of Qant and QapnáP included, so the test asks for a plain ASCII
    # tail as well and only the English pair comes out. A Salish word opening ʕu and carrying no
    # other mark would be read as English here. Neither paper holds one.
    if (token[:2] == "Qu") and token[1:].isascii():
        return False
    if any((mark in token) for mark in MARKS):
        return True
    # A ’ that opens a token is the mark waiting for its letter: ’ti is t̓i. One that follows a
    # letter is the apostrophe of Lyon’s and Society’s.
    if token.startswith("’"):
        return True
    # The length mark, which always has the letter it lengthens after it: wu;;;;;t is wu·····t. A ;
    # at the end of a token is the sentence's semicolon and what carries it is English.
    if any((token[at] == ";") and token[at + 1].isalpha() for at in range(len(token) - 1)):
        return True
    # A glottal stop standing alone, left by a break the PDF put in front of it, as ixí P is.
    if token == "P":
        return True
    return "P" in token[1:]


def labialized(line):
    """One line with w read as ʷ where it follows a consonant that takes labialization."""
    out = []
    for symbol in line:
        if (symbol == "w") and out:
            previous = out[-1]
            if unicodedata.combining(previous) and (len(out) > 1):
                previous = out[-2]
            if previous.lower() in LABIALIZED:
                out.append("ʷ")
                continue
        out.append(symbol)
    return "".join(out)


def drafted(line):
    """One line of the extraction as a draft of what the page says, to be checked against the page.

    Token by token, because a line is mixed. One gloss line reads COMP DET native.person and the
    transcription above it reads ɬaʔ iʔ sqilxʷ, and the two want opposite answers about P and ’.

    The wedge goes first and over the whole line: ˇx is x̌ in the Salish and appears nowhere else,
    and it is what makes a token look Salish to the test that follows.

    LETTERS runs on a Salish token only. Its codes are ordinary characters elsewhere, and ; is the
    one that showed it: Lyon ends a clause with a semicolon in his English, and mapping the line
    without asking turned long ago over there; we came into over there· we came.
    """
    for before, after in WEDGE:
        line = line.replace(before, after)
    held = []
    for token in line.split(" "):
        if salish(token):
            token = moved_marks(token).replace("P", "ʔ")
            for before, after in LETTERS:
                token = token.replace(before, after)
        held.append(token)
    return unicodedata.normalize("NFC", labialized(" ".join(held)))
