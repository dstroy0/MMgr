# Shared ESP-IDF project setup for every on-device cycle-counter microbenchmark under
# performance_benching/. Each module's own CMakeLists.txt includes this file, then calls project().
#
# ProtoCore's version registers its repo root as an IDF component and names the
# component set explicitly. Neither happens here, for reasons that are MMgr's own.
#
# The library does not register as a component. MMgr's root CMakeLists is a plain CMake project that
# calls project(). ESP-IDF runs a component's CMakeLists in script mode to work out requirements,
# where project() is not a legal command. So the root in EXTRA_COMPONENT_DIRS fails the
# configure. Each bench's main component compiles the library sources it reaches itself, which also
# keeps the desktop build untouched. Nothing here reaches back into it.
#
# The component set stays alone as well. PlatformIO supplies its own build information component,
# and an explicit set drops it, which fails the configure a different way.

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
