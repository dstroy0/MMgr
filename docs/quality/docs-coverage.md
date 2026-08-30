# Documented symbol coverage {#qa_docs_coverage}

What counts as documented, what is not yet, and why the docs build does not fail over it.

## How it is measured

`docs/Doxyfile` sets `EXTRACT_ALL = NO`, and that is not a style preference. Doxygen disables
`WARN_IF_UNDOCUMENTED` whenever `EXTRACT_ALL` is `YES` — so `YES` does not weaken the coverage
signal, it switches it off, and the run then reports a clean tree no matter how much is
undocumented.

With it `NO`, the warning stream is the measurement:

```sh
doxygen docs/Doxyfile
grep -c 'is not documented' docs/doxygen-warnings.log
```

The Docs workflow captures the same log, counts it by class, and writes the totals into the job
summary.

## Where it stands

Every module has a `@file` block and every free function has `@brief`, `@param` and `@return`. The
remaining gap is two specific classes:

| class                  | roughly | why                                                    |
| ---------------------- | ------: | ------------------------------------------------------ |
| dispatch-table members |    ~180 | each `(*name)(args)` loculus inside a `<Mod>Ns` struct |
| macro definitions      |     ~77 | `MMGR_DBL_*`, `MMGR_RING_*`, the directive macros      |
| typedefs               |     ~11 | mostly internal shapes                                 |

The dispatch-table members are the bulk, and they are a genuine question rather than an oversight:
each loculus points at a free function that is already documented, so documenting the loculus as well
duplicates the prose and creates a second copy to keep in step. The alternatives are to document
each loculus, or to accept the gap and let the reader follow the pointer.

## Why the build does not fail on it

`WARN_AS_ERROR` is `NO`. Failing the docs build today would block every deploy for a backlog rather
than for a regression, which trains everyone to ignore the gate — the failure mode a gate exists to
prevent.

The intended end state is one line in `docs/Doxyfile`:

```
WARN_AS_ERROR = FAIL_ON_WARNINGS_PRINT
```

Not `YES`. `YES` aborts Doxygen at the first warning, so you fix one, re-run, and find the next, and
you never see how much is left. `FAIL_ON_WARNINGS_PRINT` completes the run, prints everything, and
then exits non-zero — so one run gives you the whole list.

Nothing in the workflow changes when that flips. The doxygen step becomes the gate on its own.

## Structural warnings are different

A structural warning — an unterminated code span, an unresolved `@ref`, an angle bracket read as an
HTML tag — corrupts the rendered page from that point onward. Those are worth fixing immediately and
the workflow lists them individually in the job summary rather than counting them.

Two real examples, both already fixed, both invisible until the site was built:

- A literal backtick inside an ASCII bit-layout table in `verbum_scrutor.h` opened a code span that
  never closed and swallowed the rest of the file. Bit layouts and ASCII tables belong inside
  `@verbatim` / `@endverbatim`.
- `<Mod>Ns` in `mmgr_compiler_directives.h` was parsed as an HTML tag. Angle-bracket placeholders
  need backticks.

Both are in the comment rules in `CONTRIBUTING.md`.

## Spelling

`cspell` runs over `README.md` and `docs/**`, not over `src/`. This library's vocabulary is nineteen
Latin module names and a wall of SWAR terminology; a spellchecker pointed at the C comments would
spend its life being told that `memoria_anularis` is a word.

The project dictionary is a flat `words` list in `cspell.json` rather than a separate dictionary
file, so there is one place to look and a diff shows exactly which term a change introduced.
