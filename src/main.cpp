// accept inputs
// -> run matchers
// -> process metadata gathered
//   --> add annotations if needed

//import tcc;
#include "tcc-census-visitor.cpp"
#include "tcc-utils-threads.cpp"
#include "logger.h"

#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>

#include <llvm/Support/CommandLine.h>

#include <future>
#include <fstream>
#include <stack>
#include <mutex>

using namespace clang::tooling;
using namespace llvm::cl;
using namespace llvm;
using namespace tcc;
using llvm::errs;

static llvm::cl::OptionCategory tccCategory("tcc run options");

//unsigned SUMMARY_DEPTH = 0;
//static cl::opt<int> optSummaryDepth(
//        "summary-depth",
//        cl::desc("Control depth of summary tree in output"),
//        cl::init(4), cl::cat(tccCategory));

static cl::opt<unsigned> optVerbosity(
        "v",
        cl::desc("Control output log level: 0(None), 1(Errors), 2(Warnings), 3(Info), 4(Debug)"),
        cl::init(0), cl::cat(tccCategory));

static cl::opt<bool> optIgnoreCompileDB(
        "no-db",
        cl::desc("Ignore compile db and use input c filenames"),
        cl::init(0), cl::cat(tccCategory));

static cl::opt<bool> optJobs(
        "jobs",
        cl::desc("Maximum number of threads to use; if not specified, system-supported max is used"),
        cl::init(0), cl::cat(tccCategory));

static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

static cl::extrahelp Morehelp("\nREDO More help text...\n");

///
void setLogLevel(unsigned verbosity) {
    switch(verbosity) {
        case 0: // None
            SEVERITY_FILTER = 0; break;
        case 1: // Errors
            SEVERITY_FILTER = 8; break;
        case 2: // Warnings
            SEVERITY_FILTER = 12; break;
        case 3: // Info
            SEVERITY_FILTER = 14; break;
        case 4: // Debug
            SEVERITY_FILTER = 15; break;
        default: // Errors
            SEVERITY_FILTER = 8; break;
    }
}

auto filterC(std::vector<std::string> input) -> std::vector<std::string> {
    std::vector<std::string> cSources;
    for(auto const &f: input) {
        SmallString<255> absPath;
        if(f.size() < 2)
            continue;
        if(f.substr(f.size() - 2) == ".c") {
            if(!(llvm::sys::fs::real_path(f, absPath))) {
                cSources.push_back(f);
            }
        }
    }

    return cSources;
}

auto run(unsigned jobs, std::vector<std::string> sources,
       CompilationDatabase const &cdb)
    -> int;

///

auto main(int argc, const char **argv) -> int {
    FLOG = fopen("tcc-dump.txt", "w");
    if(FLOG == nullptr) {
        fmt::print(stderr, "Exiting: cannot open tcc log file: tcc-dump.txt\n");
        return 1;
    }

    auto expectedParser = CommonOptionsParser::create(argc, argv, tccCategory);
    if(!expectedParser) {
        errs() << expectedParser.takeError();
        return 1;
    }

    auto &args = expectedParser.get();

    //SUMMARY_DEPTH = optSummaryDepth;
    setLogLevel(optVerbosity);
    auto cfiles = filterC((optIgnoreCompileDB)
            ? (args.getSourcePathList())
            : (args.getCompilations().getAllFiles()));
    if(cfiles.empty()) {
        fmt::print("Exiting: No C files to process!");
    }

    auto nbJobs = (optJobs) ? (optJobs) : (std::thread::hardware_concurrency());

    auto rc = run(nbJobs, cfiles, args.getCompilations());

    fclose(FLOG);

    return rc;
    //return 0;
}

//using TCCContext = TCCMatchData;

void printHistories(TCCStore const &tdb,
        std::unordered_map<std::string, CastHistoryInstance> const &instances) {
    for(auto const &[node, hi]: instances) {
        fmt::print(FLOG, "History for {}:\n", node);
        fmt::print(FLOG, "{}\n", String(tdb, hi));
        fmt::print(FLOG, "end History for {}:\n\n", node);
        llvm::outs() << "History for " << node << ":\n";
        llvm::outs() << String(tdb, hi) << "\n";
    }
}

