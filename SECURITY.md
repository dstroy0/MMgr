# Security {#proj_security}

## What this library is responsible for

MMgr hands out bounded views over storage a caller supplied. Its security properties are therefore
narrow and worth stating exactly, because the gap between what it does and what a reader assumes it
does is where the bugs live.

**It does bound its own reads and writes.** Every entry takes a capacity or a read cap and stops
there. A span that runs out of room latches `overflow` and keeps latching it; a short read latches
`err`. Neither is cleared by a later call. A long run of appends is checked once at the end, never
after every one.

**It does zero secrets on request.** `occult.wipe()` writes through a `volatile` word pointer. The
store cannot be optimized away the way a plain `memset` before a free can be. That is the only
thing in the library that promises a value is actually gone from memory.

**It checks its extents at the declaration and never at run time.** A cellblock is declared, never
initialized: the pool macro emits the storage and its alignment, `LocusCarcerum(name, ...)` emits the
state as data, and no call inspects a base and a length while the program runs.

A cellblock can no longer disagree with the storage behind it, because both come from the same
declaration. The pool asserts its own `sizeof` against the byte count it was handed, and
`MMGR_CARCER_BODY` asserts the same `sizeof` is a power of two and holds at least one cell. A
mismatch fails the build.

What is still taken as written is everything reached through a pointer the library did not hand out.
An address the caller supplies is used as given.

**It does not protect against a caller holding a stale pointer.** Interim storage is released by
mark and never by pointer. Nothing is reallocated and nothing moves. A pointer handed out after a mark
is dead the moment that mark is released - and it still points at readable memory.

**A cellblock is shared on atomic words.** Both boundaries are one atomic word, a take is one swap on
it, a give is one and on the cell's own word, every count is an atomic word, and no entry takes a
lock. A cell is never handed to two contexts, and every byte given back comes off the tiers once the
cells above it are given back; a temporary mark is still good only to the caller that took it, since
a rewind drops every temporary cell taken since the mark whoever took it. The ring in
`memoria_anularis` is single-producer single-consumer, and it is not safe for more.

## Hardening the build

Set `MMGR_DEBUG_CHECKS=1` and point `MMGR_ASSERT` at something that aborts. That is the `checks`
environment, and it is a real gate: it compiles in the contract asserts, and a violated precondition
fails a test in place of being a no-op nobody notices. Run it in CI as well as locally.

## Reporting

Open a private security advisory at
<https://github.com/dstroy0/MMgr/security/advisories/new>, or e-mail dquigg123@gmail.com. Please
include the environment (`host`, `word32`, `word16`, `idx16`, `checks`), the compiler, and the
smallest input that shows the behavior.

This is a pre-1.0 library maintained by one person. There is no patch SLA. Fixes land on `main` and
are noted in @ref proj_changelog.
