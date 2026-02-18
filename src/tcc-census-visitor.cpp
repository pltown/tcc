#ifndef TCCCENSUSVISITOR_H
#define TCCCENSUSVISITOR_H

//module;

#include "tcc-historyInstance.cpp"

#include "logger.h"

#include <clang/Tooling/Tooling.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/AST.h>
#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>

//#include <clang/Driver/Options.h>
//#include <clang/Frontend/ASTConsumers.h>
#include <tuple>

using namespace tcc;
using namespace clang;
using namespace clang::tooling;

//export module tcc:census;

//import :casts;
//import :tccnode;
//import :utils;

// for each match callback
//  - record stats from match: ptr/void ptr/cast/operation type/function/file/etc
//  - create TCCNodes (lhs, rhs)
//  - create dominator constraint
//
// for each history root
//  - record stats from history
//

using CastHistories = TCCStore::CastHistories;
static void append(CastHistories &archive,
        TCCNode::KeyRef const &key,
        std::optional<TypeProvenanceConstraint> constraint = {}) {

    auto [it, _] = archive.try_emplace(key, key);
    if(constraint) {
        it->second.append(std::move(*constraint));
    }
}

static void logUpdate(TCCNodesDB const &db,
        TCCNode::KeyRef const &src,
        TCCNode::KeyRef const &dest) {
    auto const &rs = db.get(src);
    auto const &rd = db.get(dest);

    TCC_DEBUG("CensusUpdate", "[{} => {}] <full:{} => {}>",
            src, dest, String(rs), String(rd));
}

//export namespace tcc {
namespace tcc {

    using TCCCollection = std::tuple<TCCNodesDB, CastHistories>;

    auto gather(TCCCollection a, TCCCollection const &b) -> TCCCollection {
        using std::get;
        using std::to_string;
        auto const logKey = "a[" + to_string(get<0>(a).size()) + ";" + to_string(get<1>(a).size()) + "]"
            + "<= b[" + to_string(get<0>(b).size()) + ";" + to_string(get<1>(b).size()) + "]";

        TCC_DEBUG_FN(logKey);
        // gather tcc nodes
        get<0>(a).append(get<0>(b));

        // gather cast histories
        for(auto const &[k, history]: get<1>(b)) {
            auto &histories = get<1>(a);
            if(auto it = histories.find(k); it != std::end(histories)) {
                // add constraints from b.history
                //it->getValue().append(std::move(history.constraints()));
                it->second.append(std::move(history.constraints()));
            }
            else {
                histories.insert({k, std::move(history)});
            }
        }
        return a;
    }

    // Actual info extraction from ast
    class TCCCensusVisitor: public RecursiveASTVisitor<TCCCensusVisitor> {
    public:
        explicit TCCCensusVisitor(clang::ASTContext *context):
            context_(*context) {}

        auto shouldVisitImplicitCode() const  -> bool {
            return false;
        }

        // visit todo
        // assignment, initialization
        // decl, vardecl, etc
        // struct definition, union definion: maybe not

        auto VisitVarDecl(VarDecl *vd) -> bool;
        auto VisitBinaryOperator(BinaryOperator *bop) -> bool;
        void handleBinaryAssignment(BinaryOperator const *bop, Expr const *lhs, Expr const *rhs);
        void handleBinaryArithmetic(BinaryOperator const *bop, Expr const *lhs, Expr const *rhs);
        void trackAssignment(TCCNode &&src, TCCNode &&dest);
        //
        auto VisitCallExpr(CallExpr const *call) -> bool;
        void handleFptrCall(CallExpr const *call, DeclRefExpr const *fptr);
        void handleFunctionCall(CallExpr const *call, FunctionDecl const *fn);
        auto VisitCastExpr(CastExpr const *cast) -> bool;
        //
        auto VisitMemberExpr(MemberExpr *mex) -> bool;
        auto VisitUnaryOperator(UnaryOperator *uop) -> bool;
        void handleUnaryAddressOf(UnaryOperator const *uop, Expr const *e);
        void handleUnaryDeref(UnaryOperator const *uop, Expr const *e);

        // Declaration->dump(): dumping ast nodes will show which nodes are already being visited
        auto results() -> TCCCollection {
            return {std::move(db_), std::move(hdb_)};
        }

