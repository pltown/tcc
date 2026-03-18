# tcc

## Requirements

 - libzstd-dev
 - libfmt-dev (>=11.2)

(v18.0.0 or higher):
 - clang
 - libclang-dev
 - llvm-dev
 - libclang-common-dev

## Build

Use the included `setup-and-build.sh` to setup CMake and build directory.

```sh
$ CC=clang CXX=clang++ ./setup-and-build.sh
```

Ensure clang version is 18.0.0 or higher.
On Ubuntu 22.04 this will require installation of clang-18 and related packages.
CMake find package can mismatch clang and llvm version (building with llvm-18 but clang-14).
The included `setup-and-build.sh` avoids this problem by specifying both llvm and clang paths in CMake setup

Meson setup is experimental and not working well currently.

## Running
Ensure that the project builds successfully before running the analyzer.

Sample invocation (single source, no compile_commands.json):
```sh
$ bin/tcc-check -v 0 --summary-depth 10 --no-db <source>.cpp
```

For multi-file project, first generate compile_commands.json:
```sh
$ bin/tcc-check -v 0 --summary-depth 10 compile_commands.json [--jobs 1]
```

