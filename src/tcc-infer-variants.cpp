// type inference
//  - walk instance trees filtering union casts
//  - check for union tag field
//  - utilize context to add additional constraints
//
#ifndef TCCINFERVARIANTS_H
#define TCCINFERVARIANTS_H

#include "tcc-historyInstance.cpp"
#include "tcc-tccnode.cpp"

#include <llvm/Support/JSON.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <stack>
#include <string>
#include <iterator>

//export module tcc:infer::variants;

//import :tccnode;

using namespace tcc;
using namespace std::string_literals;

namespace {
    template<std::forward_iterator Iter>
    auto sjoin(Iter start, Iter end, char delim = ',') -> std::string {
        std::ostringstream out;
        auto it = start;
        while(it != end && next(it) != end) {
            out << *it << delim;
            it++;
        }
        out << *it;
        return out.str();
    }

    auto sjoin(std::vector<std::string> const &sv) -> std::string {
        return sjoin(sv.begin(), sv.end());
    }

    auto serialize(QualType qt) -> std::string;

    auto serialize(RecordDecl const *rd) -> std::string {
        std::vector<std::string> s;
        for(auto const *fd: rd->fields()) {
            s.push_back(serialize(fd->getType()));
        }
        return sjoin(s);
    }

    auto serializePointerType(QualType qt) -> std::string {
        auto const *pt = qt->getPointeeOrArrayElementType();
        while(pt && (pt->isPointerType() || pt->isArrayType())) {
            pt = pt->getPointeeOrArrayElementType();
        }

        if(pt) {
            return serialize(pt->getCanonicalTypeInternal());
        }

        TCC_ERROR("bad type", "Skipping: cannot get pointer to pointee type");
        return "";
    }

    auto serialize(QualType qt) -> std::string {
        auto const logKey = qt.getAsString();

        auto tn = qt.getAsString();
        if(qt->isRecordType()) {
            TCC_DEBUG(logKey, "Type is a record type");
            if(auto const *rd = qt->getAsRecordDecl()) {
                return serialize(rd);
            }
            TCC_ERROR(logKey, "Skipping: RecordDecl is nullptr for record type");
            return {};
        }

        if(qt->isPointerType() || qt->isArrayType()) {
            TCC_DEBUG(logKey, "Type is pointer/array");
            return serializePointerType(qt);
        }

        return tn;
    }

    auto definedType(TCCStore const &tdb, TCCNode::KeyRef const &key) -> QualType {
        auto const logKey = key;
        auto const &n = tdb.getNode(key).ast_.node_;
        if(!n) {
            TCC_WARN(logKey, "AST pointers for tcc nodes are invalid");
            return {};
        }

        if(n.template is<clang::ValueDecl const*>()) {
            return n.template get<clang::ValueDecl const*>()->getType();
        }

        else if(auto const *e = dyn_cast<Expr const>(n.template get<clang::Stmt const*>())) {
            return e->getType();
        }

        return {};
    }

} // end anonymous namespace

//export namespace tcc {
namespace tcc {

    auto doesBContainA(TCCStore const &tdb,
            QualType a,
            QualType b)
        -> bool {

        if(a.isNull() || b.isNull()) {
            TCC_ERROR("FirstMemberCheck", "Skipping: cannot check first member if A or B have invalid clang::QualType");
            return false;
        }

        auto const na = a.getAsString();
        auto const nb = b.getAsString();

        auto const logKey = "{" + na + "," + nb + "}?";
        TCC_DEBUG_FN(logKey);

        auto recordA = serialize(a);
        auto recordB = serialize(b);
        TCC_DEBUG(logKey, "record A: {}", recordA);
        TCC_DEBUG(logKey, "record B: {}", recordB);

        /*
        if(recordA.size() > recordB.size()) {
            TCC_DEBUG(logKey, "B does not contain A: A is bigger than B");
            return false;
        }
        */

        return recordB.find(recordA) != std::string::npos;
    }

    // b <: a
    auto isAFirstMemberOfB(TCCStore const &tdb,
            QualType a,
            QualType b) -> bool {

        if(a.isNull() || b.isNull()) {
            TCC_ERROR("FirstMemberCheck", "Skipping: cannot check first member if A or B have invalid clang::QualType");
            return false;
        }

        auto ta = a.getAsString();
        auto tb = b.getAsString();

        auto const logKey = "{" + ta + "," + tb + "}?";
        TCC_DEBUG_FN(logKey);

        // hasAB => b <: a
        // for b <: a, fields of a must be present in b
        TCC_DEBUG(logKey, "Checking if '{}' is first member in '{}'", ta, tb);

        auto recordA = serialize(a);
        auto recordB = serialize(b);
        TCC_DEBUG(logKey, "record A: '{}'", recordA);
        TCC_DEBUG(logKey, "record B: '{}'", recordB);
        TCC_INFO(logKey, "IsFirstMember? {}", recordB.starts_with(recordA));
        return recordB.starts_with(recordA);
    }

