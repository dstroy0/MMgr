# DMA plan {#proj_dma_plan}

**Purpose:** Bring `memoriam_praetereo` onto the pool declaration, and know which of its
open questions are settled, which are measured, and which are still yours to answer.
**Scope:** `src/memoriam_praetereo/memoriam_praetereo.h`, `src/memoriam_praetereo/memoriam_praetereo.c`,
`include/mmgr.h`, `test/performance_benching/praet/`

Sections marked **planned** describe work that is not in the tree. Everything else carries a
citation and describes the tree as it stands on the date at the foot.

## What a channel is today

A channel is a `uint8_t` index. `PraetCfg` carries it alongside the peripheral, a loopback flag
and a completion callback, and `PraetTransferCfg` carries it alongside a buffer pointer and a byte
count (`src/memoriam_praetereo/memoriam_praetereo.h:82-100`). The count of channels a build has is
a separate compile-time number, held in a `PraetInit` filled from `MMGR_PRAET_CHANNELS` and
`MMGR_PRAET_BUF_SIZE` (`src/memoriam_praetereo/memoriam_praetereo.h:53-62`).

Neither config struct ties an index to storage (`src/memoriam_praetereo/memoriam_praetereo.h:82-100`).
A caller writes `.channel = 2` and the library takes it, so the
association between a channel and the bytes it moves lives in the caller's head. The four entries
are `open`, `tx_submit`, `close` and `poll` (`src/memoriam_praetereo/memoriam_praetereo.h:108-115`),
and each one is reached with a channel number.

## The declaration a channel gets instead

**Planned.** `MemoriamPraetereo(name_, pool_)` follows the shape the ring already uses. The ring's
form emits the claim guard for the pool and declares the ring's own storage
(`src/memoria_anularis/memoria_anularis.h:143-145`). A channel declaration does the same for a DMA
channel, so a pool cannot be dressed as a cellblock and a DMA buffer at once.

The channel is then reached by the name of its pool, and the count of channels falls out of how
many declarations a translation unit contains. `MMGR_PRAET_CHANNELS` goes away for the reason
`MMGR_CARCER_MAX` did: a ceiling over declarations sizes nothing once the declarations state their
own extent.

Two sites cannot declare channels of the same name. Pool names are unique inside a translation
unit (`include/mmgr.h:147-151`), and the channel is named for its pool, so the earlier requirement
that two sites may reuse a channel name is withdrawn.

The declaration has to sit at file scope. `MMGR_PARS_CLAIMED_ONCE` emits an enumerator
(`include/mmgr.h:168-172`), and an enumerator inside a function body is block scoped, so a channel
declared inside a function shadows the file-scope guard instead of colliding with it. That case
compiles today and needs its own must-fail test.

## Guards the declaration inherits

A pool declaration emits three guards, and a channel declared over a pool gets all three at no
cost of its own.

`MMGR_PARS_DECLARED_ONCE` catches a second declaration of one pool name inside a translation unit,
and prints the identifier `name_##_declared_twice_but_a_pool_symbol_names_one_region_only`
(`include/mmgr.h:147-151`). `MMGR_PARS_TOKEN` emits an initialized object with external linkage, so
two translation units declaring one pool name give the linker two strong definitions and it refuses
them by name (`include/mmgr.h:130`). `MMGR_PARS_CLAIMED_ONCE` catches a pool dressed twice, keyed on
the pool alone with no dresser's name in it (`include/mmgr.h:168-172`).

Where the library needs to report the mistake in its own words, `MMGR_ERROR_ATTR` puts the text on
a declaration and the diagnostic arrives at the site that referenced it
(`include/mmgr.h:91-95`). The linker names a symbol. This names what the caller did.

## Bounds on a transfer

**Planned.** `PraetTransferCfg` takes a buffer pointer and a separate byte count today
(`src/memoriam_praetereo/memoriam_praetereo.h:95-100`), so a caller can state a length the buffer
does not have. Entries take the pool type instead, and the extent arrives with the pointer. A macro
can be generic over a per-pool type where a function cannot, and a bare `void *` then fails to
satisfy the parameter.

For allocated storage the mechanism is `MMGR_ALLOC_SIZE`, which states which argument gives the
extent of what an entry returns (`include/mmgr.h:112-116`). `locus_carcerum` carries it on both
bound allocation entries and both free functions
(`src/locus_carcerum/locus_carcerum.h:173`, `:188`, `:487`, `:537`).

Measured against those entries at `-O2`, with a legal-use control clean in every column: a write
past the cell by an explicit length, an index below the cell, one index past the cell, an index far
past the cell but still inside the pool, and an index past the pool are all reported. The
diagnostics name the cell extent. `-fanalyzer` added nothing over `-O2` at either optimization
level, so the two analyzer environments once planned for `MMGR_ENVIRONMENTS`
(`CMakeLists.txt:101-107`) are not needed.

