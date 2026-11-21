# Who owns what {#concept_ownership}

Every pointer in this library belongs to something with a shorter life than you expect. This page is
the rules.

## The one rule

**MMgr owns nothing.** Every byte it passes out is inside a buffer you gave it. Every "allocation" is
a bump of a pointer into that buffer, and every "free" sets a cell's header free.

So the question is never "did something free this". It is "is the thing it points into still valid,
and did the cell it lies in go back".

## Lifetimes, shortest first

| what you hold         | dies when                                                |
| --------------------- | -------------------------------------------------------- |
| a temporary pointer   | `persistent_buf_release` takes it, or its cellblock dies |
| a span over temporary | same, and it does not know                               |
| a persistent pointer  | `persistent_buf_release` takes it, or its cellblock dies |
| the cellblock         | its declaration goes out of scope, which is never        |
| the pool              | the same, and it outlives the cellblock dressed over it  |

Read that table downward: the row below a row ends everything above it. A span over a cell dies when
the cell goes back, and nobody tells the span.

## The two tiers are one algorithm

```c
void *const cell = prison.work.temporary_buf_alloc(256u, reader);

/* ... use cell ... */

prison.work.persistent_buf_release(cell, reader);
```

The temporary tier is the persistent tier with its base at the cellblock's size. It grows down, a
take bumps its top, and a give frees the cell's header. A take the gap cannot meet merges its free
runs and reuses them. The one release serves both tiers, since the cell's offset says which tier it
lies in. The tiers differ in what they are for: a temporary cell lives inside one call, a persistent
cell lives on.

Nothing reallocates. `cell` dereferences with no fault after its release and returns whatever is
there, which, after the next allocation, is somebody else's data. This is the single likely way to
misuse this library. On a max-security cellblock the release zeroes the cell first, and a read
afterwards gets zeros and not the previous prisoner. On a minimum-security one the release scrubs
nothing.

The discipline that prevents it: **a temporary take and its release live in the same function**.
Do not return the pointer or store it anywhere that outlives the function. If you need the result to
survive, copy it into the persistent tier or into a caller-supplied span before the release.

## Spans borrow and do not know

```c
mmgr_span s = EMBED_CALL(spat.from, SpatiumCfg, .buf = p, .cap = 256u);
```

`s` holds `p`. It does not own it, cannot extend it, and will not notice when it dies.
`s.buf` says which buffer it got. Whether that buffer is still alive is not a question a span can
answer.

A span is safe to copy by value, safe to pass by value, and safe to return **only if
its target outlives the return**. A function that gives the cell back and returns a span over
interim commits a use-after-free with extra steps.

## A cellblock does not ask who is calling

A cellblock is one dress over one pool. It does not ask which context reached it: a memory manager
has no concept of a worker. The moment it tries to have one, it needs a count of them and an index
per allocation. It then needs a registry that maps platform contexts to that index, and a spare
cellblock for when the index is wrong.

One entry answers an ownership question directly:

```c
/* is this pointer inside this cellblock at all */
prison.work.who_owns_buf(cell);
```

One unsigned subtract and one compare. It exists for asserts and for a debugger, never for control
flow. It says the pointer is inside the cellblock's storage, and nothing about whether it is live.

If two execution contexts must not share, declare two pools and hand each context its own. Neither
cellblock learns there were two: there is nothing to count, nothing to index, and nothing to
check. See @ref mod_confin_guide.

## A take names its owner

The caller says who a cell is for. Every `persistent_buf_alloc`, `temporary_buf_alloc`,
`persistent_buf_release` and `buf_grow` takes a `const CellOwner *` beside its size or its prisoner,
and none takes `NULL`. The owner is the library's, and it is data. The program lists its owners
once. `MMGR_OWNERS` positions the table of their records and defines each owner's const pointer into
it, and the linker resolves the pointer. Nothing registers at run time, and nothing runs before the
first take:

