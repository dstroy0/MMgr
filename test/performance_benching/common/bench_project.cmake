# Shared ESP-IDF project setup for every on-device cycle-counter microbenchmark under
# performance_benching/. Each module's own CMakeLists.txt includes this file, then calls project().
#
# Adapted from ProtoCore's, which registers its repo root as an IDF component and names the
# component set explicitly. Neither is done here, for reasons that are MMgr's own.
#
# The library is not registered as a component. MMgr's root CMakeLists is a plain CMake project that
# calls project(), and ESP-IDF runs a component's CMakeLists in script mode while working out
# requirements, where project() is not a legal command; naming the root in EXTRA_COMPONENT_DIRS
# therefore fails the configure. Each bench's main component compiles the library sources it reaches
# itself, which also keeps the desktop build untouched - nothing here reaches back into it.
#
# The component set is left alone as well. PlatformIO supplies its own build information component,
# and naming the set explicitly drops it, which fails the configure a different way.

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
