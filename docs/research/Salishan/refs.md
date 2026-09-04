# Salish sources

**Purpose:** Find the source behind any line in the extracted corpora, and know what is held on disk against what is only cited.
**Scope:** `build/papers/`, `build/corpora/`, `build/audio/`, and `tools/dev_env/Salishan/`

Everything here is generable using the tools/dev_env/ scripts or reachable at a stated address. Where a recording is known to exist but is not a member of the corpus, it is noted.

## Speakers

| Speaker | Language | Recorded |
|---|---|---|
| Kʷəɬtəzétkʷu (Bernice Garcia), c̓əɬétkʷu (Coldwater) | nɬeʔkepmxcín | 2023, published 2024 |
| Bev Phillips, Lytton First Nation (ƛ̓q̓əmcín) | nɬeʔkepmxcín | 2024 and 2025 |
| wlwlmelst (Maurice Michell), Southern yutémkt dialect | nłeʔkepmxcín | published 2024 |
| K̓weswapáw̓ (Linda Redan), Qayqáyten | St'át'imcets | 31 October 2025 |
| Qwa7yán'ak (Carl Alexander), Nxwísten (Bridge River) | St'át'imcets | 7 July 2025 |
| Mary George, Sliammon | Mainland Comox (ayajuthem) | 1969 to 1980 |
| Noel George Harry, Tommy Paul | Mainland Comox (ayajuthem) | 1969 to 1980 |
| Dr. Margaret Siwallace | Nuxalk | about 1975, published 2015 |
| George Lezard, Penticton Indian Reserve | Nsyilxcən | 1966, aged eighty-five |
| Nellie Guitterez, Upper Nicola Indian Band | Nsyilxcən | 1978 or 1979 |
| Kiláwnaʔ (Andrew McGinnis), Penticton Indian Reserve | Nsyilxcən | 9 October 2014 |
| Lottie Lindley, Upper Nicola | Nsyilxcən | published 2013 |
| Sam Mitchell | St'át'imcets | in van Eijk and Williams 1981 |
| Susie Sampson Peter, Upper Skagit | Lushootseed | Leon Metcalf, 1950 to 1958 |
| Martha LaMont, Tulalip-Skagit | Lushootseed | Leon Metcalf 1952, Thom Hess 1963 |
| Martha Lamont, Northern dialect | Lushootseed | Leon Metcalf, 1950s |
| Annie Jack, Southern dialect | Lushootseed | Leon Metcalf, 1950s |

Conditions the speakers set:

* Bernice Garcia asks it be acknowledged she is a Kamloops Indian Residential School speaker re-learning her language.
* wlwlmelst shares his four stories freely for people connecting with the language. They came from his mother nxwelinek and his grandmother ʔústko.
* George Lezard's narrative: transcribed by Larry Pierre 1970, updated by permission of Arnie Baptiste, his son.
* Nellie Guitterez's story: reprinted by permission of Lynne Jorgesen, her great-granddaughter.

## Text sources, extracted

Eleven papers, each read by its own file in `tools/dev_env/Salishan/corpus_script_extraction/` and each with a hand extraction beside it in `hand_extraction/`. Every token of the language in each is accounted for in the corpus built from it, and `coverage_check.py` is what says so. For the two Lyon papers that statement is about the extracted text, which is the font's own encoding and not what the page prints. The section below on those two says what that means.

Nothing here needs fetching by hand. `python tools/dev_env/Salishan/get_papers.py` reads the archive index, downloads these eleven and converts them. Every address below was checked; the local filename in `build/papers/` is the PDF's own name.

