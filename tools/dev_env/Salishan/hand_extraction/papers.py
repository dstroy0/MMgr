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

from glyph_names import decoded as robertson  # noqa: E402
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

# The 1975 Hilbert and Hess typescript writes the glottal stop as ? the way the 1983 one does, and
# keeps the rest of the orthography: ə, č, š, ɬ, the barred lambda, and the wedge over x. Its
# scanned text is OCR and loses most of that, so the marks here are what the page prints and the
# check reports the distance between the two.
HILBERT_HESS = "?ə" + "čšɬƛᶻʷ" + "̌̓"

# Robertson writes the Thompson and Shuswap of the Chinuk pipa letters in Americanist symbols, and
# says so on page 30: č, š and x̌ʷ, the last of them a dot under an x. The dot is a combining mark
# and the shared set does not carry it, so /x̣əƛ’ would be found by its ƛ and x̣aw by nothing at all.
ROBERTSON = SHARED + "̣čš"

# Wolfe is a comparative reconstruction and its forms are affixes, not words, so the marks have to
# reach a suffix written entirely in plain letters with one accent on it. =álus and =áləs carry
# nothing of the shared set and are Sechelt and Songish for 'eye'.
#
# The accents are given both ways on purpose. The check composes to NFC before it looks, so á
# arrives as one character and never as an acute, while ə́ has no composed form and keeps its acute
# standing on its own. Carrying only the composed vowels loses every ə́ in the paper, and carrying
# only the combining acute loses every á.
#
# ε and έ are the Greek epsilon and the Greek epsilon with tonos, and both are in the paper beside
# the IPA ɛ. The author writes Cw =ən̓έʔ with the Greek letter and Cw =ɛ́n̓əʔ with the IPA one, so a
# set holding one of them reads half the Halkomelem data as English.
WOLFE = SHARED + "ʸːɛεέŋᶿθǰčšĺ" + "áéíóú" + "̌́"

# Nater argues that Bella Coola has words with no vowel in them, so most of his data is a run of two
# or three consonants and nothing else. That puts a hole in direction two on this paper and no mark
# set closes it.
#
# The apostrophe was tried and taken out again. Nater writes every ejective with one, so putting ’ in
# the set makes c’p and sp’ visible; it also makes every English gloss visible, because a gloss ends
# in a closing quote and bare() only removes a quote pair from one token. ‘(it is a) stone’ arrives
# as four tokens and the last of them is stone’, which then reads as a word of the language. That is
# a few hundred false holes to buy about thirty real ones, so ’ stays out.
#
# What is left uncovered is every entry written in plain ASCII: kp 'each, all, every', tp 'spotted',
# ps, px, sx, kx, and the ejective-only forms above. Direction one still checks all of them, because
# it looks for what the table wrote down in the paper. Direction two cannot ask about them, and that
# is a limit of the check on this paper rather than a hole in the table.
NATER = SHARED + "̩̌"

# Lyon's inchoative survey cites its roots with a square root sign, and that sign is what makes half
# of them visible. √piq 'white', √nir 'smooth', √mir, √yus and √tar carry no character of the shared
# set at all, and a check built without √ reads every one of them as English. It occurs 129 times in
# the paper and never outside a root citation, so it costs nothing to carry.
#
# The apostrophe is not a letter in this paper, unlike Nater's. Lyon writes every ejective with the
# combining comma above, 397 times, and the 129 right single quotes are all closing quotes.
LYON_INCH = SHARED + "̌́" + "áíúé" + "ɣš√"

# Kim writes the lateral fricative as ɫ, which is a third character for it. salish_marking carries ɬ
# and ł and says papers differ on which they use; this is the paper that makes it three. The set also
# has to hold ˀ as well as ʔ, because footnote 11 says the paper writes a phonemic glottal stop with
# one and a rule-derived glottal stop with the other, and both are all over the reduplicants.
#
# Two characters in here render identically and are not the same code point: the combining comma
# above U+0313 and the Greek koronis U+0343, 166 and 11 of them. NFC maps the koronis onto the comma,
# and oracle_check normalizes before it looks, so they meet. Nothing to do beyond knowing it.
KIM = SHARED + "ɫˀščóéɔ" + "̦́ʹ"

