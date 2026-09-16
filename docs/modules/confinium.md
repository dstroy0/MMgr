# Locus carcerum — the region {#mod_confin_guide}

One region, carved from both ends by one allocator. Persist grows up from the base, interim grows
down from the top, and the free middle between them is what both take from.

## When to reach for it

- You have a buffer and you need to hand pieces of it out.
- Some of those pieces live as long as the program and some live for one operation.
- You want the failure to be "this region is full", not "the system is out of memory".

## What it composes with

Everything sits on top of it. @ref mod_spat_guide views what a region hands out; @ref mod_anular_guide
moves those views between a producer and a consumer.

## Declaring one

Declare the pools, then dress them. A pool declaration emits the storage and its alignment;
`LocusCarcerum` emits each cellblock's state and the entries bound to it. All of it is initialized
data, and nothing either one does happens at run time:

```c
ParsMemoriaeInternae(work, 2048);
ParsMemoriaeInternae(keys, 2048);

LocusCarcerum(prison, MMGR_MINIMUM_SECURITY(work), MMGR_MAXIMUM_SECURITY(keys));
```

**Nothing caps the count.** As many cellblocks as you declare pools for:

```c
ParsMemoriaeInternae(alpha, 256);
ParsMemoriaeInternae(beta, 256);
ParsMemoriaeInternae(gamma, 256);
ParsMemoriaeInternae(delta, 256);

LocusCarcerum(four, MMGR_MINIMUM_SECURITY(alpha), MMGR_MINIMUM_SECURITY(beta),
              MMGR_MINIMUM_SECURITY(gamma), MMGR_MAXIMUM_SECURITY(delta));
```

Three cellblocks is as legal as four, and the sizes need not relate to each other: each is declared
over its own pool, so nothing divides and nothing is rounded. The count is bounded only by how many
`MMGR_CARCER_W` lines the header carries; another count is another line.

A cellblock is reached by the name of its pool, and a pool name stands for one region. Declaring one
name twice fails the build, so two sites cannot hold cellblocks of the same name and a program never
has two meanings for one symbol.

Requiring a power of two is what keeps the arithmetic free: an offset inside a cellblock is masked
instead of divided, which holds only for a power of two. `MMGR_CARCER_BODY` asserts it at the
declaration, against the pool's own `sizeof` and not against the count it was handed.

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
char *const table = prison.work.persistent_buf_alloc(512);
if (table == NULL) {
    return -1;                       /* the region is full; nothing moved */
}

const size_t mark = prison.work.temporary_buf_mark();
char *const  buf  = prison.work.temporary_buf_alloc(256);

/* ... use buf ... */

prison.work.temporary_buf_release(mark);
prison.work.persistent_buf_release(table);
```

@ref ref_glossary decodes the Latin the module names are built from.

Note what each release is given. The temporary one takes a **mark** — a value `temporary_buf_mark`
handed back — and drops everything carved since. The persistent one takes the **cell itself**,
because the block's own header carries its size.

## Giving back a secret

One call gives a persistent tenancy back, and what it does depends on which pool it is:

```c
prison.work.persistent_buf_release(table);   /* minimum security: the bytes are left as they are */
prison.keys.persistent_buf_release(secret);  /* maximum security: they are cleared first */
```

The guarantee is in the declaration rather than in a flag or a second call, so a caller cannot ask
for a wipe and not get one, and cannot forget to. `prison.keys` has no unwiped release to reach for.
The extent cleared is the block's own, read from its header, so a caller cannot under-wipe a tenancy
by naming fewer bytes than it holds.

`mmgr_zero_buf` clears an address and a count in place without giving anything back, for when a secret
is finished with but the storage is not.

Bytes are cleared on release rather than on hand-out. That means `persistent_buf_alloc` does **not**
return zeroed storage: a block released from a minimum-security pool and handed out again still
carries what the last occupant left. Anything whose bytes are worth clearing is declared under
`MMGR_MAXIMUM_SECURITY`, whose release zeroes the cell first, and that is what keeps the next
occupant from reading it.

@note The clear uses `volatile` machine-width stores, so it survives however dead the bytes look to
the optimizer afterwards, and it handles a length or an address that is not a whole number of words
through byte edges.

## Marks nest

`temporary_buf_mark` returns a value and stores nothing, so a caller may hold two at once:

```c
const size_t outer = prison.work.temporary_buf_mark();
/* ... */
const size_t inner = prison.work.temporary_buf_mark();
/* ... */
prison.work.temporary_buf_release(inner);  /* inner run back */
prison.work.temporary_buf_release(outer);  /* and the rest */
```

`temporary_buf_reset` is the same step against the pool's own size, named because it is what the end
of a dispatch does.

## Gotchas

**A take that does not fit returns NULL and moves nothing.** Both ends fail closed rather than
crossing into each other. Test the return; that is the check.

**`buf_available` is the raw gap between the ends.** A persistent take also needs a block header out
of it, so the gap is not the largest request that will succeed. Ask for what you want and test the
answer rather than predicting it from this.

**A pointer that outlives its mark is dead and still readable.** Nothing moves and nothing is
scrubbed by an interim rollback, so it dereferences fine and returns whatever the next take put
there. Keep a mark and its release in the same function.

**`who_owns_buf` is for asserts.** It says a pointer is inside the pool's storage. It does not say
the pointer is live.

## Sizing it

```c
const size_t left  = prison.work.buf_available();
const size_t grown = pool->persistent_end;                 /* how far the bottom has reached */
const size_t peak  = pool->persistent_hw;                  /* MMGR_ENABLE_HW_MEM_CAPACITY_CB only */
```

A pool's counters are its own state, read rather than asked for — there is no
accessor that would be a second way to spell a member read.

`persistent_end` bounds the block chain rather than counting what is held: a freed block inside the
chain stays in it until a release trims the end.

The peaks are `persistent_hw` and `temporary_hw`, one per end so that neither is a maximum over the other,
and they are only maintained when `MMGR_ENABLE_HW_MEM_CAPACITY_CB` is on — off by default, so a run
without it leaves both at zero and a reading from that run means nothing. @ref guide_first_region has
the procedure.

## Reference

@ref mod_confin "Generated reference for locus_carcerum"
