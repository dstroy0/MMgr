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

A twelfth, `2012_Robertson`, now has both. `extract_robertson.py` reads it, `coverage_check.py` puts it at 100 percent like the other eleven, and both hand-extraction checks grade it.

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

Nine of the eleven verify against their paper in both directions: no form written that the paper does not hold, no token in the paper that no row covers. The other two are the papers whose extracted text is not what the page says, and both are now read off the rendered pages in full. The section after this one is about them and about what their remaining disagreements measure.

Counts below are what `oracle_check.py` and `reader_check.py` report on 2026-09-04. "Asked for" is how many distinct written forms the rows yield, which is the denominator for the column beside it. "Invented" is a form the reader put in the corpus that no row asks for.

| Paper | Rows read by hand | Forms asked for | Reader |
|---|---|---|---|
| Mellesmoen and Kye, ICSNL 61 | 467 | 443 | reproduces it exactly |
| Hilbert, ICSNL 1983 | 60 | 52 | 2 not found, 8 over-runs it flags itself, 1 invented |
| Matthewson and Redan, ICSNL 61 | 104 | 103 | 44 not found, 217 invented |
| Alexander and Davis, ICSNL 61 | 541 | 532 | 339 not found, 914 invented |
| Nater, ICSNL 50 | 285 | 285 | 29 not found, 80 invented |
| LaFontaine and Janzen, ICSNL 59 | 226 | 225 | 81 not found, 136 invented |
| Garcia, Hannon and Stacey, ICSNL 59 | 371 | 369 | 150 not found, 748 invented |
| Mary George, ICSNL 56 | 1661 | 1654 | 663 not found, 574 invented, 362 in the wrong dialect |
| Hall and Phillips, ICSNL 60 | 406 | 399 | 209 not found, 521 invented |
| Lyon, ICSNL 50 | 1417 | 1413 | 1037 not found, 2134 invented, 193 in the wrong dialect |
| Lindley and Lyon, 2013 | 653 | 653 | 312 not found, 996 invented, 198 typed differently |
| Robertson, 2012 | 195 | 193 | 92 not found, of which 59 are prose the reader does not read |

Only Mellesmoen and Kye reproduces its table.

`2013_Lindley_Lyon` is read off the rendered pages: all twelve texts, both the running transcription and the interlinear, with the footnotes, free translations, commentary, the appendix paradigms and the abbreviation list. What remains is 46 forms the table holds that the draft does not, and 37 strings in the draft that no row holds. All 83 fall in one of the five classes the section below names, which makes the count a measurement of the draft.

Its reader takes `build/papers/2013_Lindley_Lyon.page.txt` and is graded in the orthography the table is written in. It misses 312 of the 653 rows, and 56 of those 312 are a single token. The other 256 are a row holding a whole translation, parse line or narrative paragraph, and the reader writes each of those as separate items instead of as the one string the row asks for.

`19-Lyon_ICSNL50_final-78` is read the same way and is finished. The 1417 rows cover the title, the abstract, all three stories as running text and again as interlinear, the twenty-five footnotes, the section metadata and the references. What remains is 41 forms the table holds that the draft does not, and 32 strings in the draft that no row holds, and the two lists are the same finding twice: every one of the 32 is a form the draft got wrong and the table therefore does not carry, so both fall in the classes the section below names.

Its reader misses 1037 of the 1413 forms, and 5 of those are a single token. The rest are a gloss line, a parse line, a word gloss or a running paragraph held whole, against a reader that writes the interlinear a token at a time.

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

Six of the 146 PDFs in `build/papers/` carry a font that renumbers its glyph codes with an `/Encoding` `/Differences` array and declares no `/ToUnicode` map. An extractor is then handed code numbers with nothing to turn them into, reads them in a default encoding, and what can land in the `.txt` is the font's private alphabet.

Every one of the six has now been opened against its own rendered pages, and they came out four different ways.

| Paper | Fonts declaring no map back to Unicode | What the page says happened |
|---|---|---|
| `19-Lyon_ICSNL50_final-78` | NimbusRomNo9L Regu, Medi, ReguItal | damaged, and read off the pages in full |
| `2013_Lindley_Lyon` | NimbusRomNo9L Regu, Medi, ReguItal | damaged the same way, and read off the pages in full |
| `Lyon-final` | NimbusRomNo9L Regu, Medi, MediItal, ReguItal | damaged, by a different table |
| `2012_Robertson` | Symbol and five TrueType subsets | damaged four ways, one of them decodable exactly |
| `21-Abraham_ICSNL50_final-4` | NimbusRomNo9L Regu, Medi | clean |
| `2011_Lonsdale_Matsushita` | Courier, Times-Roman, Times-Italic, CMSY10 | clean |

