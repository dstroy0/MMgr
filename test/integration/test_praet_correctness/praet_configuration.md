# Configuring a praet schedule context

**Purpose:** Declare a schedule context that compiles, and read any diagnostic this configuration produces without opening the headers.
**Scope:** `test/integration/test_praet_correctness/praet_praefinitum.h`, `praet_horologiorum_custos.h`, `praet_platform_detection.h`, `praet_iudex.h`, `praet_ordo.h`

This module is under construction and lives entirely in `test/`. Nothing described here is in `src/`.

## What a configuration is made of

Four things answer a configuration, and they answer it in different places.

Six build knobs arrive as `-D` on the command line or as a `#define` ahead of the include. Four of them describe the part, and are read in `praet_praefinitum.h`. Two more describe the clock, and are read in `praet_horologiorum_custos.h`, which also reads a seventh knob on one arm.

One answer is given at the declaration instead, as a token passed to `PraetOrdoContext` (`praet_ordo.h:214-219`). It is not a build knob, and setting the build knob that used to hold it stops the build (`praet_praefinitum.h:115-117`).

Several facts are derived from the architecture and are not answerable at all. `praet_platform_detection.h` reads what the compiler predefines and settles which family this is, how wide its register is, and whether it defines a cycle counter. One of those decides what an unanswered clock source falls back to (`praet_horologiorum_custos.h:71-80`).

`MMGR_ACCEPT_DEFAULTS` is the last piece. It changes nothing about which value a knob takes. What it changes is whether an incomplete configuration stops the build (`praet_iudex.h:31-36`).

## How an unanswered knob reports

Every knob that was not set takes a documented default, raises a warning naming itself and the value it took, and leaves a flag behind. No knob stops the build where it sits. `praet_iudex.h` reads the flags after every knob has spoken and stops once (`praet_iudex.h:33-36`).

The order matters more than it looks. An `#error` halts its translation unit at the line it appears on, so a build missing four knobs would report the first, get corrected, then report the second. Four rounds would deliver four facts that were all knowable on the first pass. Warning at each knob and stopping at the end delivers them together.

A build with nothing declared reports six warnings and one error:

```
warning: #warning "PRAET_CHANNELS was not set and took the library default of 8. Set it to the channels your part gives one engine."
warning: #warning "PRAET_SETTLE_MICROS was not set and took the library default of 0, so this build waits for nothing after an attach. Set it to what your engine takes to come up."
warning: #warning "PRAET_KEEPALIVE_MICROS was not set and took the library default of 1000. Set it to how long a moving channel may go unkicked on your part before it has stopped."
warning: #warning "PRAET_RECOVERY was not set and took the library default of 0, so a stalled transfer cannot be backed out or scrubbed in this build. Set it to 1 to have that machinery, 0 to say you meant to leave it out."
warning: #warning "No clock is declared, so PRAET_CLOCK_HZ took the library default of 1000000 and a tick is read as one microsecond. Every deadline in this module is then wrong by whatever the real frequency is. Set it to the frequency of the clock this reads."
warning: #warning "PRAET_CLOCK_SOURCE was not set and was derived as PRAET_CLOCK_CALLER, because this architecture defines no cycle counter a timer could be pinned to. Say so, and supply the clock."
error: #error "This build did not declare every knob this module reads. Each one that took a library default is named in a warning above this line, with the value it took and what to set it to. Set them, or define MMGR_ACCEPT_DEFAULTS to build on the defaults and keep the warnings as the record of which ones you took."
```

`PRAET_CLOCK_CORE` is absent from that list because the derivation put this build on the caller's clock, and nothing pins a timer there. On an architecture that defines a counter, the source falls back to `PRAET_CLOCK_OWN` and the core joins the list.

Nothing in this depends on a build system. These are preprocessor directives, so `gcc` or `clang` invoked on one file behaves the way any tree driving them does. A keep-going build (`ninja -k 0`, `make -k`) collects failures across translation units, which is the same idea at a different scale and is not something this needs.

## The build knobs

### PRAET_CHANNELS

Logical channels one context carries. Sizes every per-channel array in `PraetOrdo` (`praet_ordo.h:265-275`).

