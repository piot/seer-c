cmake_minimum_required(VERSION 3.16.3)
project(seer C)
enable_testing()

add_subdirectory(deps/piot/clog/src/lib)
add_subdirectory(deps/piot/discoid-c/src/lib)
add_subdirectory(deps/piot/flood-c/src/lib)
add_subdirectory(deps/piot/imprint/src/lib)
add_subdirectory(deps/piot/monotonic-time-c/src/lib)
add_subdirectory(deps/piot/nimble-steps-c/src/lib)
add_subdirectory(deps/piot/nimble-steps-serialize-c/src/lib)
add_subdirectory(deps/piot/tiny-libc/src/lib)
add_subdirectory(deps/piot/transmute-c/src/lib)


add_subdirectory(src)
