#ifndef HARNESS_H
#define HARNESS_H

#include "common.h"

#include "../src/tcc-census-visitor.cpp"

#include <clang/Tooling/Tooling.h>

#include <stack>
#include <iostream>

using namespace tcc;
using namespace std;

struct TestAction: ASTFrontendAction {


};

inline auto analyze(char const *code) -> TCCCollection {
    TCCCollection results;
    auto ast = tooling::buildASTFromCode(code);
    //auto ast = tooling::buildASTFromCodeWithArgs(code, {"-X", "c"}); // force C parsing with -X c
    if(!ast) {
        throw std::runtime_error("buildASTFromCodeWithArgs failed!");
    }

    Counter stats;
    TCCCensusConsumer collector(&(ast->getASTContext()), stats);
    collector.HandleTranslationUnit(ast->getASTContext());
    //printSourceStats(stats);
    return collector.results();
}

/*
inline auto analyze(char const *source) -> TCCCollection {
    // create consumer or use actionfactory
    struct TestAction: ASTFrontendAction {
        TestAction(TCCCollection &results): results_(results) {}
        ~TestAction() {
            if(observer) {
                results = observer.results();
            }
        }

        auto CreateASTConsumer(CompilerInstance &ci, llvm::StringRef) override {
            Counter stats;
            auto consumer = std::make_unique<TCCConsumer>(ci.getASTContext(), stats);
            observer = consumer.get();
            return consumer;
        }

        TCCCollection &results;
        TCCConsumer *observer = nullptr;
    };

    TCCCollection ccr;
    auto ok = tooling::runToolOnCodeWithArgs(
            make_unique<TestAction>(ccr),
            source,
            {"-x", "c"});   // -x c forces C parsing

    if(!ok) {
        throw std::runtime_error("runToolOnCodeWithArgs: parse failure");
    }

    return ccr;
}
*/

struct TestDB {
    using KeyRef = TCCNode::KeyRef;

    TestDB(TCCCollection &&results):
        census(std::move(get<0>(results)),
                std::move(get<1>(results))),
        nodes_(census.db()),
        histories_(census.hdb()) {}

    TCCStore census;
    TCCNodesDB const &nodes_;
    CastHistories const &histories_;
    std::unordered_map<std::string, CastHistoryInstance> instances_;

    auto hasNode(KeyRef const &key) -> bool {
        return nodes_.contains(key);
    }

    void makeInstances() {
        instantiateHistories(census, instances_);
    }

    auto hasInstance(KeyRef const &key) -> bool {
        return instances_.contains(key);
    }

    auto instanceFinderFor(KeyRef const &root) {
        return [this, root](KeyRef const &key) -> std::optional<CastHistoryInstance> {
            if(!hasInstance(root)) {
                fmt::print(stdout, "[ERROR] No history instance for {}\n", root);
                return {};
            }

            std::stack<CastHistoryInstance> unseen;
            unseen.push(instances_.at(root));
            while(!unseen.empty()) {
                auto top = std::move(unseen.top());
                unseen.pop();

                if(top.id() == key) {
                    return top;
                }

                for(auto const &n: top.nexts()) {
                    unseen.push(n);
                }
            }
            return {};
        };
    }

    /*
    auto history(KeyRef const &key) {
        return [h = histories_[key]](KeyRef const &n) -> bool {
            return h.getConstraintFor(n).has_value();
    }
    */
};

#endif // end HARNESS_H