A missing `/ToUnicode` is not the fault by itself. 141 of the 146 hold a font without one and nearly all of them extract correctly, because a standard encoding already says what the codes mean and every extractor has that table. It is the renumbering that does the damage, and 6 of those 141 renumber.

The renumbering is a risk and not a verdict. Two of the six extract correctly, and both were checked line for line against a rendered page.

`21-Abraham_ICSNL50_final-4` is St'át'imc in the van Eijk orthography, which is ASCII apart from the accented vowels, so its renumbered codes never reach a character they would damage. All four pages come through, down to the apostrophes: page 1's first line reads `Ats’xenlhkán ta sásqets áku7 Nséq’a (Charlie Mack’s), lti Líl’wata Tsel’álh c.walh,` on the page and in the text.

`2011_Lonsdale_Matsushita` is clean for a different reason: it prints its Lushootseed in the ASCII transliteration its own parser reads, so there is nothing on the page for a font to lose. Page 5 sets `LEFT-WALL ?u+ da?a +d ?ElgWE? ?E kWi s+ gWistalb ti?E? SukWE? .` and the extraction gives that string exactly. The `?` is the glottal stop, `E` the schwa and `W` the labialization, on the page as much as in the file.

The list says which files to open. The page says what happened to each.

Page 23 of `2013_Lindley_Lyon` prints

> cítxʷsəlx uɬ t̓i nyʕ̓ip ck̓aʔítət

and `build/papers/2013_Lindley_Lyon.txt` holds

> cítxws@lx uì ’ti ny ’Qip c ’kaPít@t

Every schwa is `@`, every lateral fricative `ì`, every pharyngeal `Q`, every glottal stop `P`; the wedge stands in front of its letter as `ˇx`, the ejective mark stands in front of its letter instead of over it, and the word `ck̓aʔítət` is split. `pypdf` and `pypdfium2` lose the same things, so this is a property of the file and not of one library.

**One part of the loss cannot be inverted.** Labialization is written with a raised w and the page also has a plain w. Both arrive as `w`, so page `kʷukʷ` and page `wist` are the same string in the text and nothing in the file separates them.

The consequence for method is why this section exists. A hand extraction taken from one of these `.txt` files records the font's encoding and not the paper, and it then verifies clean against that same file in both directions, because both sides of the check are reading the one damaged artifact. That is how this was found here, after two such tables had been built.

What replaces it: `pdf2png.py` renders the pages to `build/pages/<stem>/page_NNN.png` and the reading is done from the image. `draft_page_text.py` writes `build/papers/<stem>.page.txt`, a draft of the page in the shared orthography, line for line with the extraction so a page of one is a page of the other, and `lyon_encoding.py` holds the rules it applies. The draft is a starting point a person corrects against the rendered page, never a source. Two things it cannot decide:

