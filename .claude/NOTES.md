# NOTES

Written 2026-09-16 by the tree-wide objectives pass. Read this before working in MMgr. Paths are
relative to the repository root unless stated otherwise.

MMgr is both a consumer and a provider in this tree. It is the upstream of `include/MMgr` in
ProtoCore and idemIP, and it consumes embedded_types. Both of those edges change.

## 1. Where the tools are, and where they go

Every tool in this repository sits in one undifferentiated directory, `tools/dev_env/`:

`against_libc.py`, `codemask.py`, `dedup.py`, `gen_ancorae_formae.py`,
`gen_ascii_persona_bitorum.py`, `gen_pow5.py`, `gen_praet_scenarios.py`, `ladder_analysis.R`,
`language_variance.R`, `math_hazards.py`, `move_code.py`, `names.tsv`, `nsconv.py`,
`nsconv_test.py`, `ratio_normality.R`, `readclean.py`, `readclean_test.py`, `rename_modules.py`,
`sizes.py`, `src2png.py`, `strip_comments.py`, `target_sizes.py`, `yank_includes.py`.

`test/harness.py` is a test driver, not a tool, and stays under `test/`.

Objective 5 moves every repository's tools under `tools/`. MMgr already has the top-level name:
`[layout] tools` is not stated in `repotools.toml`, so it inherits the default `tools`. The work
here is internal — split `dev_env/` by kind using the toolkit's own set names, so that promoting a
tool upstream later is a rename rather than a re-sort.

Four of these already have an upstream twin and should converge rather than be filed twice:

- `math_hazards.py` — anchor_sift fetches the same tool from the toolkit set `docs/docs_maint`.
- `src2png.py` — anchor_sift fetches the same tool from `media_tools/source_render`.
- `sizes.py` and `dedup.py` — `repotools.toml` lines 76-80 already name both as worth fetching once
  they are promoted.

One must not converge. `repotools.toml` lines 76-80 record that the toolkit's `readclean.py` is a
different tool wearing the same name, sharing no API at all with the shape-aware C blinder here,
and that fetching it would orphan `readclean_test.py`, which passes twenty-five assertions against
the local one. Keep the local tool. If the name collision survives the move, rename the local one
rather than the test.

`tools/dev_env/__pycache__/` holds four `.pyc` files on disk. Untracked; leave it.

## 2. The dependency standard, and what MMgr must change

The decided standard is **submodule-first**. Where a repository takes a directory from another
repository, it takes it as a git submodule, narrowed with `--no-cone` sparse patterns recorded in
`.gitmodules` as `sparsePaths`, and the bootstrap script asserts the post-narrow shape.

### MMgr is step 0 for the whole tree

MMgr carries two unrelated histories, and nothing else in the dependency work can proceed until
this is resolved. Measured in this repository:

