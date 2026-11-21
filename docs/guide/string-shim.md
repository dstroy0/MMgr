# Dropping in for &lt;string.h&gt; {#guide_string_shim}

An opt-in header that redirects the `mem*` and `str*` family onto this library's bounded entries.

## What it is

`mmgr_string_shim.h` claims libc's include guards and then defines `memcpy`, `strlen`, `strstr`,
`strncmp`, `strlcpy` and the rest as macros over MMgr's SWAR implementations.

It is for existing code you do not want to rewrite, on a target where you prefer not to link libc's
string functions at all.

```c
#include "mmgr_string_shim.h"   /* FIRST. Before anything that might include <string.h> */
#include "some_existing_code.h"
```

## Include order decides everything

The shim works because it claims libc's include guards. It defines `_STRING_H`, `_STRING_H_`,
`__STRING_H__`, `_STRING_H_INCLUDED` and `_INC_STRING` itself. A later `#include <string.h>`
anywhere in the translation unit expands to nothing and the real declarations never arrive.

That works only if the shim gets there first. If something above it already pulled in the real
`<string.h>`, libc's declarations are in scope. The shim's macros then rewrite calls that libc
declared differently.

Nothing detects this for you. There is no `#error` in the header and no way for it to tell that
libc got there first. By then the guards are already set, and the shim's own `#ifndef` sees them and
does nothing. Put the include first and retain it first.

## What is mapped

| you write                                         | you get                                                |
| ------------------------------------------------- | ------------------------------------------------------ |
| `memcpy`, `memmove`, `memcmp`, `memchr`, `memset` | `memor.*`, SWAR, a word at a time                      |
| `strlen`, `strnlen`                               | `cellul.len`, bounded by `MMGR_STR_MAX`                |
| `strcmp`, `strcasecmp`                            | `cellul.eq`                                            |
| `strncmp`, `strncasecmp`                          | `cellul.diff`                                          |
| `strstr`, `strcasestr`                            | `cellul.find`, sieved on the rarest byte of the needle |
| `strchr`                                          | `cellul.chr`                                           |
| `strlcpy`                                         | `cellul.copy`, always terminates                       |

Every `str*` one takes a read cap, explicitly or through `MMGR_STR_MAX`. This is how it differs from
libc: there is no entry here that will run forward until it happens to find a zero.

## Three places the behavior differs

These are not bugs and they will not change. Read them before you assume a drop-in is
actually a drop-in.

**`strcmp` and `strncmp` do not order.** They answer 0 when the strings are equal and 1 when they
are not. That is the right truth value, and so `if (strcmp(a, b) == 0)` and `if (strcmp(a, b))` both
behave, but there is no sign. **A `qsort` comparator built on either one will not sort.** Use
`cellul.diff` and compare the bytes yourself if you need an ordering.

**`strlcpy` returns the length it wrote,** not the length of the source. BSD's returns the source
length so `>= size` detects truncation; here the truncation test is the return against `limit - 1`.

**`memcmp` does order.** It comes from `memor.cmp` and returns negative, zero or positive the way
libc's does. It is the string compares that lose the sign and not the memory ones.

## Why strcpy and strcat are missing

There is no bounded form of `strcpy`. It promises "copy until the source ends", and the
destination's size is not one of its arguments. So there is nothing to check against and no cap to
pass. The same is true of `strcat`.

A bounded definition of them would be worse than none. It would change what they do while the call
sites still look correct, and truncation that nobody checks is its own bug.

So the shim does not provide the names, and because it also blocks libc's declarations, a call to
one does not compile. That build failure names the file, and that call site is precisely one worth a
look. Replace it with `strlcpy`, which takes the destination's size and always
terminates.

## MMGR_STR_MAX

`strlen`, `strcmp`, `strcasecmp`, `strstr`, `strcasestr` and `strchr` take no length, and so
`MMGR_STR_MAX` bounds them. It answers "how far will this read before it gives up on a terminator".
It exists because an unterminated string is a real thing in data that arrives from outside.

Set it to the largest string your program can legitimately hold. Too small silently truncates work
you meant to do; too large lets a runaway read scan further before it stops. It is a cap and sizes
no buffer: nothing allocates from it, and no pool takes its size from it.

## When not to use it

Do not reach for the shim just to get the SWAR implementations. Call them directly:

```c
EMBED_CALL(memor.cpy, MemoriaCfg, .dst = dst, .src = src, .bytes = n);

const char *const hit = EMBED_CALL(cellul.find, CatenaFinitaCfg,
                                  .src = hay,    .cap = hay_cap,
                                  .other = needle, .other_cap = needle_cap,
                                  .ci = EMBED_FALSE);
```

Direct calls name their arguments and carry their caps explicitly. They retain libc's semantics out
of the picture, and they do not depend on include order anywhere in your build. The shim is for code
you will not touch.