    private:
        using VisitorBase = RecursiveASTVisitor<TCCCensusVisitor>;
        friend class RecursiveASTVisitor<TCCCensusVisitor>;

    private:
        clang::ASTContext &context_;
        TCCNodesDB db_;
        CastHistories hdb_;
    };

    // generic actions on AST
    // store matchdata
    class TCCCensusConsumer: public ASTConsumer {
    public:
        explicit TCCCensusConsumer(ASTContext * context):
            visitor_(context) {}

        void HandleTranslationUnit(ASTContext &context) override {
            //visitor_.TraverseDecl(context.getTranslationUnitDecl());
            visitor_.TraverseAST(context);
        }

        auto results() -> TCCCollection {
            return visitor_.results();
        }

    private:
        TCCCensusVisitor visitor_;
    };

    // entrypoint
    class TCCCensusCollectionAction: public clang::ASTFrontendAction {
    public:
        TCCCensusCollectionAction() = default;

        auto CreateASTConsumer(CompilerInstance &compiler,
                llvm::StringRef InFile)
            -> std::unique_ptr<ASTConsumer> override {

            // visitor construct args
            return std::make_unique<TCCCensusConsumer>(&compiler.getASTContext());
            //return std::unique_ptr<clang::ASTConsumer>(
            //        new TCCCensusConsumer(&compiler.getASTContext()));//, visitor args)
        }
    };
}

auto isPointerArithmeticOperation(BinaryOperator const &bop) -> bool {
    if(bop.isAdditiveOp()) {
        auto lhsType = bop.getLHS()->getType();
        auto rhsType = bop.getRHS()->getType();

        return lhsType->isPointerType()
            || lhsType->isArrayType()
            || rhsType->isPointerType()
            || rhsType->isArrayType();
    }

    return false;
}

auto TCCCensusVisitor::VisitVarDecl(VarDecl *vd) -> bool {
    auto const logKey = String(context_, *vd);
    TCC_DEBUG_FN(logKey + " <@" + stringLocation(context_, *vd) + ">");

    if(vd->hasInit()) {
        auto const *init = vd->getInit();
        auto src = makeTCCNodeForExpr(context_, *init);
        auto dest = makeTCCNodeForVarDecl(context_, *vd);
        trackAssignment(std::move(src), std::move(dest));
    }

    return true;
}

auto TCCCensusVisitor::VisitBinaryOperator(BinaryOperator *bop) -> bool {
    auto const logKey = String(context_, *bop);
    TCC_DEBUG_FN(logKey + " <@" + stringLocation(context_, *bop) + ">");


    //auto const *rhs = getChildFromSub<DeclRefExpr>(bop->getRHS());
    //if(!rhs) {
    //    TCC_DEBUG(logKey, "No DRE in RHS expr({}), likely a literal; skipping", String(context_, *bop));
    //    return true;
    //}
    //auto const *lhs = getChildFromSub<DeclRefExpr>(bop->getLHS());
    //if(!lhs) {
    //    TCC_DEBUG(logKey, "No DRE in LHS expr({}), unlikely(unless function return)!; stopping", String(context_, *bop));
    //    return false;
    //}
    //
    //auto src = db_.add(makeTCCNodeForBinarySubExpr(context_, *rhs, sourceLocation(*rhs)));
    //auto dest = db_.add(makeTCCNodeForBinarySubExpr(context_, *lhs, sourceLocation(*lhs)));
    //
    //auto ck = CastContext::Kind::Unknown;
    //if(auto const &sn = db_.get(src); sn.tmd_.fptrType_) {
    //    ck = CastContext::Kind::FptrAssignment;
    //}

    if(bop->isAssignmentOp()) {
        handleBinaryAssignment(bop, bop->getLHS(), bop->getRHS());
    }
    else if(bop->isMultiplicativeOp() || bop->isAdditiveOp()) {
        handleBinaryArithmetic(bop, bop->getLHS(), bop->getRHS());
    }

    /*
    //std::string type;
    if(bop->isPtrMemOp()) {
        // C++ not C
        VisitMemberPointer(bop, bop->getLHS(), bop->getRHS());
    }
    else if(bop->isShiftOp()) {
        type = "Shift";
    }
    else if(bop->isBitwiseOp()) {
        type = "Bitwise";
    }
    else if(bop->isComparisonOp()) {
        type = "Comparison";
    }

    auto scope = db_.getTCCKey(dest).prefix();
    CastContext cc(ck, type, scope);
    if(bop->isAssignmentOp()) {
        TCC_DEBUG(logKey, "(Assignment) Adding to call context: ({} = {})", dest, src);
        cc.insert(dest, src);
    }

    append(hdb_, src, TypeProvenanceConstraint({src, dest}, std::move(cc)));
    append(hdb_, dest);
    */

    return true;
}

