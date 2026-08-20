# Changelog {#proj_changelog}

All notable changes to this project are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

This file is generated from the git history by `git-cliff` using `docs/cliff.toml`. Edit the commit
messages, not this file.

## [Unreleased]

### Added

- Documentation site: Doxygen configuration, the Ledger theme, the group tree, and the narrative
  page set under `docs/`.

### Changed

- Every source banner reads `memmanager`. Sixteen headers still carried the banner of the project
  they were derived from.
- Module naming vocabulary is now uniform: the region type is a confinium everywhere, including in
  the package metadata and the developer tooling.

### Fixed

- `verbum_scrutor.h`: a literal backtick inside the ASCII block layout opened a code span that never
  closed, which swallowed the remainder of the file in the generated documentation. The block is now
  `@verbatim`.
- `mmgr_compiler_directives.h`: `<Mod>Ns` was parsed as an HTML tag in two comment blocks.
- `tools/dev_env/readclean.py`: the entry-member pattern matched one fixed signature that no module
  in this tree uses, so no dispatch entry was ever recognized as an entry. It now matches any
  function-pointer member of a dispatch table.

## [0.1.0] - 2026-08-20

Initial release.