Two conditions on that result, both measured. The bound survives only where the call is not folded
away, which the dispatch table convention already guarantees
(`include/mmgr.h:106-108`). And link-time optimization removes it: with `MMGR_LTO` on, which is the
default (`CMakeLists.txt:18`), the allocator inlines across the translation unit boundary and the
cell-level extent is gone. A check build has to turn it off. The shipping build keeps it.

## The state word and closing a channel

**Planned.** `close` returns nothing (`src/memoriam_praetereo/memoriam_praetereo.h:112`, `:146`), and
the caller has no way to learn whether the channel closed. Hardware teardown writes the disable and
polls the busy bit, so a second call is the normal path.

A state word carries the four-state core in its low bits with flags above it, in one machine word.
`close` writes the request, the word reports the result, and the poll reads the word, so `close`
keeps its signature and the caller has something to poll. That makes the state word the completion
mechanism for teardown.

Exclusion on that word is a compile-time setting with three values, and a part pays for what it
has. No qualifier where there is no interrupt handler and no second core. `volatile` for an
interrupt handler on the same core. Acquire and release for a second core, which is the pattern
`memoria_anularis` already uses. Measured on the host, all three produce identical object size and
leave the cell bounds firing, so the qualifier on the state word does not reach the buffer. That
result holds wherever it is built, because it is a property of what the compiler can see. The host
figure itself is not a result: x86-64 emits no barrier for acquire or release, and Xtensa and
RISC-V both need real fence instructions.

The qualifier stays on the state word. A `volatile` buffer accessor would suppress the folding the
bounds checks depend on.

## Direction on an overlapping transfer

`memoria_operor` has two moves and the caller chooses between them. A DMA submit has no caller to
ask, because both endpoints arrive as addresses. Real controllers walk forward, so an overlap with
the destination above the source corrupts.

The comparison is unsigned, and `locus_carcerum` already reads two addresses through `uintptr_t`
for exactly this reason. The branchless form builds a mask from that comparison and indexes a two
entry table, which `carcer_hw` also does when it selects a high-water mark. On a 16-bit build a
full-width address comparison takes several instructions and a masked offset comparison takes one,
which is where `word16` (`CMakeLists.txt:101-107`) should separate the two.

`test/performance_benching/praet/` holds the A/B for it. Three arms: the caller states the
direction, the dispatch compares and branches, the dispatch builds a mask and selects. Three cases:
destination above source every call, below every call, and alternating, because a branch costs
something only when it is wrong. Each head-to-head row runs in both orders, because on the C6 the
arm that runs second has measured about 1.1 cycles more whatever it is.

Correctness runs before any timing, and the bench refuses to report a number if it fails. The
reference is a byte walk written in the bench, because an expectation taken from the code under
test proves nothing. It covers five overlap shapes at three lengths, and the worst shape is a
destination one byte above the source, where a forward walk overwrites the byte it is about to
read.

Images exist for both parts. The numbers belong on the S3 and the C6, and a host figure for this
is a stopwatch reading.

## The port layer and the host simulator

Four hooks carry `EMBED_WEAK` and refuse or do nothing by default, so a build links without a port
(`src/memoriam_praetereo/memoriam_praetereo.h:168`, `:181`, `:193`, `:205`). A board support file
replaces one by defining the same name.

**Planned.** The host simulator is a strong definition of those four names. It implements the
contract a board support file implements, so what it catches is an integration defect.

Hardware visits a few of the states an implementation can reach, in whatever order the silicon
produces. A simulator reaches them on purpose: completion immediately, never, inside the submit
call, after a close, and twice for one transfer; completions out of submission order; channel counts
no part has; fewer bytes moved than were asked for, and more; refusal at each of the four hooks; an
engine that cannot address a given region, which exercises the internal and external decision
without a part that has PSRAM; and alignment and burst rules stricter than any real controller.

Behavior is scripted. A simulator that completes on a coin flip produces a failure nobody can
reproduce, so a test states what the engine does and in what order, and a failure is a fixture.
Fuzzing sits on top of that layer once it exists.

It composes with the guard page in `test/support/`, which puts a region between two inaccessible
pages and traps the touch. A channel buffer placed against one turns a simulated overrun into a
fault at the instruction that caused it.

Three arms, separated by what each may do when it fails. Correctness runs scripted engines against
fixed expectations and fails the build. Optimization reports cycles per submit and bytes of state
per channel, and never gates, which is the standing the bench results already have. Examination
reports which states and transitions the correctness arm reached, and is what shows that arm has
holes.

Scenarios are C tables, not JSON. A table compiles into an image and the same fixture can be
pointed at a real part; a JSON file needs a filesystem and the comparison is gone. `numeros_scribo`
takes a spec array for the same reason. Where a table cannot express a scenario, a callback does:
completing inside the submit call, or completing only while another channel is busy.

Python writes the scenarios and emits C tables. It decides what the scenarios are. C decides what
happens when one runs. The moment the engine's behavior lives in Python, the port stops being a
port. Sweeps follow `MMGR_ENVIRONMENTS`: the harness builds the suite once per scenario and each
becomes its own CTest target, so a failure names the scenario and one scenario can run alone.

