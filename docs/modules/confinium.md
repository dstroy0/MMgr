# Locus carcerum: the region {#mod_confin_guide}

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
over its own pool: nothing divides and nothing is rounded. The count is bounded only by how many
`MMGR_CARCER_W` lines the header carries; another count is another line.

A cellblock is reached by the name of its pool, and a pool name stands for one region. Declaring one
name twice fails the build: two sites cannot hold cellblocks of the same name and a program never
has two meanings for one symbol.

The alignment is a power of two, and that keeps the arithmetic free: an offset inside a
cellblock is rounded by a mask instead of divided, which holds only for a power of two. A pool's size
is any count that holds one cell, which `MMGR_CARCER_BODY` asserts at the declaration, against the
pool's own `sizeof` and not against the count it was handed.

## The two ends

Both ends are the same block allocator. Each keeps a chain of blocks, each block behind its own
header, and each carves from the free middle. All that differs is which way the boundary moves,
and what that buys you:

|              | persist                             | interim                        |
| ------------ | ----------------------------------- | ------------------------------ |
| grows        | up, from the base                   | down, from the top             |
| lifetime     | as long as it likes                 | one call                       |
| given back   | one tenancy at a time, in any order | by mark, or one at a time      |
| a take costs | a carve                             | a carve                        |

The persistent end holds what outlives a call: key material, a runtime seed, a constant a run works
out once. The interim end holds what a call needs while it runs. A take at either end is one move of
the boundary. A release merges the block with its free neighbors and returns a free block at the
boundary to the free middle. A take the middle cannot meet reads its own end's chain for a free block
that fits.

## Cells that move

Movable cells are an option a program assigns once, through its guard's `movable_init`. A cellblock with no
option takes and gives as it always does, each entry testing one word, and its `movable_buf_alloc` answers
`NULL`. The call hands over storage for the option, the sizes, and the calls of the thread the mover runs on.
The storage lies outside the prison, so nothing of the option pins a tier, and a pool declared at compile time
gives it a link-time address. `MMGR_MOVABLE_STORAGE_BYTES` states what it takes.

An assigned option also says the cellblock serves threads, and that makes it one room. A take the gap cannot meet
waits its turn at the tier's moving bit in place of being refused, and reads the other tier for a free cell before it
is refused. A persistent take can so lie in the temporary tier, where a rewind past it drops it, so a program assigning
the option gives its cells back one at a time. A cellblock with no option refuses at once and keeps each take to its own
tier, since the context taking may be an interrupt over the bit's holder.

`movable_buf_alloc` answers a const pointer to the cell's slot in the translation table. The slot is the mutable
word: it names where the cell stands now, with the moving bit in its low bit, and it is MMgr's alone. The pointer
is the caller's identity for the cell, and it stands for the cell's life however often the cell moves. Nothing
hands out the cell's address. `movable_buf_read`, `movable_buf_write`, `movable_buf_compare` and
`movable_buf_measure` take the identity and an offset, check the span against the cell, and do the work
themselves, and so every touch of the cell passes through MMgr. A span past the cell's end is refused.

Each call goes through a turnstile. There are as many as the width the program states, which the system's
logical processors bound, one to a cache line, and a thread starts at the one its id hashes to. A call writes the
slot it is reaching into its turnstile, orders that ahead of its reads, reads the slot and tests the moving bit.
A move sets the moving bit, orders that ahead of its reads, and reads every turnstile. Whichever runs second sees
the other's write, so a cell never moves under a call. A call that finds its cell moving, or every turnstile busy,
waits in the queue behind them, and a move is one copy long. A call is atomic against a move and not against
another writer: two contexts writing the same bytes order themselves.

Every entry that changes the cellblock passes a turnstile too, which is what makes occupancy a reading of the
whole cellblock. A take or a give never waits for one. Where none is free it goes on, and that counts as pressure.

The mover runs on the thread the program gave. Each reading compares `mmgr_cellblock_fragmentation`, the bytes the
tiers occupy that no cell's payload holds as a percentage of the bytes they occupy, against
`MMGR_MOVABLE_SETTLE_PERCENT`. Over it, `MMGR_MOVABLE_QUIET_READINGS` in a row must find at most half the turnstiles
occupied. The mover then locks turnstiles one at a time, up to half of them, watching the queue at the rest. If each
lock leaves that steady it moves cells, the highest first, each into the first free cell below it, reading the queue
between moves. If removing a turnstile raises the pressure, it lets every lock go and doubles its wait. A sort that
ends with nothing left to move is remembered, and is not run again until the reading rises past what it left or
falls under the threshold first.

