# Locus carcerum: the region {#mod_confin_guide}

One region, carved from both ends by one allocator. Persist grows up from the base, interim grows
down from the top, and the free middle between them is what both take from.

## When to reach for it

- You have a buffer and you need to hand pieces of it out.
- Some of those pieces live as long as the program and some live for one operation.
- You want the failure to be "this region is full", not "the system is out of memory".

## What it composes with

Everything sits on top of it. @ref mod_spat_guide views what a region passes out; @ref mod_anular_guide
moves those views between a producer and a consumer.

## Declaring one

Declare the pools, then dress them. A pool declaration emits the storage and its alignment;
`LocusCarcerum` emits each cellblock's state and the entries bound to it. All of it is data with
initial values, and nothing either one does happens at run time:

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

Three cellblocks is as legal as four, and the sizes need not relate to each other. Each sits over
its own pool: nothing divides and nothing rounds. Only the count of `MMGR_CARCER_W` lines in the
header bounds the count; another count is another line.

You reach a cellblock by the name of its pool, and a pool name is for one region. A second
declaration of one name fails the build. Two sites cannot hold cellblocks of the same name, and a
program never has two meanings for one symbol.

The alignment is a power of two, and that retains the arithmetic free. An offset inside a cellblock
rounds with a mask and not with a divide, which holds only for a power of two. A pool's size is any
count that holds one cell. `MMGR_CARCER_BODY` asserts that at the declaration, against the pool's
own `sizeof` and not against the count it got.

## The two ends

Both ends are the same block allocator. Each retains a chain of blocks, each block behind its own
header, and each carves from the free middle. All that differs is which way the boundary moves,
and what that buys you:

|              | persist                             | interim                             |
| ------------ | ----------------------------------- | ----------------------------------- |
| grows        | up, from the base                   | down, from the top                  |
| lifetime     | as long as it likes                 | one call                            |
| given back   | one tenancy at a time, in any order | one tenancy at a time, in any order |
| a take costs | a carve                             | a carve                             |

The persistent end holds what outlives a call: key material, a runtime seed, a constant a run works
out once. The interim end holds what a call needs while it runs. The two ends are one algorithm with
the base at opposite sides of the pool. A take at either end is one pass through the cellblock's
turnstiles and one fetch-add on the boundary. A release sets the block's header free. Where the
block was at the boundary, the release also trims the boundary back over the free blocks there.

A take the middle cannot meet closes the cellblock, waits out the passes under way, and walks its
own end alone. It merges every run of free blocks into one. It takes the run that fits farthest from
the middle and splits it where its rest holds a block. It returns the free run at the boundary to
the middle, and it bumps the boundary where no run fits. Only then does the cellblock refuse the
take.

## Cells that move

Movable cells are an option a program assigns once, through its guard's `movable_init`. A cellblock
with no option takes and gives as it always does, and each entry tests one word. Its
`movable_buf_alloc` answers `NULL`.

The call passes over storage for the option, the sizes, and the calls of the thread the mover runs
on. The storage lies outside the prison, and nothing of the option
pins a tier. A pool declared at compile time gives it a link-time address.
`MMGR_MOVABLE_STORAGE_BYTES` states what it takes.

`movable_buf_alloc` answers a const pointer to the cell's entry in the translation table. The entry
is the mutable word: it names where the cell is now, and it is MMgr's alone. The pointer is the
caller's identity for the cell, and it holds for the cell's life however often the cell moves.
Nothing passes out the cell's address.

`movable_buf_read`, `movable_buf_write`, `movable_buf_compare` and `movable_buf_measure` take the
identity and an offset. They check the span against the cell and do the work themselves, and so
every touch of the cell passes through MMgr. The call refuses a span past the cell's end. One
fetch-or on the table's claim bits claims an entry: the lowest free bit nearest the last claim.

Each call is one pass through the cellblock's turnstiles, badged with the id of the thread that
calls. One fetch-or claims the lane, and the call reads the entry's word, copies the bytes, and lets
the lane go. A move is a writer. It sets the cellblock's closed bit, waits out every pass under way,
and copies the cell. It then flips the entry to the copy, sets the cell left behind free and opens
the cellblock.

A call that finds the closed bit touches nothing, backs off one step on the bit and tries once more.
So a cell never moves under a call, and a move is one copy long. A call is atomic against a move and
not against another writer: two contexts that write the same bytes order themselves.

Every take passes the turnstiles too, and the option counts the passes: that count makes pressure a
reading of the whole cellblock. A give passes none: the cell's words are its holder's alone.

The mover runs on the thread the program gave. Each reading compares
`mmgr_series_cellularum_fragmentation` against `MMGR_MOVABLE_SETTLE_PERCENT`. That reading is the
bytes the persistent tier occupies that no cell's payload holds, as a percentage of the bytes it
occupies. The temporary tier reads nothing, since its cells last one call and its top comes back as
they end.