| Paper | Reader | PDF |
|---|---|---|
| Three Glossed Nɬeʔkepmxcín Narratives by Kʷəɬtəzétkʷu (Bernice Garcia). Garcia, Hannon and Stacey. ICSNL 59, 2024 | `extract_garcia.py` | `https://lingpapers.sites.olt.ubc.ca/files/2024/07/ICSNL59_Garcia_Hannon_Stacey_final.pdf` |
| ɬ cutés us ɬ qəɬmín ɬ tmíxʷ (When Old One Created the Earth). Hall and Phillips. ICSNL 60, 2025 | `extract_hall_phillips.py` | `https://lingpapers.sites.olt.ubc.ca/files/2025/07/HallPhillipsICSNL60.pdf` |
| Four Stories by wlwlmelst. LaFontaine and Janzen. ICSNL 59, 2024 | `extract_lafontaine_janzen.py` | `https://lingpapers.sites.olt.ubc.ca/files/2024/07/ICSNL59_LaFontaine_Janzen_final.pdf` |
| Cw7aoz káti7 láti7 ku naxwít (There was definitely no snake there). Matthewson and Redan. ICSNL 61 | `extract_matthewson_redan.py` | `https://lingpapers.sites.olt.ubc.ca/files/2026/07/Matthewson_Redan_ICSNL61.pdf` |
| I Tsícwas sQwa7yán'ak Áku7 Graveyard Valley. Alexander and Davis. ICSNL 61 | `extract_alexander_davis.py` | `https://lingpapers.sites.olt.ubc.ca/files/2026/07/AlexanderDavis_ICSNL61.pdf` |
| Mary George Personal Narratives. John Hamilton Davis. ICSNL 56, 2021 | `extract_mary_george.py` | `https://lingpapers.sites.olt.ubc.ca/files/2021/08/ICSNL56_DavisJ_2_final-1.pdf` |
| A Bella Coola tale: The Frog Children. Nater. ICSNL 50, 2015 | `extract_nater_bella_coola.py` | `https://lingpapers.sites.olt.ubc.ca/files/2018/01/22-Nater-Bella-Coola-tale-10.pdf` |
| Three Okanagan stories about priests. Lyon. ICSNL 50, 2015 | `extract_lyon_priests.py` | `https://lingpapers.sites.olt.ubc.ca/files/2018/01/19-Lyon_ICSNL50_final-78.pdf` |
| 12 more Upper Nicola Okanagan narratives. Lindley and Lyon. 2013 | `extract_lindley_lyon.py` | `https://lingpapers.sites.olt.ubc.ca/files/2018/01/2013_Lindley_Lyon.pdf` |
| Poking Fun in Lushootseed. Vi taqʷšəblu Hilbert. ICSNL 1983 | `extract_hilbert.py` | `https://lingpapers.sites.olt.ubc.ca/files/2018/03/1983_Hilbert.pdf` |
| A Comparative Analysis of Stress in Northern and Southern Lushootseed. Mellesmoen and Kye. ICSNL 61 | `extract_mellesmoen_kye.py` | `https://lingpapers.sites.olt.ubc.ca/files/2026/07/Mellesmoen_Kye_ICSNL61.pdf` |

Each reader writes into `build/corpora/`: the marked record, a `.pure.txt` holding only target-language speech, a `.unclassifiable.tsv` listing what it could not type, and for the two Lyon papers a `.words.txt` giving the word forms recovered from the interlinear.

Records are named speaker first: `<spoken by>_<paper>_<who wrote it down>_Salish_<language>_<year>_<mixed>`. The speaker is who the corpus is of.

## Hand extraction

The reader is not the source of the corpus. Each paper is read by a person into `tools/dev_env/Salishan/hand_extraction/<paper>.oracle.tsv`, which says for every form in the paper what it is, and the reader is graded against that.

Nine of the eleven verify against their paper in both directions: no form written that the paper does not hold, no token in the paper that no row covers. The other two are the papers whose extracted text is not what the page says, and the section after this one is about them.

Counts below are what `oracle_check.py` and `reader_check.py` report on 2026-09-03. "Asked for" is how many distinct written forms the rows yield, which is the denominator for the column beside it. "Invented" is a form the reader put in the corpus that no row asks for.