    //   - if a |? b; substitute a with <a | b>
    //      - if a |? c && condition.lhs is same for both constraints; substitute <a | b> with <a | [b, c]>
    //      //- if a |? c; substitute <a | b> with <a | [b , c]>
    //      - if b |? c; substitute <a | b> with <a | b | c>
    struct VariantConstraint {
        std::string lhs;
        std::set<std::string> rhs;
        std::vector<std::string> conds;
        std::string tag = "@Unknown";

        auto value() const -> std::string {
            return sjoin(rhs.begin(), rhs.end(), '|');
        }
    };

    auto String(VariantConstraint const &c) -> std::string {
        return "["s + std::to_string(c.conds.size()) + "]<" + c.lhs + "|?" + c.value() + ">";
    }

    template<std::contiguous_iterator Iter>
    void substitute(VariantConstraint const &constraint, Iter start, Iter end) {
        auto const logKey = String(constraint);
        TCC_DEBUG_FN(logKey);

        auto haveSameConditions = [](auto const &c1, auto const &c2) -> std::string {
        //auto haveSameConditions = [](auto const &c1, auto const &c2) -> bool {
            std::unordered_set<std::string> condVars;
            for(auto const &c: c1) {
                if(auto tag = c.substr(0, c.find(" ==")); !tag.empty()) {
                    condVars.insert(tag);
                }
            }
            bool atleast1match = false;
            std::string lastCond;
            for(auto const &c: c2) {
                if(condVars.contains(c.substr(0, c.find(" ==")))) {
                    atleast1match = true;
                    lastCond = c;
                }
            }
            return lastCond; //atleast1match;
        };

        std::for_each(start, end, [&](auto &c) {
            TCC_DEBUG(logKey, "Checking: {}", String(c));

            if(c.lhs == constraint.lhs) {
                if(auto tag = haveSameConditions(constraint.conds, c.conds); !tag.empty()) {
                    for(auto const &r: constraint.rhs) {
                        c.rhs.insert(r);
                    }
                }
            }
        });
    }

    using VariantConstraints = std::vector<VariantConstraint>;
    using Unifier = std::unordered_map<std::string, std::pair<std::string, std::string>>;

    void jsonOut(Unifier const &u, char const * fname) {
        using namespace llvm;
        std::error_code ec;
        llvm::raw_fd_ostream jout(fname, ec, llvm::sys::fs::FA_Write);
        if(ec) {
            TCC_ERROR("json-dump", "Could not open file: {}", ec.message());
            return;
        }

        json::Object root;
        json::Array jvs;
        for(auto const &[base, variants]: u) {
            json::Object jb;;
            json::Object jv;;
            auto const &[tag, vs] = variants;
            jv["tag"] = tag;
            jv["fields"] = vs;
            jb[base] = std::move(jv);
            jvs.push_back(std::move(jb));
        }
        json::Value fv(std::move(jvs));
        jout << fv << "\n";
    }

    auto resolve(VariantConstraints const &constraints) -> Unifier {
        std::list<Unifier::value_type> unifier;

        std::vector<VariantConstraint> remaining(begin(constraints), end(constraints));

        for(auto it = begin(remaining); it != std::end(remaining); ++it) {
            auto const &constraint = *it;
            substitute(constraint, std::next(it), std::end(remaining));
            unifier.emplace_back(constraint.lhs, std::pair(constraint.tag, constraint.value()));
        }

        Unifier u(unifier.size());
        for(auto const &[l, tagr]: unifier) {
            auto const &[tag, r] = tagr;
            u[l].first = tag;
            u[l].second = r;
        }

        return u;
    }

