#ifndef TCCHISTORYINSTANCE_H
#define TCCHISTORYINSTANCE_H

//module;

#include "tcc-casts.cpp"
#include "tcc-tccnode.cpp"

#include "logger.h"

#include <llvm/ADT/SmallSet.h>
#include <llvm/ADT/StringMap.h>

#include <stack>
#include <string>
#include <numeric>

//export module tcc:historyinstance;

//import :tccnode;

//export namespace tcc {
namespace tcc {
    class TCCStore {
    public:
        using CastHistories = std::unordered_map<std::string, CastHistory>;
        using KeyRef = TCCNode::KeyRef;

        auto getKey(KeyRef id) const -> TCCKey {
            return db_.getTCCKey(id);
        }

        auto getNode(KeyRef id) const -> TCCNode const& {
            return db_.get(id);
        }

        auto getHistory(KeyRef id) const -> CastHistory const& {
            return histories_.at(id);
        }

        auto db() const -> TCCNodesDB const & {
            return db_;
        }

        auto hdb() const -> CastHistories const & {
            return histories_;
        }

        TCCStore(TCCNodesDB &&db_,
                CastHistories &&histories):
            db_(std::move(db_)),
            histories_(std::move(histories)) {}

    private:
        TCCNodesDB db_;
        CastHistories histories_;
    };

    // Return string because TCCKey should not be created here.
    using ExtendedCastContext = std::vector<CastContext>;
    auto String(ExtendedCastContext const &ecc) -> std::string {
        return "ecc:{kind: extended, size: " + std::to_string(ecc.size()) + "}";
    }

    auto derefIdFromContext(ExtendedCastContext ecc, TCCKey id) -> std::string {
        auto logKey = String(id) + "{" + String(ecc) + "}";
        TCC_DEBUG_FN(logKey);

        auto sid = String(id);
        for(auto const &cc: ecc) {
            for(auto const &[key, val]: cc.data()) {
                TCC_DEBUG(logKey, "[{} ↦ {}]  ({})", key, val, sid);
                llvm::outs() << "[" << logKey <<  "] [" << key << " ↦ " << val << "]  (" << sid << ")\n";
                if(key == val) {
                    TCC_DEBUG(logKey, "key == value; skip");
                    llvm::outs() << "[" << logKey <<  "] [" << key << " == " << val << "]; SKIP\n";
                    continue;
                }
                else if(key == sid) {
                    return std::string(val);
                }
                else if(key == id.prefix()) {
                    return std::string(val) + id.tail();
                }
            }
        }
        TCC_DEBUG(logKey, "end: {} = {}", String(id), sid);
        llvm::outs() << "[" << logKey <<  "] END deref'd: " << sid << "\n";
        return sid;
    }

    class TypeProvenanceSubstitution {
        // TypeProvenanceSubstitution is a mapped/instantiated constraint
        // recording how the substitution is made
    public:
        using Constraint = TypeProvenanceConstraint;

        TypeProvenanceSubstitution(ExtendedCastContext gcc,
                TypeProvenanceConstraint constraint):
            context_(gcc),
            constraint_(constraint) {}

        auto get(TCCStore const &tdb) -> Constraint {
            // check if susbtitution is possible
            auto sub = derefIdFromContext(context_, tdb.getKey(constraint_.child()));
            if(sub != constraint_.child()) {
                // return a new constraint with substituted child and context of substitution (TODO: context of substitution? Or constraint context? Context of substitution may be wrong and is better stored as ExtendedCastContext)
                replacement_ = TypeProvenanceConstraint({constraint_.dom(), sub}, constraint_.context());
                return replacement_.value();
            }
            return constraint_;
            //std::pair<TCCNode::KeyRef, TCCNode::KeyRef> substitution_;
        }

        auto child(TCCStore const &tdb) -> TCCNode::KeyRef {
            if(replacement_) {
                return replacement_.value().child();
            }
            replacement_ = get(tdb);
            return replacement_.value().child();
        }

        //auto replacement() -> std::optional<Constraint> {
        //    return replacement_;
        //}

        auto oc() const -> TypeProvenanceConstraint {
            return constraint_;
        }