void printSourceStats(tcc::Counter const &stats) {
    constexpr auto logKey = "source-stats";
    //fmt::print(FLOG, "[{}] Function Decls: {}\n", logKey, stats.value<FunctionDecl>());
    fmt::print(FLOG, "[{}] Var Decls: {}\n", logKey, stats.value<VarDecl>());
    fmt::print(FLOG, "[{}] Exprs: {}\n", logKey, stats.value<Expr>());
    fmt::print(FLOG, "[{}] BinaryOperators: {}\n", logKey, stats.value<BinaryOperator>());
    fmt::print(FLOG, "[{}] BinOp Assignments: {}\n", logKey, stats.value("BinaryOperator-Assignment"));
    fmt::print(FLOG, "[{}] BinOp Arithmetic: {}\n", logKey, stats.value("BinaryOperator-Arithmetic"));
    fmt::print(FLOG, "[{}] Call Exprs: {}\n", logKey, stats.value<CallExpr>());
    fmt::print(FLOG, "[{}] CallExprs via Fptrs: {}\n", logKey, stats.value("CallExpr-Fptr"));
    fmt::print(FLOG, "[{}] Casts: {}\n", logKey, stats.value<CastExpr>());
    fmt::print(FLOG, "[{}] Member Accesses: {}\n", logKey, stats.value<MemberExpr>());
    fmt::print(FLOG, "[{}] UnaryOperators: {}\n", logKey, stats.value<UnaryOperator>());
    fmt::print(FLOG, "[{}] Unary AddressOfs: {}\n", logKey, stats.value("UnaryOperator-AddressOf"));
    fmt::print(FLOG, "[{}] Unary Derefs: {}\n", logKey, stats.value("UnaryOperator-Deref"));
}

auto run(unsigned jobs, std::vector<std::string> sources,
       CompilationDatabase const &cdb) -> int {

    TCC_DEBUG_FN("run");
    TCCThreadPool pool({jobs});

    std::vector<std::unique_ptr<ASTUnit>> asts;
    ClangTool tool(cdb, sources);
    // build ast for each file
    if(tool.buildASTs(asts) != 0) {
        errs() << "Error in building AST for specified sources\n";
        return 1;
    }

    std::vector<std::shared_future<TCCCollection>> futures;

    // launch matchAST inside thread for each ast
    unsigned tid = 0;
    for(auto const &ast: asts) {
        auto file = std::string_view(ast->getOriginalSourceFileName());
        fmt::print(stdout, "[{}] Creating thread for {}\n", tid, file);

        auto f = pool.async([&, file, tid]() -> TCCCollection {
            fmt::print(stdout, "[{}] Starting thread for {}\n", tid, file);

            Counter stats;
            TCCCensusConsumer collector(&(ast->getASTContext()), stats);
            collector.HandleTranslationUnit(ast->getASTContext());
            printSourceStats(stats);

            return collector.results();
        });

        fmt::print(stdout, "[{}] Enqueing future (current total = {})\n", tid, futures.size());
        futures.push_back(std::move(f));
        tid++;
    }

    fmt::print(stdout, "Waiting on futures (total = {})\n", futures.size());
    std::vector<TCCCollection> mds;
    for(auto &f: futures) {
        if(!f.valid()) {
            fmt::print(stdout, "Bad future!\n");
            continue;
        }

        // append tccnodes and history(ies) to global store
        mds.push_back(f.get());
    }
    pool.wait();

    fmt::print(stdout, "All threads complete!\n");

    if(mds.empty()) {
        fmt::print(stdout, "Empty census!\n");
        return 0;
    }

    auto census = mds.size() > 1
        ? (std::accumulate(next(begin(mds), 1), end(mds), mds[0], gather))
        : (mds[0]);

    fmt::print(stdout, "Finished future processing\n");

    // Outro
    using std::get;
    TCCStore tdb(std::move(get<0>(census)), std::move(get<1>(census)));
    fmt::print(stdout, "Status:\n");
    fmt::print(stdout, "TCCNodes: {}\n", tdb.db().size());
    fmt::print(stdout, "Histories: {}\n", tdb.hdb().size());

    TCC_DEBUG("run", "Status:");
    TCC_DEBUG("run", "TCCNodes: {}", tdb.db().size());
    TCC_DEBUG("run", "Histories: {}", tdb.hdb().size());
    TCC_DEBUG("run", "Beginning history instantiation");
    auto constexpr logKey = "instantiation";
    std::unordered_map<std::string, CastHistoryInstance> historyInstances;

    instantiateHistories(tdb, historyInstances);

    TCC_DEBUG("run", "Completed history instantiation");
    fmt::print(stdout, "Completed history instantiation\n");

    TCC_DEBUG("run", "Printing instantiated histories");
    fmt::print(stdout, "Printing instantiated histories\n");

    printHistories(tdb, historyInstances);

    fmt::print(stdout, "Finished run\n");

    return 0;
}

