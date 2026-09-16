# This research now lives in anchor_sift

**Purpose:** Find the work that used to be under this directory, and know what its license terms are now.
**Scope:** `docs/research/`, `tools/dev_env/`

The anchor sift research, the ledger, the Salishan corpus work and the Python tools that produce all of it have moved to their own repository:

> **https://github.com/dstroy0/anchor_sift**

That is where the work continues. Anything here is a copy kept for reference and the repository above is the one to read.

## What moved

`docs/research/` and `tools/dev_env/` in full: the ledger, the method, the terms glossary, the Salishan sources and derivation, the hand extractions with their speaker index, and the 237 Python tools that build and check them.

## What stayed

The C implementation of the sift and its benches. `src/impensa_ancorae_acus/` holds the byte cost lookup and `test/bench/bench_ancorae_{sift,lattice,entropy,ab,cycles}.c` holds the measurements. The ledger cites those paths, and they live here and not in the research repository.

## Licensing

**Every license already offered for this work is transferred to the anchor_sift repository and applies there on the same terms.** Nobody holding one needs to do anything, and no term changes because the files moved.

The terms are unchanged from what this tree carries: AGPL-3.0-or-later, or a negotiated commercial license, or an educator's license issued to a person. `LICENSE` and `LICENSES/` are reproduced in the new repository verbatim so the two cannot drift.

## For anyone holding a license through a department

A computer science department that wants to keep its access to MMgr can keep it, and nothing here withdraws that. It is worth saying plainly that MMgr is a memory manager for embedded parts. It is careful work and it is not novel, and holding access to it grants nothing about the research above beyond what the license already says.

The part worth reading is in the anchor_sift repository.

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
**Date:** 2026-09-04
