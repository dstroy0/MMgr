# Anchor-sift on Salishan

**Purpose:** Apply the anchor-sift method to the Salishan papers: find the language, and decide what may join a pure corpus.
**Scope:** `tools/dev_env/Salishan/`

The method is in `../anchor-sift-method.md`. This is what happens when it is fed these papers.

## The idea

English is the target. What English does not account for is what we want.

That inverts the problem into the half that is resourced. Nobody has to know Nsyilxcən or Lushootseed to find it in a paper. English has to be known, and the residue is the language, a gloss, or a page of font damage.

## The flow

```
1  pure target corpus      one per language, from the nine hand-read papers
2  pure contarget corpus   English, from the translations those readers marked
3  sift noise              screen a paper against the contarget
4  truthy against residue  measure the noise, and let section 3 say if it may be read
```

## What runs it

| Directory | Holds |
|---|---|
| `anchor_sift/` | the algorithm: squash, total variation, split-half, support and entropy |
| `corpus_script_extraction/` | the nine hand-written readers and their repairs and checks |
| `anchor_sift_algorithmic_extraction/` | the sift applied to the 143 papers with no reader |

`anchor_sift.py` has nothing to tune and no per-language term. Everything else decides what to hand it.

## Results

Nine papers read by hand, one reader each, every token accounted for:

```
9 of 9 papers at 100.0% coverage, 0 missing
2,128 lines of known-pure target language
```

The five language anchors separate cleanly from each other and from English: 15 of 15 pairs readable, distances 0.635 to 0.818 against split-half floors of 0.15 to 0.33.

Sifted from the 143 papers with no reader:

```
8,212 candidate lines across 142 papers
17,726 residue lines, the small set worth a person's time
112 papers carry the language their own front matter names
```

Admission on each corpus's own growth curve:

| Language | Pure | Candidates | Admitted | D_self before | after |
|---|---|---|---|---|---|
| nɬeʔkepmxcín | 451 | 338 | 338 | 0.3302 | 0.3077 |
| St'át'imcets | 360 | 876 | 280 | 0.2899 | 0.3443 |
| Nsyilxcən | 793 | 414 | 40 | 0.1523 | 0.1751 |
| Comox | 426 | 1288 | 0 | 0.1956 | 0.1956 |
| Nuxalk | 98 | 1193 | 0 | 0.2536 | 0.2536 |

nɬeʔkepmxcín took every candidate and got more coherent. Comox and Nuxalk refused all of theirs: tipping the whole set in took Comox from 407 cells to 2586 and its split-half distance from 0.196 to 0.457, which is a second distribution arriving rather than more of the first.

Eight languages hold candidates and have no hand-read corpus to grow: Lushootseed 907, Halkomelem 768, Montana Salish 134, Twana 71, Secwepemctsín 37, Upper Chehalis 22, Squamish 19, Straits 16.

## What it cannot do

Support is still climbing on every one of these corpora. None has seen its own alphabet, so a candidate that looks nothing like a member is not thereby wrong, and resembling what is already there is the wrong test.

The sift finds candidates. It does not read them. The corpus is still hand extracted and verified in three passes, for line and for notation, and this exists to say where to look.

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
**Date:** 2026-09-03
