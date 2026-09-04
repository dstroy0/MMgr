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

Eleven papers, each read by its own file in `tools/dev_env/Salishan/corpus_script_extraction/` and each with a hand extraction beside it in `hand_extraction/`. Every token of the language in each is accounted for in the corpus built from it; `coverage_check.py` is what says so.

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

| Paper | Rows read by hand | Reader |
|---|---|---|
| Mellesmoen and Kye, ICSNL 61 | 466 | reproduces it exactly |
| Hilbert, ICSNL 1983 | 60 | 8 over-runs flagged, 2 more run on by one line |
| Matthewson and Redan, ICSNL 61 | 102 | disagrees on 77 of 102, see below |
| the remaining eight | not yet read | ungraded |

Each hand extraction is a `.oracle.tsv` with its prose in a `.oracle.md` beside it, so the table passes a CSV linter: a header on line 1, no comment syntax, the same field count on every row.

`oracle_check.py` tests the hand extraction against the paper both ways. `reader_check.py` tests the reader against the hand extraction.

Two errors it found that coverage could not, because coverage was 100 percent through both:

* The Hilbert record credited Vi Hilbert as the speaker. She wrote the paper; the twenty-one examples were said by her aunt **Susie Sampson Peter** of the Upper Skagit and by **Martha LaMont**, recorded by Leon Metcalf between 1950 and 1958 and by Thom Hess in 1963.
* Example 2's English translation is the one line "High class, high class was Raven." The record had it with the next eleven lines of Hilbert's commentary welded on, as something Susie Sampson Peter said. The typescript sets examples in an indented column and the essay at full width; the extraction dropped the indent and the width is what recovers it.

* **Six of the eleven PDFs break words in half.** They leave a space after a stacked diacritic, so `K̓weswapáw̓` arrives as `K̓` and `weswapáw̓`. Counts: Hall and Phillips 996, Garcia 943, LaFontaine and Janzen 478, Mellesmoen and Kye 298, Matthewson and Redan 169, Mary George 159. The coverage check cannot see it, because it puts both sides through the same repair and a word broken on both sides matches itself. `inserted_space.py` closes it; `ʷ` is held out, because it is a spacing letter and a space after one is a real boundary.

Matthewson and Redan carries a second form of the same damage that no rule can close safely. The PDF also breaks *before* a marked letter: `cácl̓ep` arrives as `các l̓ep`. The same shape is a real word boundary at `lta q̓íl̓qa`, and nothing in the characters separates the two. The reader leaves both alone; the hand extraction has the true forms. That paper's reader is also blind to sections 1.2 and 1.3, where the story's title and five St'át'imcets forms are cited in prose.

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

`build/papers/` holds 152 extracted paper texts. Nine have readers. The rest are being built.

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
