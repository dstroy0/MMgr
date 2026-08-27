# Shared ESP-IDF project setup for every on-device microbenchmark under performance_benching/.
# Each feature's own CMakeLists.txt includes this file, then calls project().
#
# The library is not registered as a component here. MMgr's root CMakeLists is a plain CMake project
# that calls project(), and ESP-IDF runs a component's CMakeLists in script mode while working out
# requirements, where project() is not a legal command. Naming the root in EXTRA_COMPONENT_DIRS
# therefore fails the configure. Each bench's main component compiles the library's sources itself,
# which also keeps the desktop build untouched.
#
# The component set is left alone as well. PlatformIO supplies its own build information component,
# and naming the set explicitly drops it, which fails the configure a different way.

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
