# Banned words, phrases and shapes

**Purpose:** One list of everything the three standards ban, so the documentation pass after the port
to `src` has something to check against instead of re-reading three skills per paragraph.
**Scope:** `~/.claude/skills/code-c11/SKILL.md`, `~/.claude/skills/code-comments/SKILL.md`,
`~/.claude/skills/code-documentation/SKILL.md`, and the MMgr memory rules that outrank them.

This is a checkpoint, not a standard. Where it disagrees with a skill, the skill wins.

## Hard bans, single tokens

| token | where | why it is banned |
|---|---|---|
| `rather`, `rather than` | comments, docs | the hinge of the contrast tic. State the fact; give the alternative its own sentence. |
| `spelling` | comments | stands in for the thing instead of naming it. Write `_Static_assert`, or "the C11 form". |
| `so a` | comments | opens the personified consequence clause every time. |
| `add up` | docs | an idiom standing in for what the code does. |
| `load-bearing` | comments, docs | claudese. |
| em-dash | comments, docs | the single most recognizable machine tell. Period, comma, semicolon, or recast. |

Hyphens inside compound words and the ranges in a `path:line` citation are not em-dashes.

## Nothing inanimate speaks

A name, a token, or a type does not do any of these:

`say` · `signal` · `encode` · `convey` · `make clear` · `advertise` · `announce`

Banned by name: *"and the spelling says so at every table it declares"*. Cut the clause; the sentence
survives it.

## Personified machinery

*"a build is asked what it supports"* · *"the header answers"* · *"the compiler is told"*

A build is not asked anything. Name what the code does.

## Rhetorical sentence shapes

Antithesis, parallelism, and colon-then-elaboration are essay construction. Banned by name:

- *"Declared, not allocated."*
- *"A number, not a guess."*
- *"Both ends X through the same Y and Z through the same W. Only ... differs: ..."*
- *"A is B's alone; C does D and nothing more."*

The X-not-Y shape sounds decisive and carries almost nothing. Two of them in one paragraph is the same
tell as an em-dash.

## Inverted and aphoristic clauses

*"Spelling it once keeps the question here"* · *"Both are properties of the C standard, not of a
compiler"* · *"That is the difference between a helper naming a step and a helper costing a call"*

These read as conclusions to an argument nobody made.

## Claudese

`Certainly!` · `Let's dive in` · `It is important to remember` · any preamble, apology,
meta-commentary, or wrap-up filler.

## American English only

No `-ise` or `-isation` where American takes `-ize` or `-ization`. No `-our` for `-or`. No `-re` for
`-er`. No doubled `l` in `modelled`, `labelled`, `signalled`. `whilst` and `amongst` are out.

Correct: `initialize` `behavior` `synchronize` `optimization` `devirtualize` `analog` `color` `center`
`defense` `gray` `catalog` `license`

## Verbs wearing a definite article

*the take* · *the carve* · *the fit walk* · *the merge* · *the give-back*

Nominalizing an operation invents a dialect one file speaks. The call allocates, frees, splits,
searches. Where a noun is needed it is *allocation*, *block*, *request*.

## Vocabulary from outside the module's metaphor

`locus_carcerum` is a place of prisons. A cell holds a **prisoner**, never a **tenant**. Rental
vocabulary is banned: `tenant` · `lease` · `landlord`.

Memory management has settled terms and they are correct: allocate, free, realloc, release, persist,
split, coalesce, first fit, free list, header, payload, alignment, high-water mark, top, base. Do not
mint a private word for an operation that already has one.

## Names

Banned as names: any single character, with or without an underscore. `r_` `w_` `_a` `a` `b` `d` `x`
`n` `_1`

Banned as abbreviations: `blk` for cellblock · `ctx` for context · `hw` for hardware · `buf` where the
full word fits. The exception is a term the field itself abbreviates, spelled the way the field spells
it.

Banned as lies: `args` for a single value · `scratch` for a temporary · a plural for one thing.

Banned as macro parameters: a letter. A parameter is a real name with a trailing underscore
(`attribute_`, `bytes_`, `region_`). Positional discards in a counting macro are `slot1_`…`slotN_`
with the returned position named for its purpose.

**Not violations.** The storage-class markers `s_` `g_` `e_` `t_` `v_` `c_` `p_` `m_` when they prefix
a real name. Identifiers the standard or the compiler defines, spelled as given: `_Static_assert`
`_Alignas` `_Bool` `_Generic` `_Noreturn` `_Atomic` `_Pragma` `__VA_ARGS__` `__FILE__` `__LINE__`
`__func__` `__attribute__` `__has_attribute` `__has_builtin` `__GNUC__` `__clang__` `__cplusplus`
`__STDC_VERSION__` `__BYTE_ORDER__` `__ARM_FEATURE_UNALIGNED`. A bare `_` between paste operators, as
in `region_##_##name_##_bytes`, which is a token being pasted into a symbol.

## Uncited absolutes

`always` · `never` · `only` · `every` · `cannot`

Each carries a citation in the same sentence, or it comes out. A claim about behavior needs one. A
sentence naming what a module is for does not, and neither does a consequence the reader draws from
the sentence before it.

Citation density is its own defect. A `path:line` on every sentence breaks the line the prose is
walking. Group a passage's evidence into one citation at the point it is claimed.

## Structural bans

**Code.** Side effects in a controlling expression without a preceding justification. An increment
folded into a volatile access. A cast across signedness or width with no comment. An untyped numeric
literal. `/* */` inside a function body. Dead code. Address arithmetic from a container and an index
where a symbol exists. A null check, a bounds test, a `goto cleanup`, a pointer typedef, or a hoisted
declaration imported because another codebase does it.

**Comments.** One block per declaration; a family of near-identical macros takes one block each. A
`@param` documenting a letter. Commented-out code.

**Docs.** Aspirational documentation. A guarantee stated twice. A section for removed code. A heading
with no body. A figure that does not state what it shows and which file it describes.

## What a passing edit still has to do

A section is written for someone reading it start to finish. A paragraph that is a row of disconnected
true facts has failed even where every fact is correct and every citation holds. A passage rewritten
until it satisfies a rule and no longer says anything is a regression, and the rule was applied
wrongly. Read the result against the original before keeping it.

## Where the praet suite stands, 2026-09-01

158 hits across 16 files, counting the single-token bans and the inanimate-speaks verbs only. The
sentence shapes are not machine countable and are not in that figure, so the real number is higher.

Heaviest: `test_praet_correctness.c` at 36, `praet_ordo.h` at 25, `praet_configuration.md` at 13,
`praet_ordo.c` at 14, `praet_praefinitum.h` at 12.

None of this is corrected yet. The module is going to hardware and then to `src`, and prose about
files that are about to move is work done twice. This figure is here so the pass after the port has
something to measure against.

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
**Date:** 2026-09-01
