#ifndef LOGGER_H
#define LOGGER_H


// LLVM debug/trace does not support multi-file/multi-threaded logging
// Debug logs:
//  - DEBUG(dbgs() << "message");
//  - DEBUG_WITH_TYPE("my-pass", dbgs() << "message");
//
//  Enable debug output:
//  $ clang -mllvm -debug my_source.c
//  $ clang -mllvm -debug-ony=my_cfg my_source.c
//
//  https://groups.google.com/g/llvm-dev/c/HU6h2JNuz_Q
//  https://llvm.org/docs/ProgrammersManual.html#the-debug-macro-and-debug-option
//
// Advanced tracing:
//  - build with xray:
//  $ clang -fxray-instrument mysource.c -o my_exe
//
//  - run with tracing, select logging mode:
//  $ XRAY_OPTIONS="xray_mode=xray-basic" ./my_exe
//
//  - analyze traces
//  $ llvm-xray convert --chrome-trace xray-log.my_exe.* > trace.json
//
//  https://releases.llvm.org/8.0.1/docs/XRay.html
//

#include <fmt/core.h>
#include <fmt/format.h>

#include <iostream>
#include <chrono>
#include <filesystem>
#include <unistd.h>
#include <fstream>
#include <cstring>
#include <cstdio>

//https://www.cs.cmu.edu/afs/cs/academic/class/15745-s15/public/lectures/L6-LLVM2-1up.pdf
//https://llvm.org/docs/ProgrammersManual.html#the-llvm-debug-macro-and-debug-option
// File io: https://llvm.org/doxygen/classllvm_1_1raw__fd__ostream.html
FILE *FLOG = nullptr;
unsigned SEVERITY_FILTER = 1 << 4;

namespace tcc {
    namespace logging {
        enum severity: unsigned {
            Debug = 1 << 0,
            Info = 1 << 1,
            Warn = 1 << 2,
            Error = 1 << 3
        };
    }
}

// replace fmt with llvm::formatv()
template<>
struct fmt::formatter<tcc::logging::severity>: formatter<string_view> {
    format_context::iterator format(tcc::logging::severity s, format_context &ctx) const {
        string_view ret = "Unknown";
        switch(s) {
            case tcc::logging::severity::Debug:
                ret = "DEBUG"; break;
            case tcc::logging::severity::Info:
                ret = "INFO"; break;
            case tcc::logging::severity::Warn:
                ret = "WARN"; break;
            case tcc::logging::severity::Error:
                ret = "ERROR"; break;
        }
        return formatter<string_view>::format(ret, ctx);
    }
};

void log_(tcc::logging::severity severity,
        char const *filename,
        char const *function,
        int line,
        std::string_view key,
        std::string_view msg) {
    static unsigned counter = 0;
    if(counter == 10) {
        if(fflush(FLOG) != 0) {
            std::cerr << "Error flushing tcc log buffer\n";
        }

        auto fd = fileno(FLOG);
        if(fd == -1) {
            std::cerr << "Error getting file descriptor for tcc log\n";
        }
        else if(fdatasync(fd) == -1) {
        //else if(fsync(fd) == -1) {
            std::cerr << "Error syncing file to disk\n";
        }
        counter = 0;
    }

    if(SEVERITY_FILTER & severity) {
        counter++;
        fmt::print(FLOG, "[{:<5}] {}:{}():{}: {} | {}\n", severity,
                std::filesystem::path(filename).filename().c_str(),
                function, line, key, msg);
    }
}

void log(tcc::logging::severity severity,
        char const *filename, char const *function, int line,
        std::string_view key,
        std::string_view msg) {
    log_(severity, filename, function, line, key, msg);
}

template<typename... Args>
void log(tcc::logging::severity severity,
        char const *filename, char const *function, int line,
        std::string_view key,
        std::string const &msg, Args&&... args) {
    log_(severity, filename, function, line, key,
        fmt::vformat(msg, fmt::make_format_args(args...)));
}

class FunctionLogger {
public:
    FunctionLogger(tcc::logging::severity severity,
            char const *file,
            char const *function,
            int line,
            std::string_view key):
        severity_(severity),
        file_(file),
        fn_(function),
        line_(line),
        key_(key) {

        log(severity_, file_.c_str(), fn_.c_str(), line_, key_, "begin");
    }

    ~FunctionLogger() {
        log(severity_, file_.c_str(), fn_.c_str(), line_, key_, "end");
    }

private:
    tcc::logging::severity severity_;
    std::string file_;
    std::string fn_;
    int line_;
    std::string_view key_;
};

#define TCC_LOG_LEVEL_DEBUG {}
#define TCC_LOG_LEVEL_INFO {}
#define TCC_LOG_LEVEL_WARN {}
#define TCC_LOG_LEVEL_ERROR {}

#define TCC_LOG(severity, key, fmt, ...) \
    log(severity, __FILE__, __FUNCTION__, __LINE__, key, fmt __VA_OPT__(, )__VA_ARGS__)

#define TCC_LOG_FN(severity, key) \
    FunctionLogger logFunction##__FUNCTION__(severity, __FILE__, __FUNCTION__, __LINE__, key)

#ifdef TCC_LOG_LEVEL_DEBUG
#define TCC_DEBUG(key, fmt, ...) \
    TCC_LOG(tcc::logging::severity::Debug, key, fmt __VA_OPT__(,) __VA_ARGS__)

#define TCC_DEBUG_FN(key) \
    TCC_LOG_FN(tcc::logging::severity::Debug, key)
#endif

#ifdef TCC_LOG_LEVEL_INFO
#define TCC_INFO(key, fmt, ...) \
    TCC_LOG(tcc::logging::severity::Info, key, fmt __VA_OPT__(,) __VA_ARGS__)

#define TCC_INFO_FN(key) \
    TCC_LOG_FN(tcc::logging::severity::Info, key)
#endif

#ifdef TCC_LOG_LEVEL_WARN
#define TCC_WARN(key, fmt, ...) \
    TCC_LOG(tcc::logging::severity::Warn, key, fmt __VA_OPT__(,) __VA_ARGS__)

#define TCC_WARN_FN(key) \
    TCC_LOG_FN(tcc::logging::severity::Warn, key)
#endif

#ifdef TCC_LOG_LEVEL_ERROR
#define TCC_ERROR(key, fmt, ...) \
    TCC_LOG(tcc::logging::severity::Error, key, fmt __VA_OPT__(,) __VA_ARGS__)

#define TCC_ERROR_FN(key) \
    TCC_LOG_FN(tcc::logging::severity::Error, key)
#endif

#endif // end LOGGER