* **Labialization**, for the reason above. It writes `ʷ` after the consonants that take one, which is right for `kʷ qʷ xʷ x̌ʷ` and wrong wherever a real `w` follows a consonant. It errs in both directions, and page 315 has one of each. `púlxwi` gets a `ʷ` it should not have, and section 3.2 settles that by parsing the word `√púlx-wi`. `lkʷu·········t` loses the one it should have, because the inserted space split it into `lk` and `wu·········t` before the rule could see the consonant, and section 3.2 parses that one `√lkʷ=ut`. The rule runs over the whole line, so it reaches the English as well: stanza 117's word gloss `he.fell.off.backwards` comes out `he.fell.off.backʷards`.
* **Word boundaries.** The PDF inserts a space in front of a letter carrying a mark. `s ’plá ’ks@lx` is one word and `iP ’kl` is two, and both are a space in front of a marked letter. Page 25 settles the first as `sp̓lák̓səlx`, page 24 the second as `iʔ k̓l`.
* **The space that was not inserted.** Where the PDF leaves the mark against its letter there is nothing to key on, and a medial `’` is also the apostrophe of `Lyon’s`. `nkw’ritkw`, `xw’tilx` and `wa’y` keep the mark standing in front of its letter; the pages read them `nkʷr̓itkʷ`, `xʷt̓ilx` and `way̓`.
* **A P-initial word with no other mark on it.** `Pitx`, `Pums`, `Pistkm` and `Paksyilx` come through as English, because a capital P opening a token that holds nothing else is the shape of `Papers` and `Penticton`.
* **The dropped space.** At a change of font and at some line ends the PDF loses the space entirely, so `Okanagan yaxʷt`, `uɬ ny`, `məɬ ixíʔ` and `axáʔ ikɬcítxʷ` arrive welded into one token.
* **A run of the length mark before a closing quote.** The raised dot arrives as `;`, and a rule turns it back wherever a letter or a spacing mark follows it, because a lone `;` is also Lyon's semicolon. A run of eight is never punctuation, and `staʔx̌íləm “whoa········”` is the one place where a run is followed by a closing quote instead. It occurs twice, both in `19-Lyon_ICSNL50_final-78`, and `2013_Lindley_Lyon` holds no run at all.
* **A one-letter gloss label that is also a glyph code.** A gloss token is recognized by a run of two capitals, because one capital on its own is also `iP` and `Philosophical` (`lyon_encoding.py:116`). Stanza 108's gloss line labels the question marker `Q`, one capital, so the token falls through to the Salish branch and comes out `ʕ`. Nothing inside the token separates the two readings, and the drafter does not track which line a token sits on. It occurs twice, both in `19-Lyon_ICSNL50_final-78`.

All 83 disagreements the check reports on `2013_Lindley_Lyon` fall in these classes now that it has been read off the pages in full, and nothing else does. The last two classes occur in the priests paper alone. That count is the measurement the draft exists to produce.

One thing the extraction is better evidence for than the rendered page. The transposition moves a mark off its letter and preserves it, so a token's mark count survives the channel exactly. At the scale these pages render, a run like `m̓y̓m̓y̓á` cannot be told from `m̓ym̓á` by eye, and reading the page alone undercounts. The text settles it: stanza 112 arrives as `s- m̓ y̓- m̓ y̓-á y̓-s`, which is five marks, so the word is `s-m̓y̓-m̓y̓-áy̓-s`. Take the count from the text and the boundaries from the page.

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

The deletion fires where the paper changes font mid-sentence. Footnote 5 of the same paper sets its cited forms in italic inside roman prose, and the two arrive as `Okanaganyaxʷt` and `Okanaganyaʕyáʕt`, each one token where the page prints two words. The second is recoverable, because `yaʕyáʕt` is a common word and appears on its own elsewhere in the paper. The first is not: `yaxʷt` occurs nowhere else, so nothing in the text says where the boundary was. Which of the two happens depends on the rest of the corpus and not on the token, which is the second reason this is a distribution.

The collapse sets a ceiling. No recovery, however good, separates page `kʷukʷ` from page `wist` in the mutated text, because the channel destroyed the distinction instead of disguising it. Anything downstream that claims to have recovered a labialized consonant on these two papers is claiming something the file cannot support.

A detector keyed to a glyph inventory reads the mutated side as holding no language at all. That is not hypothetical: `is_language_token` did exactly that on these two papers until the marks set for them was corrected, and it reported a paper with eleven unread texts as fully covered. A detector reading the distribution of symbols survives the same input. The pure corpus says how well a vector separates the target language from the English around it. This pair says how much mutation it takes before it stops separating them at all.

Nothing in the tree measures the per-symbol rates yet. `font_substitution.py` scores a candidate mapping by how many mutated tokens become forms attested in a clean paper, which is a proxy for the answer. The page-verified table is the answer itself for the lines it covers, and it grows one text at a time.

**The distribution was going to convert the rest, and that has now been tested.** Four of the six papers above are set in NimbusRomNo9L, and the two with page-verified tables beside them supplied the candidate mapping for the other two. Both of those were checked against their own rendered pages. Neither takes it, for two different reasons.

`21-Abraham_ICSNL50_final-4` has nothing to convert, for the reason the section above gives. `font_substitution.py` on it is measuring a paper that is already right.