| Paper | Rows read by hand | Forms asked for | Reader |
|---|---|---|---|
| Mellesmoen and Kye, ICSNL 61 | 467 | 443 | reproduces it exactly |
| Hilbert, ICSNL 1983 | 60 | 52 | 2 not found, 8 over-runs it flags itself, 1 invented |
| Matthewson and Redan, ICSNL 61 | 104 | 103 | 44 not found, 217 invented |
| Alexander and Davis, ICSNL 61 | 541 | 532 | 339 not found, 914 invented |
| Nater, ICSNL 50 | 285 | 285 | 29 not found, 80 invented |
| LaFontaine and Janzen, ICSNL 59 | 226 | 225 | 81 not found, 136 invented |
| Garcia, Hannon and Stacey, ICSNL 59 | 371 | 369 | 150 not found, 748 invented |
| Mary George, ICSNL 56 | 1661 | 1654 | 663 not found, 362 in the wrong dialect |
| Hall and Phillips, ICSNL 60 | 406 | 399 | 209 not found, 521 invented |
| Lyon, ICSNL 50 | 927 | 920 | the table is being reread from the page |
| Lindley and Lyon, 2013 | 70 | 70 | text 1 of 12 read from the page, 11 outstanding |

Only Mellesmoen and Kye reproduces its table. On the two Lyon papers most of the invented count is scope: the reader reads the whole paper and the table does not yet cover it.

Three of the eleven carry a `.oracle.md` beside the table, which is where a note about that paper goes. The `.tsv` itself holds no comment syntax and no prose header, so a CSV linter can read it: a header on line 1 and the same field count on every row.

`oracle_check.py` tests the hand extraction against the paper both ways. `reader_check.py` tests the reader against the hand extraction.

Two errors it found that coverage could not, because coverage was 100 percent through both:

* The Hilbert record credited Vi Hilbert as the speaker. She wrote the paper; the twenty-one examples were said by her aunt **Susie Sampson Peter** of the Upper Skagit and by **Martha LaMont**, recorded by Leon Metcalf between 1950 and 1958 and by Thom Hess in 1963.
* Example 2's English translation is the one line "High class, high class was Raven." The record had it with the next eleven lines of Hilbert's commentary welded on, as something Susie Sampson Peter said. The typescript sets examples in an indented column and the essay at full width; the extraction dropped the indent and the width is what recovers it.

* **Six of the eleven PDFs break words in half.** They leave a space after a stacked diacritic, so `K̓weswapáw̓` arrives as `K̓` and `weswapáw̓`. Counts: Hall and Phillips 996, Garcia 943, LaFontaine and Janzen 478, Mellesmoen and Kye 298, Matthewson and Redan 169, Mary George 159. The coverage check cannot see it, because it puts both sides through the same repair and a word broken on both sides matches itself. `inserted_space.py` closes it; `ʷ` is held out, because it is a spacing letter and a space after one is a real boundary.

The stress accents are held out of that repair. A word can end in a stressed vowel, and `ntes neʔé e sqyéytn` in LaFontaine and Janzen closes to `neʔée`, which the language does not have. Mellesmoen and Kye passes its own wider set instead, because that PDF prints two spaces at a real boundary and a lone space after an acute there is always the inserted one.

Matthewson and Redan carries a second form of the same damage that no rule can close safely. The PDF also breaks *before* a marked letter: `cácl̓ep` arrives as `các l̓ep`. The same shape is a real word boundary at `lta q̓íl̓qa`, and nothing in the characters separates the two. The reader leaves both alone; the hand extraction has the true forms. That paper's reader is also blind to sections 1.2 and 1.3, where the story's title and five St'át'imcets forms are cited in prose.

Three more the hand extraction found, none of them visible to a coverage check:

* **`reader_check` was reading a fraction of each record.** Its span pattern rejected any kind holding a space, and nine of the eleven readers write one: `symbol note`, `running speech`, `stage direction`, `word gloss`, `free translation`, `cited example`, `morpheme entry`, `speaker comment`, `orthography chart`.
* **The apostrophe is a letter.** `’` is glottalization in Nuxalk and in Lyon's Okanagan, and stripping it as punctuation turned the enclitic `˽c’` into `˽c`.
* **Nater's reader kept section 2 as six blocks of prose** where the paper lists about forty prepositions, articles, deictics and enclitics, each with its own gloss, and never read section 1, where `sma` and `sʔalac’i` are defined.

