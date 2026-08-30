# Carceribus — the region {#mod_confin_guide}

One region, carved from both ends by one allocator. Persist grows up from the base, interim grows
down from the top, and the free middle between them is what both take from.

## When to reach for it

- You have a buffer and you need to hand pieces of it out.
- Some of those pieces live as long as the program and some live for one operation.
- You want the failure to be "this region is full", not "the system is out of memory".

## What it composes with

Everything sits on top of it. @ref mod_spat_guide views what a region hands out; @ref mod_infin_guide
moves those views between a producer and a consumer.

## Declaring one

One declaration per region. It emits each pool's storage, its alignment, its state and the entries
bound to it, all as initialized data - nothing it does happens at run time:

```c
Carceribus(prison, MMGR_SOLUTA(work, 2048), MMGR_SECURA(keys, 2048));
```

**Nothing caps the count.** As many pools as you declare:

```c
Carceribus(four, MMGR_SOLUTA(a, 256), MMGR_SOLUTA(b, 256), MMGR_SOLUTA(c, 256), MMGR_SECURA(d, 256));
```

Three pools is as legal as four, and the sizes need not relate to each other: each pool declares its
own storage, so nothing divides and nothing is rounded. The pool count is bounded only by how many
`MMGR_CARCER_W` lines the header carries; another count is another line.

Every symbol a declaration emits carries the region's name, so a program may declare as many regions
as it likes and two of them may each hold a pool called the same thing under different watches.

Requiring a power of two twice over is what keeps the arithmetic free: an offset inside a pool is
masked rather than divided, and because every pool's size is a power of two its base lands aligned to
that size, so the pool after it starts at a sum that needs no rounding.

## The two ends

Both ends are the same block allocator. Each keeps a chain of blocks, each block behind its own
header, and each carves from the free middle. All that differs is which way the boundary moves —
and what that buys you:

|              | persist                             | interim                        |
| ------------ | ----------------------------------- | ------------------------------ |
| grows        | up, from the base                   | down, from the top             |
| lifetime     | as long as it likes                 | one call                       |
| given back   | one tenancy at a time, in any order | the whole run at once, by mark |
| a take costs | a walk of the chain, then a carve   | a carve                        |

The persistent end holds what outlives a call: key material, a runtime seed, a constant a run works
out once. The interim end holds what a call needs while it runs. A take at the persistent end looks
for a freed block it can reuse first; a take at the interim end never does, because nothing there is
released one at a time and the walk would be pure cost.

## Worked example

```c
char *const table = prison.work.persist_capio(512);
if (table == NULL) {
    return -1;                       /* the region is full; nothing moved */
}

const size_t mark = prison.work.interim_mark();
char *const  buf  = prison.work.interim_capio(256);

/* ... use buf ... */

prison.work.interim_reddo(mark);
prison.work.persist_reddo(table);
```

`capio` is _take_, `reddo` is _give back_. @ref ref_glossary has the rest of the verbs.

Note what each `reddo` is given. The interim one takes a **mark** — a value `interim_mark` handed
back — and drops everything carved since. The persistent one takes the **tenancy itself**, because
the block's own header carries its size.

## Giving back a secret

One call gives a persistent tenancy back, and what it does depends on which pool it is:

```c
prison.work.persist_reddo(table);   /* work is soluta, so the bytes are left as they are */
prison.keys.persist_reddo(secret);  /* keys is secura, so they are cleared first */
```

The guarantee is in the declaration rather than in a flag or a second call, so a caller cannot ask
for a wipe and not get one, and cannot forget to. `prison.keys` has no unwiped release to reach for.
The extent cleared is the block's own, read from its header, so a caller cannot under-wipe a tenancy
by naming fewer bytes than it holds.

`mmgr_carcer_wipe` clears an address and a count in place without giving anything back, for when a secret
is finished with but the storage is not.

Bytes are cleared on release rather than on hand-out. That means `persist_capio` does **not** return
zeroed storage: a block released with the plain `reddo` and handed out again still carries what the
last tenant left. Anything whose bytes are worth clearing is released with `secura_reddo`, and that
is what keeps the next tenant from reading it.

@note The clear uses `volatile` machine-width stores, so it survives however dead the bytes look to
the optimizer afterwards, and it handles a length or an address that is not a whole number of words
through byte edges.

## Marks nest

`interim_mark` returns a value and stores nothing, so a caller may hold two at once:

```c
const size_t outer = prison.work.interim_mark();
/* ... */
const size_t inner = prison.work.interim_mark();
/* ... */
prison.work.interim_reddo(inner);  /* inner run back */
prison.work.interim_reddo(outer);  /* and the rest */
```

`interim_reset` is the same step against the pool's own size, named because it is what the end
of a dispatch does.

## Gotchas

**A take that does not fit returns NULL and moves nothing.** Both ends fail closed rather than
crossing into each other. Test the return; that is the check.

**`octas_praesto` is the raw gap between the ends.** A persistent take also needs a block header out
of it, so the gap is not the largest request that will succeed. Ask for what you want and test the
answer rather than predicting it from this.

**A pointer that outlives its mark is dead and still readable.** Nothing moves and nothing is
scrubbed by an interim rollback, so it dereferences fine and returns whatever the next take put
there. Keep a mark and its `reddo` in the same function.

**`owns` is for asserts.** It says a pointer is inside the pool's storage. It does not say
the pointer is live.

## Sizing it

```c
const size_t left  = prison.work.octas_praesto();
const size_t grown = pool->persist_end;                 /* how far the bottom has reached */
const size_t peak  = pool->persist_hw;                  /* MMGR_ENABLE_HW_MEM_CAPACITY_CB only */
```

A pool's counters are its own state, read rather than asked for — there is no
accessor that would be a second way to spell a member read.

`persist_end` bounds the block chain rather than counting what is held: a freed block inside the
chain stays in it until a release trims the end.

The peaks are `persist_hw` and `interim_hw`, one per end so that neither is a maximum over the other,
and they are only maintained when `MMGR_ENABLE_HW_MEM_CAPACITY_CB` is on — off by default, so a run
without it leaves both at zero and a reading from that run means nothing. @ref guide_first_region has
the procedure.

## Reference

@ref mod_confin "Generated reference for carceribus"
