#ifndef TCCCASTCONTEXT_H
#define TCCCASTCONTEXT_H

//module;

#include "tcc-tccnode.cpp"

#include "logger.h"

#include <llvm/ADT/SmallSet.h>
#include <llvm/ADT/StringMap.h>

#include <unordered_map>
#include <string>
#include <numeric>

//export module tcc:castContext;

//import :tccnode;

//export namespace tcc {
namespace tcc {
    class CastContext {
    public:
        enum Kind {
            #define CC_KIND(name) name,
            #include "tcc-cast-context.def"
            #undef CC_KIND
        };

        using KeyRef = TCCNode::KeyRef;
        using ContextMap = std::unordered_map<KeyRef, KeyRef>; //llvm::StringMap<KeyRef>;
        using MappedNode = ContextMap::value_type;

    public:
        CastContext() = default;
        CastContext(Kind k,
                std::string id,
                std::string container):
            kind_(k),
            id_(id),
            containerFn_(container){}

        auto kind() const -> Kind {
            return kind_;
        }

        // TODO Why?
        void setKind(Kind k) {
            kind_ = k;
        }


        auto id() const -> KeyRef {
            return id_;
        }

        auto scope() const -> std::string {
            return containerFn_;
        }

        auto size() const -> std::size_t {
            return mappedNodes_.size();
        }

        auto empty() const -> bool {
            return size() == 0;
        }

        //auto insert(llvm::StringRef key, KeyRef val) -> bool {
        auto insert(KeyRef key, KeyRef val) -> bool {
            return mappedNodes_.emplace(key,val).second;
        }

        auto data() const -> ContextMap const& {
            return mappedNodes_;
        }

        friend auto operator==(CastContext const &a,
                CastContext const &b)
            -> bool {
            return a.id_ == b.id_
                && a.kind_ == b.kind_
                && a.containerFn_ == b.containerFn_
                && a.mappedNodes_ == b.mappedNodes_;
        }
        // member access operator

    private:
        Kind kind_  = Kind::Unknown;
        // id (subscript) could be dom->child pair or name of function or type of operation
        // should include location (for scope)
        std::string id_ = "Default (probably invalid) context";
        // function where the context is applicable (superscript)
        std::string containerFn_;
        ContextMap mappedNodes_;
    };

    auto String(CastContext::Kind k) -> std::string {
        switch(k) {
            #define CC_KIND(name) case CastContext::name: return #name;
            #include "tcc-cast-context.def"
            #undef CC_KIND
        }
    }

    auto String(CastContext const &cc) -> std::string {
        auto tag = cc.id() + "@" + cc.scope();
        return String(cc.kind())
            + "<" + tag + ">"
            + "[" + std::to_string(cc.data().size()) + "]"
            + "<" + tag + "/>";
    }
}

#endif // end TCCCASTCONTEXT