        auto context() const -> ExtendedCastContext {
            return context_;
        }

    private:
        ExtendedCastContext context_;
        TypeProvenanceConstraint constraint_;
        std::optional<Constraint> replacement_ = {};
        //std::pair<TCCNode::KeyRef, TCCNode::KeyRef> substitution_;
    };

    // CastHistoryInstance is a cast history where the constraints have been instantiated
    class CastHistoryInstance {
    public:
        using KeyRef = TCCNode::KeyRef;

        CastHistoryInstance(CastHistory const *history,
                TypeProvenanceSubstitution substitution):
            id_(history->id()),
            history_(history),
            substitution_(substitution) {}

        void append(CastHistoryInstance &&ci) {
            nexts_.push_back(std::move(ci));
        }

        auto id() const -> KeyRef {
            return id_;
        }

        auto context() const -> ExtendedCastContext {
            return substitution_.context();
        }

        auto resolution(TCCStore const &tdb) const -> KeyRef {
            return substitution_.child(tdb);
        }

        auto substitution(TCCStore const &tdb) const -> TypeProvenanceConstraint {
            return substitution_.get(tdb);
        }

        auto ref() const -> CastHistory const* {
            return history_;
        }

        auto nexts() const -> std::vector<CastHistoryInstance> {
            return nexts_;
        }

    private:
        KeyRef id_; // get type_ from metadata
        CastHistory const *history_; // original history template
        mutable TypeProvenanceSubstitution substitution_;

        // CHI is recursive and must store adjacent vertices/edges
        std::vector<CastHistoryInstance> nexts_;
        //std::string label_; // context.fn name or type
    };

    auto makeCastHistoryInstance(TCCStore const &tdb,
            ExtendedCastContext gcc,
            TCCNode::KeyRef leaf,
            TypeProvenanceConstraint const &constraint)
        -> CastHistoryInstance {

        TypeProvenanceSubstitution cs(gcc, constraint);
        return CastHistoryInstance(&(tdb.getHistory(leaf)), std::move(cs));
    }

    std::size_t nbSpace = 0;
    auto String(TCCStore const &tdb,
            CastHistoryInstance const &hi)
        -> std::string {


        auto const &id = hi.id();
        auto const &next = hi.resolution(tdb);
        auto const &node = tdb.getNode(id);

        auto indent = std::string(nbSpace, '|');
        std::string history;
        history.reserve(256);
        //// TODO TODO: Add context number in instantiation so that the common one can be filtered out
        //for(auto const &cc: hi.context()) {
        //history += indent;
        /*
        if(hi.context().size() > 0) {
            auto cc = hi.context().front();
            if(cc.kind() == CastContext::Kind::Unknown) {
                history += "[CC{~Unknown~}](";
                //continue;
            }
            else {
                history += "[CC{id: '" + cc.id()
                    + "', kind: '" + String(cc.kind())
                    + "', scope: '" + cc.scope() + "'}](";
            }
        }
        */
        if(hi.context().size() > 0) {
            auto cc = hi.context().front();
            history += "[cc: " + cc.id() + "]";
        }
        history += node.type_;
        if(id != next) {
            history += "(" + next + ") via <" + node.id() + ">";
        }
        else {
            history += "(" + next + ")";
        }
        if(hi.nexts().empty()) {
            history += ")";
            //history += "\n" + std::string(nbSpace, ' ') + ")";
            //history += "\n" + indent + ")";
            //nbSpace -= (nbSpace > 0) ? 2 : 0;
            return history;
        }

        history += "\n" + indent + "=> {\n";
        nbSpace += 2;
        //history += " =>〈";
        auto toReplace = false;
        for(auto const &next: hi.nexts()) {
            history += "+" + std::string(nbSpace + 1, '-') + String(tdb, next);
            history += ",\n"; // + std::string(nbSpace, ' ');
            toReplace = true;
        }
        //if(history.back() == ',') {
        if(toReplace) {
            indent = std::string(nbSpace, ' ');
            std::string replacing = ",\n"; // + indent;
            history.replace(history.length() - replacing.length(), 2, "})");
            //history.back() = '}';
            //history.replace(history.length() - 1, std::strlen("〉"), "〉");
        }
        else {
            history += "})";
            //history += "〉";
        }
        nbSpace -= (nbSpace > 0) ? 2 : 0;
        //history += ")";
        return history;
    }

