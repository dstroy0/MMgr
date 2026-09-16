# test/support

Helpers shared by suites: fixtures, fakes, and anything a suite needs that is not
itself under test. Nothing here is compiled into the library.

## Components with their own document

- [docs/research/anchor-sift.md](../../docs/research/anchor-sift.md) - the digest oracle, the byte and
  bit difference pair, and what an anchor proves against what it only costs. `mmgr_sha256` appears
  there as the test oracle and the generator of the uniform control corpus, so the strength of that
  control is the strength of its vectors.
