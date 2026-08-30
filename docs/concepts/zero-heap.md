# Why there is no malloc {#concept_zero_heap}

The case for giving up dynamic allocation, and the bill that comes with it.

## What a heap actually costs

A general allocator promises you can ask for any size at any time and give it back in any order. On
a host that promise is cheap. On a device with 64 KB of RAM and a watchdog it is expensive in four
separate ways, and only the first is the one people mention.

**It is not decidable.** A build that links `malloc` has a footprint you cannot state. You can
measure a run, but you cannot say what the worst case is, because the worst case depends on the
sequence of requests. "It fits" becomes an observation rather than a property.

**It fragments.** Free space and usable space stop being the same number. The failure arrives after
hours of correct operation, at the request that happened to want a contiguous block, and it does not
reproduce.

**It fails late and everywhere.** Every call site that allocates is a call site that can fail, so
either every one carries a branch nobody exercises, or some of them do not and the null propagates.

**Its timing is unbounded.** A free-list walk is not a fixed number of instructions. In an interrupt
path that is not an inconvenience, it is a defect.

## What MMgr does instead

Storage is decided at compile time and carved at run time by bumping a pointer.

- The sizes are `MMGR_PLAINTEXT_CONFIN_SIZE`, `MMGR_SECURE_CONFIN_SIZE` and whatever buffer you hand
  `LocusCarcerum`. All are known when the binary is linked.
- A take is a bounds check and an addition. Same cost every time.
- Failure is `NULL` from a take against a region you sized, not an out-of-memory condition arriving
  from somewhere else in the program.
- There is no free list, so there is nothing to fragment.

The library reaches for `stddef`, `stdint` and `stdatomic` and nothing else. There is no `stdlib.h`
anywhere in `src/`, which is a property you can check rather than a claim you have to trust:

```sh
grep -rn 'include <stdlib' src/    # no matches
```

## The bill

**You have to size it.** This is the whole cost and it is not small. Nobody is going to find you
more memory at runtime.

The way through it is measurement, not arithmetic. Build the `checks` environment, run the real
workload, and read the high-water marks:

```sh
cmake -S . -B build -DMMGR_BUILD_TESTS=ON
cmake --build build
./build/test/checks/<your workload>
```

`buf_available()` is per pool — `prison.work.buf_available()` — and reports the bytes still between
the two tiers at the moment you call it (`src/locus_carcerum/locus_carcerum.h:123`), not the largest
it ever got.

For the largest, build with `MMGR_ENABLE_HW_MEM_CAPACITY_CB`. Every take then keeps a peak in the pool's own state,
one per tier: `persistent_hw` tracks `persistent_end` and `temporary_hw` tracks the bytes taken from
the top. It is off by default, so a workload run without it leaves both at zero. Read the field, add whatever margin your failure policy wants, then set the
knob in @ref ref_configuration.

**You give up free-anything-anytime.** Persist unwinds; interim releases by mark; a tenant resets as
a whole. If your data structure genuinely needs arbitrary-order release of arbitrary-size objects
with a long tail of lifetimes, this library is the wrong shape and no amount of configuration will
change that.

**A lie about a capacity is unrecoverable.** A pool believes the size its declaration gave it.
Pass it a length longer than the buffer and every bounds check afterwards is computed against a
number that was never true. See @ref proj_security.

## When to reach for it

It fits when the set of things you allocate is known, the sizes are bounded, and the lifetimes are
either "forever" or "until this operation finishes" — which describes most protocol handling, most
parsing, and most device I/O.

It does not fit a workload whose shape is discovered at runtime and whose objects outlive each other
in an arbitrary order. Use a heap for that, and use it deliberately.