Default 8, with a warning (`praet_praefinitum.h:50-54`).

### PRAET_SETTLE_MICROS

Microseconds the engine spends coming up before it will take a transfer. One timer for the context, because settling is a property of the engine and not of a channel (`praet_ordo.h:249-250`).

Default 0, with a warning (`praet_praefinitum.h:64-68`). A build taking that default waits for nothing after an attach.

### PRAET_KEEPALIVE_MICROS

Microseconds a running channel may go unkicked before it reads stalled. This is a watchdog window and never a transfer length. An unkicked window means the engine stopped moving, and says nothing about the transfer having finished.

Default 1000, with a warning (`praet_praefinitum.h:81-85`).

### PRAET_RECOVERY

Whether a stalled transfer can be backed out or scrubbed. Legal values are 0 and 1, and a third value stops the build where it sits (`praet_praefinitum.h:108-110`). That one does not join the basket, because every later gate would be reading a value nobody can interpret.

Default 0, with a warning (`praet_praefinitum.h:98-102`).

With it on, the context carries `bound`, `bound_bytes`, `start`, `length`, `position`, `boundary_crc`
and `word_boundary_crc` (`praet_ordo.h:267-275`), and six entries exist that do not otherwise:
`praet_ordo_relatio` taking an offset and a length, `praet_ordo_efficere` taking a position,
`praet_ordo_situs`, `praet_ordo_commotus_est`, `praet_ordo_boundary_crc`
(`praet_ordo.h:417-519`) and `praet_ordo_resolve` (`praet_ordo.h:605-623`).

With it off, `praet_ordo_relatio` takes no span and `praet_ordo_efficere` takes no position
(`praet_ordo.h:521-562`). A call site written for the other form fails to compile. The watchdog
still runs, so a channel still reads stalled; what is gone is any statement about which bytes were
touched.

### PRAET_CLOCK_HZ

Ticks the clock counts in one second. Every deadline in this module is microseconds, and this is what a tick is scaled against (`praet_horologiorum_custos.h:127`).

Two conditions hold at compile time. The frequency divides evenly into microseconds, and it is at least one megahertz (`praet_horologiorum_custos.h:142-147`). Every part this library targets runs at a whole number of megahertz, and a clock below one megahertz cannot resolve a microsecond.

Default 1000000, with a warning (`praet_horologiorum_custos.h:55-59`). That default reads a tick as a microsecond. It is not an estimate of any part's frequency, because an estimate would make every deadline wrong by however much it missed by.

### PRAET_CLOCK_SOURCE

`PRAET_CLOCK_CALLER` where the caller already runs a counter this reads. `PRAET_CLOCK_OWN` where this pins its own timer to a core. A third value stops the build (`praet_horologiorum_custos.h:84-86`).

Unset, it is derived from `PRAET_PLATFORM_HAS_CYCLE_COUNTER` (`praet_horologiorum_custos.h:71-80`). Both arms warn, and each says which way it went and why.

### PRAET_CLOCK_CORE

The core a pinned timer runs on. Read only where the source is `PRAET_CLOCK_OWN`, and setting it on the other arm stops the build (`praet_horologiorum_custos.h:115-117`).

Default `PRAET_PLATFORM_CLOCK_CORE`, with a warning (`praet_horologiorum_custos.h:105-109`).

## The declaration

A context is declared, and the declaration is the instantiation. It emits initialized data, so nothing runs before `main` to make a context usable.

```c
PraetOrdoContext(s_schedule, AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC_DISABLE);
```

The second argument answers whether a recovery checksums the word the engine was inside. It is one of two tokens (`praet_ordo.h:77-81`), and both of them report at every build that declares a context (`praet_ordo.h:116-125`). A choice that only spoke up one way would train a reader to take its silence as the safe answer, and neither answer here is safe by default.

The token is independent of `PRAET_RECOVERY`. Turning recovery on does not turn the check on, and asking for the check on a build with recovery off fails an assertion naming what to do (`praet_ordo.h:216-218`).