# Nater's etymological database carries the worst set of lookalike collisions in the archive, and
# one of them does not resolve itself.
#
# The schwa is written two ways: ǝ, the turned e at U+01DD, 183 times, and ə, the schwa at U+0259,
# 52 times. These are separate characters with no canonical equivalence, so NFC leaves both standing
# and a set holding only ə is blind to 183 tokens. That is unlike Kim's koronis, which NFC folds
# onto the comma above without anyone having to know.
#
# The lambda is written two ways as well, ƛ at U+019B 128 times and λ at U+03BB 3 times, and the
# uvular fricative is the Greek chi, which the shared set already carries.
#
# What is deliberately left out is ‟, U+201F. It is this paper's ejective mark, in ƛ‟p and q‟ʷasta,
# and it is also the closing quote of all 1275 English glosses, which is why there are 1763 of them.
# Carrying it would make every gloss a word of the language. That leaves the plain-ASCII forms
# invisible to direction two, qla and smt and sqala among them, which is the same limit Nater's
# voiceless-words paper has and is recorded in refs.md for the same reason.
NATER_ETYM = SHARED + "ǝ√" + "áíúà" + "ᴗɢʁʒščɣλˑ"

# Hall, Luntzlara, Mellesmoen and Reid on the control directive. The dot below is what this paper
# writes its rounded uvular with, in x̣íɬ and sóx̣ʷest and c̓x̣, 40 times; the shared set does not carry
# it and Robertson's entry adds it for the same reason. The acute vowels are given composed and
# combining both, on the same argument as Wolfe: NFC folds á into one character and leaves ə́ as a
# schwa carrying an acute of its own.
#
# ǰ and θ are in here to make ʔayʔaǰuθəm and -θi visible. Neither is nɬeʔkepmxcín. Section 3.1.1
# argues from Comox and from St'át'imcets, and the who column is what keeps those two out of the
# target stream; a set that could not see them would leave them for direction two to never ask about.
#
# Three characters are deliberately out.
#
# ː, U+02D0, occurs five times and every one is a typo for a colon: 1992ː65, this variationː, in
# transitive verbsː. Carrying it would make paperː and verbsː words of the language.
#
# ’ is not the ejective in this paper, unlike Nater's. The ejective is the combining comma above, 95
# times, and the 172 right quotes are closing quotes and English possessives. It would buy the two
# practical-orthography names Snk’y’peplhxw and Nlaka’pamux and cost every Thompson’s and one’s.
#
# ∅ marks a null morpheme inside an underlying form. It is notation and the forms it sits in are
# analysis, which the table holds under a kind that keeps them out of the pure stream anyway.
HALL_CTR = SHARED + "̣" + "áéíóúè" + "́" + "ǰθ"

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
    ("1975_Hilbert_Hess.oracle.tsv",
     "1975_Hilbert_Hess",
     "ViHilbert-ThomHess_ANoteOnAeConstructionsInLushootseed_HilbertHess"
     "_Salish_lushootseed_1975_mixed.txt",
     None, HILBERT_HESS),
    ("2012_Robertson.oracle.tsv",
     "2012_Robertson",
     "CharleyAlexisMayoos-WilliamCelestin_BCIndigenousPeoplesChinukPipaScript_Robertson"
     "_Salish_nlekepmxcin-secwepemctsin_2012_mixed.txt",
     robertson, ROBERTSON),
    # No speaker slot on this one. Every form in it is cited from a published dictionary of one of
    # eighteen languages, so there is nobody the corpus is of, and the who column carries the
    # language instead. A reader has to split on that column: the forms are Sliammon, Sechelt,
    # Squamish, Halkomelem, Straits, Klallam, Lushootseed, Twana, Tillamook and Tsamosan together,
    # and pouring them into one pure corpus would build a corpus of no language at all.
    ("WolfeICSNL60.oracle.tsv",
     "WolfeICSNL60",
     "unstated_LexicalSuffixesAndConnectivesInProtoCentralSalishAndBeyond_Wolfe"
     "_Salish_centralsalish_2025_mixed.txt",
     closed_spaces, WOLFE),
    # Another paper with no speaker slot. The forms are Nater's own from his 1990 dictionary and his
    # 1984 grammar, and the comparanda are Heiltsuk, Oowekyala, Kwak̓wala and Haisla, which are North
    # Wakashan and not Salish at all. The who column is what keeps those out of a Nuxalk corpus.
    ("ICSNL59_Nater_2_final.oracle.tsv",
     "ICSNL59_Nater_2_final",
     "unstated_VoicelessWordsInBellaCoolaFactVsFiction_Nater"
     "_Salish_nuxalk_2024_mixed.txt",
     closed_spaces, NATER),
    # Elicited from two Westbank speakers, so this one has a speaker slot again. Its cognate columns
    # carry Spokane, Secwepemctsín, Lillooet, nxaʔamxčín, Thompson and Coeur d'Alene, all Salish and
    # none of them Nsyilxcən, so the who column keeps them out of the target stream.
    ("LyonICSNL60_Inch-2.oracle.tsv",
     "LyonICSNL60_Inch-2",
     "DelphineDerricksonArmstrong-DaveMichele_NsyilxcnInchoativesAndTheirDistributions"
     "AcrossRootTypes_Lyon_Salish_nsyilxcen_2025_mixed.txt",
     closed_spaces, LYON_INCH),
    # The first Twana paper here, and Twana has no anchor yet. Every form is Drachman's, which Kim
    # calls the only reliable reference in existence for this. Example (1) is the trap: it opens the
    # paper where the data usually starts and it is Tillamook in Edel's 1939 transcription, not
    # Twana, so the who column has to carry it.
    ("Kim_TwanaReduplication_final.oracle.tsv",
     "Kim_TwanaReduplication_final",
     "unstated_TheTruncatedReduplicationInTwana_Kim"
     "_Salish_twana_2017_mixed.txt",
     closed_spaces, KIM),
    # The largest paper in the set: an etymological partition of the whole Bella Coola verbo-nominal
    # lexicon, about 1275 entries over 52 pages, each one a line number, an English gloss, the Bella
    # Coola form and its cognate. The cognate column carries Proto-Salish, Coast and Interior Salish,
    # North Wakashan, Proto-Athabascan and Nootka, so the who column does the same work here that it
    # does in Wolfe, over a longer table.
    ("2013_Nater.oracle.tsv",
     "2013_Nater",
     "unstated_HowSalishIsBellaCoola_Nater"
     "_Salish_nuxalk_2013_mixed.txt",
     closed_spaces, NATER_ETYM),
    # Every form is cited from Thompson and Thompson's 1992 grammar and 1996 dictionary, so there is
    # no speaker slot for the paper as a whole. Two examples are Bev Phillips out of Hall and
    # Phillips 2025, which is another paper in this table, and the acknowledgement footnote quotes
    # kʷaɬtèzetkʷ introducing herself in the language. The who column carries those three.
    #
    # This is the first paper here that prints two forms of every example: a surface form in square
    # brackets and an underlying form in slashes. Only the bracketed one was ever said. The slashed
    # forms, the derivation tables' intermediate lines, and the starred forms the analysis predicts
    # and rejects are all held out of the pure stream, on the same rule that holds out Kim's
    # underlying forms and Wolfe's reconstructions.
    ("Hall-et-al_-ICSNL_61-1.oracle.tsv",
     "Hall-et-al_-ICSNL_61-1",
     "unstated_CtrlAltDeleteTheControlDirectiveAndAssociatedTDeletionInNlekepmxcin"
     "_HallLuntzlaraMellesmoenReid_Salish_nlekepmxcin_2026_mixed.txt",
     None, HALL_CTR),
)