Over the threshold, `MMGR_MOVABLE_QUIET_READINGS` in a row must find the cellblock quiet. Quiet means
no pass since the last reading, and twice the open lanes of the turnstiles at or under the option's
width plus one. The mover then moves cells one at a time, the highest first, each into the first free
run below it. The cellblock stays open between moves, and the mover yields the processor and reads
the same after each. A pass between moves says the moves meet pressure, and the mover backs off and
doubles its wait.

The mover remembers a sort that ends with nothing left to move. It does not run that sort again
until the reading rises past what the sort left or first falls under the threshold.

`mmgr_movable_buf_move` is the mover's step, for a caller with no thread to give.

A cellblock retains its own running total in `in_use`, counted where each owner's is. An owner's
counts span every cellblock it takes from. Bytes that lie in no cellblock, a ring's among them,
reach an owner's counts through `mmgr_owner_bytes_taken`. A lane's touches are a telemetry row: the
lane counts the passes through it, with the last badge's id beside them. `mmgr_movable_touches`
reads them, and nothing decides on them.

A pointer inside a movable cell to the cell's own bytes goes stale when it moves. So a cell that
moves addresses its own bytes by offset.

## Worked example

```c
char *const table = prison.work.persistent_buf_alloc(512, reader);
if (table == NULL) {
    return -1;                       /* the region is full; nothing moved */
}

char *const buf = prison.work.temporary_buf_alloc(256, reader);

/* ... use buf ... */

prison.work.persistent_buf_release(buf, reader);
prison.work.persistent_buf_release(table, reader);
```

The glossary, @ref ref_glossary, decodes the Latin of the module names.

Both releases get the **cell itself**, because the block's own header carries its size and its
offset says which end it came from. One release serves both ends.

## Giving back a secret

One call gives a persistent tenancy back, and what it does depends on which pool it is:

```c
prison.work.persistent_buf_release(table, reader);   /* minimum security: the bytes are left as they are */
prison.keys.persistent_buf_release(secret, reader);  /* max security: they are cleared first */
```

The guarantee is in the declaration, never in a flag or a second call. A caller cannot ask for a
wipe and not get one, and cannot forget to. `prison.keys` has no unwiped release to reach for. The
release clears the block's own extent, read from its word. A caller cannot under-wipe a tenancy with
a count smaller than it holds.

`mmgr_zero_buf` clears an address and a count in place and gives nothing back. It is for a secret
whose use ends while its storage stays in use.

A release clears bytes, and a hand-out never does. So `persistent_buf_alloc` does **not** return
zeroed storage. A block released from a minimum-security pool and passed out again still carries
what the last occupant left. Declare anything whose bytes are worth a clear under
`MMGR_MAXIMUM_SECURITY`. Its release zeroes the cell first and retains the next occupant from a read
of it.

@note The clear uses `volatile` machine-width stores: it survives however dead the bytes look to
the optimizer afterwards. It handles a length or an address that is not a whole number of words
through byte edges.

## Gotchas

**A take that does not fit returns NULL and moves nothing.** Both ends fail closed, and neither
crosses into the other. Test the return; that is the check.

**`buf_available` is the raw gap between the ends.** A persistent take also needs a block header out
of it: the gap is not the largest request that will succeed. Ask for what you want and test the
answer; do not predict it from this.

**A pointer that outlives its release is dead and still readable.** Nothing moves, and a minimum
security pool scrubs nothing: it dereferences fine and returns whatever the next take put there.

**`who_owns_buf` is for asserts.** It says a pointer is inside the pool's storage. It does not say
the pointer is live.

## Sizing it

```c
const size_t left  = prison.work.buf_available();
const size_t grown = mmgr_series_cellularum_persistent_end(pool);  /* how far the bottom has reached */
const size_t peak  = pool->persistent_hw;                  /* MMGR_ENABLE_HW_MEM_CAPACITY_CB only */
```

A pool's peaks are its own state, read as members. The two boundaries share one atomic word, and
`mmgr_series_cellularum_persistent_end` and `mmgr_series_cellularum_temporary_top` read each off it.

The persistent end bounds the block chain and counts nothing of what is busy. A freed block inside
the chain merges with its free neighbors and stays in it for a take the middle cannot meet.

The peaks are `persistent_hw` and `temporary_hw`, one per end so that neither is a max over the
other. They exist and change only when `MMGR_ENABLE_HW_MEM_CAPACITY_CB` is on, off by default. A run
without it leaves both at zero, and a reading from that run means nothing. @ref guide_first_region
has the procedure.

## Reference

@ref mod_confin "Generated reference for locus_carcerum"