Three things are refused. A misspelled token fails on an unknown type name carrying the token it was given, since the declarator pastes it onto a type that exists for the two spellings and nothing else (`praet_ordo.h:133`). A plain `1` or `TRUE` fails the same way, because those are values and neither names a type. `PRAET_RECOVERY_CRC` as a build knob is refused outright (`praet_praefinitum.h:115-117`).

## The attach surface

A channel reaches memory through a pool, by name, and the name is the whole of what is checked.

```c
ParsMemoriaeInternae(frame_pool, 4096);

PraetOrdoContext(s_frame_dma, AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC_ENABLE);
PraetChannel(s_frame_dma, 0, frame_pool);
```

`PraetChannel` says which pool a channel is over (`praet_ordo.h:384-392`). It emits an enumerator
whose name carries the context, the channel and the pool. `PraetAttach` and `PraetSubmit` both name
that enumerator (`praet_ordo.h:403`), so reaching either with a pool the channel is not over is an
undeclared identifier printing the triple that was written. This is the shape `locus_carcerum` uses,
where `MMGR_CARCER_BODY` pastes the site and the pool into `prisonsite_##_##name_##_ctx` and a
cellblock's entries cannot be handed another cellblock's bytes. The channel joins the paste here
because a context has several and they may be over different pools.

The channel number is pasted, so it is written as a plain literal. `0u` pastes to `channel0u` and
names an enumerator nobody declared.

`PraetChannel` also asserts the channel is below `PRAET_CHANNELS` and that the pool has bytes in it
(`praet_ordo.h:385-388`). The entry still refuses an out of range channel at run time, and the
surface settles it before anything runs.

```c
(void)PraetAttach(s_frame_dma, 0, frame_pool, PRAET_REGION_INTERNAL);
(void)PraetSubmit(s_frame_dma, 0, frame_pool, 1024u, 512u);
```

`PraetAttach` hands the entry `mmgr_pars_storage_##pool_` and `pool_##_bytes` (`praet_ordo.h:360-363`).
Both are emitted by `ParsMemoriaeInternae` and `ParsMemoriaeExternum` and by nothing else, and both
have internal linkage, so a translation unit that did not declare the pool cannot reach either. What
that tests is not who owns the bytes. It is whether whoever hands them over can answer everything
about them: the address, the extent, and which channel is over them. A caller with all three has said
the bytes are legal to touch.

Attaching does not claim the pool. `MMGR_PARS_CLAIMED_ONCE` exists because a cellblock and a ring both
write their own records into the bytes they dress, and two of those over one pool would each believe
they owned it. A channel writes no records into the pool, so a ring can be a DMA destination, which is
the arrangement the module is for. The cost is that two channels may target one pool and nothing
reports it, which reads the same as two channels on one peripheral: it can collide, and this library
does not have the caller's plan.

`praet_ordo_relatio` takes no pointer (`praet_ordo.h:441`). The address came from the pool named
at the attach, so a transfer adds an offset and a length. `PraetSubmit` compares that span against
`sizeof(mmgr_pars_storage_##pool_)` while compiling (`praet_ordo.h:461-466`). That is what the
compiler laid down, and not the count the declaration was handed. A span that does not fit gives a
negative bitfield width, and the member's name is the diagnostic. Measured at `-O0` and at `-O2`: a
span that fits reports nothing at either, and one that does not fails at both.

The region arrives as one of two tokens, `PRAET_REGION_INTERNAL` or `PRAET_REGION_EXTERNAL`
(`praet_tabula_vexillorum.h:258-263`), which are token ids the same way the statuses are. A misspelling is an
undeclared identifier carrying the name that was written. The count of them is asserted to fit the
descriptor field (`praet_tabula_vexillorum.h:325`).

## The poll

`praet_ordo_poll` is what a caller calls, and the only thing they have to (`praet_ordo.h:680`).
Everything a channel does between an attach and a completion happens there: the port is asked how far
each running channel has got, the watchdog is fed with the answer, an elapsed settle is cleared, a
channel nothing reported on is marked stalled, and a detach whose transfer has finished completes.

It reports nothing. What a caller wants is in the flag word, which the call leaves current, and one
load reads it. A return value would be a second way to learn the same thing and the two would drift.