`Lyon-final` is damaged and wants a different table. It is John Lyon on the linguistic evidence for a Francis Drake landing in Oregon, and the language data in it comes from Frachtenberg's Oregon transcriptions. `ì` for `ɬ` carries over and so does the transposed acute, but the paper also needs a transposed macron the Okanagan papers never use, `E` for `ɛ`, and several more marks. One code disagrees outright. `;` is the raised length dot in the Okanagan papers and the glottalization mark here, so page 24's `kʼeuʼts!` arrives as `k;eu´ts!`, and applying the Okanagan table to this paper would silently turn every glottalization into a length mark. `font_substitution.py` puts 4 of 407 occurrences into attested forms against 5 before it, which is the same answer measured from the other side.

So the conversion table is per document and not per font family. Reading these two papers off the page still recovers two papers and still produces a table, and what that table converts is the two of them.

`2012_Robertson` is damaged four ways, and one of them costs nothing. Its extractor could not resolve a glyph and printed the glyph's name instead, so the text holds `/uni0294oo /uni026C /xé/uni0294` where page 30 sets a morphemic line. A `/uniXXXX` name carries the code point it stands for, and `/uni0294` is `ʔ`, `/uni026C` is `ɬ`, `/uni0259` is `ə` and `/uni019B` is `ƛ`. `glyph_names.py` turns all 103 of them back, along with `/combiningdotbelow` and `/combiningacuteaccent`, and after it runs no glyph name is left standing anywhere in the paper. That part is arithmetic and exact.

The other three came off the pages, and the first of them is why the decode is not the whole recovery.

* **Labialization, collapsed exactly as it is in the Lyon papers.** Page 30 sets `/kʷú[·kʷ]piʔ` and the text gives `/kwú[·kw]pi/uni0294`. The paper's own prose loses it too: page 30 names its symbols `(č, š, xʷ)` and the text holds `( č, š, x w)`, with a space where the raised letter was. Every flag the hand extraction raises on this paper so far is this one.
* **A word broken across two lines with no hyphen.** 42 lines of the extraction hold nothing but a fragment: `Da` above `vid D. Robertson`, `ma` above `ximal efficiency`. Most of them are in the interlinear, where the break falls inside a gloss: `A` above `UG-`, `s` above `ick`, `gr` above `eet`. A rule that joins every fragment line to the one under it would be wrong in the alphabet tables, where `wa`, `wi` and `waw` are Chinuk pipa letter names and each is a line of its own.
* **A glyph repeated where the page prints it once.** Page 1's epigraph prints `t’íxʷəɬ γ-ʔ-[χ-]q’y-n’-tén` with one `ɬ` and one `ʔ`, and the text holds four of each. It happens on 2 lines in the paper and both are set in bold italic.

The hand extraction is 195 rows and covers both Salish texts whole, the Chinuk pipa and Chinook Jargon forms cited through the analysis, and the two bibliography entries written in Chinuk Wawa. What it leaves is 40 forms the table holds that the text does not, and 21 strings in the text that no row holds. Those two lists are the same finding twice, as they are in the priests paper: 14 of the 21 are the damaged form of something the table carries correctly, 4 are the doubled epigraph, and 3 are page ranges in the bibliography that `is_language_token` reads as language because they hold a digit inside a token.

Texts 3 through 6 are Chinook Jargon, which is a pidgin. They are the paper's own material and they are not Salish, so nothing in them belongs in a Salish pure corpus. The table marks who each form belongs to for that reason, and `Chinook Jargon` in that column is the instruction to a reader to keep the form out of the target stream.

`extract_robertson.py` reads it against that table. It finds all 20 stanzas of Text 1 and all 7 of Text 2 with their numbering intact, and it reproduces the four rows of a stanza wherever the extraction did not break a word inside a line. Of the 92 rows it does not reproduce, 59 are the analysis prose and the cited forms, which the reader does not read and which stay in the record as unclassified. The 33 that sit inside the two texts split as 10 differing from the reader by the lost labialization alone and 23 by a word the extraction broke in the middle of a line, where the first half ends a long line and the reader has nothing short to key on.

Two things in that reader are worth naming because no other paper here needs them. The transliteration row is plain ASCII, so `o l ha l kukpi,` carries no character of the modern orthography and the test every other reader leans on would call it English. It is marked by where it sits. And the pure stream takes the transliteration row alone: the morphemic row is Robertson's analysis and holds forms nobody wrote, which is the distinction `salish_marking.py` draws between what was said and what was worked out afterward.

