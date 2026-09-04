# Anchor-sift on Salishan

**Purpose:** Apply the anchor-sift method to the Salishan papers: find the language, and decide what may join a pure corpus.
**Scope:** `tools/dev_env/Salishan/`

The method is in `../anchor-sift-method.md`. This is what happens when it is fed these papers.

## The idea

English is the target. What English does not account for is what we want.

That inverts the problem into the half that is resourced. Nobody has to know Nsyilxcən or Lushootseed to find it in a paper. English has to be known, and the residue is the language, a gloss, or a page of font damage.

## The flow

```
1  pure target corpus      one per language, from the eleven hand-read papers
2  pure contarget corpus   English, from the translations those readers marked
3  sift noise              screen a paper against the contarget
4  truthy against residue  measure the noise, and let section 3 say if it may be read
```

## What runs it

| Directory | Holds |
|---|---|
| `anchor_sift/` | the algorithm: squash, total variation, split-half, support and entropy |
| `corpus_script_extraction/` | the eleven hand-written readers and their repairs and checks |
| `anchor_sift_algorithmic_extraction/` | the sift applied to the papers with no reader |

`anchor_sift.py` has nothing to tune and no per-language term. Everything else decides what to hand it.

## Results

Eleven papers read by hand, one reader each, every token accounted for:

```
11 of 11 papers at 100.0% coverage, 0 missing
2314 lines of known-pure target language
```

Seven anchors: six languages and English. 20 of the 21 pairs are readable, at distances 0.510 to 0.835 against split-half floors of 0.157 to 0.641. The pair that does not read is Lushootseed against Nsyilxcən, 0.620 apart with the Lushootseed anchor's own floor at 0.641. That is a fact about the anchor and not about the two languages. Lushootseed is 219 lines out of two papers in two dialects and two orthographies, and section 3 refuses to read anything at that sample size.

That floor is the resolution the whole run is judged at, because the run takes the worst anchor's. At 0.641, 3 of the 106 papers naming a language the anchors know clear it, and the distributions agree with the prose on all 3. The other 103 come back unreadable, which is the method declining to answer.

Sifted from the papers with no reader:

```
144 papers written to build/corpora/sifted
13190 lines nearer the language, 20361 residue for a person to look at
114 of the 144 carry the language their own front matter names
```

Admission on each corpus's own growth curve:

| Language | Pure | Candidates | Admitted | D_self before | after | Support after |
|---|---|---|---|---|---|---|
| nɬeʔkepmxcín | 451 | 339 | 339 | 0.3098 | 0.3196 | 1249 |
| St'át'imcets | 371 | 868 | 280 | 0.2768 | 0.3459 | 1520 |
| Nsyilxcən | 749 | 4898 | 1000 | 0.1571 | 0.1893 | 1322 |
| Lushootseed | 219 | 1262 | 1262 | 0.6411 | 0.4163 | 2377 |
| Comox | 426 | 1275 | 0 | 0.1956 | 0.1956 | 407 |
| Nuxalk | 98 | 1359 | 0 | 0.2536 | 0.2536 | 346 |

nɬeʔkepmxcín took every candidate and stayed where it was. Comox and Nuxalk refused all of theirs: tipping the whole set into Comox takes it from 407 cells to 2586 and its split-half distance from 0.196 to 0.457, which is a second distribution arriving instead of more of the first.

Lushootseed took all 1262 and its split-half distance fell from 0.641 to 0.416. The admission rule is a ceiling and nothing else, so a corpus sitting above the band it belongs in will admit whatever pulls it down. The row stands because it is what the tool reports, and what it says is that the Lushootseed anchor is too small to gate on.

Seven languages hold candidates and have no hand-read corpus to grow: Halkomelem 739, Montana Salish 136, Twana 72, Secwepemctsín 33, Straits 23, Squamish 17, Upper Chehalis 14.

## What it cannot do

Support is still climbing on every one of these corpora. None has seen its own alphabet, so a candidate that looks nothing like a member is not thereby wrong, and resembling what is already there is the wrong test.

The sift finds candidates. It does not read them. The corpus is still hand extracted and verified in three passes, for line and for notation, and this exists to say where to look.

The Lushootseed anchor is the one to fix next, and the fix is more hand extraction. Two papers in two dialects give a corpus whose own split-half distance is 0.641, which is above every between-anchor distance the run cares about and sets the resolution for all of them. Growing it is what lets the other 103 papers be read at all.

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
**Date:** 2026-09-04
