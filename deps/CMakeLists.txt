# ------------------------------------------------------------------------------------------------
# embedded_types
# ------------------------------------------------------------------------------------------------
# The widths, the machine word, the static assertion, the attribute wrappers, the dispatch-layout
# assertions and the entry point macros are not MMgr's. They are the same boilerplate every library
# here carries, so they are declared once in their own repository and this one names them as they
# come. There is no alias layer: src/ writes EMBED_ directly.
#
# Tracks main. The two repositories move together: when the widths change, the code and the tests
# here change with them, and embedded_types carries its own suite for its own half.
include(FetchContent)

# SOURCE_DIR puts the library at deps/embedded_types. Without it the checkout goes to
# build/_deps/embedded_types-src, one clone of the same commit per build tree, and there are eight
# build trees in this working copy. FetchContent's own bookkeeping stays in the build tree.
FetchContent_Declare(
  embedded_types
  GIT_REPOSITORY https://github.com/dstroy0/embedded_types.git
  GIT_TAG main
  GIT_SHALLOW TRUE
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/embedded_types")

# Its suites are its own. This build compiles MMgr's, and a consumer that wanted the other would
# configure that repository directly.
set(EMBEDDED_TYPES_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(embedded_types)