`line_breaks.py` holds the join, because `coverage_check.py` has to apply it too. That check compares a paper against its extraction and its own header says both sides go through the same transformation first, and a join the reader makes and the check does not reports every welded word as a hole. With it applied the paper is at 100 percent like the other eleven.

`lyon_encoding.py` holds that table for NimbusRomNo9L as it stands, with the labialization rule still a guess and the insertion rate still unmeasured.

### What the corpus measures now

The pure corpus is 2314 lines and 16898 tokens across the eleven papers with readers. 749 of those lines and 6762 of those tokens, 40 percent of it, come from the two papers set in the font that renumbers its codes.

Both of those readers now take `build/papers/<stem>.page.txt`. They used to take `<stem>.txt`, and what that put in the corpus was the font's alphabet with only the substitutions `font_repair.py` covers applied to it. The measurement that says so is `2013_Lindley_Lyon`, whose table is read off the pages in full and which takes no repair, so the table and the corpus compare token for token with nothing in between.

| | reading the extraction | reading the page text |
|---|---|---|
| corpus tokens the table holds | 1231 of 1878, 66% | 2204 of 2535, 87% |
| marks standing in front of their letter | 1832 | 54 |
| labialized consonants written with a plain `w` | 1630 | 42 |

The counts in the last two rows are for both Lyon papers together. The corpus used to hold `iks’ma’yɬtím` where the page holds `iksm̓ay̓ɬtím`, and `sqwlqwlstwíxwtət` where the page holds `sqʷlqʷlstwíxʷtət`. It now holds what the page holds.

What is left of the 13 percent is the inserted space: a word arrives in pieces and the corpus keeps the pieces, so `incítxʷ` is in it as `incítx w`. `space_repair.py` exists to close those and cannot reach these, because the vocabulary it joins with comes from the interlinear's form column, which in this paper is the parse and carries morpheme hyphens the running text does not.

`page_text.py` is what the two readers swap in for `font_repair.py`. Every repair in it returns its line unchanged, because applying the mapping to text that has already been through it destroys the text. `P` to `ʔ` turns `Papers` into `ʔapers` and the gloss label `APPL` into `AʔʔL`. Its `language_line` and `carries_orthography` had to be rewritten instead of passed through, because `font_repair`'s versions ask whether a line holds a character only the damaged orthography writes, and the page text holds none of them by construction.

`coverage_check.py` reads the same page text for these two, under a `page` entry in its repair list, and all eleven papers are back to 100 percent coverage.

### What a pure corpus is for

The purity of both sources is the constraint everything else answers to. The target is the Salishan ingestion and the contarget is the English reference, and a boundary drawn between two distributions says nothing about the languages if either side is holding something other than what it claims.

Once each dialect's corpus is pure, the dialects become each other's contargets. A boundary drawn between Nsyilxcən and Nɬeʔkepmxcín is a claim about where one ends and the other begins, and it is worth drawing because of what sits near it: forms that two dialects share and a third has lost, which is where onomatopoeia that has gone out of one of them would show.

**That comparison has to run on a sound representation in binary, not on the characters.** These orthographies write the same sound differently and the difference is arbitrary. The lateral fricative is `ɬ` in one paper here and `ł` in another. The glottal stop is `ʔ` in most and the digit `7` in the van Eijk papers. Labialization is a raised `ʷ` in some and a plain `w` in others. A model counting character bigrams reads two dialects that share a word as maximally far apart, because it is measuring the transcriber and not the speaker. Encoding each segment as its features and comparing those is what makes the distance a fact about the languages.

The plan those two things sit inside runs in three stages. First the pure text corpus, which fixes what each dialect writes and is what the whole hand extraction workflow above exists to produce. Then a sound representation measured off recordings, where a segment is a set of binary features and the alphabet in `TEXT_SPACE` maps onto those features one written symbol at a time. Then a probability distribution over that representation, which is the thing an interdialect boundary is drawn on. The word webs are built on the encoded forms, so two dialects writing one word two ways arrive at one node.

The recordings are what the sound representation is measured from, and `build/audio/` holds three of them. All three are nɬeʔkepmxcín and two are Bev Phillips, so they are enough to build an encoding against and short of what a second dialect would take. The section below names the recordings that exist and are not published with their papers.