/* C++ not C
void TCCCensusVisitor::VisitMemberPointer(BinaryOperator const *bop,
        Expr const *lhs,
        Expr const *rhs) {
    auto const logKey = String(context_, *bop);
    TCC_DEBUG_FN(logKey);

    std::string type = "MemberAccess";
}
*/

void TCCCensusVisitor::handleBinaryAssignment(BinaryOperator const *bop,
        Expr const *lhs,
        Expr const *rhs) {
    auto const logKey = String(context_, *bop);
    TCC_DEBUG_FN(logKey + " <@" + stringLocation(context_, *bop) + ">");

    auto const *ldre = getChildFromSub<DeclRefExpr>(lhs);
    if(!ldre) {
        TCC_DEBUG(logKey, "No DRE for LHS expr({}), unlikely(unless function return)!; skipping", String(context_, *bop));
        return;
    }

    auto src = makeTCCNodeForExpr(context_, *rhs);
    auto dest = makeTCCNodeForBinarySubExpr(context_, *ldre, sourceLocation(*lhs));
    trackAssignment(std::move(src), std::move(dest));
}

void TCCCensusVisitor::handleBinaryArithmetic(BinaryOperator const *bop,
        Expr const *lhs,
        Expr const *rhs) {
    auto const logKey = String(context_, *bop);
    TCC_DEBUG_FN(logKey + " <@" + stringLocation(context_, *bop) + ">");

    // TODO Check if array 
    auto isLptr = lhs->getType()->isPointerType();
    auto isRptr = rhs->getType()->isPointerType();
    if(!isLptr && !isRptr) {
        TCC_DEBUG(logKey, "Skipping: no pointer in either lhs({}) or rhs({})",
                String(context_, *lhs), String(context_, *rhs));
        return;
    }

    auto const *ldre = getChildFromSub<DeclRefExpr>(lhs);
    auto const *rdre = getChildFromSub<DeclRefExpr>(rhs);
    if(!ldre && !rdre) {
        TCC_DEBUG(logKey, "Skipping: no dre in either lhs({}) or rhs({})",
                String(context_, *lhs), String(context_, *rhs));
        return;
    }

    auto dreToBinaryOp = [&](auto const &dre) {
        // Use expr instead of dre (expr => bop) as visit expr will take care of dre => expr
        auto src = db_.add(makeTCCNodeForExpr(context_, *dre));
        auto dest = db_.add(makeTCCNodeForExpr(context_, *bop));
        logUpdate(db_, src, dest);

        auto label = std::string(bop->getOpcodeStr()) + "(" + src + ")";
        auto scope = db_.getTCCKey(dest).prefix();
        CastContext cc(CastContext::Kind::PointerArithmetic, label, scope);
        append(hdb_, src, TypeProvenanceConstraint({src, dest}, std::move(cc)));
        append(hdb_, dest);
    };

    // If two ptrs are involved, both ptrs' histories involve bop
    if(isLptr && ldre) {
        dreToBinaryOp(lhs);
    }
    if(isRptr && rdre) {
        dreToBinaryOp(rhs);
    }
}

