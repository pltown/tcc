project( 'tcc', ['c', 'cpp'],
  version : '0.1.0',
  default_options : [
    'cpp_std=c++20',
    'warning_level=2',
    'buildtype=debug',
  ],
)

# --- Enforce Clang >= 18 (toolchain correctness) ---
cpp = meson.get_compiler('cpp')
if cpp.get_id() != 'clang'
  error('This project must be built with Clang (clang++-18). Found: ' + cpp.get_id())
endif
if cpp.version().version_compare('<18')
  error('Need clang++ >= 18, found clang++ ' + cpp.version())
endif

# Dependencies
dep_llvm = dependency('llvm')
if not dep_llvm.found()
  dep_llvm = dependency('llvm-18', required : true)
endif

dep_clang_cpp = dependency('libclang-cpp')
if not dep_clang_cpp.found()
  dep_clang_cpp = dependency('clang-cpp')
endif
if dep_clang_cpp.found()
  dep_clang_cpp = dependency('libclang-cpp-18')
endif
if dep_clang_cpp.found()
  dep_clang_cpp = dependency('clang-cpp-18', required : true)
endif

dep_libs = [
  dep_llvm,
  dep_clang_cpp,
  dependency('threads'),
  dependency('fmt', required: true)
  ]

# llvm_config/find_library is lower level and more prone to breaking
## --- Find llvm-config-18 (authoritative flags/paths on Ubuntu) ---
#llvm_config = find_program('llvm-config-18', required : true)
#
#llvm_cxxflags_raw = run_command(llvm_config, '--cxxflags', check : true).stdout().strip()
#llvm_ldflags_raw  = run_command(llvm_config, '--ldflags',  check : true).stdout().strip()
#llvm_libdir       = run_command(llvm_config, '--libdir',   check : true).stdout().strip()
#
## llvm-config may include -std=...; we must not let it override Meson's c++20.
#llvm_cxxflags = []
#foreach f : llvm_cxxflags_raw.split()
#  if not f.startswith('-std=')
#    llvm_cxxflags += f
#  endif
#endforeach
## --- Libraries: clang-cpp + LLVM shared (prevents "DSO missing from command line") ---
## Prefer LLVM 18 libdir returned by llvm-config-18.
#clang_cpp_lib = cpp.find_library('clang-cpp', dirs : [llvm_libdir], required : true)
#
## Ubuntu ships libLLVM-18.so (name is usually LLVM-18 for find_library)
#llvm_shared_lib = cpp.find_library('LLVM-18', dirs : [llvm_libdir], required : true)
#
## Other deps
#threads = dependency('threads')
#fmt = dependency('fmt', required : true)
#exe = executable(
#  exe_name,
#  sources,
#  cpp_args : llvm_cxxflags,
#  link_args : llvm_ldflags_raw.split(),
#  dependencies : [threads, fmt],
#  link_with : [clang_cpp_lib, llvm_shared_lib],
#  #install : true,
#)


# RTTI compatibility (mirror LLVM build)
has_rtti = run_command(llvm_config, '--has-rtti', check : true).stdout().strip()
if has_rtti == 'NO'
  llvm_cxxflags += '-fno-rtti'
endif

#lib_inc = include_directories('include')
exe_name = 'tcc'
sources = files('src/main.cpp')
exe = executable(
  exe_name,
  sources,
  #include_directories : lib_inc,
  dependencies : dep_libs,
  #install : true,
)

# export dependency from this lib
#tcc_lib_dep = declare_dependency(
#  include_directories : lib_inc,
#  link_with : tcc_lib, # name of lib created with static_library() instead of executable()
#  dependencies : dep_libs,
#  )