- `main` is at `69d7cb8965d56e3917b060579be3d28a36c36c84` ("human language signature detection
  research", 2026-09-04).
- ProtoCore's gitlink is `ebc417397e6f64797369dd1515f1da50aa0e1122` ("Namespace every module, and
  unblock CI", 2026-08-21 18:42:25 -0400).
- idemIP's gitlink is `37c65c48525b8bc98691d989d4b7d64fbdc75049` ("Test what was written and never
  run", 2026-08-21 19:19:01 -0400), 37 minutes after ProtoCore's.
- `git branch --contains` puts both on branch `opt`. `git merge-base opt main` returns nothing:
  the two histories are unrelated. Neither pin is an ancestor of `main`.

Re-pointing the consumers is a **port, not a SHA swap**. The `opt`-era tree has no `include/`
directory and a different `src/` module set from `main`. Whoever fixes this decides which history
survives and then ports the consumers to it.

### MMgr's own dependency stays as it is, pinned

`deps/CMakeLists.txt` declares embedded_types through `FetchContent` with `GIT_TAG main`. This is
the one documented exception to submodule-first, and it is deliberate: the dependency is consumed
at CMake configure time by a build that must work for someone who has never run any of the Python
tooling, and converting it would make a C build depend on Python.

The change is to kill the floating ref only. Replace `GIT_TAG main` with the explicit
40-character SHA `b0aacdaafd986b8c5b23f127f312a02a5cfd13e6`, and move the existing "Tracks main.
The two repositories move together" sentence down into the comment beside the SHA, since it stops
being true the moment the pin lands. That SHA is embedded_types `HEAD` today and is also what
`deps/embedded_types` is checked out at, so the pin is a no-op for the current build tree.

No `.gitignore` change is needed. Lines 7-8 are `deps/*` then `!deps/CMakeLists.txt`, and
`git ls-files deps` returns exactly one path, `deps/CMakeLists.txt`. The nested embedded_types
checkout is working-tree-only and is not committed.

### Nothing to unwind for repotools

`repotools.toml` lines 73-80 leave `[fetch] sets` unset on purpose, and there is no
`repotools.lock` here. MMgr vendors no toolkit file, so the retirement of `repotools fetch`, the
lock file and the stamp subsystem does not touch this repository.

`.gitattributes` is present here. That matters under submodules: a mount has no CRLF normalization
layer of its own. A consumer cloning this repository on a machine with a different
`core.autocrlf` gets whatever `.gitattributes` says. Do not delete it.

## 3. Backgrounded agents may commit

Backgrounded agents are permitted to commit in this repository. The commit message is **terse and
names category, subject and type only** — for example `docs build bugfix`. No body, no attribution
trailer, no prose.

Two preconditions apply here specifically:

- `.claude/settings.json` is `{"worktree": {"bgIsolation": "none"}}`. Backgrounded agents share
  this working tree and this index with the foreground session. Stage explicitly with
  `git add <named paths>`. Do not use `git commit -a` and do not use bare `git add .`.
- `core.hooksPath` is unset in this clone and `.git/hooks` holds only samples. MMgr ships no hooks
  of its own. A commit here currently runs zero gates, including the banned-word guard at
  `C:/Users/Douglas/.claude/commit_guard/`. Wire the gates before granting commit authority in
  practice.

## 4. The build script this repository needs

Objectives 8 and 18: every repository needs one script that builds all of `src/` and `examples/`
and walks a user through it, with a worked invocation in the README.

What MMgr has today:

- `README.md:85-86` — `cmake -S . -B build -DMMGR_BUILD_TESTS=ON` then `cmake --build build`.
- `README.md:92-94` — the harness route: `python test/harness.py test`, `... ab`, `... coverage`.
- `README.md:97-101` — the `MMGR_CMAKE_ARGS` and `MMGR_BUILD_ROOT` path-length caveat, which is
  genuinely useful and should survive any rewrite.

The two gaps:

- **There is no `examples/` directory at all.** `repotools.toml:32` states `examples = []` and
  explains why the key is stated rather than omitted. Objective 18's "src/ and examples/" cannot be
  satisfied until `examples/` exists. A 22-module library whose only runnable artifacts are test
  suites and benches needs one worked use per module group.
- **The documented first build silently requires the network.** `deps/CMakeLists.txt` clones
  embedded_types from GitHub during configure. The Building section never says a clone happens and
  names no offline escape. Add one line stating it, and name the escape.

## 5. Where the skills live

`D:/git_project/repos/owned/private/repo_tools/skills`

Five skills are there now: `code-python`, `code-shell`, `code-verify`, `docs-readme`,
`repotools-workflow`. None of them is installed anywhere a harness discovers skills, and four of
five declare a frontmatter `name` that differs from their directory name, so every cross-reference
between them currently cites an identifier that cannot be typed. Objective 6 rebuckets and rewrites
them; an install mechanism lands first, since a skill that is not where the harness reads it does
nothing.

MMgr is C11. The two skills that bind work here, `code-c11` and `code-comments`, live only at
`C:/Users/Douglas/.claude/skills/` today and are moving into the repository path above.

Two things about the `mmgr-workflow` skill at `C:/Users/Douglas/.claude/skills/mmgr-workflow/`:

- Line 42 reads "Git is `commit` and `push` only, and only when asked. No `checkout`, `stash`,
  `reset`, `status`, `log`, `diff`, branch or remote operations." That is the rule objective 7
  overturns for this tree. A backgrounded agent committing on its own initiative was not asked, and
  an agent forbidden `git status` and `git diff` cannot compose a category/subject/type message nor
  verify what it is about to stage. Treat section 3 above as governing until that line is amended.
- Line 77 defers to "the MMgr memory directory". No such directory exists. `MMgr/.claude/` holds
  `settings.json` and an empty `worktrees/coverage-100/`. **This file is that reference's
  replacement.**

`MMgr/.claude/worktrees/coverage-100/` is an empty leftover and `MMgr/.git/worktrees` does not
exist, so there is no registration behind it. Flagged for the repo agent; not removed here.

## 6. The prose gates are becoming pre-commit gates

Objective 12 adds an AI-word detector and objective 13 adds a British-English ban, and both become
pre-commit gates in every repository. British spellings are banned in comments, docstrings and
description blocks unless the subject itself is British.

MMgr runs no gate today. `repotools.toml` lines 69-71 leave `[hooks] gates` unset on purpose, and
`core.hooksPath` is unset. The scope when the gates land is `[prose] roots` at lines 59-67:
`README.md`, `CHANGELOG.md`, `CONTRIBUTING.md`, `SECURITY.md`, `dma-plan.md`, `writing-bans.md`,
`docs`. Note that `writing-bans.md` already states this repository's writing rules; reconcile it
with the new gates rather than letting the two disagree.

The reference implementations exist in anchor_sift at `maint/prose/ai_detect.py` and
`maint/prose/english_gate.py`, beside `maint/prose/docs_check.py`. They are promoted upstream into
repo_tools and reach MMgr from there.

## 7. The theory layout

Every repository, public and private, gets exactly two directories for written work:

- `workbook/` — locally authored, top level, a sibling of `src/` and `tools/`. The book about
  **this** repository: its engine, its results, its reproduction instructions.
- `theory/` — wholly a git dependency of the `theory_bucket` repository. Nothing is authored here.
  Every directory inside arrived from the theory_bucket remote, and the whole directory can be
  deleted and re-fetched without losing work. Each upstream book is pulled individually. A
  consumer takes only the books it asks for.

All theory, from every public and private repository, is authored upstream in theory_bucket.

**What MMgr has today: neither.** There is no `theory/`, no `theory_bucket/` and no `workbook/`.
`docs/` is the only prose tree besides the root markdown files, and it is reference documentation
about the library rather than a book. Nothing in this repository violates the layout.

What that means going forward:

- Do not create `theory/` until there is a book to mount. An empty mount point is a liability under
  the refusal rule, which enumerates the mount and refuses on any file it cannot account for.
- If MMgr acquires a book about its allocator — the DMA plan's successor, the A/B results against
  libc, the ladder analysis — the general theory is authored in theory_bucket and returns through a
  `theory/` submodule, and the MMgr-specific reproduction prose goes in `workbook/` at the root.
- `dma-plan.md` at the root is the closest thing here to workbook material. When `workbook/` is
  created it is the first candidate to move into it.