The port is asked before the short circuit, not after (`praet_ordo.c:376-407`). A part with no
interrupt to raise the set volatile has nothing else that could, so a poll that short circuited on
that volatile alone would never learn the engine had moved. That was measured on the arm where
progress is reported and no interrupt exists, and the second report never reached the flag word. The
short circuit itself is intact: a channel that moved is one of the things that makes it false, and a
poll with nothing to do costs one volatile read and a return.

The settle comparison is lifted out of the walk. One deadline serves the whole context, so it is
answered once and read per channel. Both timer tests put the flag first, so a channel that is not
settling and a channel that is not running each cost a mask and no comparison.

`praet_hw_progress` is the fifth port hook (`praet_ordo.h:299`). The four in
`memoriam_praetereo.h` have no way to report how far a transfer has got, and every controller this
library targets exposes a remaining count. It is declared here and defined by the port, and unlike the
other four it carries no `EMBED_WEAK` refusing default, because the schedule and the engine compile
into one translation unit where a weak default and a strong definition of one name are a duplicate.
A version of this in `src` would carry the default the others do. Nothing is proposed for `src` yet.

## What the architecture decides

`praet_platform_detection.h` names no part, no vendor and no board. A list of parts covers the ones known on the day it was written and misses every part released afterwards, so what is tested is the architecture level that every member of a family defines. An ATSAMD21 answers the ARMv6-M question the way every other Cortex-M0+ does.

The family comes from `__arm__` or `__aarch64__` or `__thumb__`, from `__riscv`, and from `__XTENSA__` or `__xtensa__`. Anything else is a host build. Exactly one of the four is selected, and an assertion says so.

Within ARM, the subfamily comes from `__ARM_ARCH` and `__ARM_ARCH_PROFILE`. Those two name every Cortex subfamily, and a pre-Cortex core defines neither profile nor a level above 6. Within RISC-V it is `__riscv_xlen` with the embedded profiles reported by `__riscv_32e`, `__riscv_64e` or `__riscv_abi_rve`. Within Xtensa it is the calling convention, `__XTENSA_WINDOWED_ABI__` or `__XTENSA_CALL0_ABI__`.

`PRAET_PLATFORM_HAS_CYCLE_COUNTER` is the one derived fact that changes behavior. It is 0 on ARMv6-M, which has no DWT at all, and on a host build, which has no core to pin a timer to. It is 1 elsewhere. The flag says the architecture defines a counter and never that this part implemented it: DWT_CYCCNT is optional even where the DWT is present, RISC-V `cycle` can be closed off by `mcounteren`, and an Xtensa core can be configured without the timer option. Finding out for certain is the port's job, and this decides which default to take.

`PRAET_PLATFORM_XLEN` is the register width, and `EMBED_WORD_BITS` is asserted not to exceed it. The environments build at 16 and 32 bits on a 64-bit host deliberately, so narrower is expected and only wider is wrong.

Unaligned access is `EMBED_FAST_UNALIGNED_LOAD`, which `embed_compiler_directives.h:374-381` already provides. This module does not define its own.

## Legality matrix

Every row is a configuration and what it produces. `warning` continues; `error` and `assert` stop.

