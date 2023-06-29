cmake_minimum_required(VERSION 3.17)
project(seer C)
enable_testing()

set(CMAKE_C_STANDARD 99)


if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(isDebug TRUE)
else()
    set(isDebug FALSE)
endif()

if (isDebug)
    message("Debug build detected")
    add_compile_definitions(CONFIGURATION_DEBUG TYRAN_CONFIGURATION_DEBUG)
    set(CONFIGURATION_DEBUG 1)
endif()


add_executable(seer_test
    main.c
    test.c
    ${local_deps_src}
)

add_test(NAME seer_test
         COMMAND seer_test)

if (WIN32)
target_link_libraries(seer_test seer)
else()
target_link_libraries(seer_test seer m)
endif(WIN32)