```c
#define PROGRAM_OWNERS(owner_) owner_(reader, "reader")
MMGR_OWNERS(PROGRAM_OWNERS)

void *const cell = prison.work.persistent_buf_alloc(256u, reader);
/* ... */
prison.work.persistent_buf_release(cell, reader);

void *const scratch = prison.work.temporary_buf_alloc(256u, reader);
/* ... */
prison.work.persistent_buf_release(scratch, reader);
```

The list is a macro that applies one macro to each owner, `owner_(name_, text_)`. `name_` is the
identity the owner's module names, and `text_` is the name a reader prints. `MMGR_OWNERS` expands it
three ways in the one translation unit that owns the program's memory:

- an enumeration that gives each owner its place;
- `mmgr_owner_table`, which holds one record for each in that order, with `mmgr_owner_table_count`
  beside it;
- `const CellOwner *const name_` for each, which points at its record.

A list longer than `MMGR_CARCER_OWNERS` fails the build. The cellblock refuses a take that names no
record of the table, the same way it refuses a take the gap cannot meet. One
unsigned subtract puts the pointer inside the records positioned, and one remainder puts it on a
record's first byte; `mmgr_owner_listed` asks the same question. A reader walks the table by its
count, and `mmgr_owner_in_use` and `mmgr_owner_hw` read a record's counts.

A cell of either tier counts to its owner until its release.

`mmgr_carcer_hw_clock_ms` and `mmgr_carcer_hw_misnamed` are weak. The default clock answers 0 every
time; the default report does nothing. A port defines both names.

The take adds the cell's payload, read off its header, to `reader.in_use` and raises `reader.hw` to
it; the release takes the same bytes off. Every cell retains its owner in the word behind its
payload. A release and a growth each count the cell's bytes off that owner.

A release or a growth must name it. The cellblock refuses one that names another owner, and the
refusal changes nothing, the payload of a max security cell included. It calls
`mmgr_carcer_hw_misnamed` with the owner named, the owner the cell counts to and its bytes. The
owner's record is the table's storage. The caller writes it through the const pointer it holds, as
it writes a movable entry. The cellblock still does not ask which context reached it.

## Concurrency

Every context that reaches a cellblock shares it. Each tier's boundary, each cell's owner word and
every owner's count is an atomic word, and every move of one is a single step:

- a take bumps its boundary by one fetch-add through the cellblock's turnstiles;
- a give sets the owner word's completion bit by one fetch-or;
- a count moves by one add.

No take or give waits for another. A compaction, a growth and a move close the cellblock, wait out
the passes under way, and work alone. One build serves a host thread, a part's task and a GPU
thread.

The ring in `memoria_anularis` is **single-producer, single-consumer only**. Two producers on one
ring is not a slower correct program, it is a broken one. See @ref mod_anular_guide.

## What a check would catch, and what none of them do

The preconditions that the library asserts are in `spatium`, `bitorum_introitus_exitus`,
`memoria_anularis`, `memoriam_praetereo` and `ascii_persona_bitorum`. They catch a span with no
buffer, a bit writer with no capacity and a ring with no storage. They also catch a channel that
does not exist and a character class out of range.

In a build you ship none of them fire. The default `MMGR_ASSERT` in `mmgr.h` type-checks the
condition with `sizeof` and then discards it: it costs nothing and cannot rot.

The `checks` environment is where they are checks. `MMGR_DEBUG_CHECKS=1` selects the form in
`test/support/mmgr_host_traps.h` that traps. It reports the expectation, the file and the line and
then aborts. An expectation a caller broke fails a test there and is not a no-op nobody notices. It
is the only environment that evaluates an assert. So an expectation that nothing exercises under
`checks` is an expectation that no test checks.

A target with no `stderr` and no `abort` defines `MMGR_ASSERT` itself before it includes the header,
and the library uses neither form.

`locus_carcerum` reaches for `MMGR_FATAL` instead, which holds in every build. A release that gets a
prisoner from another cellblock shows that the caller does not know which memory it holds. To go on
would move this cellblock's boundaries with a header read out of another's bytes.

What no check catches, in any configuration, is a **stale pointer to a released cell**. Nothing can: the
memory is readable, the value is plausible, and nothing in the pointer records that the cell went back.
That one is on you, and it is why a take and its release belong in the same function.
