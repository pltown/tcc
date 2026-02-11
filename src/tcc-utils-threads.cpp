#ifndef TCCUTILSTHREADS_H
#define TCCUTILSTHREADS_H

#include <llvm/Config/llvm-config.h>
#include <llvm/Support/ThreadPool.h>

namespace tcc {

    #if LLVM_VERSION_MAJOR > 18
    using TCCThreadPool = llvm::DefaultThreadPool;
    #else
    using TCCThreadPool = llvm::ThreadPool;
    #endif

} // end namespace tcc

#endif // end TCCUTILSTHREADS_H
