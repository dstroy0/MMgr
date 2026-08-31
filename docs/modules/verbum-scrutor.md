# Verbum scrutor — the SWAR scanner {#mod_scrut_guide}

Word-at-a-time lane primitives. Branch-free, and the performance core of the library.

## When to reach for it

- You are writing a scan over bytes and want it to run a word at a time.
- You need "does this word contain X" answered without a loop over its bytes.
- You are implementing something in the `mem*` or `str*` shape and want the same engine the rest of
  the library uses.

Most callers never touch it directly — they use @ref mod_memor_guide or @ref mod_cellul_guide, which
are built on it.

## What it composes with

It is the bottom of the stack. @ref mod_memor_guide, @ref mod_cellul_guide, @ref mod_ascii_guide and
@ref mod_anchor_guide all sit on it.

## Worked example

Find the first zero byte in a buffer, a word at a time:

```c
size_t find_zero(const uint8_t *p, size_t n)
{
    size_t i = 0;
    for (; i + MMGR_SWAR_BYTES <= n; i += MMGR_SWAR_BYTES) {
        const embed_word w = EMBED_CALL(word.load, ScrutWordCfg, .at = p + i);
        const embed_word m = EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = w);
        if (m != 0u) {
            return i + EMBED_CALL(lane.first, ScrutLaneCfg, .mask = m);
        }
    }
    for (; i < n; ++i) {
        if (p[i] == 0) { return i; }
    }
    return n;
}
```

The tail is where off-by-ones live, which is why `word16` exists as an environment — it reaches the
tail four times sooner than a 64-bit carrier. See @ref ref_environments.

## Three tables

The entries split by what they operate on, and each has its own config struct.

| table  | operates on         | entries                                                                                                             |
| ------ | ------------------- | ------------------------------------------------------------------------------------------------------------------- |
| `word` | a whole word        | `load`, `load_al`, `fold_lower`, `count`                                                                            |
| `lane` | the lanes of a word | `ge`, `le`, `sub7`, `has_zero`, `eq`, `xor_`, `fam_eq`, `any_upper`, `any_digit`, `alpha`, `count`, `first`, `last` |
| `mask` | a mask of lanes     | `spread`, `drop_first`, `drop_last`, `bytes_below`, `lanes_below`, `before`, `tail`, `run`, `run_edge`              |

`lane.first` and `lane.last` are address order, not bit order. On a big-endian target they are wired
to the opposite internal entries from a little-endian one, so a caller never has to know which way
the lanes run. `mask.drop_first` and `mask.drop_last` are wired the same way.

`lane.count` counts set lanes in a mask; `word.count` is unrelated and reports how many words cover
a byte length.

## Gotchas

**The carrier is the machine word, always.** There is no separate lane width to set: the scanner
reads `EMBED_WORD_BITS` directly, so there is no second number to disagree with the first. A narrower
carrier is never faster — it moves the same cache line and answers for fewer lanes.

**A gate may be a superset.** Some entries answer "possibly" rather than "definitely" because a false
yes only costs a re-check while a false no is a bug. Read the brief on each entry before assuming an
exact answer.

**There are no builtins here, deliberately.** Measured: the open-coded trailing-zero fold was 3.609
cycles against 3.680 for `__builtin_ctzll`, and `__builtin_popcountll` is a libgcc call on baseline
x86-64. See @ref concept_swar.

**The bodies are in a `.c`, and the build has to be optimized for that to be free.** The internals
are `EMBED_INLINE` within the module, but the entries a caller reaches are ordinary functions in
another translation unit. At `-O2` with LTO the bench binary contains no call to any of them — they
inline through the dispatch table and through the archive. Built with no optimization the same calls
survive, and `lane.has_zero` measures 22 cycles instead of 3. Configure with a `CMAKE_BUILD_TYPE`.

## Reference

@ref mod_scrut "Generated reference for verbum_scrutor" · @ref concept_swar for how SWAR works ·
@ref ref_performance for what it costs
