# Running the scripts

**Purpose:** Rebuild every corpus, check, and sift result in this directory from the papers on disk.
**Scope:** `tools/dev_env/*.py`

Run from the repository root. Everything writes under `build/`.

## Getting the papers first

None of this works without `build/papers/`, which is not in the repository. Every source is listed with its address in `refs.md`, and every ICSNL paper is linked from one page:

```
https://lingpapers.sites.olt.ubc.ca/icsnl-volumes/
```

Download the PDFs, convert each to text, and name the text file after the PDF. The readers open `build/papers/<pdf name>.txt`, so `19-Lyon_ICSNL50_final-78.pdf` becomes `19-Lyon_ICSNL50_final-78.txt`. The nine the readers need are named in `refs.md` with their exact addresses.

Fetch them with a browser. UBC answers a command-line request with a bot-defense captcha and hands back an HTML page carrying the name of the file you asked for.

Page markers matter. The readers expect `===== page N =====` on its own line between pages, which is what the coverage check reports positions against.

## Order

Nothing here depends on network access. The sift depends on the readers, because its second anchor is the corpus they produce.

```
1. the nine readers      build/corpora/*.txt, *.pure.txt, *.unclassifiable.tsv, *.words.txt
2. coverage_check.py     reads 1, reports missing tokens
3. english_sift.py       reads 1 for both anchors
4. sift_extract.py       reads 1, writes build/corpora/sifted/
```

## Where the tools are

```
tools/dev_env/Salishan/anchor_sift/                          the algorithm
tools/dev_env/Salishan/corpus_script_extraction/             the nine readers
tools/dev_env/Salishan/anchor_sift_algorithmic_extraction/   the sift applied
```

Each script finds the repository by walking up to the tree holding `build/`, so they run from any working directory and survive being moved again.

## The nine readers

One per paper. Each takes no arguments and rebuilds that paper's corpus. Written `S/` for
`tools/dev_env/Salishan/corpus_script_extraction/`.

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
```

Each prints its line counts, how many target-language spans reached the pure stream, and how many lines it could not sort. The two Lyon readers also report how many interlinear blocks read cleanly.

## Checks

```
python S/coverage_check.py
```

Diffs every language token in each paper against the corpus built from it. Both sides are put through the same repairs first. Expected result is 0 missing on all nine.

```
python S/font_substitution.py <damaged paper> <clean reference> [more]
python S/font_substitution.py 19-Lyon_ICSNL50_final-78 LyonICSNL60_Inch-2
```

```
python S/case_delta.py
```

Matches the sifted output case-sensitively against the pure corpus. A form that matches only once
case is folded is the same word written wrong, and the count of those is the formatting damage.

Tests a candidate character mapping by counting how many damaged tokens become forms attested in a clean paper. Read the change between the two rates, not either one alone.

## The sift

Written `A/` for `tools/dev_env/Salishan/anchor_sift/` and `X/` for
`tools/dev_env/Salishan/anchor_sift_algorithmic_extraction/`.

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
```

Reads which language each paper is about out of its own title and abstract.

```
python X/sift_extract.py
```

Sorts every paper without a reader against both anchors and writes `build/corpora/sifted/`: one file per paper, one candidate corpus per language, and `index.tsv` over the lot.

```
python X/language_check.py
```

Asks whether the distributions agree with what each paper says it is about, with section 3 deciding whether the reading may be made at all.

```
python X/corpus_growth.py
python X/corpus_admit.py
```

The first prints each corpus's split-half distance, support and entropy as it grows. The second admits candidates in batches while the corpus stays on that curve, and writes what it admitted and what it refused.

## What the modules are for

These are imported, not run.

| Module | Holds |
|---|---|
| `A/anchor_sift.py` | the method itself: squash, total variation, split-half, support, entropy |
| `A/english_sift.py` | the English anchor and the per-line screen built on it |
| `S/salish_marking.py` | the T and N marking convention, `tagged_spans`, `CAPS_RUN`, ligatures |
| `S/salish_unsorted.py` | the flag file, and what counts as a language token |
| `S/font_repair.py` | the Lyon substitution table and the three forms of applying it |
| `S/space_repair.py` | putting back together words the extraction split internally |

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
**Date:** 2026-09-03
