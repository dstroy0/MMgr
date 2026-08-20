# AGPL-3.0-or-later, in practice {#proj_license_notes}

@warning This page is a plain-language summary written by the author, not legal advice. The license
text in `LICENSE` governs. If the distinction matters to your project, ask a lawyer rather than a
README.

## What the project is under

GNU Affero General Public License, version 3 or later. Every source file carries an SPDX identifier:

```c
// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
```

So the license of any file is a machine-readable fact rather than something to infer from a
directory it happens to sit in.

## The part that surprises people

MMgr is consumed as **source**, by `add_subdirectory` or by PlatformIO and Arduino copying `src/`
into your build. There are no `install()` rules and no binary artifact — see @ref guide_install for
why.

That means the ordinary "am I linking or copying" distinction does not apply here in the way it does
for a shared library. Your build compiles these files into your program.

The AGPL's distinguishing clause is section 13: if you modify the program and let users interact
with it **over a network**, those users must be able to get the corresponding source. That obligation
attaches to network interaction, not only to shipping binaries — which is what separates the AGPL
from the GPL.

For a memory manager on a device this often does not arise. For a memory manager inside a networked
service, it does. Which of those you are building is the question worth answering before you adopt
it.

## Citing it

`CITATION.cff` is machine-readable, and GitHub renders a "Cite this repository" control from it.

```
Quigg, Douglas (dstroy0). MMgr, version 0.1.0. 2026.
https://github.com/dstroy0/MMgr
```

## If the terms do not suit you

Ask. The copyright is held by one person, which means dual licensing is a conversation rather than a
policy question — <dquigg123@gmail.com>.

Saying so here is deliberate. A project that offers no route other than the AGPL gets silently passed
over by people who would otherwise have used it and contributed back.

## Contributing

Contributions are accepted under the same license. There is no CLA. Opening a pull request means you
have the right to contribute the code and are doing so under AGPL-3.0-or-later.

Keep the SPDX header on every new file. `check_stale_facts` in the docs tooling asserts that every
file under `src/` carries the project banner, so a missing one fails a pull request rather than
being noticed a year later.