## Papers whose extracted text is not the page

Six of the 146 PDFs in `build/papers/` carry a font that renumbers its glyph codes with an `/Encoding` `/Differences` array and declares no `/ToUnicode` map. An extractor is then handed code numbers with nothing to turn them into, reads them in a default encoding, and what lands in the `.txt` is the font's private alphabet.

| Paper | Fonts declaring no map back to Unicode |
|---|---|
| `19-Lyon_ICSNL50_final-78` | NimbusRomNo9L Regu, Medi, ReguItal |
| `2013_Lindley_Lyon` | NimbusRomNo9L Regu, Medi, ReguItal |
| `Lyon-final` | NimbusRomNo9L Regu, Medi, MediItal, ReguItal |
| `21-Abraham_ICSNL50_final-4` | NimbusRomNo9L Regu, Medi |
| `2011_Lonsdale_Matsushita` | Courier, Times-Roman, Times-Italic, CMSY10 |
| `2012_Robertson` | Symbol and five TrueType subsets |

A missing `/ToUnicode` is not the fault by itself. 141 of the 146 hold a font without one and nearly all of them extract correctly, because a standard encoding already says what the codes mean and every extractor has that table. It is the renumbering that does the damage, and 6 of those 141 renumber.

Page 23 of `2013_Lindley_Lyon` prints

> cítxʷsəlx uɬ t̓i nyʕ̓ip ck̓aʔítət

and `build/papers/2013_Lindley_Lyon.txt` holds

> cítxws@lx uì ’ti ny ’Qip c ’kaPít@t

Every schwa is `@`, every lateral fricative `ì`, every pharyngeal `Q`, every glottal stop `P`; the wedge stands in front of its letter as `ˇx`, the ejective mark stands in front of its letter instead of over it, and the word `ck̓aʔítət` is split. `pypdf` and `pypdfium2` lose the same things, so this is a property of the file and not of one library.

**One part of the loss cannot be inverted.** Labialization is written with a raised w and the page also has a plain w. Both arrive as `w`, so page `kʷukʷ` and page `wist` are the same string in the text and nothing in the file separates them.

The consequence for method is why this section exists. A hand extraction taken from one of these `.txt` files records the font's encoding and not the paper, and it then verifies clean against that same file in both directions, because both sides of the check are reading the one damaged artifact. That is how this was found here, after two such tables had been built.

What replaces it: `pdf2png.py` renders the pages to `build/pages/<stem>/page_NNN.png` and the reading is done from the image. `draft_page_text.py` writes `build/papers/<stem>.page.txt`, a draft of the page in the shared orthography, line for line with the extraction so a page of one is a page of the other, and `lyon_encoding.py` holds the rules it applies. The draft is a starting point a person corrects against the rendered page, never a source. Two things it cannot decide:

* **Labialization**, for the reason above. It writes `ʷ` after the consonants that take one, which is right for `kʷ qʷ xʷ x̌ʷ` and wrong wherever a real `w` follows a consonant.
* **Word boundaries.** The PDF inserts a space in front of a letter carrying a mark. `s ’plá ’ks@lx` is one word and `iP ’kl` is two, and both are a space in front of a marked letter. Page 25 settles the first as `sp̓lák̓səlx`, page 24 the second as `iʔ k̓l`.

`get_papers.py` applies the same test inside `converted()` and writes a `.unfaithful` file beside the text naming the fonts. It fires only when a PDF is converted, and conversion skips any PDF that already has text beside it, so the 146 already on disk carry no such file. The table above was produced by running the same test over them directly.

### The delta is a mutation probability distribution

Everything above treats the damage as a hazard to route around. It is also the one calibrated mutation sample in the archive, and the damaged text is kept for that reason.