void TCCCensusVisitor::trackAssignment(TCCNode &&srcN,
        TCCNode &&destN) {
    auto const logKey = srcN.id() + "->" + destN.id();

    auto scope = destN.key().prefix();
    auto ck = CastContext::Kind::Assignment;
    if(srcN.tmd_.fptrType_) {
        ck = CastContext::Kind::FptrAssignment;
    }
    auto src = db_.add(std::move(srcN));
    auto dest = db_.add(std::move(destN));
    CastContext cc(ck, src + "->" + dest, scope);
    TCC_DEBUG(logKey, "({}) Adding to tcc context: ({} = {})",
            String(ck), dest, src);
    cc.insert(dest, src);

    logUpdate(db_, src, dest);
    append(hdb_, src, TypeProvenanceConstraint({src, dest}, std::move(cc)));
    append(hdb_, dest);
}

auto TCCCensusVisitor::VisitCallExpr(CallExpr const *call) -> bool {
    auto const logKey = String(context_, *call);
    TCC_DEBUG_FN(logKey + " <@" + stringLocation(context_, *call) + ">");

    std::unordered_map<std::string, TCCNode::KeyRef> argHistories;
    auto processArg = [&](Expr const &arg,
            CastContext &cc,
            std::size_t pos) {
        TCC_DEBUG(logKey, "Building lhs(arg) for '{}'", String(context_, arg));
        auto src = db_.add(makeTCCNodeForExpr(context_, arg));
        auto dest = db_.add(makeTCCNodeForParamFromCall(context_, *call, pos));

        auto keySrc = db_.getTCCKey(src);
        auto keyDest = db_.getTCCKey(dest);
        if(keyDest.prefix() != keySrc.prefix()
                && keyDest.prefix().find_last_of(".") == std::string::npos) {
            TCC_DEBUG(logKey, "[cc.size() = {}] Adding: ({} = {})", cc.size(), dest, src);
            cc.insert(dest, src);
            TCC_DEBUG(logKey, "[cc.size() = {}]", cc.size());
        }

        argHistories[src] = dest;
    };

    auto const *fn = getCalleeDecl(context_, *call);
    if(!fn) {
        TCC_DEBUG(logKey, "Cannot get function decl from callexpr; checking for fptr decl");
        // likely a fptr that is unresolved at the moment
        // TODO: // fptr(args...) --> <fptr-qn>(<argqn>..)
        if(auto const *fptrDRE = getFptrFromFptrCall(context_, *call)) {
            TCC_DEBUG(logKey, "Found fptr decl for callee");
            //handleFptrCall(call, fptrDRE);
            auto fname = String(context_, *fptrDRE);
            CastContext cc(CastContext::Kind::FptrCall, String(context_, *call), fname);
            std::size_t pos = 0;
            std::for_each(call->arg_begin(), call->arg_end(),
                [&](auto const *arg) {
                    processArg(*arg, cc, pos++);
                });
            for(auto const &[src, dest]: argHistories) {
                logUpdate(db_, src, dest);
                append(hdb_, src, TypeProvenanceConstraint({src, dest}, cc));
                append(hdb_, dest);
            }
        }
        else {
            TCC_ERROR(logKey, "Skipping: callee function or fptr decl not found");
        }
        return true;
    }

    if(fn->getBuiltinID()) {
        TCC_DEBUG(logKey, "Skipping: builtin function");
        return true;
    }
    else {
        //handleFunctionCall(call, fn);
        auto fname = String(context_, *fn);
        CastContext cc(CastContext::Kind::FunctionCall, String(context_, *call), fname);

        std::size_t pos = 0;
        std::for_each(call->arg_begin(), call->arg_end(),
            [&](auto const *arg) {
                processArg(*arg, cc, pos++);
            /*
                TCC_DEBUG(logKey, "Building lhs(arg) for '{}'", String(context_, *arg));
                auto src = db_.add(makeTCCNodeForExpr(context_, *arg));
                //auto src = db_.add(makeTCCNodeForCallArg(context_, *call, *arg));
                auto dest = db_.add(makeTCCNodeForParamFromCall(context_, *call, pos++));

                auto sk = db_.getTCCKey(src);
                auto dk = db_.getTCCKey(dest);
                // To get to hof.$1.$0 = f.$0, avoid:
                // 1. (destk.prefix == srck.prefix => Links params: hof.$1.$0 = hof.$0
                // 2. (destk.prefix.find_last_of(".") => hof.$1.$0 = hof.var
                if(dk.prefix() != sk.prefix()
                        && dk.prefix().find_last_of(".") == std::string::npos) {
                    TCC_DEBUG(logKey, "[cc.size() = {}] Adding: ({} = {})",
                            cc.size(), dest, src);
                    cc.insert(dest, src);
                    TCC_DEBUG(logKey, "[cc.size() = {}]", cc.size());
                }

                argHistories[src] = dest;
            */
            });
        for(auto const &[src, dest]: argHistories) {
            logUpdate(db_, src, dest);
            append(hdb_, src, TypeProvenanceConstraint({src, dest}, cc));
            append(hdb_, dest);
        }
    }

    return true;
}