| Configuration | Result | Where |
|---|---|---|
| Every knob declared | builds, silent except the declaration's token | |
| Any knob unset, no `MMGR_ACCEPT_DEFAULTS` | one warning per unset knob, then one error | `praet_iudex.h:33-36` |
| Any knob unset, `MMGR_ACCEPT_DEFAULTS` defined | one warning per unset knob, builds | `praet_iudex.h:31` |
| `PRAET_RECOVERY` neither 0 nor 1 | error | `praet_praefinitum.h:108-110` |
| `PRAET_RECOVERY_CRC` defined at all | error | `praet_praefinitum.h:115-117` |
| `PRAET_CLOCK_SOURCE` neither token | error | `praet_horologiorum_custos.h:84-86` |
| `PRAET_CLOCK_SOURCE` is `PRAET_CLOCK_OWN`, architecture has no counter | error | `praet_horologiorum_custos.h:91-93` |
| `PRAET_CLOCK_CORE` set, source is `PRAET_CLOCK_CALLER` | error | `praet_horologiorum_custos.h:115-117` |
| `PRAET_CLOCK_HZ` not a whole number of megahertz | assert | `praet_horologiorum_custos.h:142-143` |
| `PRAET_CLOCK_HZ` below one megahertz | assert | `praet_horologiorum_custos.h:145-147` |
| `EMBED_WORD_BITS` wider than the register | assert | `praet_platform_detection.h:313` |
| More than one architecture family selected, or none | assert | `praet_platform_detection.h:80` |
| Declaration token misspelled | error naming the token given | `praet_ordo.h:133` |
| Declaration token is a value, such as `1` | error naming the value given | `praet_ordo.h:143` |
| `AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC_ENABLE` with `PRAET_RECOVERY` 0 | assert | `praet_ordo.h:216-218` |
| A channel submitting a span of a pool it is not over | error naming the context, channel and pool | `praet_ordo.h:403` |
| A channel attached over a pool it is not declared over | error naming the context, channel and pool | `praet_ordo.h:403` |
| A channel the context never declared with `PraetChannel` | error naming the context, channel and pool | `praet_ordo.h:403` |
| A span past the pool, by length or by offset | error naming the span | `praet_ordo.h:461-466` |
| An ordinary array where a pool belongs | error naming the array | `praet_ordo.h:360-363` |
| `PraetChannel` on a channel past `PRAET_CHANNELS` | assert | `praet_ordo.h:385-386` |
| `PraetChannel` over a pool with no bytes | assert | `praet_ordo.h:387-388` |
| A region that is neither token | error naming the region written | `praet_tabula_vexillorum.h:258-263` |
| More region token ids than the descriptor field holds | assert | `praet_tabula_vexillorum.h:325` |
| Two statuses sharing a token id, or one missing from the list | assert | `praet_tabula_vexillorum.h:290` |
| Region descriptor overlapping a status | assert | `praet_tabula_vexillorum.h:295` |
| Region descriptor off a byte boundary | assert | `praet_tabula_vexillorum.h:304` |
| Region descriptor past the top of the flag word | assert | `praet_tabula_vexillorum.h:306` |
| The field write unable to fill the region, or overrunning it | assert | `praet_tabula_vexillorum.h:312-315` |
| Two core states sharing a value, or one past the mask | assert | `praet_tabula_vexillorum.h:283` |
| An entry clearing a bit inside the region | assert | `praet_ordo.c:62` |
| An entry clearing a bit the map does not account for | assert | `praet_ordo.c:65` |

Every row is driven by a sweep that requires the named diagnostic, not the exit code. A build that failed for an unrelated reason passes an exit-code check while proving nothing.

## Two complete configurations

A host build, with recovery and the boundary word check on. A host defines no counter to pin a timer
to, so the clock is the caller's and no core is named:

```
-DPRAET_CHANNELS=8u
-DPRAET_SETTLE_MICROS=40u
-DPRAET_KEEPALIVE_MICROS=250u
-DPRAET_RECOVERY=1
-DPRAET_CLOCK_HZ=240000000u
-DPRAET_CLOCK_SOURCE=PRAET_CLOCK_CALLER
```

```c
ParsMemoriaeInternae(frame_pool, 128);

PraetOrdoContext(s_frame_dma, AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC_ENABLE);
PraetChannel(s_frame_dma, 0, frame_pool);
```

That configuration compiles and reports exactly one line, from the token. Every other knob was
answered. Measured on this host with GCC 13.2.0 at `-O2 -Wall -Wextra`.

An ESP32-S3 at its top frequency wants the same knobs with the clock pinned instead:

```
-DPRAET_CLOCK_SOURCE=PRAET_CLOCK_OWN
-DPRAET_CLOCK_CORE=1u
```

Xtensa defines a cycle counter, so that arm is available there. It is refused on a host, where the
same two lines produce the `praet_horologiorum_custos.h:91-93` error alongside the token's line. The S3 build has
not been run; what has been checked is that a host refuses it, which is the derivation working.

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
**Date:** 2026-09-01
