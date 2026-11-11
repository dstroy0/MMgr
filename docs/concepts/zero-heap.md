# Why there is no malloc {#concept_zero_heap}

The case for giving up dynamic allocation, and the bill that comes with it.

## What a heap actually costs

A general allocator promises you can ask for any size at any time and give it back in any order. On
a host that promise is cheap. On a device with 64 KB of RAM and a watchdog it is expensive in four
separate ways, and people mention only the first.

**It is not decidable.** A build that links `malloc` has a footprint you cannot state. You can
measure a run, but you cannot say what the worst case is, because the worst case depends on the
sequence of requests. "It fits" becomes an observation in place of a property.

**It fragments.** Free space and usable space stop being the same number. The failure arrives after
hours of correct operation, at the request that happened to want a contiguous block, and it does not
reproduce.

**It fails late and everywhere.** Every call site that allocates is a call site that can fail.
Either every one carries a branch nobody exercises, or some of them do not and the null propagates.

**Its timing is unbounded.** A free-list walk is not a fixed number of instructions. In an interrupt
path that is not an inconvenience, it is a defect.

## What MMgr does instead

Storage is decided at compile time and carved at run time by bumping a pointer.

- The sizes are the byte counts in your pool declarations alone. No build knob sizes a
  pool, and all of them are known when the binary is linked.
- An allocation is a bounds check and an addition. Same cost every time.
- Failure is `NULL` from an allocation against a pool you sized, not an out-of-memory condition
  arriving from somewhere else in the program.
- There is no free list: a release merges a cell with its free neighbors.

The library reaches for no standard header. Its types, attributes and atomics come from embedded_types,
and there is no standard include anywhere in `src/`, which is a property you can check in place of a claim you have to trust:

```sh
grep -rn '^#include <' src/    # no matches
```

## The bill

**You have to size it.** This is the whole cost and it is not small. Nobody is going to find you
more memory at runtime.

The way through it is measurement in place of arithmetic. Build the `checks` environment, run the real
workload, and read the high-water marks:

```sh
cmake -S . -B build -DMMGR_BUILD_TESTS=ON
cmake --build build
./build/test/checks/<your workload>
```

`buf_available()` is per cellblock, `prison.work.buf_available()`, and reports the bytes still
between the two tiers at the moment you call it, never the largest it ever got.

For the largest, build with `MMGR_ENABLE_HW_MEM_CAPACITY_CB`. Every allocation then keeps a peak in
the cellblock's own state, one per tier: `persistent_hw` tracks the persistent end and `temporary_hw`
tracks the bytes taken from the top. It is off by default. A workload run without it leaves both
at zero. Read the field, add whatever margin your failure policy wants, then size the pool.

**A cell goes back in any order, on either tier.** A release frees a cell and returns a free cell at a tier's
boundary to the gap. A take of either tier bumps its boundary while the gap holds it. Where the gap cannot, the
take merges the tier's free cells and reuses the one that fits farthest from the gap. A cell released below its
tier's boundary stays laid, with its owner word and its take's nanosecond. It stays until a take reuses it or the
boundary comes back over it.

**A cellblock can no longer be lied to about its extent.** It takes the count from the pool's own
declaration, and the pool asserts that count against its `sizeof`: the two cannot disagree. What is
still taken as written is any address the library did not hand out. See @ref proj_security.

## When to reach for it

It fits when the set of things you allocate is known, the sizes are bounded, and the lifetimes are
either "forever" or "until this operation finishes", which describes most protocol handling, most
parsing, and most device I/O.

It does not fit a workload whose shape is discovered at runtime and whose objects outlive each other
in an arbitrary order. Use a heap for that, and use it deliberately.
