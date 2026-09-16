# Dropping in for &lt;string.h&gt; {#guide_string_shim}

An opt-in header that redirects the `mem*` and `str*` family onto this library's bounded entries.

## What it is

`mmgr_string_shim.h` claims libc's include guards and then defines `memcpy`, `strlen`, `strstr`,
`strncmp`, `strlcpy` and the rest as macros over MMgr's SWAR implementations.

It is for the case where you have existing code you do not want to rewrite, on a target where you
would rather not link libc's string functions at all.

```c
#include "mmgr_string_shim.h"   /* FIRST. Before anything that might include <string.h> */
#include "some_existing_code.h"
```

## Include order is the whole of it

The shim works by claiming libc's include guards. It defines `_STRING_H`, `_STRING_H_`,
`__STRING_H__`, `_STRING_H_INCLUDED` and `_INC_STRING` itself, so a later `#include <string.h>`
anywhere in the translation unit expands to nothing and the real declarations never arrive.

That only works if the shim gets there first. If something above it already pulled in the real
`<string.h>`, libc's declarations are in scope and the shim's macros then rewrite calls that were
declared differently.

Nothing detects this for you. There is no `#error` in the header and no way for it to tell that
libc got there first, because by then the guards are already set and the shim's own `#ifndef` sees
them and does nothing. Put the include first and keep it first.

## What is mapped

| you write                                         | you get                                                 |
| ------------------------------------------------- | ------------------------------------------------------- |
| `memcpy`, `memmove`, `memcmp`, `memchr`, `memset` | `memor.*` — SWAR, a word at a time                      |
| `strlen`, `strnlen`                               | `cellul.len` — bounded by `MMGR_STR_MAX`                |
| `strcmp`, `strcasecmp`                            | `cellul.eq`                                             |
| `strncmp`, `strncasecmp`                          | `cellul.diff`                                           |
| `strstr`, `strcasestr`                            | `cellul.find` — sieved on the rarest byte of the needle |
| `strchr`                                          | `cellul.chr`                                            |
| `strlcpy`                                         | `cellul.copy` — always terminates                       |

Every `str*` one takes a read cap, explicitly or through `MMGR_STR_MAX`. That is the difference from
libc: there is no entry here that will run forward until it happens to find a zero.

## Three places the behavior differs

These are not bugs and they are not going to change. Read them before you assume a drop-in is
actually a drop-in.

**`strcmp` and `strncmp` do not order.** They answer 0 when the strings are equal and 1 when they
are not. That is the right truth value, so `if (strcmp(a, b) == 0)` and `if (strcmp(a, b))` both
behave, but there is no sign. **A `qsort` comparator built on either one will not sort.** Use
`cellul.diff` and compare the bytes yourself if you need an ordering.

**`strlcpy` returns the length it wrote,** not the length of the source. BSD's returns the source
length so `>= size` detects truncation; here the truncation test is the return against `limit - 1`.

**`memcmp` does order.** It comes from `memor.cmp` and returns negative, zero or positive the way
libc's does. It is the string compares that lose the sign, not the memory ones.

## Why strcpy and strcat are missing

There is no bounded spelling of `strcpy`. What it promises is "copy until the source ends", and the
destination's size is not one of its arguments — so there is nothing to check against and no cap to
pass. The same is true of `strcat`.

Defining them to something bounded would be worse than leaving them out: it would change their
meaning while leaving the call sites looking correct, and truncation that nobody checks is its own
bug.

So the names are simply not provided, and because the shim also blocks libc's declarations, a call
to one does not compile. That build failure names the file, and that call site is exactly the one
worth looking at. Replace it with `strlcpy`, which takes the destination's size and always
terminates.

## MMGR_STR_MAX

`strlen`, `strcmp`, `strcasecmp`, `strstr`, `strcasestr` and `strchr` take no length, so they are
bounded by `MMGR_STR_MAX`. It is the answer to "how far will this read before giving up on finding a
terminator", and it exists because an unterminated string is a real thing that happens to data
arriving from outside.

Set it to the largest string your program can legitimately hold: too small silently truncates work
you meant to do, too large means a runaway read scans further before stopping. It is a cap, not a
buffer size — nothing is allocated from it, and no pool is sized by it.

## When not to use it

Do not reach for the shim just to get the SWAR implementations. Call them directly:

```c
EMBED_CALL(memor.cpy, MemoriaCfg, .dst = dst, .src = src, .bytes = n);

const char *const hit = EMBED_CALL(cellul.find, CatenaFinitaCfg,
                                  .src = hay,    .cap = hay_cap,
                                  .other = needle, .other_cap = needle_cap,
                                  .ci = EMBED_FALSE);
```

Direct calls name their arguments, carry their caps explicitly, keep libc's semantics out of the
picture, and do not depend on include order anywhere in your build. The shim is for code you are not
going to touch.