    auto instantiate(TCCStore const &tdb, CastHistory const &h) -> std::optional<CastHistoryInstance> {
        auto const logKey = h.id();
        TCC_DEBUG_FN(logKey);

        struct Frame {
            TypeProvenanceConstraint const &constraint;
            size_t branchIdx = 0;
            std::vector<CastHistoryInstance> nexts = {};
        };

        std::optional<CastHistoryInstance> out;
        std::stack<CastContext> gcc;
        std::stack<Frame> stack;
        std::unordered_map<std::string, bool> seen;

        // 1. make global context stack that changes with dfs
        auto instanceContext = [&gcc_=gcc]() -> ExtendedCastContext {
            auto gcc = gcc_;
            TCC_DEBUG("instanceContext", "Current context stack size: {}", gcc.size());
            ExtendedCastContext ecc;
            ecc.reserve(gcc.size());
            while(!gcc.empty()) {
                ecc.push_back(gcc.top());
                gcc.pop();
            }
            TCC_DEBUG("instanceContext", "Created context: {}", String(ecc));
            return ecc;
        };

        auto instantiateAndPop = [&]() {
            auto &&frame = stack.top();
            auto &[constraint, _, children] = frame;
            //auto &&[constraint, _, children] = stack.top();
            auto leafKey = constraint.child();
            auto tops = String(constraint);

            // instantiate history
            TCC_DEBUG(logKey, "(pop)[Top= {}]", tops);
            //llvm::outs() << "(pop)[Top= {" << tops << "}]\n";

            // 2. instantiate using the context stack
            auto instance = makeCastHistoryInstance(tdb, instanceContext(), leafKey, constraint);

            // add to stack if not seen TODO (must be done before pop)
            auto next = instance.resolution(tdb);
            TCC_DEBUG(logKey, "(pop)[Top= {}] | instance resolution: {}", tops, next);
            //llvm::outs() << "(pop) next |" << next << "\n";
            /*
            if(next != leafKey) {
                // destroy moved frame
                if(!stack.empty()) {
                    stack.pop();
                }
                stack.push(frame);
                stack.push(Frame(instance.substitution(tdb), 0, {}));
                gcc.push({});
                return;
            }
            */

            TCC_DEBUG(logKey, "(pop)[Top= {}] instance: {} | Appending [{}] children",
                    tops, instance.id(), children.size());
            //llvm::outs() << "(pop) Appending built instances (" << children.size() << ")\n";
            // append all branch instances
            for(auto &&c: children) {
                instance.append(std::move(c));
            }

            TCC_DEBUG(logKey, "(pop)[Top= {}] Destroying top frame", tops);
            //llvm::outs() << "(pop) Destroying top frame" << "\n";
            // mark this leaf for append in next pop
            //  i.e. append instance to parent after pop(at leaf) in case of branch (a => [b => c, d])
            if(!stack.empty()) {
                // destroy frame
                stack.pop();
                if(!gcc.empty()) {
                    TCC_DEBUG(logKey, "(pop)[gccTop= {}] Destroying gcc top frame", String(gcc.top()));
                    gcc.pop();
                }
                TCC_DEBUG(logKey, "(pop)[Top= <unk>] Destroying top frame: {}", tops);
                //llvm::outs() << "(pop) Destroyed top frame" << "\n";
            }

            if(!stack.empty()) {
                tops = String(stack.top().constraint);
                TCC_DEBUG(logKey, "(pop)[Top= {}] Adding instance of ({}) to top's next list",
                        tops, instance.id());
                //llvm::outs() << "(pop) Appending instance (" << instance.id() << ") to next children\n";
                stack.top().nexts.push_back(std::move(instance));
            }
            else {
                TCC_DEBUG(logKey, "(pop)[Top= []] Finished stack, setting frame out: {}", instance.id());
                //llvm::outs() << "(pop) Finished stack, setting frame out:" << instance.id() << "\n";
                out = std::move(instance);
                TCC_DEBUG(logKey, "(pop)[Top= []] Draining gcc stack");
                while(!gcc.empty()) {
                    TCC_DEBUG(logKey, "(pop)[gccTop= {}] gccPop()", String(gcc.top()));
                    gcc.pop();
                }
            }
        };


        // push root to stack
        TCC_DEBUG(logKey, "Pushing starter: {} => {}", h.id(), h.id());
        TypeProvenanceConstraint starter({"<Dummy>", h.id()});
        Frame init(starter);
        stack.push(init);
        gcc.push({});

        //llvm::outs() << "[Instantiation] id | " << h.id() << "\n";

        auto const &historyDB = tdb.hdb();
        while(!stack.empty()) {
            auto &[topConstraint, pos, _] = stack.top();
            auto strTop = String(topConstraint);
            seen[strTop] = true;    // there may be duplicate constraints on same history
            TCC_DEBUG(logKey, "(stack)[Top= {}] Mark seen", strTop);
            //llvm::outs() << "(stack) seen = true | " << strTop << "\n";

            if(historyDB.find(topConstraint.child()) == std::end(historyDB)) {
                // Should not happen; if constraint is created it should have a history
                TCC_WARN(logKey, "(stack)[Top= {}] Skipping ({}); error: no history found for '{}'",
                        strTop, topConstraint.dom(), topConstraint.child());
                //llvm::errs() << "(stack) No history found for '" << topConstraint.child() << "'\n";
                //llvm::errs() << "(stack) Skipping probable leaf | " << strTop << "\n";

                //instantiateAndPop();
                stack.pop();
                TCC_DEBUG(logKey, "(stack)[Top= []] Draining gcc stack");
                if(!gcc.empty()) {
                    TCC_DEBUG(logKey, "(pop)[gccTop= {}] gccPop()", String(gcc.top()));
                    gcc.pop();
                }
                continue;
            }

            auto const &topHistory = tdb.getHistory(topConstraint.child());
            auto const &constraints = topHistory.constraints();

            //llvm::outs() << "(stack) top is " << strTop << "\n";
            //llvm::outs() << "(stack) pos = " << pos << "; topHistory.constraints.size() = " << constraints.size() << "\n";
            TCC_DEBUG(logKey, "(stack)[Top= {}] pos={}; topHistory.constraints.size()={}",
                    strTop, pos, constraints.size());
            if(!constraints.empty() && pos < constraints.size()) {
                auto const &nextTop = constraints[pos++];
                TCC_DEBUG(logKey, "(stack)[Top= {}] Next({}): | pos={}; remaining={}",
                        strTop, String(nextTop), pos, constraints.size());
                //llvm::outs() << "(stack) pos = " << pos << "; remaining = " << constraints.size() << "\n";
                //llvm::outs() << "(stack) next = " << String(nextTop) << "\n";
                if(!seen[String(nextTop)]) {
                    stack.push({nextTop, 0, {}});
                    auto const &[newTop, _, __] = stack.top();
                    //TCC_DEBUG(logKey, "(stack)[Top= {}] Updated stack", String(newTop));
                    TCC_DEBUG(logKey, "(stack)[Top= {}] Updating context stack with '{}'", strTop, String(newTop.context()));
                    gcc.push(nextTop.context());
                }
            }
            else {
                // reached leaf node
                TCC_DEBUG(logKey, "(stack)[Top= {}] Leaf | topHistory.constraints.size() == pos == {}", strTop, pos);
                //llvm::outs() << "(stack) topHistory.constraints.size() == pos == " << pos << "\n";
                //llvm::outs() << "(stack) Instantiating leaf | " << strTop << "\n";
                instantiateAndPop();
            }
        }

        TCC_DEBUG(logKey, "Finished instantiation for: {}", h.id());
        //llvm::outs() << "(stack) Finished instantiation | " << h.id() << "\n";

        return out;
    }

} // end namespace tcc

#endif // end TCCHISTORYINSTANCE
