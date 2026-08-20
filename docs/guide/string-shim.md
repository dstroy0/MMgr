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

## Include order is the whole contract

The shim works by claiming libc's include guards — it defines `_STRING_H`, `_STRING_H_` and the
platform equivalents itself, so that a later `#include <string.h>` expands to nothing.

That only works if the shim gets there first. If something above it already pulled in the real
`<string.h>`, libc's declarations are already in scope and the shim's macros then rewrite calls to
functions that were declared differently.

The header does not leave this to chance. It checks for the guards it is about to claim and stops
the build:

```
#error mmgr_string_shim.h must be included before <string.h>
```

An `#error` you can read beats a link error three translation units away.

## What is mapped

| you write                                         | you get                                                    |
| ------------------------------------------------- | ---------------------------------------------------------- |
| `memcpy`, `memmove`, `memcmp`, `memchr`, `memset` | `memor.*` — SWAR, a word at a time                         |
| `strlen`, `strnlen`                               | `cellul.len` — bounded by `MMGR_STR_MAX`                   |
| `strcmp`, `strncmp`                               | `cellul.diff` / `cellul.eq`                                |
| `strstr`, `strchr`                                | `cellul.find` / `cellul.chr` — anchored on the rarest byte |
| `strlcpy`, `strncpy`                              | `cellul.copy` — always terminates                          |

Every one takes a read cap. That is the difference from libc: there is no entry here that will run
forward until it happens to find a zero.

## Why strcpy and strcat are missing

They are deliberately left undefined, and this is the most useful thing the shim does.

There is no bounded spelling of `strcpy`. Its contract is "copy until the source ends", and the
destination's size is not one of its arguments — so there is nothing to check against and no cap to
pass. The same is true of `strcat`.

Leaving them undefined turns a silent buffer overflow into a **link error**, at build time, naming
the file. Defining them to something bounded would be worse: it would change their meaning while
leaving the call sites looking correct, and truncation that nobody checks is its own bug.

If a link fails on `strcpy` after adding the shim, that call site is exactly the one worth looking
at. Replace it with `cellul.copy`, which takes a destination capacity and always terminates.

## MMGR_STR_MAX

Every `str*` replacement is bounded by `MMGR_STR_MAX`. It is the answer to "how far will this read
before giving up on finding a terminator", and it exists because an unterminated string is a real
thing that happens to data arriving from outside.

Set it to the largest string your program can legitimately hold. Too small silently truncates work
you meant to do; too large means a runaway read scans further before stopping. It is a cap, not a
buffer size — nothing is allocated from it.

## When not to use it

Do not reach for the shim just to get the SWAR implementations. Call them directly:

```c
memor.cpy(dst, src, n);
cellul.find(hay, hay_len, needle, needle_len, MMGR_FALSE);
```

Direct calls are clearer at the call site, carry their caps explicitly, and do not depend on include
order anywhere in your build. The shim is for code you are not going to touch.
