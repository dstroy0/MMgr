# Documented symbol coverage {#qa_docs_coverage}

What counts as documented, what is not yet, and why the docs build does not fail over it.

## How it is measured

`docs/Doxyfile` sets `EXTRACT_ALL = NO`, and that is not a style preference. Doxygen disables
`WARN_IF_UNDOCUMENTED` whenever `EXTRACT_ALL` is `YES`. So `YES` does not weaken the coverage
signal: it switches it off. The run then reports a clean tree no matter how many symbols have no
documentation.

With it `NO`, the warning stream is the measurement:

```sh
doxygen docs/Doxyfile
grep -c 'is not documented' docs/doxygen-warnings.log
```

The Docs workflow captures the same log, counts it by class, and writes the totals into the job
summary.

## Where it is

Every module has a `@file` block and every free function has `@brief`, `@param` and `@return`. The
rest of the gap is three specific classes:

| class                  | roughly | why                                                    |
| ---------------------- | ------: | ------------------------------------------------------ |
| dispatch-table members |    ~180 | each `(*name)(args)` loculus inside a `<Mod>Ns` struct |
| macro definitions      |     ~77 | `MMGR_DBL_*`, `MMGR_RING_*`, the directive macros      |
| typedefs               |     ~11 | mostly internal shapes                                 |

The dispatch-table members are the bulk, and they are a genuine question and not an oversight.
Each loculus points at a free function that already has its documentation. A second copy on the
loculus duplicates the prose and makes one more copy to retain in step. The alternatives are to document
each loculus, or to accept the gap and let the reader follow the pointer.

## Why the build does not fail on it

`WARN_AS_ERROR` is `NO`. A docs build that fails today blocks every deploy for a backlog and not for
a regression. That trains everyone to ignore the gate, the failure mode a gate exists to prevent.

The intended end state is one line in `docs/Doxyfile`:

```
WARN_AS_ERROR = FAIL_ON_WARNINGS_PRINT
```

Not `YES`. `YES` aborts Doxygen at the first warning, and so you fix one, re-run, and find the next, and
you never see how much stays. `FAIL_ON_WARNINGS_PRINT` completes the run, prints everything, and
then exits non-zero, and so one run gives you the whole list.

Nothing in the workflow changes when that flips. The doxygen step becomes the gate on its own.

## Structural warnings are different

A structural warning, an unterminated code span, an unresolved `@ref`, an angle bracket read as an
HTML tag, corrupts the rendered page from that point onward. Fix those immediately. The workflow
lists them individually in the job summary and does not count them.

Two cases of this class, and neither shows until the site builds:

- A literal backtick inside an ASCII bit-layout table opens a code span that never closes and
  swallows the rest of the file. Bit layouts and ASCII tables belong inside `@verbatim` /
  `@endverbatim`.
- Doxygen reads `<Mod>Ns` in a comment as an HTML tag. Angle-bracket placeholders need backticks.

Both are in the comment rules in `CONTRIBUTING.md`.

## Form

`test/ste_check.py` checks `README.md`, `CONTRIBUTING.md`, `SECURITY.md` and `docs/**` against the
ASD-STE100 rules. `test/ste_ratchet.tsv` holds the count of findings each document may
carry, and `harness.py ste` fails where a document carries more.

`test/ste_names.txt` lists the technical names the check accepts, one word a line. So there is one
place to look, and a diff shows exactly which name a change adds.
