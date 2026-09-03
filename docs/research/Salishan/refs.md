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

Conditions the speakers set:

* Bernice Garcia asks it be acknowledged she is a Kamloops Indian Residential School speaker re-learning her language.
* wlwlmelst shares his four stories freely for people connecting with the language. They came from his mother nxwelinek and his grandmother ʔústko.
* George Lezard's narrative: transcribed by Larry Pierre 1970, updated by permission of Arnie Baptiste, his son.
* Nellie Guitterez's story: reprinted by permission of Lynne Jorgesen, her great-granddaughter.

## Text sources, extracted

Nine papers, each read by its own file in `tools/dev_env/`. Every token of the language in each is accounted for in the corpus built from it; `tools/dev_env/coverage_check.py` is what says so.

Every address below was checked. The local filename in `build/papers/` is the PDF's own name, so any paper in the archive resolves the same way.

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

Each reader writes four files into `build/corpora/`: the marked record, a `.pure.txt` holding only target-language speech, a `.unclassifiable.tsv` listing what it could not type, and for the two Lyon papers a `.words.txt` giving the word forms recovered from the interlinear.

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
