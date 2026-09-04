#!/usr/bin/env python3
# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Which hand extraction goes with which paper and which record.
#
#   Usage:  from papers import EVERY
#
# One table, read by both checks. Two copies of it drift, and then the check that grades a reader
# against a hand extraction is grading it against a different paper than the check that grades the
# hand extraction against its source.
#
# Each entry is the oracle's filename, the paper's stem in build/papers, the record the reader wrote,
# the repair that reader applies to its source, and what that paper writes its language with. A paper
# whose reader repairs nothing carries None, and the checks then compare against the paper as it
# arrived.
#
# The marks are a field because they are not the same question in every paper. Most of these are
# written in the modern orthography and share ʔ, ə and ɬ. The 1983 Hilbert typescript is damaged and
# writes the glottal stop as ? and the schwa as ~, J or G. A check built on the shared marks finds
# nothing in it at all. The stress paper needs a wider set, because yidád and báyac hold none of the
# shared marks and are words all the same.
#
# The records are named speaker first. The speaker is who the corpus is of, and the linguist is the
# one who learned it from them.

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                                "corpus_script_extraction"))

from inserted_space import closed_spaces  # noqa: E402
from mary_george_repair import repaired as mary_george  # noqa: E402
from mellesmoen_kye_repair import repaired as mellesmoen_kye  # noqa: E402
from salish_marking import TEXT_SPACE  # noqa: E402

# The space every paper here is represented in. This file spelled the union out a second time until
# the copies were noticed. salish_marking holds the one definition and names what is in it.
SHARED = TEXT_SPACE

# The stress paper writes yidád ‘fish trap’, báyac ‘meat’ and x̌il ‘lost’ with no glottal stop, no
# schwa and no lateral, so the shared set reads all three as English.
STRESS = SHARED + "ǰᶻθáíúàìù" + "̌́̀"

# The 1983 typescript's damaged orthography. Nothing in the shared set appears in it.
DAMAGED = "?~JG@V%]!"

# Lyon's two papers are checked against a drafted page text, which is written in the same
# orthography as every other paper here, so the shared set applies to them. Two characters it does
# not carry are ordinary in Okanagan: the wedge over x in x̌ast and x̌minks, and the raised dot Lyon
# writes length with in ya·ʕt and cxʷú·yəlx. A token whose only mark is one of those reads as
# English without them.
#
# An earlier version of this listed the characters of the extraction instead, back when the check
# read that. None of them survive the draft, so direction two asked about nothing and reported a
# paper with eleven unread texts as complete.
OKANAGAN = SHARED + "̌·"

# Papers whose text extraction is not what the page says. Both of these are TeX Type1 with a custom
# encoding and no ToUnicode map. pypdf and pypdfium2 lose the same things: the page prints
# cítxʷsəlx uɬ ti nyʕip and the text holds cítxws@lx uì ’ti ny ’Qip, with the ejective mark landing
# in front of its letter instead of over it.
#
# draft_page_text.py writes build/papers/<stem>.page.txt for these, line for line with the
# extraction, and the checks read that instead. Every rule it applies came off a rendered page, but
# one of them guesses: page kʷ and page wist both arrive as w, and the draft labializes a w after
# the consonants that take it. Until a person has read the pages, .page.txt is a draft and these two
# papers are still listed here.
NOT_FAITHFUL = ("19-Lyon_ICSNL50_final-78", "2013_Lindley_Lyon")

# What to read for a paper whose extraction is not the page.
PAGE_TEXT = "%s.page.txt"

EVERY = (
    ("Mellesmoen_Kye_ICSNL61.oracle.tsv",
     "Mellesmoen_Kye_ICSNL61",
     "MarthaLamont-AnnieJack_AComparativeAnalysisOfStressInNorthernAndSouthernLushootseed"
     "_MellesmoenKye_Salish_lushootseed_2026_mixed.txt",
     mellesmoen_kye, STRESS),
    ("1983_Hilbert.oracle.tsv",
     "1983_Hilbert",
     "SusieSampsonPeter-MarthaLaMont_PokingFunInLushootseed_Hilbert"
     "_Salish_lushootseed_1983_mixed.txt",
     None, DAMAGED),
    ("Matthewson_Redan_ICSNL61.oracle.tsv",
     "Matthewson_Redan_ICSNL61",
     "Kweswapaw-LindaRedan_Cw7aozKati7Lati7KuNaxwit_MatthewsonRedan"
     "_Salish_statimcets_2026_mixed.txt",
     closed_spaces, SHARED),
    ("AlexanderDavis_ICSNL61.oracle.tsv",
     "AlexanderDavis_ICSNL61",
     "Qwa7yanak-CarlAlexander_ITsicwasSQwa7yanakAku7GraveyardValley_AlexanderDavis"
     "_Salish_statimcets_2026_mixed.txt",
     None, SHARED),
    ("22-Nater-Bella-Coola-tale-10.oracle.tsv",
     "22-Nater-Bella-Coola-tale-10",
     "MargaretSiwallace_ABellaCoolaTale_Nater_Salish_nuxalk_2015_nomixed.txt",
     None, SHARED),
    ("ICSNL59_LaFontaine_Janzen_final.oracle.tsv",
     "ICSNL59_LaFontaine_Janzen_final",
     "wlwlmelst-MauriceMichell_FourStoriesByWlwlmelst_LaFontaineJanzen"
     "_Salish_nlekepmxcin_2024_mixed.txt",
     closed_spaces, SHARED),
    ("ICSNL59_Garcia_Hannon_Stacey_final.oracle.tsv",
     "ICSNL59_Garcia_Hannon_Stacey_final",
     "Kweltezetkwu-BerniceGarcia_ThreeGlossedNlekepmxcinNarratives_GarciaHannonStacey"
     "_Salish_nlekepmxcin_2024_mixed.txt",
     closed_spaces, SHARED),
    ("ICSNL56_DavisJ_2_final-1.oracle.tsv",
     "ICSNL56_DavisJ_2_final-1",
     "MaryGeorge_MaryGeorgePersonalNarratives_JohnHamiltonDavis"
     "_Salish_ayajuthem_2021_mixed.txt",
     mary_george, SHARED),
    ("HallPhillipsICSNL60.oracle.tsv",
     "HallPhillipsICSNL60",
     "BevPhillips_WhenOldOneCreatedTheEarth_HallPhillips"
     "_Salish_nlekepmxcin_2025_nomixed.txt",
     closed_spaces, SHARED),
    ("19-Lyon_ICSNL50_final-78.oracle.tsv",
     "19-Lyon_ICSNL50_final-78",
     "GeorgeLezard-NellieGuitterez-AndrewMcGinnis_ThreeOkanaganStoriesAboutPriests_Lyon"
     "_Salish_nsyilxcen_2015_nomixed.txt",
     None, OKANAGAN),
    ("2013_Lindley_Lyon.oracle.tsv",
     "2013_Lindley_Lyon",
     "LottieLindley_TwelveMoreUpperNicolaOkanaganNarratives_LindleyLyon"
     "_Salish_nsyilxcen_2013_nomixed.txt",
     None, OKANAGAN),
)
