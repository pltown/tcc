#ifndef TCCCASTS_H
#define TCCCASTS_H

//module;

#include "tcc-castContext.cpp"
#include "tcc-tccnode.cpp"

#include "logger.h"

#include <llvm/ADT/SmallSet.h>
#include <llvm/ADT/StringMap.h>

#include <unordered_map>
#include <string>
#include <numeric>

//export module tcc:casts;

//import :tccnode;

//export namespace tcc {
namespace tcc {
    class TypeProvenanceConstraint {
    public:
        using KeyRef = TCCNode::KeyRef;
        using ConstrainedNodes = std::pair<KeyRef, KeyRef>;
        using Contexts = std::vector<CastContext>;

        auto dom() const -> KeyRef {
            return nodes_.first;
        }

        auto child() const -> KeyRef {
            return nodes_.second;
        }

        auto context() const -> Contexts {
            return contexts_;
        }

        explicit TypeProvenanceConstraint(ConstrainedNodes n,
                std::optional<Contexts> context = {}):
            nodes_(n),
            contexts_(context.value_or(Contexts())) {}

        friend auto operator==(TypeProvenanceConstraint const &a,
                TypeProvenanceConstraint const &b)
            -> bool {
            return a.nodes_ == b.nodes_
                && a.contexts_.size() == b.contexts_.size()
                && a.contexts_ == b.contexts_;
        }

    private:
        ConstrainedNodes nodes_;
        Contexts contexts_;
    };

    auto String(TypeProvenanceConstraint const &dc) -> std::string {
        return dc.dom()  + " 🡒 " + dc.child(); //🡒 ->  //🡺,🡒,➔,➝,➞,⟶,⇾,=>,➾
    }

    // Cast history is a collection of dominator constraints
    class CastHistory {
    public:
        using KeyRef = TCCNode::KeyRef;
        using TypeProvenanceConstraints = llvm::SmallVector<TypeProvenanceConstraint>;

        explicit CastHistory(KeyRef id,
                std::optional<TypeProvenanceConstraints> constraints = {}):
            id_(id),
            constraints_(constraints.value_or(TypeProvenanceConstraints())) {}

        // delete operator=
        //
        auto id() const -> KeyRef {
            return id_;
        }

        // prevent duplication how? with instantiation
        void append(TypeProvenanceConstraints const &constraints) {
           constraints_.append(constraints);
        }

        void append(TypeProvenanceConstraint const &constraint) {
            constraints_.push_back(constraint);
        }

        auto constraints() const -> TypeProvenanceConstraints const & {
            return constraints_;
        }

        auto getConstraintFor(KeyRef child) const -> std::optional<TypeProvenanceConstraint> {
            for(auto const &c: constraints_) {
                if(c.child() == child) {
                    return c;
                }
            }
            return {};
        }

        //points-to, may-pointsto

        friend auto operator==(CastHistory const &h1, CastHistory const &h2) -> bool {
            return h1.id_ == h2.id_;
        }

        friend auto operator<=>(CastHistory const &h1, CastHistory const &h2) {
            return h1.id_ <=> h2.id_;
        }

    private:
        KeyRef id_;
        TypeProvenanceConstraints constraints_;
    };

} // end namespace tcc

#endif // end TCCCASTS
