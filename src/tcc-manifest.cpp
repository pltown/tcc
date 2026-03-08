#ifndef TCC_MANIFEST_H
#define TCC_MANIFEST_H

//module;

#include "tcc-castContext.cpp"

#include <clang/AST/ASTContext.h>
#include <clang/AST/AST.h>
#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>

#include <llvm/ADT/DenseSet.h>
#include <llvm/ADT/PointerUnion.h>

#include <unordered_map>
#include <stack>
#include <typeindex>
#include <string_view>
#include <string>
#include <algorithm>
#include <concepts>

using namespace clang;

//export module tcc:manifest;

//export namespace tcc {
namespace tcc {
    template<typename T>
    concept Trackable =
    !std::is_reference_v<T>
    && (std::is_pointer_v<T>
            || std::is_base_of_v<clang::Stmt, T>
            || std::is_base_of_v<clang::Decl, T>);

    class Counter {
    public:
        template<Trackable T>
        auto value() const -> std::size_t {
            if(auto it = types_.find(make_CounterKey<T>()); it != std::end(types_)) {
                return it->second.count;
            }
            return 0;
        }

        auto value(std::string_view key) const -> std::size_t {
            if(auto it = namedTypes_.find(std::string(key)); it != std::end(namedTypes_)) {
                return it->second.count;
            }
            return 0;
        }

        template<Trackable T>
        void bump() {
            ++types_[make_CounterKey<T>()];
        }

        void bump(std::string_view key) {
            ++namedTypes_[std::string(key)];
        }

        template<Trackable T>
        auto track(std::string label = "") -> Counter& {
            auto [it, inserted] = types_.emplace(make_CounterKey<T>(), CountedType{});
            if(inserted && !label.empty()) {
                it->second.label = std::move(label);
            }
            return *this;
        }

        auto track(std::string key, std::string label = "") -> Counter& {
            auto [it, inserted] = namedTypes_.emplace(std::move(key), CountedType());
            if(inserted && !label.empty()) {
                it->second.label = std::move(label);
            }
            return *this;
        }

    private:
        struct CountedType {
            //std::type_index id_;
            std::size_t count;
            std::string label; // maybe move to map value

            auto operator++() noexcept -> CountedType & {
                ++count;
                return *this;
            }

            auto operator++(int) noexcept -> CountedType {
                auto tmp = *this;
                ++count;
                return tmp;
            }
        };
        /*
        struct CounterHash {
            auto operator()(CounterKey const &tc) const noexcept -> std::size_t {
                return std::hash<std::type_index>{}(tc.id_);
            }
        };

        template<typename T>
        static auto make_CounterKey() -> CounterKey {
            return CounterKey{std::type_index(typeid(T)), typeid(T).name()};
        }
        */
        template<Trackable T>
        static auto make_CounterKey() -> std::type_index {
            return typeid(T);
        }

        std::unordered_map<std::type_index, CountedType> types_;
        std::unordered_map<std::string, CountedType> namedTypes_;
        //std::unordered_map<CounterKey, std::size_t, CounterHash> namedTypes_;

        unsigned count_ = 0;
    };

    class TCCManifest {
    public:
        //using Contexts = TypeProvenanceConstraint::Contexts;
        using Contexts = std::vector<tcc::CastContext>;

        auto context() const -> ASTContext & {
            return context_;
        }
        auto context() -> ASTContext & {
            return context_;
        }

        auto isSeen(llvm::PointerUnion<Decl const*, Stmt const*> node) -> bool {
            return seenNodes_.contains(node);
        }

        void markSeen(llvm::PointerUnion<Decl const*, Stmt const*> node) {
            seenNodes_.insert(node);
        }

        auto conditionContext() -> Contexts {
            TCC_DEBUG("manifestCond", "Manifest contexts: {}", conditions_.size());
            Contexts ccs;
            auto tovec = conditions_;
            while(!tovec.empty()) {
                ccs.push_back(tovec.top());
                tovec.pop();
            }
            return ccs;
        }

        void popConditionContext() {
            if(!conditions_.empty()) {
                TCC_DEBUG("manifestCond", "[{}] Popping top: {}", conditions_.size(),
                    String(conditions_.top()));
                conditions_.pop();
            }
        }

        void pushConditionContext(CastContext const &cc) {
            TCC_DEBUG("manifestCond", "[{}] Pushing: {}", conditions_.size(),
                    String(cc));
            conditions_.push(cc);
        }

        /*
        auto conditionContext() -> Contexts {
            Contexts ccs;
            //Contexts::const_iterator top = topConditionContext_;
            if(topConditionContext_ != conditionContext_.begin()) {
                std::copy(begin(conditionContext_), topConditionContext_, std::back_inserter(ccs));
            }
            return ccs;
            //return conditionContext_;
        }

        void pushConditionContext(CastContext const &cc) {
            if(topConditionContext_ == conditionContext_.end()) {
                conditionContext_.push_back(cc);
                topConditionContext_ = conditionContext_.end();
            }
            else {
                *topConditionContext_ = cc;
                //topConditionContext_ = conditionContext_.insert(std::next(topConditionContext_), cc);
                ++topConditionContext_;
            }
        }

        void popConditionContext() {
            if(topConditionContext_ != conditionContext_.begin()) {
                --topConditionContext_;
            }
        }
        */

        TCCManifest(ASTContext *context, Counter &stats):
            context_(*context),
            SourceCounter(stats) {}

    private:
        ASTContext &context_;
        //Contexts conditionContext_;
        std::stack<CastContext> conditions_;
        Contexts::iterator topConditionContext_;
        llvm::DenseSet<Expr *> seenExprs_;
        llvm::DenseSet<Decl *> seenDecls_;
        llvm::DenseSet<llvm::PointerUnion<Decl const*, Stmt const*>> seenNodes_;

    public:
        //std::optional<FunctionDecl const*> function;
        Counter &SourceCounter;
        //std::string Condition;
    };
}

#endif // TCC_MANIFEST_H