`mmgr_movable_buf_move` is the mover's step, for a caller with no thread to give. With one turnstile there is none
to lock while leaving one open, and occupancy decides alone.

A cellblock keeps its own running total in `in_use`, counted where each owner's is. An owner's counts span every
cellblock it takes from, and bytes that lie in no cellblock, a ring's among them, reach an owner's through
`mmgr_owner_bytes_taken`. A thread's touches are a telemetry row, summed after the fact from turnstiles nobody
occupies and read with `mmgr_movable_touches`; nothing decides on them.

A pointer inside a movable cell to the cell's own bytes goes stale when it moves, and a cell that moves addresses
its own bytes by offset. A host site with the option assigned is not made anew, since the mover reads its block.

## Worked example

```c
char *const table = prison.work.persistent_buf_alloc(512, &reader);
if (table == NULL) {
    return -1;                       /* the region is full; nothing moved */
}

const size_t mark = prison.work.temporary_buf_mark();
char *const  buf  = prison.work.temporary_buf_alloc(256, &reader);

/* ... use buf ... */

prison.work.temporary_buf_release(mark);
prison.work.persistent_buf_release(table, &reader);
```

@ref ref_glossary decodes the Latin the module names are built from.

Note what each release is given. The temporary one takes a **mark**, a value `temporary_buf_mark`
handed back, and drops everything carved since. The persistent one takes the **cell itself**,
because the block's own header carries its size.

## Giving back a secret

One call gives a persistent tenancy back, and what it does depends on which pool it is:

```c
prison.work.persistent_buf_release(table, &reader);   /* minimum security: the bytes are left as they are */
prison.keys.persistent_buf_release(secret, &reader);  /* maximum security: they are cleared first */
```

The guarantee is in the declaration, never in a flag or a second call. A caller cannot ask
for a wipe and not get one, and cannot forget to. `prison.keys` has no unwiped release to reach for.
The extent cleared is the block's own, read from its word. A caller cannot under-wipe a tenancy
by naming fewer bytes than it holds.

`mmgr_zero_buf` clears an address and a count in place without giving anything back, for when a secret
is finished with but the storage is not.

Bytes are cleared on release, never on hand-out. That means `persistent_buf_alloc` does **not**
return zeroed storage: a block released from a minimum-security pool and handed out again still
carries what the last occupant left. Anything whose bytes are worth clearing is declared under
`MMGR_MAXIMUM_SECURITY`, whose release zeroes the cell first and keeps the next
occupant from reading it.

@note The clear uses `volatile` machine-width stores: it survives however dead the bytes look to
the optimizer afterwards, and it handles a length or an address that is not a whole number of words
through byte edges.

## Marks nest

`temporary_buf_mark` returns a value and stores nothing. A caller may hold two at once:

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

**A take that does not fit returns NULL and moves nothing.** Both ends fail closed in place of
crossing into each other. Test the return; that is the check.

**`buf_available` is the raw gap between the ends.** A persistent take also needs a block header out
of it: the gap is not the largest request that will succeed. Ask for what you want and test the
answer in place of predicting it from this.

**A pointer that outlives its mark is dead and still readable.** Nothing moves and nothing is
scrubbed by an interim rollback: it dereferences fine and returns whatever the next take put
there. Keep a mark and its release in the same function.

**`who_owns_buf` is for asserts.** It says a pointer is inside the pool's storage. It does not say
the pointer is live.

## Sizing it

```c
const size_t left  = prison.work.buf_available();
const size_t grown = mmgr_cellblock_persistent_end(pool);  /* how far the bottom has reached */
const size_t peak  = pool->persistent_hw;                  /* MMGR_ENABLE_HW_MEM_CAPACITY_CB only */
```

A pool's peaks are its own state, read as members. The two boundaries share one atomic word, and
`mmgr_cellblock_persistent_end` and `mmgr_cellblock_temporary_top` read each off it.

The persistent end bounds the block chain and counts nothing of what is busy: a freed block inside the
chain merges with its free neighbors and stays in it for a take the middle cannot meet.

The peaks are `persistent_hw` and `temporary_hw`, one per end so that neither is a maximum over the other,
and they are only maintained when `MMGR_ENABLE_HW_MEM_CAPACITY_CB` is on, off by default. A run
without it leaves both at zero and a reading from that run means nothing. @ref guide_first_region has
the procedure.

## Reference

@ref mod_confin "Generated reference for locus_carcerum"
