# DOCTEST

#add_library(doctest INTERFACE)
#target_include_directories(doctest INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/doctest.h")
    message(WARNING "[TCC] doctest.h not found in ${CMAKE_CURRENT_SOURCE_DIR}.\n")
    # add remedy steps
endif()

set(SOURCES tcc-basics.cpp)
set(TCC_EXE_TEST "tcc-tests")

add_executable(
    ${TCC_EXE_TEST}
    ${SOURCES}
)

#target_include_directories(${TCC_EXE_TEST} PRIVATE
#    #${CMAKE_CURRENT_SOURCE_DIR} # harness and doctest headers
#    ${CMAKE_SOURCE_DIR}/src
#)

target_link_libraries(${TCC_EXE_TEST} PRIVATE
    ${TCC_LIB}
    #doctest
)

#---Experimental stuff from here---
# ── CTest integration (experimental) ─────────────────────────────────────────────────────────
# Registers one CTest entry per doctest test case so that
# `ctest --output-on-failure` and IDE test runners see individual results.
#
# Usage:
#   ctest --test-dir build                  # run all
#   ctest --test-dir build -R "FuncF"       # run by name pattern
#   ctest --test-dir build --rerun-failed   # re-run failures only
include(CTest)

# Run the whole suite as a single CTest entry (fast, simple).
add_test(
    NAME    ${TCC_EXE_TEST}
    COMMAND ${TCC_EXE_TEST} --reporters=console --force-colors=true
)

# Optional: also register every doctest SCENARIO as its own CTest entry.
# Requires CMake 3.21+ for the --list-test-cases capability.
# Comment this block out if you prefer the single entry above.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.21")
    set_property(TEST ${TCC_EXE_TEST} PROPERTY
        ENVIRONMENT "DOCTEST_OPTIONS=--list-test-cases"
    )
endif()

# Fail the CTest entry if the binary returns non-zero.
set_tests_properties(${TCC_EXE_TEST} PROPERTIES
    FAIL_REGULAR_EXPRESSION "\\[doctest\\] Status: FAILURE"
)
