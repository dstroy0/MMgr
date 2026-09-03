# The anchor-sift detector

**Purpose:** Check a body of text for corruption against a known inventory, measure whether it carries a signature that separates it from other languages, and read either result without mistaking encoding density or sample size for the thing being measured.
**Scope:** `tools/dev_env/salish_purity.py`, `tools/dev_env/corpus_gate.py`, `tools/dev_env/byte_signature.py`, `tools/dev_env/case_or_splitting.py`, `tools/dev_env/cluster_profiles.py`, `tools/dev_env/evidential_pressure.py`

## 1. What it produces

The instrument answers one question at a time and answers it with one bit.

Given a body of text $T$ and the inventory $S$ its writing is built from, where each $s \in S$ is the set of characters that can carry one distinctive part of that writing, it reports how much of the inventory survives in the text (`tools/dev_env/salish_purity.py:57-71`):

$$\operatorname{sets}(T) \;=\; \sum_{s \in S} \mathbb{1}\bigl[\, \exists\, c \in T \ \text{with}\ c \in s \,\bigr]$$

Given two bodies of text reduced to distributions $P$ and $Q$ over the $65536$ ordered byte pairs, it reports how far apart they sit as total variation distance (`tools/dev_env/byte_signature.py:96-99`):

$$D(P,Q) \;=\; \tfrac{1}{2} \sum_{k=0}^{65535} \bigl| P(k) - Q(k) \bigr|$$

$D$ runs from $0$ when the two distributions agree everywhere to $1$ when they share no byte pair at all.

No output carries a reason. A text that fails the inventory check is not repaired by the check, and the check names no substitute for what went missing. That is the relationship an error-detecting code has to an error-correcting one. Over a code of minimum distance $d_{\min}$, detecting $t$ errors requires $d_{\min} \ge t+1$, and correcting the same $t$ requires $d_{\min} \ge 2t+1$. Correction costs about twice the redundancy, because detection establishes that a received word is not a codeword while correction identifies which codeword was sent. This instrument was built with the first and not the second.

The consequence is that every result is a proposal for a person to accept or reject. The instrument reports that a file lost its ejectives. What the file said is outside what it computes.

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
**Date:** 2026-09-03
