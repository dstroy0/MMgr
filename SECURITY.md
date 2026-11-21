# Security {#proj_security}

## What this library is responsible for

MMgr passes out bounded views over storage a caller supplied. Its security properties are therefore
narrow. They are worth an exact statement, because the gap between what it does and what a reader
assumes it does is where the bugs live.

**It does bound its own reads and writes.** Every entry takes a capacity or a read cap and stops
there. A span that runs out of room latches `overflow`, and the latch holds; a short read latches
`err`. No later call clears either. A caller checks a long run of appends once at the end, never
after every one.

**It does zero secrets on request.** `occult.wipe()` writes through a `volatile` word pointer. The
compiler cannot remove that store the way it can remove a plain `memset` before a free. That is the
only thing in the library that promises a value is actually gone from memory.

**It checks its extents at the declaration and never at run time.** You declare a cellblock and
never initialize it. The pool macro emits the storage and its alignment, and `LocusCarcerum(name, ...)`
emits the state as data. No call inspects a base and a length while the program runs.

A cellblock cannot disagree with the storage behind it, because both come from the same
declaration. The pool asserts its own `sizeof` against the byte count it gets. `MMGR_CARCER_BODY`
asserts that the same `sizeof` is a power of two and holds at least one cell. A mismatch fails the
build.

The library still takes as written everything it reaches through a pointer it did not hand out. It
uses an address the caller supplies as given.

**It does not protect against a caller that holds a stale pointer.** The interim tier releases
storage by mark and never by pointer. Nothing reallocates and nothing moves. A pointer passed out
after a mark is dead the moment the caller releases that mark - and it still points at readable
memory.

**Contexts share a cellblock on atomic words.** Each boundary is one atomic word. A take is one
fetch-add on it through the cellblock's turnstiles. A give is one fetch-or on the cell's owner word.
Every count is an atomic word, and no entry takes a lock.

The cellblock never passes a cell to two contexts. Every byte that a give returns comes off the
tiers once the cells above it return too. A temporary mark is still good only to the caller that took it. A rewind drops every temporary cell
taken since the mark, whoever took it. The ring in `memoria_anularis` is single-producer
single-consumer, and it is not safe for more.

## Hardening the build

Set `MMGR_DEBUG_CHECKS=1` and point `MMGR_ASSERT` at something that aborts. That is the `checks`
environment, and it is a real gate. It compiles in the contract asserts, and a violated
precondition fails a test and is not a no-op nobody notices. Run it in CI as well as locally.

## Reporting

Open a private security advisory at
<https://github.com/dstroy0/MMgr/security/advisories/new>, or e-mail dquigg123@gmail.com. Please
include the environment (`host`, `word32`, `word16`, `idx16`, `checks`), the compiler, and the
smallest input that shows the behavior.

This library has one maintainer. There is no patch SLA. Fixes land on `main`, and the changelog,
@ref proj_changelog, notes them.
