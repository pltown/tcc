# DOCTEST

#add_library(doctest INTERFACE)
#target_include_directories(doctest INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/doctest.h")
    message(WARNING "[TCC] doctest.h not found in ${CMAKE_CURRENT_SOURCE_DIR}.\n")
    # add remedy steps
endif()

set(SOURCES tcc-basics.cpp)
set(TCC_TESTS_EXE TCCTests)

add_executable(
    ${TCC_TESTS_EXE}
    ${SOURCES}
)

target_include_directories(
    ${TCC_TESTS_EXE} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} # harness and doctest headers
    ${CMAKE_SOURCE_DIR}/src
)

if (TARGET LLVM)
    target_link_libraries(${TCC_TESTS_EXE} PRIVATE LLVM)
elseif (TARGET LLVM::LLVM)
    target_link_libraries(${TCC_TESTS_EXE} PRIVATE LLVM::LLVM)
else()
    target_link_directories(${TCC_TESTS_EXE} PRIVATE ${LLVM_LIBRARY_DIRS})
    target_link_libraries(${TCC_TESTS_EXE} PRIVATE ${LLVM_LIBRARIES})
endif()

target_link_libraries(${TCC_TESTS_EXE} PRIVATE
    clang-cpp
    fmt::fmt)
    #    doctest)

# ── CTest integration ─────────────────────────────────────────────────────────
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
    NAME    ${TCC_TESTS_EXE}
    COMMAND ${TCC_TESTS_EXE} --reporters=console --force-colors=true
)

# Optional: also register every doctest SCENARIO as its own CTest entry.
# Requires CMake 3.21+ for the --list-test-cases capability.
# Comment this block out if you prefer the single entry above.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.21")
    set_property(TEST ${TCC_TESTS_EXE} PROPERTY
        ENVIRONMENT "DOCTEST_OPTIONS=--list-test-cases"
    )
endif()

# Fail the CTest entry if the binary returns non-zero.
set_tests_properties(${TCC_TESTS_EXE} PROPERTIES
    FAIL_REGULAR_EXPRESSION "\\[doctest\\] Status: FAILURE"
)
