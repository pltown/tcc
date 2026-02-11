#!/bin/bash

# Ensure clang 18+
CLANG_REQUIRED=18
CLANG_VERSION_STR=$(clang --version | grep version)
CLANG_VERSION=$(echo "${CLANG_VERSION_STR}" | grep -oP 'version \K\d+' | head -n 1)

if [ -z "${CLANG_VERSION}" ]; then
    echo "Cannot determine clang version from: ${CLANG_VERSION_STR}"
    exit 1
fi

if [ "${CLANG_VERSION}" -lt "${CLANG_REQUIRED}" ]; then
    echo "clang major version (${CLANG_VERSION}) is insufficient (required: >=${CLANG_REQUIRED}"
    exit 1
fi

CMAKE_LLVM_DIR="/usr/lib/llvm-${CLANG_VERSION}"
#CMAKE_LLVM_DIR="/usr/bin/llvm-${CLANG_VERSION}/lib/cmake/llvm"
CMAKE_CLANG_DIR="/usr/lib/cmake/clang-${CLANG_VERSION}"
#CMAKE_CLANG_DIR="/usr/bin/llvm-${CLANG_VERSION}/lib/cmake/clang"

echo "Found clang version ${CLANG_VERSION}"
echo "Setting llvm dir: ${CMAKE_LLVM_DIR}"
echo "Setting clang dir: ${CMAKE_CLANG_DIR}"

# Avoid conda conflicts
CMAKE_IGNORED_PATHS="/usr/local/bin/miniconda3"

echo "Adding to cmake ignore paths: ${CMAKE_IGNORED_PATHS}"

cmake -S . -B build -G Ninja \
    -DLLVM_DIR=${CMAKE_LLVM_DIR} \
    -DClang_DIR=${CMAKE_CLANG_DIR} \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DLLVM_TARGETS_TO_BUILD=X86 \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_IGNORE_PATH="${CMAKE_IGNORED_PATHS}"
    #-DLLVM_USE_LINKER=lld \
    #-DLLVM_PARALLEL_LINK_JOBS=2 \

# build
ninja -C build

# or if using meson (experimental)

# meson setup build --native-file meson.ini
# meson compile -C build

