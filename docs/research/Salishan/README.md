# The Salishan corpus work now lives in anchor_sift

**Purpose:** Find the Salishan sources, extractions and derivation that used to be under this path, and know what their license terms are now.
**Scope:** `docs/research/Salishan/`

Everything that was here moved on 2026-09-04:

> **https://github.com/dstroy0/anchor_sift**

The corpus work sits under `docs/research/Salishan/` in that repository, in the same layout it had here.

## Whose words those are

The corpus is Salishan speech, written down, and it belongs to the people who spoke it. The index in the new repository opens every entry with the speaker, before the linguist who published and before the person who read the paper into a table. A linguist wrote the paper and a person read it into a file, and neither of those is whose language it is.

## What moved

| was here | is now |
|---|---|
| `pure_corpus/` | the twenty hand extractions, speaker first |
| `refs.md` | every source, held or cited, with addresses |
| `corpus-derivation.md` | how wrong the corpus could be, and what that number rests on |
| `anchor-sift-salishan.md` | what happens when the method is fed these papers |

The tools that build and check all of it moved with them, from `tools/dev_env/Salishan/` in this tree to the same path in that one.

## Licensing

**Every license already offered for this work is transferred to the anchor_sift repository and applies there on the same terms.** Nobody holding one needs to do anything, and no term changes because the files moved. `LICENSE` and `LICENSES/` are reproduced there verbatim so the two cannot drift.

The terms are unchanged from what this tree carries: AGPL-3.0-or-later, or a negotiated commercial license, or an educator's license issued to a person.

## One condition that is not in the license text

The tools in that repository can regenerate language and can produce predictive speech. **A tool for language that comes out of that work requires a human to review its output.** Whether a regenerated form is still somebody's language belongs to a native speaker and not to an algorithm, and for a language with few remaining speakers publishing one as though a person had said it is not a recoverable harm. The repository's own README states this in full.

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
**Date:** 2026-09-04
