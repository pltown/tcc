cmake_minimum_required(VERSION 3.28)

project(tcc LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

## Address Sanitizer (for debug build only)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-inline") #sets globally
#add_compile_options(-fno-inline)
#add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
#add_link_options(-fsanitize=address)

# Target-specific flags
#target_compile_options(${TCC_EXE} PRIVATE $<$<CONFIG:Debug>:-fsanitize=address -fno-omit-frame-pointer -g>)
#target_link_options(${TCC_EXE} PRIVATE $<$<CONFIG:Debug>:-fsanitize=address>)

# compiler check
message(STATUS "[TCC] C compiler:   ${CMAKE_C_COMPILER}")
message(STATUS "[TCC] CXX compiler: ${CMAKE_CXX_COMPILER}")

# Require clang 18+
if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "[TCC] This project must be built with clang++ 18 or higher. "
                        "[TCC] Current: ${CMAKE_CXX_COMPILER_ID} (${CMAKE_CXX_COMPILER})")
endif()

# CMake provides CMAKE_CXX_COMPILER_VERSION (e.g., 18.1.3)
if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 18)
    message(FATAL_ERROR "[TCC] Need clang++ 18 or higher. Current: ${CMAKE_CXX_COMPILER_VERSION} "
                        "[TCC] (${CMAKE_CXX_COMPILER}). Reconfigure with -DCMAKE_CXX_COMPILER=clang++-18")
endif()

#list(PREPEND CMAKE_PREFIX_PATH "/usr/lib/llvm-18/lib/cmake")
#list(PREPEND CMAKE_PREFIX_PATH
#    "/usr/lib/llvm-18/lib/cmake"
#    "/usr/lib/llvm-18/lib/cmake/llvm"
#    "/usr/lib/llvm-18/lib/cmake/clang"
#)

# Prefer Ninja
#set(CMAKE_GENERATOR "Ninja" CACHE INTERNAL "")

# Dependencies
find_package(LLVM REQUIRED CONFIG)
if (LLVM_PACKAGE_VERSION VERSION_LESS 18)
    message(FATAL_ERROR "[TCC] Need LLVM >=18, got ${LLVM_PACKAGE_VERSION}")
endif()

find_package(Clang REQUIRED CONFIG)
if (DEFINED CLANG_PACKAGE_VERSION AND CLANG_PACKAGE_VERSION VERSION_LESS 18)
    message(FATAL_ERROR "[TCC] Need Clang >= 18, got ${CLANG_PACKAGE_VERSION}")
endif()

find_package(fmt REQUIRED)
find_package(fmt CONFIG REQUIRED)
message(STATUS "[TCC] Found fmt version ${fmt_VERSION}")

message(STATUS "[TCC] Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "[TCC] Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "[TCC] Using ClangConfig.cmake in: ${Clang_DIR}")

# LLVM include dirs + defs + CMake helpers
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
#include(${LLVM_CMAKE_DIR}/AddLLVM.cmake)
# Not working: AddClang.cmake is not found
#include(${Clang_CMAKE_DIR}/AddClang.cmake) # needed for add_clang_tool etc macros

# TCC Target
##set(TCC_LLVM_DIR ${LLVM_DIR})
##set(TCC_INCLUDE_DIRS
##   ${LLVM_INCLUDE_DIRS}
##   ${CLANG_INCLUDE_DIRS}
##)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin/)

# TCCLIB----------------------------------------------------------
# Make visitor a lib to use with tests
set(TCC_LIB "tcclib")

add_library(${TCC_LIB} STATIC
    src/tcc-census-visitor.cpp
)

llvm_map_components_to_libnames(LLVM_LIBS support core irreader)
#target_include_directories(${TCC_LIB}
#    PUBLIC
#    ${CMAKE_CURRENT_SOURCE_DIR}
#    SYSTEM
#    ${LLVM_INCLUDE_DIRS}
#)
target_link_libraries(${TCC_LIB} PUBLIC
    clang-cpp
    fmt::fmt
    ${LLVM_LIBS}
)
# end TCCLIB------------------------------------------------------

# TCCEXE----------------------------------------------------------
set(TCC_SOURCES
    src/main.cpp
)

set(TCC_EXE "tcc-check")

add_executable(${TCC_EXE}
    ${TCC_SOURCES}
)

# RTTI compatibility with LLVM
if (DEFINED LLVM_ENABLE_RTTI AND NOT LLVM_ENABLE_RTTI)
    target_compile_options(${TCC_EXE} PRIVATE -fno-rtti)
endif()

##set(LLVM_LINK_COMPONENTS
##    Option
##    Support
##    AllTargetsAsmParsers
##    AllTargetsDescs
##    AllTargetsInfos
##)

# Different distros expose different targets; incorrect target handling causes multiple ToolingOptions to be loaded leading to core dumps
if (TARGET LLVM)
    target_link_libraries(${TCC_EXE} PRIVATE LLVM)
elseif (TARGET LLVM::LLVM)
    target_link_libraries(${TCC_EXE} PRIVATE LLVM::LLVM)
else()
    target_link_directories(${TCC_EXE} PRIVATE ${LLVM_LIBRARY_DIRS})
    target_link_libraries(${TCC_EXE} PRIVATE ${LLVM_LIBRARIES})
endif()

#clang_target_link_libraries(${TCC_EXE} # Cannot work without AddClang.cmake
target_link_libraries(${TCC_EXE} PRIVATE
    #clangAST
    #clangASTMatchers
    #clangBasic
    #clangFrontend
    #clangSerialization
    #clangTooling
    ${TCC_LIB}
    clang-cpp           # makes other components redundant
    #LLVMSupport
    fmt::fmt
)
# end TCCEXE------------------------------------------------------

# TCCTEST---------------------------------------------------------
message(STATUS "[TCC] Building tests")
add_subdirectory(tests)