## Illegal configurations

**Planned as a suite.** Under the goal of catching mistakes at compile time, the rejection is the
feature, and none of the guards above is exercised by a build that succeeds.

A must-fail case declares the identifier it expects and the harness matches it. CTest's `WILL_FAIL`
sees an exit code, and a build that fails for a second reason passes such a test while proving
nothing. Every sweep carries a control that must compile and run, and a column whose control fails
is void.

Proven already, against the shipped headers: one pool name declared twice in a translation unit,
one pool name declared in two translation units, one pool dressed as a cellblock and as a ring, and
one pool dressed as a ring twice.

Expected and not yet written: `ParsMemoriaeExternum` where `MMGR_ENABLE_EXTRAM` is off, which fails
on the name because the macro is declared only under that flag (`include/mmgr.h:210-221`);
`MemoriamPraetereo` where `MMGR_ENABLE_DMA` is off, which fails the same way
(`src/memoriam_praetereo/memoriam_praetereo.h:20`); a pool too small for one transfer; a channel
declared in a header two translation units include, which collides on the token; and a channel over
a pool from another translation unit, which fails as an undefined reference because the storage is
static (`include/mmgr.h:195`).

Known open: a channel declared at block scope compiles, as described above. And a channel over an
external pool on a part whose engine cannot address external memory has no answer yet, because
`memoria_externa` decides reach at run time.

## The check build

Warnings are the product of the check build. Its objects are discarded, so its flags are free of
what the shipping build needs. It compiles everything with warnings not fatal and fails once at the
end, which hands a whole list to the user in one pass. Three things decide whether that list is
worth reading. Ninja needs `-k 0`, or one hard error truncates it. Warnings are complete and hard
errors are not, since a translation unit stops at its first hard error. Diagnostics from a header
reached by many translation units repeat unless the collector deduplicates on file and line.

## Order of work

1. Flash `praet` to the S3 and the C6, capture the direction A/B, and record the rows.
2. Declare `MemoriamPraetereo(name_, pool_)`, and remove `MMGR_PRAET_CHANNELS`.
3. Move the entries onto the pool type, so a transfer carries its extent.
4. Add the state word, and make `close` a request the caller polls.
5. Build the illegal configuration suite, with a control in every sweep.
6. Build the host simulator as a port, correctness arm first.
7. Turn link-time optimization off in the check build.

## Decisions already taken

These were tried and rejected. Each is recorded so it is not proposed again.

**A claim flag held at run time.** A `claimed` member in a pool struct records that something took
the pool and stops nothing, and it puts run-time state in a library that settles this before the
program runs. The compile-time literal does the same work for no bytes.

**`_Pragma("GCC poison")` from inside the dressing, and `#undef` on the pool name.** Poison reaches
too far, and `#undef` removes a macro definition, which an object declaration is not.

**Parameterized header includes for the pool and the dressing.** A macro expansion cannot contain a
directive, so `#ifndef` and `#error` per pool would turn both into includes and make the user define
the parameters.

**Linker section aggregation for a whole-program total.** Both the ELF and the PE forms work
cross-unit on the host, and the pool footprint is readable off the linked image with one
`objdump -h`. A walk between the start and stop symbols steps by `sizeof` while the linker places
each record on its own alignment boundary, and the two disagreed at 24 against 32, so the walk read
a field out of the previous record's padding. Records also arrive in link order.

**Ping-pong as two channels with a completion arming the other.** ESP32 GDMA, i.MX eDMA and Zephyr
all express it as descriptor linkage, and `memoria_anularis` already does N-way through its segment
entries. Ping-pong is a segment count of two.

**A warning when both endpoints are the same pool.** Compaction and header duplication are real
uses. A warning on correct code teaches people to ignore warnings.

**Refusing a zero-length transfer.** On eDMA a zero byte count ends the minor loop and completes,
and AMD's descriptor engine completes a zero-length descriptor. A length computed at run time can
be zero, and refusing it puts a branch on every call site.

**A `volatile` typedef over the pool for the check build.** Measured across forty rows, it changed
no result. It neither suppressed a check nor added one.

## What the superintendent rank needs

**Planned.** Nothing in the tree knows about more than one site. A whole-program footprint cannot be
a compile-time constant, because a static assertion sees one translation unit and a consumer's pools
are invisible to it. `MMGR_PARS_TOKEN` gives every pool a symbol the linker sees
(`include/mmgr.h:130`), which is what a build-step total would be built on.

Where a user states a budget, the two sides stay separate. The user's list is a claim, the
declarations are the fact, and the assertion compares them. A pool declared and not listed, listed
and not declared, or listed at the wrong size fails the build. Neither side is the oracle for the
other, which is the same reason the pool's own assertion compares `sizeof` against the count it was
handed (`include/mmgr.h:197`).

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
**Date:** 2026-09-01