Two encodings of one text, aligned line for line, with the page settling which is right, is a measured channel. `build/papers/<stem>.txt` is the mutated side, `.page.txt` is the drafted side, and the hand extraction read off the rendered page is ground truth for both. No other paper here has a second encoding of the same page to compare against.

The mutations sort into five kinds, and the last two are what make this a distribution instead of a rewrite table.

| Kind | Instances | Invertible |
|---|---|---|
| Substitution, one symbol for one | `ə`→`@`, `ɬ`→`ì`, `ʕ`→`Q`, `ʔ`→`P`, `ƛ`→`ň`, `·`→`;` | yes |
| Collapse, two symbols onto one | `ʷ`→`w`, and plain `w`→`w` | no |
| Transposition | a combining mark over its letter becomes a spacing mark before it: `k̓`→`’k`, `x̌`→`ˇx` | yes |
| Insertion | a space appears in front of a letter carrying a mark | not decidable alone |
| Deletion | the space at a change of font is dropped, welding two words | not decidable alone |

The insertion does not always fire. `2013_Lindley_Lyon.txt` holds `wa ’y` in most places and `wa’y` in three, for the one word the page prints as `way̓`. So its rate lies strictly between 0 and 1, and the same input shape has two outputs. That is a probability, not a rule, and it is why a repair applied without the page welds words that were never one word.

The deletion fires where the paper changes font mid-sentence. Footnote 5 of the same paper sets its cited forms in italic inside roman prose, and the two arrive as `Okanaganyaxʷt` and `Okanaganyaʕyáʕt`, each one token where the page prints two words. The second is recoverable, because `yaʕyáʕt` is a common word and appears on its own elsewhere in the paper. The first is not: `yaxʷt` occurs nowhere else, so nothing in the text says where the boundary was. Which of the two happens depends on the rest of the corpus rather than on the token, which is the second reason this is a distribution.

The collapse sets a ceiling. No recovery, however good, separates page `kʷukʷ` from page `wist` in the mutated text, because the channel destroyed the distinction rather than disguising it. Anything downstream that claims to have recovered a labialized consonant on these two papers is claiming something the file cannot support.

A detector keyed to a glyph inventory reads the mutated side as holding no language at all. That is not hypothetical: `is_language_token` did exactly that on these two papers until the marks set for them was corrected, and it reported a paper with eleven unread texts as fully covered. A detector reading the distribution of symbols survives the same input. The pure corpus says how well a vector separates the target language from the English around it. This pair says how much mutation it takes before it stops separating them at all.

Nothing in the tree measures the per-symbol rates yet. `font_substitution.py` scores a candidate mapping by how many mutated tokens become forms attested in a clean paper, which is a proxy for the answer. The page-verified table is the answer itself for the lines it covers, and it grows one text at a time.

**The distribution is what converts the rest.** Four of the six papers above are set in NimbusRomNo9L, and two of those four are the ones getting a page-verified table beside them. That makes the mapping measured on `2013_Lindley_Lyon` and `19-Lyon_ICSNL50_final-78` the first candidate to try on `Lyon-final` and `21-Abraham_ICSNL50_final-4`, without reading their pages first. It is a candidate and not a certainty: each of those embeddings carries its own subset prefix and its own `/Differences` array, so two documents setting type in one font can still number their codes differently. `font_substitution.py` scores a candidate and the page settles it. `2011_Lonsdale_Matsushita` and `2012_Robertson` are set in other fonts and need their own measurement.

This is why reading these two papers off the page is worth the time it costs. It recovers two papers, and it produces the conversion table for a font family, against which any paper in the remaining 993 that turns out to be set the same way can be converted and then checked.

`lyon_encoding.py` holds that table for NimbusRomNo9L as it stands, with the labialization rule still a guess and the insertion rate still unmeasured.

## Audio sources

