# Verbum scrutor — the SWAR scanner {#mod_scrut_guide}

Word-at-a-time lane primitives. Header-only, branch-free, and the performance core of the library.

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
        mmgr_scrut_word w = scrut.load(p + i);
        mmgr_scrut_word m = scrut.has_zero(w);
        if (m) {
            return i + scrut.lane_first(m);   /* which lane matched */
        }
    }
    for (; i < n; ++i) {                       /* the tail */
        if (p[i] == 0) return i;
    }
    return n;
}
```

The tail is where off-by-ones live, which is why `word16` exists as an environment — it reaches the
tail four times sooner than a 64-bit carrier. See @ref ref_environments.

## The entries, by job

| job             | entries                                                            |
| --------------- | ------------------------------------------------------------------ |
| load a word     | `load`, `load_al`                                                  |
| test all lanes  | `has_zero`, `eq`, `le`, `ge`, `xor_`                               |
| build a mask    | `spread`, `sub7`, `zero_lane`                                      |
| find which lane | `lanes`, `lane_lo`, `lane_hi`, `lane_first`, `lane_last`, `drop_*` |
| bookkeeping     | `words`, `tail_mask`, `lanes_below`, `lanes_before`                |
| ASCII family    | `any_upper`, `any_digit`, `alpha`, `fold_lower`                    |
| runs            | `run`, `run_edge`                                                  |

## Gotchas

**The carrier is the machine word, always.** `MMGR_SWAR_BITS` is derived from `MMGR_WORD_BITS` and
`#undef`ed first so it cannot be set independently. A narrower carrier is never faster — it moves the
same cache line and answers for fewer lanes.

**A gate may be a superset.** Some entries answer "possibly" rather than "definitely" because a false
yes only costs a re-check while a false no is a bug. Read the brief on each entry before assuming an
exact answer.

**There are no builtins here, deliberately.** Measured: the open-coded trailing-zero fold was 3.609
cycles against 3.680 for `__builtin_ctzll`, and `__builtin_popcountll` is a libgcc call on baseline
x86-64. See @ref concept_swar.

**It is header-only and `MMGR_INLINE`.** There is no `.c`; the CMake target exists so a consumer can
link it without crossing environments.

## Reference

@ref mod_scrut "Generated reference for verbum_scrutor" · @ref concept_swar for how SWAR works ·
@ref ref_performance for what it costs
