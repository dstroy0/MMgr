# Running the scripts

**Purpose:** Go from an empty checkout to every corpus, check and sift result in this directory, without owning the archive first.
**Scope:** `tools/dev_env/Salishan/`

Run from the repository root. Everything writes under `build/`.

## Start here

```
python -m pip install pypdf requests
python tools/dev_env/Salishan/get_papers.py
```

That fetches the eleven papers the readers need and writes `build/papers/<name>.txt` for each. Nothing else in this directory works until it has run, and it is the only step that touches the network.

`--all` fetches all 990 papers in the archive instead of the eleven. `--list` prints what would be fetched and stops. `--convert` converts PDFs already sitting in `build/papers/` and fetches nothing. `--stem <name>` takes one paper by its filename.

## Why there is a script for it

The instructions used to say to download the PDFs, convert each to text, and name the text after the PDF. Two of those three steps have a wrong answer that looks like a right one.

**The encoding.** `pdftotext` writes Latin-1 unless told otherwise, and Latin-1 has no ʔ, no ə and no ɬ. A paper converted that way still opens and still looks like a paper, with the language taken out of it.

**The reading order.** Given `-enc UTF-8` the characters survive, and the page comes out in layout order, so a running header prints above the title it sits under and a two-column table interleaves. `pypdf` reads a page in the order the PDF stores it, which is what the readers were written against, and `get_papers.py` uses it. Converting `Mellesmoen_Kye_ICSNL61.pdf` gives 1462 lines identical, in order, to the copy in `build/papers/`.

The script identifies itself by name, purpose and address in its user agent. The archive answers a request carrying none with an HTML page instead of the file; its `robots.txt` disallows only `wp-admin`, `wp-login`, the cache and trackbacks.

## Order

Nothing after the fetch needs the network. The sift depends on the readers, because its second anchor is the corpus they produce.

```
1. get_papers.py       build/papers/*.txt
2. the eleven readers  build/corpora/*.txt, *.pure.txt, *.unclassifiable.tsv
3. oracle_check.py     the hand extractions against the papers
4. reader_check.py     the readers against the hand extractions
5. coverage_check.py   reads 2, reports missing tokens
6. english_sift.py     reads 2 for both anchors
7. sift_extract.py     reads 2, writes build/corpora/sifted/
```

## Where the tools are

```
tools/dev_env/Salishan/get_papers.py                         the archive
tools/dev_env/Salishan/hand_extraction/                      the control
tools/dev_env/Salishan/anchor_sift/                          the algorithm
tools/dev_env/Salishan/corpus_script_extraction/             the eleven readers
tools/dev_env/Salishan/anchor_sift_algorithmic_extraction/   the sift applied
```

Each script finds the repository by walking up to the tree holding `build/`, so they run from any working directory.

## The eleven readers

One per paper. Each takes no arguments and rebuilds that paper's corpus. Written `S/` for `tools/dev_env/Salishan/corpus_script_extraction/`.

```
python S/extract_garcia.py
python S/extract_hall_phillips.py
python S/extract_lafontaine_janzen.py
python S/extract_matthewson_redan.py
python S/extract_alexander_davis.py
python S/extract_mary_george.py
python S/extract_nater_bella_coola.py
python S/extract_lyon_priests.py
python S/extract_lindley_lyon.py
python S/extract_hilbert.py
python S/extract_mellesmoen_kye.py
```

Each prints its line counts, how many target-language forms reached the pure stream, and how many lines it could not sort. The two Lyon readers also report how many interlinear blocks read cleanly.

## The workflow the readers exist inside

A reader is not the source of the corpus. The hand extraction is, and the reader is graded against it. Written `H/` for `tools/dev_env/Salishan/hand_extraction/`.

```
python H/oracle_check.py
```

Checks each hand extraction against the paper it was read off, in both directions. A form written down that the paper does not hold is a typing slip. A word in the paper that no row holds is a row somebody skipped, which is what a person reading a thirty-seven page paper into a table actually produces. Both must be zero.

```
python H/reader_check.py
```

Checks each reader against the hand extraction. Grades whether it found each form, whether it says the same kind, and whether it says the same dialect. Kind is the one that decides the corpus: a rejected tableau candidate filed as a citation puts a spelling the paper's own analysis rejects into the pure stream, and nothing downstream asks again.

## Checks

```
python S/coverage_check.py
```

Diffs every language token in each paper against the corpus built from it. Both sides go through the same repairs first. Expected result is 0 missing on all eleven.

```
python S/font_substitution.py <damaged paper> <clean reference> [more]
python S/font_substitution.py 19-Lyon_ICSNL50_final-78 LyonICSNL60_Inch-2
```

Tests a candidate character mapping by counting how many damaged tokens become forms attested in a clean paper. Read the change between the two rates, not either one alone.

```
python S/case_delta.py
```

Matches the sifted output case-sensitively against the pure corpus. A form that matches only once case is folded is the same word written wrong, and the count of those is the formatting damage.

## The sift

Written `A/` for `tools/dev_env/Salishan/anchor_sift/` and `X/` for `tools/dev_env/Salishan/anchor_sift_algorithmic_extraction/`.

```
python A/english_sift.py --check
```

Calibrates against the control. Prints how the known-pure corpus and the known-English spans separate, under one anchor and under two.

```
python A/english_sift.py <paper stem> [more stems]
python A/english_sift.py 1983_Hilbert
```

Scores one paper's lines against English and prints the twenty English accounts for least. A stem is a filename in `build/papers/` without its extension.

```
python X/paper_language.py
python X/sift_extract.py
python X/language_check.py
python X/corpus_growth.py
python X/corpus_admit.py
```

The first reads which language each paper is about out of its own title and abstract. The second sorts every paper without a reader against both anchors and writes `build/corpora/sifted/`. The third asks whether the distributions agree with what each paper says it is about, with section 3 deciding whether the reading may be made at all. The last two print each corpus's split-half distance, support and entropy as it grows, and admit candidates in batches while the corpus stays on that curve.

## What the modules are for

These are imported, not run.

| Module | Holds |
|---|---|
| `A/anchor_sift.py` | the method itself: squash, total variation, split-half, support, entropy |
| `A/english_sift.py` | the English anchor and the per-line screen built on it |
| `S/salish_marking.py` | the T and N marking convention, `tagged_spans`, `CAPS_RUN`, ligatures |
| `S/salish_unsorted.py` | the flag file, and what counts as a language token |
| `S/font_repair.py` | the Lyon substitution table and the three forms of applying it |
| `S/space_repair.py` | putting back together words the Lyon extraction split internally |
| `S/mellesmoen_kye_repair.py` | the inserted space, the two ejective marks, and NFC |

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
**Date:** 2026-09-03
