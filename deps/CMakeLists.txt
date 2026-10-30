# ------------------------------------------------------------------------------------------------
# embedded_types
# ------------------------------------------------------------------------------------------------
# The widths, the machine word, the static assertion, the attribute wrappers, the dispatch-layout
# assertions and the entry point macros are not MMgr's. They are the same boilerplate every library
# here carries. They are declared once in their own repository and this one names them as they
# come. There is no alias layer: src/ writes EMBED_ directly.
#
# Pinned to an explicit SHA. The two repositories move together: when the widths change, the code
# and the tests here change with them, and embedded_types carries its own suite for its own half.
# Moving to a newer one is a deliberate edit of GIT_TAG below, never a fetch that drifts. A
# floating ref is a defect.
include(FetchContent)

# SOURCE_DIR puts the library at deps/embedded_types. Without it the checkout goes to
# build/_deps/embedded_types-src, one clone of the same commit per build tree, and there are eight
# build trees in this working copy. FetchContent's own bookkeeping stays in the build tree.
FetchContent_Declare(
  embedded_types
  GIT_REPOSITORY https://github.com/dstroy0/embedded_types.git
  GIT_TAG 132d75cdde1ae2682d7d6410990f8cb70d0f018f
  # GIT_SHALLOW with a commit hash needs the remote to serve a reachable SHA to `fetch --depth 1`.
  # github.com/dstroy0/embedded_types does, measured against this SHA. The depth stays.
  GIT_SHALLOW TRUE
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/embedded_types")

# Its suites are its own. This build compiles MMgr's, and a consumer that wanted the other would
# configure that repository directly.
set(EMBEDDED_TYPES_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(embedded_types)