    void addVariantConstraints(TCCStore const &tdb,
            CastHistoryInstance const &instance,
            VariantConstraints &typeVariants,
            bool strictMode = false) {

        constexpr auto hasContextKind = [](CastContext::Kind k) {
            return [k](ExtendedCastContext const &ecc) -> bool {
                for(auto const &c: ecc) {
                    if(c.kind() == k) {
                        return true;
                    }
                }
                return false;
            };
        };

        auto isUnionCast = [&](auto const &ecc) -> bool {
            return hasContextKind(CastContext::Kind::UnionMemberCast)(ecc);
        };
        auto isExplicitCast = [&](auto const &ecc) -> bool {
            return hasContextKind(CastContext::Kind::ExplicitCast)(ecc);
        };
        auto hasConditionContext = [&](auto const &ecc) -> bool {
            return hasContextKind(CastContext::Kind::SwitchCondition)(ecc);
        };

        auto conditions = [](auto const &ecc) {
            std::vector<std::string> conds;
            for(auto const &c: ecc) {
                if(c.kind() == CastContext::Kind::SwitchCondition) {
                    conds.push_back(c.id());
                }
            }
            return conds;
        };

        auto tag = [](auto const &cond) {
            return cond.substr(0, cond.find(" =="));
        };

        auto equalTypes = [](QualType a, QualType b) -> bool {
            if(a.isNull() || b.isNull()) {
                TCC_ERROR("QualTypeCheck", "Skipping: cannot compare Qualtypes as one of A or B have invalid clang::QualType");
                return false;
            }
            return a.getUnqualifiedType() == b.getUnqualifiedType();
        };

        std::stack<CastHistoryInstance> seen;
        seen.push(instance);
        while(!seen.empty()) {
            auto top = std::move(seen.top());
            seen.pop();
            auto ta = definedType(tdb, top.id());
            for(auto &&n: top.nexts()) {
                auto tb = definedType(tdb, n.id());
                bool isTaggedUnion = !equalTypes(ta, tb)
                    && hasConditionContext(n.context())
                    && isUnionCast(n.context());

                if(strictMode) {
                    isTaggedUnion = isTaggedUnion
                        // Unlike first-member subtyping, union defines the bigger but more general type
                        // and the fields are the smaller but more specific type
                        // So we need to reverse the containment check
                        && doesBContainA(tdb, tb, ta)
                        && (serialize(ta) != serialize(tb));
                }

                auto isConditionalAndFirstMember = !equalTypes(ta, tb)
                    && hasConditionContext(n.context())
                    && isExplicitCast(n.context())
                    && isAFirstMemberOfB(tdb, ta, tb);

                if(strictMode) {
                    isConditionalAndFirstMember = isConditionalAndFirstMember
                        && (serialize(ta) != serialize(tb));
                }

                if(isTaggedUnion || isConditionalAndFirstMember) {
                    auto conds = conditions(n.context());
                    auto t = tag(conds[0]);

                    TCC_DEBUG("VariantConstraintCon", "{} |? {} through {}",
                            ta.getAsString(), tb.getAsString(), t);

                    std::set<std::string> rs;
                    rs.insert(tb.getAsString());
                    typeVariants.emplace_back(ta.getAsString(),
                            std::move(rs), conds, t);
                }

                seen.push(std::move(n));
                // TODO
                // Filter cases where condition context has a different base than 'from'
                // Filter literals
            }
        }
    }

    using CastTrees = std::unordered_map<TCCNode::KeyRef, CastHistoryInstance>;

    void inferVariants(TCCStore const &tdb, CastTrees const &histories) {
        // for each instance, if context is
        // - unionmembercast, for a => b, then b must be a union field in struct a
        //   - if condition context is present then expr c must be a tag field in struct a
        // - unionmemberaccess, then ?
        // resolution: ??
        VariantConstraints typeVariants;
        typeVariants.reserve(histories.size()); // Aribtrary. Not every history instance/node in the graph will make a variant constraint.

        std::for_each(cbegin(histories), cend(histories),
            [&](auto const &node) {
                addVariantConstraints(tdb, node.second, typeVariants);
            });

        // given:
        // Shape |? Rectangle, Shape |? Circle, Rectangle |? Square
        // expected:
        // Shape = Rectangle | Circle
        // Rectangle = Square
        auto unifierVariants = resolve(typeVariants);

        auto show = [&tdb](auto const &u, auto const &logKey) {
            if(u.empty()) {
                //TCC_INFO(logKey, "Type inference did not find any variants");
                fmt::print(FLOG, "[{}] Type inference did not find any variants\n", logKey);
                llvm::outs() << "[" << logKey << "] Type inference did not find any variants\n";
                return;
            }

            TCC_INFO(logKey, "Inferred variants:");
            llvm::outs() << "[" << logKey << "] Inferred variants:\n";
            for(auto const &[base, variants]: u) {
                auto const &[tag, vs] = variants;
                //TCC_INFO(logKey, "'name': '{}', 'tag':'{}', 'fields':['{}']", base, tag, vs);
                fmt::print(FLOG, "[{}] 'name': '{}', 'tag':'{}', 'fields':['{}']\n", logKey, base, tag, vs);
                llvm::outs() << "[" << logKey << "] name:'" << base
                    << "'; tag:'" << tag
                    << "'; fields:'" << vs << "'\n";
            }
        };

        TCC_DEBUG("printInference", "Variants:");
        show(unifierVariants, "VariantInference");
        jsonOut(unifierVariants, "tcc-variants.json");
        TCC_DEBUG("printInference", "end Variants:");

        VariantConstraints strictTypeVariants;
        strictTypeVariants.reserve(histories.size());
        std::for_each(cbegin(histories), cend(histories),
            [&](auto const &node) {
                addVariantConstraints(tdb, node.second, strictTypeVariants, true);
            });
        auto unifierStrictVariants = resolve(strictTypeVariants);

        TCC_DEBUG("printInference", "(Strict-mode) Variants:");
        show(unifierStrictVariants, "Strict-VariantInference");
        jsonOut(unifierStrictVariants, "tcc-variants-strict.json");
        TCC_DEBUG("printInference", "end (Strict-mode) Variants:");
    }

} // end namespace tcc

#endif // end TCCINFERVARIANTS