| Recording | Address | Held |
|---|---|---|
| Hall and Phillips, xʷíʔ kʷ páq (You Will Be Sorry), ICSNL 59 | `https://lingpapers.sites.olt.ubc.ca/files/2024/07/ICSNL59_Hall_Phillips_audio.mp3` | yes, 13.4 MB |
| Givens and Hall, The Moon and the Birchbark Canoe, ICSNL 58 | `https://lingpapers.sites.olt.ubc.ca/files/2023/07/ICSNL58_Givens_Hall_The_Moon_and_the_Birchbark_Canoe.mp3` | yes, 7.5 MB |
| Hall and Phillips, ɬ cutés us ɬ qəɬmín ɬ tmíxʷ, ICSNL 60 | `https://lingpapers.sites.olt.ubc.ca/files/2025/07/2025-05-29_BP_storyrevised_trimmed.mp3` | yes, 17.1 MB |

The third is Bev Phillips reading the story `extract_hall_phillips.py` reads, so it is an oracle for that extraction and not only a source.

Recordings named in papers and not published with them:

* Qwa7yán'ak, Graveyard Valley narrative. Just over half an hour, recorded by Henry Davis at Nxwísten on 7 July 2025, published with the ICSNL 61 paper.
* K̓weswapáw̓ (Linda Redan). Three minutes twenty-eight seconds, told over Zoom on 31 October 2025. Audio and video are held by her.
* George Lezard, 1966, recorded by Randy Bouchard.
* Nellie Guitterez, 1978 or 1979, recorded by Yvonne Hébert at an Elders' Gathering in Quilchena.

## Text sources

`build/papers/` holds 152 extracted paper texts and 146 of the PDFs they came from. Eleven have a reader and a hand extraction. The rest are being built. Six of the 146 are in the class the section above describes, and their texts are the font's encoding.

Fifty of them are about Lushootseed or Skagit. The heaviest, by how much they discuss it, are `Mellesmoen_Kye_ICSNL61.txt`, `Beck_2007.txt`, `1994_Beck.txt`, `1995_Beck.txt`, `1998_Cort.txt`, `2000_Barthmaier.txt`, `1996_Beck.txt`, `ICSNL58_Kye_final.txt`, `01_ICSNL55_Beck_final.txt` and `2002_Lonsdale.txt`. `1983_Hilbert.txt` is in that set.

## Verification sources

Sources used to test a transformation rather than to supply text. Each answers a question the code that made the change cannot answer about itself.

* `LyonICSNL60_Inch-2.txt`. Lyon's later paper on Nsyilxcən, whose extraction kept its characters. Used twice: to test the font substitution table, which moved attested tokens from 1 of 3599 to 811 on one paper and 2 of 4332 to 965 on the other, and to test the word list recovered from the interlinear, where none of the broken forms had its pieces attested apart and 4 percent and 36 percent were attested once joined.
* Each paper's own second printing. Hall and Phillips, Garcia, and both Lyon papers print their stories twice, once as running text and once word by word, so one printing checks a reading of the other.

## Community sources

* Puyallup Tribal Language Program, `https://www.puyalluptriballanguage.org/`
* The Salish Institute, `https://www.thesalishinstitute.com/salish-language`

## Where the volumes live

* **Every paper, one page, 993 links: `https://lingpapers.sites.olt.ubc.ca/icsnl-volumes/`.** ICSNL 1967 to present. This is where the 152 texts in `build/papers/` came from and where anyone without them starts.
* UBCWPL, `https://lingpapers.sites.olt.ubc.ca/`. Volumes 33 (1998) to 61 (2026).
* ICSNL archives, `https://blogs.ubc.ca/icsnl/icsnl-archives-and-precedings/`. Volumes 1 to 32 are the Kinkade Collection.
* nɬeʔkepmxcín Lab, `https://blogs.ubc.ca/nlab/projects-publications/`. Where the three audio addresses came from.

Whole volumes: ICSNL 50, `https://lingpapers.sites.olt.ubc.ca/files/2018/01/ICSNL2015-fullonline.pdf`.

Paper addresses follow `files/<year>/<month>/<name>.pdf`, and `<name>` is the local filename in `build/papers/`. UBC answers a command-line fetch with a bot-defense captcha; a browser gets the same file.

**Compiled By:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
**Date:** 2026-09-03