Stage two is built and the section after this one gives what it measures. What has not been built is the join between the two: nothing takes a character out of `TEXT_SPACE` and hands back the features it stands for, and nothing builds a word web on the encoded forms. `TEXT_SPACE` in `salish_marking.py` is the inventory of characters these papers are written with, which is that join's other side, and it is one definition instead of a copy per file for that reason.

`english_sift.py --check` reports the separation as healthy against that corpus. Under one anchor, 2285 known-pure lines sit at a median of 15.15 against 1899 known-English spans at 8.49, and a cut at 9.28 keeps 99.0 percent of the pure corpus while admitting 15.3 percent of the English. Under two anchors, 96 percent of the pure lines come back as language and 98 percent of the English spans as English. Those figures say the two anchors are far apart. They do not say the language anchor is the language, and `anchor-sift-salishan.md` has what the six per-language anchors do, where they fail to separate, and why.

The same measurement on the other nine papers is not comparable and should not be read as one. Each of those goes through its own repair before the comparison, and several of those orthographies write labialization with a plain `w` on purpose.

### The binary sound representation

`sound_representation/perceived_sound.py` turns a recording into two bit fields per 10 ms frame, and `binary_sound.py` writes them to `build/sound/<stem>.bits.tsv`. Four facts about hearing set its shape, and each of them is in the code.

The waveform on its own is not enough. A vowel is a harmonic series under an envelope, and two recordings of one vowel share the envelope while the harmonics sit wherever the speaker's pitch put them, so comparing samples compares the harmonics. The analysis is spectral and the envelope is smoothed off the source before anything is compared.

What a person hears is not what the microphone recorded, and it differs between listeners by physiology and by how much hearing they have lost. The weighting is an argument, with `NO_LOSS` standing for a listener who has lost none. A real audiogram is the six frequencies a hearing test is run at with the loss in dB at each, and the loss comes off a band before loudness, so a band a listener cannot hear stops reaching the code.

Some sounds are the same sound at different frequencies. A phone said by a small speaker and by a large one differs by a scaling of the whole spectrum. The 64 bands are log spaced, so that scaling is a shift along the band axis, and the magnitude of a Fourier transform along that axis is unchanged by the shift. That transform is the segment field.

And some sounds that differ only in frequency mean different things. Stress carries an acute in every orthography in this corpus, and a field made shift invariant to buy the paragraph above would put a stressed form and an unstressed one at one address. The prosody field is separate for that reason and holds loudness, pitch against the speaker's own median, how that pitch is moving, and voicing.

**Bringing the sample to maximum entropy is what makes the delta readable.** Each bit is cut at its own column's median over the recording, so it splits the frames in half, and the segment columns are rotated onto their principal axes first so that no bit restates another. All 24 segment bits come out set on 0.500 of frames on all three recordings, and all 64 of the six bit states and all 256 of the eight bit states are reached on all three. A recording that used those states evenly would be noise, and the distance from that flat reference is the structure in it.

Measured on 2026-09-04 over the three recordings in `build/audio/`, with the entropy and total variation out of `anchor_sift.py`:

| Recording | Frames | 8 bits | 10 bits | 12 bits | Prosody |
|---|---|---|---|---|---|
| Givens and Hall, ICSNL 58 | 23489 | 0.2157 | 0.2538 | 0.3161 | 0.3708 |
| Hall and Phillips, ICSNL 59 | 83249 | 0.2364 | 0.2863 | 0.3207 | 0.3498 |
| Hall and Phillips, ICSNL 60 | 134997 | 0.1859 | 0.2200 | 0.2519 | 0.4594 |

Each number is the total variation between the codes that width of field took and the flat distribution over its states. The width is part of the figure because the field has to be sampled to be read. At 12 bits the longest recording puts 33 frames in each of 4096 states; at 24 bits it puts 133820 codes in 16.8 million states, which measures the frame count and not the recording. The run reports 6, 8, 10, 12 and 14 bits for that reason, with how many states each width reached.

All three recordings came from the nɬeʔkepmxcín Lab and two of them are Bev Phillips. These numbers are the method working and not a comparison of dialects. A second dialect's recording is what turns them into a reading.

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

Sources used to test a transformation instead of to supply text. Each answers a question the code that made the change cannot answer about itself.

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
**Date:** 2026-09-04
