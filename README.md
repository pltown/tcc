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

## Tests
Unit tests are located in `/tests`.
The current CMake always builds the test suite.
To launch tests
```sh
$ bin/tcc-tests
```

## Running
Ensure that the project builds successfully before running the analyzer.

Sample invocation (single source, no compile_commands.json):
```sh
$ bin/tcc-check -v 0 --no-db <source>.cpp
```

For multi-file project, first generate compile_commands.json:
```sh
$ bin/tcc-check -v 0 compile_commands.json [--jobs 1]
```
### Enum type identification
After a successful run, if any enums are identified they will be dumped in `tcc-variants.json` and `tcc-variants-strict.json`.
The main difference between the two is that strict type checks for first-member idiom (v/s containment) if the types involved do not contain union types.

The json files contain an array of the found enums with enum name as key, followed by tag and fields.
e.g.
```c
enum ShapeType {
    Rectangle,
    Circle
};

struct Shape {
    enum ShapeType tag;
    union {
        Rectangle,
        Circle
    };
};
// Some switch that uses Shape *s; s->type;
//...
```
could produce following json (exact contents depend on how the use was identified):
```json
[{
    "struct Shape *": {"fields": "Rectangle|Circle", "tag": "s->type"}
}]
```

