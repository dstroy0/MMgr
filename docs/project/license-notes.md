# AGPL-3.0-or-later, in practice {#proj_license_notes}

@warning This page is a plain-language summary written by the author and not legal advice. The license
text in `LICENSE` governs. If the distinction matters to your project, ask a lawyer and not a
README.

## What the project is under

GNU Affero General Public License, version 3 or later. Every source file carries an SPDX identifier:

```c
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
```

So the license of any file is a machine-readable fact instead of something to infer from a
directory it happens to sit in.

## The part that surprises people

You consume MMgr as **source**, by `add_subdirectory`, or by PlatformIO and Arduino, which copy
`src/` into your build. There are no `install()` rules and no binary artifact. See @ref guide_install
for why.

So the ordinary "do I link or copy" distinction does not apply here in the way it does for a shared
library. Your build compiles these files into your program.

The AGPL's distinctive clause is section 13. Take a program you modify, where users interact with it
**over a network**. Those users must be able to get the source of that version. That obligation
attaches to network interaction, not only to binaries you ship, and that separates the AGPL from the
GPL.

For a memory manager on a device this often does not arise. For a memory manager inside a networked
service, it does. Which of those you build is the question to answer before you adopt it.

## Citing it

`CITATION.cff` is machine-readable, and GitHub renders a "Cite this repository" control from it.

```
Quigg, Douglas (dstroy0). MMgr, version 2.3.0. 2026.
https://github.com/dstroy0/MMgr
```

## If the terms do not suit you

Ask. One person owns the copyright, and so a dual license is a direct arrangement by email:
<dquigg123@gmail.com>.

This page says so on purpose. People silently pass over a project that offers no route other than
the AGPL, and those people would otherwise use it and contribute back.

## Contributing

The project accepts contributions under the same license. There is no CLA. A pull request means you
have the right to contribute the code, and you contribute it under AGPL-3.0-or-later.

Retain the SPDX header on every new file. No tool checks it, and so a review has to.