void TCCCensusVisitor::handleFptrCall(CallExpr const *call, DeclRefExpr const *fptr) {
    //fname = qualifiedNameDRE(context_, *fptrDRE);
}

void TCCCensusVisitor::handleFunctionCall(CallExpr const *call, FunctionDecl const *fn) {
}

auto TCCCensusVisitor::VisitCastExpr(CastExpr const *cast) -> bool {
    auto const logKey = String(context_, *cast);
    TCC_DEBUG_FN(logKey + " <@" + stringLocation(context_, *cast) + ">");

    auto src = db_.add(makeTCCNodeForExpr(context_, *(cast->getSubExpr())));
    auto dest = db_.add(makeTCCNodeForExpr(context_, *cast));
    if(src == dest && cast->getCastKind() != CK_BitCast) {
        TCC_DEBUG(logKey, "Skipping self-provenance in cast visit for cast kind: {}",
                CastExpr::getCastKindName(cast->getCastKind()));
        return true;
    }
    logUpdate(db_, src, dest);

    auto ck = cast->getCastKind();
    auto ccKind = CastContext::Kind::ImplicitCast;
    switch(ck) {
        case CK_BitCast:
            ccKind = CastContext::Kind::ExplicitCast; break;
        case CK_ToUnion:
            ccKind = CastContext::Kind::UnionCast; break;
            TCC_INFO("UnionCast", "Found {} => {}", src, dest);
        case CK_NullToPointer:
            ccKind = CastContext::Kind::NullToPointerCast; break;
        case CK_IntegralToPointer:
            ccKind = CastContext::Kind::IntegralToPointerCast; break;
        case CK_PointerToIntegral:
            ccKind = CastContext::Kind::PointerToIntegralCast; break;
        case CK_IntegralCast:
            ccKind = CastContext::Kind::IntegralCast; break;
        case CK_FloatingCast:
            ccKind = CastContext::Kind::FloatingCast; break;
        case CK_ToVoid:
            ccKind = CastContext::Kind::ToVoidCast; break;
        case CK_ArrayToPointerDecay:
            ccKind = CastContext::Kind::ArrayToPointerDecayCast; break;
        case CK_FunctionToPointerDecay:
            ccKind = CastContext::Kind::FunctionToPointerDecayCast; break;
        case CK_BuiltinFnToFnPtr:
            ccKind = CastContext::Kind::BuiltinFnToFnPtrCast; break;
        default:
            ccKind = CastContext::Kind::ImplicitCast; break;
    }

    auto scope = db_.getTCCKey(dest).prefix();
    CastContext cc(ccKind, String(ccKind), scope);

    append(hdb_, src, TypeProvenanceConstraint({src, dest}, std::move(cc)));
    append(hdb_, dest);

    return true;
}

// TODO TODO Record struct information
// #CIR
// struct <label> {
//    <field-label> : <C type> #[<tcc type annotation>]
//    }
//
//  annotations:
//  enum: #[enum]
//  tag: #[enum-tag]
//
auto TCCCensusVisitor::VisitMemberExpr(MemberExpr *mex) -> bool {
    auto const logKey = String(context_, *mex);
    TCC_DEBUG_FN(logKey + " <@" + stringLocation(context_, *mex) + ">");

    // unnamed member is seen as follows
    //  s->s_type | s => s_type
    //  s->s_ints | base: s->   | S => s_ints
    //  s->       | base: s     | s => ''
    //
    // always append member::base -> member::decl (this takes care of nested members too)
    // Context is union cast if memberdecl is union
    // qn for anon members includes the type def

    auto const *base = mex->getBase();
    TCCNode::KeyRef src;
    if(auto const *bmex = dyn_cast<MemberExpr>(base)) {
        TCC_DEBUG(logKey, "Base is a nested member");
        auto const *bdecl = bmex->getMemberDecl();
        if(!bmex) {
            TCC_ERROR(logKey, "Cannot get nested member base decl");
            return true;
        }
        src = db_.add(makeTCCNodeForMemberExpr(context_, *bmex, *bdecl));
    }
    else if(auto const *dre = getChildFromSub<DeclRefExpr>(base)) {
        TCC_DEBUG(logKey, "Base dre: {}", String(context_, *dre));
        src = db_.add(makeTCCNodeForDRE(context_, *base, *dre));
    }
    else {
        TCC_ERROR(logKey, "Skipping: cannot get member base decl");
        return true;
    }

    auto const *member = mex->getMemberDecl();
    if(!member) {
        TCC_ERROR(logKey, "Skipping: cannot get member decl");
        return true;
    }
    auto dest = db_.add(makeTCCNodeForMemberExpr(context_, *mex, *member));

    logUpdate(db_, src, dest);

    CastContext::Kind ck = CastContext::Kind::MemberAccess;
    if(db_.isUnionType(src)) {
        ck = CastContext::Kind::UnionMemberAccess;
    }
    else if(db_.isUnionType(dest)) {
        ck = CastContext::Kind::UnionMemberCast;
    }
    CastContext cc(ck, String(ck), db_.getTCCKey(dest).prefix());

    append(hdb_, src, TypeProvenanceConstraint({src, dest}, std::move(cc)));
    append(hdb_, dest);

    return true;
}

auto TCCCensusVisitor::VisitUnaryOperator(UnaryOperator *uop) -> bool {
    auto const logKey = String(context_, *uop);
    TCC_DEBUG_FN(logKey + " <@" + stringLocation(context_, *uop) + ">");

    auto op = UnaryOperator::getOpcodeStr(uop->getOpcode());
    if(op == "&") {
        handleUnaryAddressOf(uop, uop->getSubExpr());
        return true;
    }
    else if(op == "*") {
        handleUnaryDeref(uop, uop->getSubExpr());
        return true;
    }

   return VisitorBase::VisitUnaryOperator(uop);
}

void TCCCensusVisitor::handleUnaryAddressOf(UnaryOperator const *uop, Expr const *e) {
    auto const logKey = String(context_, *e);
    TCC_DEBUG_FN(logKey + " <@" + stringLocation(context_, *e) + ">");

    auto const *rhs = getChildFromSub<DeclRefExpr>(e);
    if(!rhs) {
        TCC_ERROR(logKey, "Skipping: no dre in expression '{}'", String(context_, *e));
        return;
    }

    // Create a cast link: e => &e
    auto src = db_.add(makeTCCNodeForDRE(context_, *e, *rhs));
    auto dest = db_.add(makeTCCNodeForExpr(context_, *uop));
    logUpdate(db_, src, dest);

    CastContext cc(CastContext::Kind::AddressOf, String(context_, *uop), db_.getTCCKey(dest).prefix());

    append(hdb_, src, TypeProvenanceConstraint({src, dest}, std::move(cc)));
    append(hdb_, dest);
}

void TCCCensusVisitor::handleUnaryDeref(UnaryOperator const *uop, Expr const *e) {
    auto const logKey = String(context_, *e);
    TCC_DEBUG_FN(logKey + " <@" + stringLocation(context_, *e) + ">");

    auto const *rhs = getChildFromSub<DeclRefExpr>(e);
    if(!rhs) {
        TCC_ERROR(logKey, "Skipping: no dre in expression '{}'", String(context_, *e));
        return;
    }

    // Create a cast link: e => *e
    auto src = db_.add(makeTCCNodeForDRE(context_, *e, *rhs));
    auto dest = db_.add(makeTCCNodeForExpr(context_, *uop));
    logUpdate(db_, src, dest);

    CastContext cc(CastContext::Kind::Deref, String(context_, *uop), db_.getTCCKey(dest).prefix());

    append(hdb_, src, TypeProvenanceConstraint({src, dest}, std::move(cc)));
    append(hdb_, dest);
}

#endif // end TCCCENSUSVISITOR_H
