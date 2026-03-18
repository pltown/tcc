// for each match callback
//  - record stats from match: ptr/void ptr/cast/operation type/function/file/etc
//  - create TCCNodes (lhs, rhs)
//  - create dominator constraint
//
// for each history root
//  - record stats from history
//

#ifndef TCCCENSUSVISITOR_H
#define TCCCENSUSVISITOR_H

//module;

#include "tcc-historyInstance.cpp"
#include "tcc-manifest.cpp"

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

using CastHistories = TCCStore::CastHistories;
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
        explicit TCCCensusVisitor(clang::ASTContext *context, Counter &stats):
            manifest_(context, stats),
            context_(*context) {
                setupCounter();
            }

        auto shouldVisitImplicitCode() const  -> bool {
            return false;
        }

        // visit todo
        // assignment, initialization
        // decl, vardecl, etc
        // struct definition, union definion: maybe not
        //auto VisitCompoundStmt(CompoundStmt *cs) -> bool;

        auto TraverseFunctionDecl(FunctionDecl *fd) -> bool;
        auto VisitFunctionDecl(FunctionDecl *fd) -> bool;

        auto VisitVarDecl(VarDecl *vd) -> bool;

        //auto TraverseCompoundStmt(CompoundStmt *cs) -> bool;
        auto VisitSwitchStmt(SwitchStmt *ss) -> bool;

        auto VisitExpr(Expr *e) -> bool;
        auto VisitBinaryOperator(BinaryOperator *bop) -> bool;
        void handleBinaryAssignment(BinaryOperator const *bop, Expr const *lhs, Expr const *rhs);
        void handleBinaryArithmetic(BinaryOperator const *bop, Expr const *lhs, Expr const *rhs);
        void trackAssignment(TCCNode &&src, TCCNode &&dest);

        auto VisitCallExpr(CallExpr *call) -> bool;
        auto VisitCastExpr(CastExpr *cast) -> bool;
        auto VisitMemberExpr(MemberExpr *mex) -> bool;
        auto VisitUnaryOperator(UnaryOperator *uop) -> bool;
        void handleUnaryAddressOf(UnaryOperator const *uop, Expr const *e);
        void handleUnaryDeref(UnaryOperator const *uop, Expr const *e);

        // visit return statement may be needed to link conditional return on base ptr
        //
        //void handleFptrCall(CallExpr const *call, DeclRefExpr const *fptr);
        //void handleFunctionCall(CallExpr const *call, FunctionDecl const *fn);
        //void checkSwitchConditionForMember(MemberExpr *mex, DeclRefExpr const *dre, TCCNode const &dest);

        // Declaration->dump(): dumping ast nodes will show which nodes are already being visited
        auto results() -> TCCCollection {
            return {std::move(db_), std::move(hdb_)};
        }

    private:
        using VisitorBase = RecursiveASTVisitor<TCCCensusVisitor>;
        friend class RecursiveASTVisitor<TCCCensusVisitor>;

        void setupCounter() {
            manifest_.SourceCounter.track<FunctionDecl>("Function Decls");
            manifest_.SourceCounter.track<VarDecl>("Var Decls");
            manifest_.SourceCounter.track<Expr>("Exprs");
            manifest_.SourceCounter.track<BinaryOperator>("BinaryOperators");
            manifest_.SourceCounter.track("BinaryOperator-Assignment", "BinOp Assignments");
            manifest_.SourceCounter.track("BinaryOperator-Arithmetic", "BinOp Arithmetic");
            manifest_.SourceCounter.track<CallExpr>("Call Exprs");
            manifest_.SourceCounter.track("CallExpr-Fptr", "CallExprs via Fptrs");
            manifest_.SourceCounter.track<CastExpr>("Casts");
            manifest_.SourceCounter.track<MemberExpr>("Member Accesses");
            manifest_.SourceCounter.track<UnaryOperator>("UnaryOperators");
            manifest_.SourceCounter.track("UnaryOperator-AddressOf", "Unary AddressOfs");
            manifest_.SourceCounter.track("UnaryOperator-Deref", "Unary Derefs");
        }

    private:
        TCCManifest manifest_;
        clang::ASTContext &context_;
        TCCNodesDB db_;
        CastHistories hdb_;
    };

    // generic actions on AST
    // store matchdata
    class TCCCensusConsumer: public ASTConsumer {
    public:
        explicit TCCCensusConsumer(ASTContext * context, Counter &stats):
            visitor_(context, stats) {}

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
    /*
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
    */
} // namespace tcc end

static void append(CastHistories &archive, TCCNode::KeyRef const &key, std::optional<TypeProvenanceConstraint> constraint = {});
static void logUpdate(TCCNodesDB const &db, TCCNode::KeyRef const &src, TCCNode::KeyRef const &dest);
auto isPointerArithmeticOperation(BinaryOperator const &bop) -> bool;

auto makeTCCNodeForExpr(TCCManifest &manifest, Expr const &e) -> TCCNode;
auto makeTCCNodeForVarDecl(TCCManifest &manifest, VarDecl const &var) -> TCCNode;
auto makeTCCNodeForBinarySubExpr(TCCManifest &manifest, DeclRefExpr const &dre, SourceLocation const &opExprLoc) -> TCCNode;
auto makeTCCNodeForParamFromCall(TCCManifest &manifest, CallExpr const &call, unsigned paramPos) -> TCCNode;
auto makeTCCNodeForDRE(TCCManifest &manifest, Expr const &originalExpr, DeclRefExpr const &dre) -> TCCNode;
auto makeTCCNodeForMemberExpr(TCCManifest &manifest, MemberExpr const &mex, ValueDecl const &member) -> TCCNode;

auto TCCCensusVisitor::TraverseFunctionDecl(FunctionDecl *fd) -> bool {
    auto const logKey = String(context_, *fd);
    TCC_DEBUG_FN(logKey + " <@" + stringLocation(context_, *fd) + ">");

    auto rc = VisitFunctionDecl(fd);
    if(!fd->hasBody()) {
        return true;
    }

    ManifestFunctionUpdater mark(manifest_, fd);
    rc = TraverseCompoundStmt(dyn_cast<CompoundStmt>(fd->getBody()));
    manifest_.markSeen({fd});
    //manifest_.resetCurrentFn();
    return rc;
}

auto TCCCensusVisitor::VisitFunctionDecl(FunctionDecl *fd) -> bool {
    auto const logKey = String(context_, *fd);
    TCC_DEBUG_FN(logKey + " <@" + stringLocation(context_, *fd) + ">");

    manifest_.SourceCounter.bump<FunctionDecl>();
    //manifest_.setCurrentFn(fd->getNameAsString());
    return true;
}

auto TCCCensusVisitor::VisitVarDecl(VarDecl *vd) -> bool {
    auto const logKey = String(context_, *vd);
    TCC_DEBUG_FN(logKey + " <@" + stringLocation(context_, *vd) + ">");

    if(vd->hasInit()) {
        VisitExpr(vd->getInit());

        manifest_.SourceCounter.bump<VarDecl>();
        auto const *init = vd->getInit();

        auto src = makeTCCNodeForExpr(manifest_, *init);
        auto dest = makeTCCNodeForVarDecl(manifest_, *vd);
        trackAssignment(std::move(src), std::move(dest));
    }
    TCC_DEBUG(logKey, "Skipping: No init");
    manifest_.markSeen(vd);

    return true;
}

/*
auto TCCCensusVisitor::TraverseCompoundStmt(CompoundStmt *cs) -> bool {
    auto const logKey = "Body <@" + stringLocation(context_, *cs) + ">";
    // covers ALL braced bodies

    for(auto *child: cs->body()) {
        if(auto *sw = dyn_cast<SwitchStmt>(child)) {
            VisitSwitchStmt(sw);
            //if(!VisitSwitchStmt(sw)) {
            //    TCC_ERROR(logKey, "Visit error at <switch> @<{}>", stringLocation(context_, *sw));
            //    return false;
            //}
        }
        else if(auto *e = dyn_cast<Expr>(child)) {
            VisitExpr(e);
            //if(!VisitExpr(e)) {
            //    TCC_ERROR(logKey, "<expr> @<{}>", stringLocation(context_, *e));
            //    return false;
            //}
        }
        else if(auto *stmt = dyn_cast<Stmt>(child)) {
            TraverseStmt(stmt);
            //if(!VisitStmt(stmt)) {
            //    TCC_ERROR(logKey, "<stmt> @<{}>", stringLocation(context_, *stmt));
            //    return false;
            //}
        }
    }

    return true;
}
*/

auto TCCCensusVisitor::VisitSwitchStmt(SwitchStmt *ss) -> bool {
    auto const logKey = String(context_, *(ss->getCond()));
    TCC_DEBUG_FN(logKey + " <@" + stringLocation(context_, *ss) + ">");

    auto *cond_ = ss->getCond();
    //VisitExpr(cond_);

    manifest_.SourceCounter.bump<SwitchStmt>();

    std::string cond = String(context_, *cond_);
    //logKey = cond;
    auto ck = CastContext::Kind::SwitchCondition;

    std::string val;
    auto * scl = ss->getSwitchCaseList();
    while(scl) {
        if(auto const *ssc = dyn_cast<CaseStmt>(scl)) {
            val = String(context_, *(ssc->getLHS()));
        }
        else {
            val = "default";
        }

        auto *tsc = scl;
        while(auto *nc = dyn_cast<SwitchCase>(tsc->getSubStmt())) {
            // cascading case
            if(auto *nsc = dyn_cast<CaseStmt>(nc)) {
                val += " | " + String(context_, *(nsc->getLHS()));
            }
            else {
                val += " | default";
            }
            tsc = nc;
        }

        CastContext cc(ck, cond + " == " + val, std::string(manifest_.currentFn()));
        //CastContext cc(ck, cond + " == " + val, getContainerFunction(context_, *ss));
        TCC_DEBUG(logKey, "Pushing context to manifest for: {} == {}", cond, val);
        manifest_.pushConditionContext(cc);
        //TraverseCompoundStmt(dyn_cast<CompoundStmt>(tsc->getSubStmt()));
        for(auto *schild: tsc->getSubStmt()->children()) {
            if(auto *se = dyn_cast<Expr>(schild)) {
                TCC_DEBUG(logKey, "Next visit: {}", String(context_, *se));
                VisitExpr(se);
            }
            else {
                TCC_DEBUG(logKey, "Next visit: {}", String(context_, *schild));
                TraverseStmt(schild);
            }
        }
        TCC_DEBUG(logKey, "Popping context from manifest for: {} == {}", cond, val);
        manifest_.popConditionContext();

        scl = tsc->getNextSwitchCase();
    }
    return true;
}

auto TCCCensusVisitor::VisitExpr(Expr *e) -> bool {
    manifest_.SourceCounter.bump<Expr>();
    auto const logKey = String(context_, *e);
    TCC_DEBUG_FN(logKey + " <@" + stringLocation(context_, *e) + ">");

    if(manifest_.isSeen(e)) {
        TCC_DEBUG(logKey, "Skipping: Already seen.");
        return true;
    }

    if(auto *mex = dyn_cast<MemberExpr>(e)) {
        return VisitMemberExpr(mex);
    }
    else if(auto *uop = dyn_cast<UnaryOperator>(e)) {
        return VisitUnaryOperator(uop);
    }
    else if(auto *bop = dyn_cast<BinaryOperator>(e)) {
        return VisitBinaryOperator(bop);
    }
    else if(auto *call = dyn_cast<CallExpr>(e)) {
        return VisitCallExpr(call);
    }
    else if(auto *ce = dyn_cast<CastExpr>(e)) {
        return VisitCastExpr(ce);
    }

    manifest_.markSeen(e);

    return true;
}

auto TCCCensusVisitor::VisitBinaryOperator(BinaryOperator *bop) -> bool {
    auto const logKey = String(context_, *bop);
    TCC_DEBUG_FN(logKey + " <@" + stringLocation(context_, *bop) + ">");

    VisitExpr(bop->getLHS());
    VisitExpr(bop->getRHS());

    if(bop->isAssignmentOp()) {
        manifest_.SourceCounter.bump<BinaryOperator>();
        handleBinaryAssignment(bop, bop->getLHS(), bop->getRHS());
    }
    else if(bop->isMultiplicativeOp() || bop->isAdditiveOp()) {
        manifest_.SourceCounter.bump<BinaryOperator>();
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

    manifest_.SourceCounter.bump("BinaryOperator-Assignment");
    auto src = makeTCCNodeForExpr(manifest_, *rhs);
    auto dest = makeTCCNodeForBinarySubExpr(manifest_, *ldre, sourceLocation(*lhs));
    trackAssignment(std::move(src), std::move(dest));
}

auto isPtr(Expr const *e) -> bool {
    return e->getType()->isPointerType();
}

void TCCCensusVisitor::handleBinaryArithmetic(BinaryOperator const *bop,
        Expr const *lhs,
        Expr const *rhs) {
    auto const logKey = String(context_, *bop);
    TCC_DEBUG_FN(logKey + " <@" + stringLocation(context_, *bop) + ">");

    // TODO Check if array
    if(!isPtr(lhs) && !isPtr(rhs)) {
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

    manifest_.SourceCounter.bump("BinaryOperator-Arithmetic");
    auto dreToBinaryOp = [&](Expr const *dre) {
        // Use expr instead of dre (expr => bop) as visit expr will take care of dre => expr
        auto src = db_.add(makeTCCNodeForExpr(manifest_, *dre));
        auto dest = db_.add(makeTCCNodeForExpr(manifest_, *bop));
        logUpdate(db_, src, dest);

        auto label = std::string(bop->getOpcodeStr()) + "(" + src + ")";
        auto scope = db_.getTCCKey(dest).scope();
        CastContext firstContext(CastContext::Kind::PointerArithmetic, label, scope);
        //TypeProvenanceConstraint::Contexts cc(1, std::move(firstContext));
        auto cc = manifest_.conditionContext();
        if(cc.empty()) {
            TCC_DEBUG(logKey, "No condition context in manifest");
        }
        else {
            TCC_DEBUG(logKey, "Condition context in manifest; size = {}", cc.size());
        }
        cc.push_back(firstContext);
        append(hdb_, src, TypeProvenanceConstraint({src, dest}, std::move(cc)));
        append(hdb_, dest);
    };

    // If two ptrs are involved, both ptrs' histories involve bop
    if(isPtr(lhs) && ldre) {
        dreToBinaryOp(lhs);
    }
    if(isPtr(rhs) && rdre) {
        dreToBinaryOp(rhs);
    }
}

void TCCCensusVisitor::trackAssignment(TCCNode &&srcN,
        TCCNode &&destN) {
    auto const logKey = srcN.id() + "->" + destN.id();

    auto scope = destN.key().scope();
    auto ck = CastContext::Kind::Assignment;
    if(srcN.tmd_.fptrType_) {
        ck = CastContext::Kind::FptrAssignment;
    }
    auto src = db_.add(std::move(srcN));
    auto dest = db_.add(std::move(destN));
    CastContext firstContext(ck, src + "->" + dest, scope);
    TCC_DEBUG(logKey, "({}) Adding to tcc context: ({} = {})",
            String(ck), dest, src);
    firstContext.insert(dest, src);
    //TypeProvenanceConstraint::Contexts cc(1, std::move(firstContext));
    auto cc = manifest_.conditionContext();
    if(cc.empty()) {
        TCC_DEBUG(logKey, "No condition context in manifest");
    }
    else {
        TCC_DEBUG(logKey, "Condition context in manifest; size = {}", cc.size());
    }
    cc.push_back(firstContext);

    logUpdate(db_, src, dest);
    append(hdb_, src, TypeProvenanceConstraint({src, dest}, std::move(cc)));
    append(hdb_, dest);
}

auto TCCCensusVisitor::VisitCallExpr(CallExpr *call) -> bool {
    auto const logKey = String(context_, *call);
    TCC_DEBUG_FN(logKey + " <@" + stringLocation(context_, *call) + ">");

    std::unordered_map<std::string, TCCNode::KeyRef> argHistories;
    auto processArg = [&](Expr &arg,
            CastContext &cc,
            std::size_t pos) {

        VisitExpr(&arg); // TODO improve?

        TCC_DEBUG(logKey, "Building lhs(arg) for '{}'", String(context_, arg));
        auto src = db_.add(makeTCCNodeForExpr(manifest_, arg));
        auto dest = db_.add(makeTCCNodeForParamFromCall(manifest_, *call, pos));

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
        // TODO: // fptr(args...) --> <fptr-qn>(<argqn>..) --> Done?
        if(auto const *fptrDRE = getFptrFromFptrCall(context_, *call)) {
            TCC_DEBUG(logKey, "Found fptr decl for callee");
            manifest_.SourceCounter.bump<CallExpr>();
            manifest_.SourceCounter.bump("CallExpr-Fptr");
            //handleFptrCall(call, fptrDRE);
            auto fname = String(context_, *fptrDRE);
            CastContext firstContext(CastContext::Kind::FptrCall, String(context_, *call), fname);
            std::size_t pos = 0;
            std::for_each(call->arg_begin(), call->arg_end(),
                [&](auto *arg) {
                    processArg(*arg, firstContext, pos++);
                });
            //TypeProvenanceConstraint::Contexts cc(1, std::move(firstContext));
            auto cc = manifest_.conditionContext();
            if(cc.empty()) {
                TCC_DEBUG(logKey, "No condition context in manifest");
            }
            else {
                TCC_DEBUG(logKey, "Condition context in manifest; size = {}", cc.size());
            }
            cc.push_back(firstContext);
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
        manifest_.SourceCounter.bump<CallExpr>();
        //handleFunctionCall(call, fn);
        auto fname = String(context_, *fn);
        CastContext firstContext(CastContext::Kind::FunctionCall, String(context_, *call), fname);

        std::size_t pos = 0;
        std::for_each(call->arg_begin(), call->arg_end(),
            [&](auto *arg) {
                processArg(*arg, firstContext, pos++);
            });
        TypeProvenanceConstraint::Contexts cc(1, std::move(firstContext));
        for(auto const &[src, dest]: argHistories) {
            logUpdate(db_, src, dest);
            append(hdb_, src, TypeProvenanceConstraint({src, dest}, cc));
            append(hdb_, dest);
        }
    }

    return true;
}

/*
void TCCCensusVisitor::handleFptrCall(CallExpr const *call, DeclRefExpr const *fptrDRE) {
    //fname = qualifiedNameDRE(context_, *fptrDRE);
}

void TCCCensusVisitor::handleFunctionCall(CallExpr const *call, FunctionDecl const *fn) {
}
*/

auto TCCCensusVisitor::VisitCastExpr(CastExpr *cast) -> bool {
    auto const logKey = String(context_, *cast);
    TCC_DEBUG_FN(logKey + " <@" + stringLocation(context_, *cast) + ">");

    VisitExpr(cast->getSubExpr());

    manifest_.SourceCounter.bump<CastExpr>(); // TODO check placement: maybe do after it is confirmed to be bitcast
    auto src = db_.add(makeTCCNodeForExpr(manifest_, *(cast->getSubExpr())));
    auto dest = db_.add(makeTCCNodeForExpr(manifest_, *cast));
    if(cast->getCastKind() == CK_LValueToRValue) {
        TCC_DEBUG(logKey, "Skipping implicit LToR cast");
        return true;
    }
    else if(src == dest && cast->getCastKind() != CK_BitCast) {
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

    auto scope = db_.getTCCKey(dest).scope();
    CastContext firstContext(ccKind, String(ccKind), scope);
    //TypeProvenanceConstraint::Contexts cc(1, std::move(firstContext));
    auto cc = manifest_.conditionContext();
    if(cc.empty()) {
        TCC_DEBUG(logKey, "No condition context in manifest");
    }
    else {
        TCC_DEBUG(logKey, "Condition context in manifest; size = {}", cc.size());
    }
    cc.push_back(firstContext);

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
    manifest_.SourceCounter.bump<MemberExpr>();
    auto const logKey = String(context_, *mex);
    TCC_DEBUG_FN(logKey + " <@" + stringLocation(context_, *mex) + ">");

    //memberdecl? Maybe not
    VisitExpr(mex->getBase());

    // unnamed member is seen as follows
    //  s->s_type | s => s_type
    //  s->s_ints | base: s->   | S => s_ints
    //  s->       | base: s     | s => ''
    //
    // always append member::base -> member::decl (this takes care of nested members too)
    // Context is union cast if memberdecl is union
    // qn for anon members includes the type def

    auto const *base = mex->getBase();
    std::optional<TCCNode> source;
    if(auto const *bmex = dyn_cast<MemberExpr>(base)) {
        TCC_DEBUG(logKey, "Base is a nested member");
        auto const *bdecl = bmex->getMemberDecl();
        if(!bmex) {
            TCC_ERROR(logKey, "Cannot get nested member base decl");
            return true;
        }
        source = makeTCCNodeForMemberExpr(manifest_, *bmex, *bdecl);
    }
    else if(auto const *dre = getChildFromSub<DeclRefExpr>(base)) {
        TCC_DEBUG(logKey, "Base dre: {}", String(context_, *dre));
        source = makeTCCNodeForDRE(manifest_, *base, *dre);
        // Additionally, this should be associated with the switch-case (if any) this expression is inside
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
    auto src = db_.add(std::move(*source)); // by this time source is valid
    auto dest = db_.add(makeTCCNodeForMemberExpr(manifest_, *mex, *member));

    logUpdate(db_, src, dest);

    CastContext::Kind ck = CastContext::Kind::MemberAccess;
    if(db_.isUnionType(src)) {
        ck = CastContext::Kind::UnionMemberCast;
    }
    else if(db_.isUnionType(dest)) {
        ck = CastContext::Kind::UnionMemberAccess;  // Nested union access
    }
    CastContext firstContext(ck, String(ck), db_.getTCCKey(dest).scope());
    //TypeProvenanceConstraint::Contexts cc(1, std::move(firstContext));
    auto cc = manifest_.conditionContext();
    if(cc.empty()) {
        TCC_DEBUG(logKey, "No condition context in manifest");
    }
    else {
        TCC_DEBUG(logKey, "Condition context in manifest; size = {}", cc.size());
    }
    cc.push_back(firstContext);

    append(hdb_, src, TypeProvenanceConstraint({src, dest}, std::move(cc)));
    append(hdb_, dest);

    return true;
}

auto TCCCensusVisitor::VisitUnaryOperator(UnaryOperator *uop) -> bool {
    auto const logKey = String(context_, *uop);
    TCC_DEBUG_FN(logKey + " <@" + stringLocation(context_, *uop) + ">");

    VisitExpr(uop->getSubExpr());

    manifest_.SourceCounter.bump<UnaryOperator>();
    auto op = UnaryOperator::getOpcodeStr(uop->getOpcode());
    if(op == "&") {
        handleUnaryAddressOf(uop, uop->getSubExpr());
        return true;
    }
    else if(op == "*") {
        handleUnaryDeref(uop, uop->getSubExpr());
        return true;
    }

   return true; //return VisitorBase::VisitUnaryOperator(uop);
}

void TCCCensusVisitor::handleUnaryAddressOf(UnaryOperator const *uop, Expr const *e) {
    auto const logKey = String(context_, *e);
    TCC_DEBUG_FN(logKey + " <@" + stringLocation(context_, *e) + ">");

    auto const *rhs = getChildFromSub<DeclRefExpr>(e);
    if(!rhs) {
        TCC_ERROR(logKey, "Skipping: no dre in expression '{}'", String(context_, *e));
        return;
    }

    manifest_.SourceCounter.bump("UnaryOperator-AddressOf");
    // Create a cast link: e => &e
    auto src = db_.add(makeTCCNodeForDRE(manifest_, *e, *rhs));
    auto dest = db_.add(makeTCCNodeForExpr(manifest_, *uop));
    logUpdate(db_, src, dest);

    CastContext firstContext(CastContext::Kind::AddressOf, String(context_, *uop), db_.getTCCKey(dest).scope());
    //TypeProvenanceConstraint::Contexts cc(1, std::move(firstContext));
    auto cc = manifest_.conditionContext();
    if(cc.empty()) {
        TCC_DEBUG(logKey, "No condition context in manifest");
    }
    else {
        TCC_DEBUG(logKey, "Condition context in manifest; size = {}", cc.size());
    }
    cc.push_back(firstContext);

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

    manifest_.SourceCounter.bump("UnaryOperator-Deref");
    // Create a cast link: e => *e
    auto src = db_.add(makeTCCNodeForDRE(manifest_, *e, *rhs));
    auto dest = db_.add(makeTCCNodeForExpr(manifest_, *uop));
    logUpdate(db_, src, dest);

    CastContext firstContext(CastContext::Kind::Deref, String(context_, *uop), db_.getTCCKey(dest).scope());
    //TypeProvenanceConstraint::Contexts cc(1, std::move(firstContext));
    auto cc = manifest_.conditionContext();
    if(cc.empty()) {
        TCC_DEBUG(logKey, "No condition context in manifest");
    }
    else {
        TCC_DEBUG(logKey, "Condition context in manifest; size = {}", cc.size());
    }
    cc.push_back(firstContext);

    append(hdb_, src, TypeProvenanceConstraint({src, dest}, std::move(cc)));
    append(hdb_, dest);
}

/*
*/


static void append(CastHistories &archive,
        TCCNode::KeyRef const &key,
        std::optional<TypeProvenanceConstraint> constraint) {

    auto contexts = [](std::vector<CastContext> const &ccs) {
        std::string str = "[";
        for(auto const &cc: ccs) {
            str += cc.id() + "<" + String(cc.kind()) + ">;";
        }
        return str + "]";
    };

    auto [it, inserted] = archive.try_emplace(key, key);

    if(!constraint) {
        return;
    }

    /*
    //CastHistories is an unorderd_map of constraints mapped to keys.
    //If not inserted => key exists. Not that constraint exists
    if(!inserted) {
        std::vector<CastContext> cc;
        if(auto oldConstraint = it->second.getConstraintFor(constraint->child())) {
                cc = oldConstraint->context();
        }

        TCC_WARN("CensusUpdate", "Updating existing Provenance constraint: <{}>[{}]",
        //TCC_WARN("CensusUpdate", "Provenance constraint already exists: <{}>[{}]",
            contexts(cc), String(*constraint));
        TCC_WARN("CensusUpdate", "New provenance constraint: <{}>[{}]",
        //TCC_WARN("CensusUpdate", "New provenance constraint <{}>[{}] ignored",
            contexts(constraint->context()), String(*constraint));
        //return;
    }
    */

    it->second.append(std::move(*constraint));
    TCC_DEBUG("CensusUpdate", "Provenance constraint: <{}>[{}]",
            contexts(constraint->context()), String(*constraint));
}

static void logUpdate(TCCNodesDB const &db,
        TCCNode::KeyRef const &src,
        TCCNode::KeyRef const &dest) {
    auto const &rs = db.get(src);
    auto const &rd = db.get(dest);

    TCC_DEBUG("CensusTracker", "Provenance: [{} => {}] <full:{} => {}>",
            src, dest, String(rs), String(rd));
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

// node-utils/factories
//----------------------
auto qualifiedNameForParm(std::string const &fn,
        unsigned parmIndex)
    -> TCCKey {

    auto const logKey = fn + ".$" + std::to_string(parmIndex);
    TCC_DEBUG_FN(logKey);
    auto qn = TCCKey("$" + std::to_string(parmIndex), fn);
    TCC_DEBUG(logKey, "QN: {}", String(qn));
    return qn;
}

auto qualifiedNameForParm(TCCManifest const &manifest,
        FunctionDecl const &fn,
        unsigned parmIndex)
    -> TCCKey {

    return qualifiedNameForParm(String(manifest.context(), fn), parmIndex);
}

auto qualifiedNameValueDecl(TCCManifest &, ValueDecl const &vd)
    -> TCCKey;
auto qualifiedNameValueDecl(TCCManifest &, ValueDecl const &vd, DeclarationNameInfo const &nameInfo)
    -> TCCKey;
auto qualifiedNameDRE(TCCManifest &, DeclRefExpr const &dre)
    -> TCCKey;
auto qualifiedNameExpr(TCCManifest &, Expr const &e)
    -> TCCKey;
auto qualifiedNameFromPossibleFptrCall(TCCManifest &, CallExpr const &call, unsigned paramPos)
    -> TCCKey;

auto qualifiedNameValueDecl(TCCManifest &manifest,
        ValueDecl const &vd)
    -> TCCKey {

    auto &context = manifest.context();
    auto logKey = String(context, vd);
    TCC_DEBUG_FN(logKey);

    auto name = vd.getDeclName();
    if(!name) {
        TCC_ERROR(logKey, "Bad name!");
        llvm::errs() << "Bad name for " << String(context, vd) << "\n";
        auto qn = TCCKey("~decl~err(" + String(context, vd) + ")");
        TCC_DEBUG(logKey, "QN: {}", String(qn));
        return qn;
    }
    TCC_DEBUG(logKey, "Name: {}", name.getAsString());
    logKey += name.getAsString();

    auto const *fn = getContainerFunctionDecl(context, vd);
    if(!fn) {
        TCC_DEBUG(logKey, "Container function is null");
        auto qn = TCCKey(name.getAsString(), "::");
        TCC_DEBUG(logKey, "QN: {}", String(qn));
        return qn;
    }

    if(auto parmIndex = getParameterIndex(*fn, name)) {
        auto qn = qualifiedNameForParm(manifest, *fn, *parmIndex);
        TCC_DEBUG(logKey, "parmIndex: {}; QN: {}", parmIndex.value(), String(qn));
        return qn;
    }

    TCC_DEBUG(logKey, "parmIndex is nullopt (identifier is not a parm)");
    auto qn = TCCKey(name.getAsString(), fn->getNameAsString());
    TCC_DEBUG(logKey, "QN: {}", String(qn));
    return qn;
}

auto qualifiedNameValueDecl(TCCManifest &manifest,
        ValueDecl const &vd,
        DeclarationNameInfo const &nameInfo)
    -> TCCKey {
    return qualifiedNameValueDecl(manifest, vd);
}

// TO CHECK TODO
auto qualifiedNameExpr(TCCManifest &manifest,
        Expr const &e)
    -> TCCKey {

    auto &context = manifest.context();
    auto const logKey = String(context, e);
    TCC_DEBUG_FN(logKey);

    auto const *dre = getChild<DeclRefExpr>(e);
    if(!dre) {
        TCC_DEBUG(logKey, "No DRE in expression, stringifying expr for qn");
        auto qn = TCCKey{"#lit/" + String(context, e) + "/"};
        TCC_DEBUG(logKey, "QN: {}", String(qn));
        return qn;
    }

    TCC_DEBUG(logKey, "DRE in expression: {}", String(context, *dre));

    // Qualified name of dre should be replaced if the expr contains dre
    if(auto const *vd = dyn_cast<VarDecl>(dre->getFoundDecl())) {
        TCC_DEBUG(logKey, "Decl from DRE: {}", String(context, *vd));
        if(vd->isFunctionPointerType() || vd->isFunctionOrFunctionTemplate()) {
            TCC_DEBUG(logKey, "DRE is fptr type: {}", String(context, *dre));
            // Any operation on function/fptr are non-type changing => qualified name of dre suffices
            auto qn = qualifiedNameDRE(manifest, *dre);
            TCC_DEBUG(logKey, "QN: {}", String(qn));
            return qn;
        }
        TCC_DEBUG(logKey, "DRE is not fptr type: {}", String(context, *dre));
    }

    //TCC_DEBUG(logKey, "No decl in dre: {}", String(context, *dre));
    auto refKey = qualifiedNameDRE(manifest, *dre);
    auto refId = refKey.id();
    auto refStr = String(context, *dre);
    TCC_DEBUG(logKey, "Reference (dre): id({}); str({})", refId, refStr);
    // replace dre name in expr string with refId
    auto expr = String(context, e);
    auto const *expr_ = e.IgnoreParenCasts();
    auto expr2 = String(context, *expr_);
    bool shouldAppendExpr = false;
    if(auto pos = expr.find(refStr); pos != std::string::npos) {
        TCC_DEBUG(logKey, "Found identifier '{}' in expr", refStr);
        //if(refStr != refId) {
        //if(expr != refStr
        //        || e.getType() != dre->getType() ) {
        if(e.getType()->getCanonicalTypeInternal() != dre->getType()->getCanonicalTypeInternal()) {
            // filter out simple ref exprs: e.g. i in `j = i`;
            TCC_DEBUG(logKey, "Expr type: '{}' v/s dre type: '{}'", Typename(context, e), Typename(context, *dre));
            TCC_DEBUG(logKey, "Expr2 type: '{}' v/s expr type: '{}' v/s dre type: '{}'",
                    Typename(context, *expr_), Typename(context, e), Typename(context, *dre));
            shouldAppendExpr = true;
        }
        //expr.replace(pos, refStr.size(), refId);
        // remove dre part
        //expr = expr.substr(0, pos) + expr.substr(pos + dreStr.size());
    }

    if(!shouldAppendExpr) {
        TCC_DEBUG(logKey, "Expr: '{}'", expr);
        TCCKey ek{
            refId,
            refKey.scope(),
        };
        TCC_DEBUG(logKey, "QN: {}", String(ek));
        return ek;
    }

    TCC_DEBUG(logKey, "Expr: '{}'", expr);
    TCCKey ek{
        refId,
        refKey.scope(),
        expr,
    };
    TCC_DEBUG(logKey, "QN: {}", String(ek));
    return ek;
}

auto qualifiedNameDRE(TCCManifest &manifest,
        DeclRefExpr const &dre)
    -> TCCKey {

    auto &context = manifest.context();
    auto const logKey = String(context, dre);
    TCC_DEBUG_FN(logKey);
    if(auto const *refd = dre.getReferencedDeclOfCallee()) {
        TCC_DEBUG(logKey, "Found referenced decl: {}", String(context, *refd));
        if(auto const *vd = dyn_cast<ValueDecl>(refd)) {
            TCC_DEBUG(logKey, "Referenced decl is value decl");
            return qualifiedNameValueDecl(manifest, *vd);
        }
    }

    if(auto const *decl = dre.getFoundDecl()) {
        TCC_DEBUG(logKey, "Found decl from dre: {}", String(context, *decl));
        if(auto const *vd = dyn_cast<VarDecl>(decl)) {
            TCC_DEBUG(logKey, "Decl is vardecl");
            return qualifiedNameValueDecl(manifest, *vd);
        }

        TCC_DEBUG(logKey, "No vardecl in dre decl; checking fptr");
        if(decl->getFunctionType()) {
            auto qn = TCCKey {
                String(context, *decl),
                //std::string(manifest.currentFn())
                getContainerFunction(context, *decl)
            };
            TCC_DEBUG(logKey, "DRE decl is a function type; qn = fn-name = '{}'", String(qn));
            return qn;
        }

        TCC_WARN(logKey, "DRE decl is neither vardecl nor fptr!");
        auto qn = TCCKey {
            String(context, dre),
            std::string(manifest.currentFn())
            //getContainerFunction(context, *decl)
        };
        TCC_WARN(logKey, "Stringified qn: '{}'", String(qn));
        return qn;
    }

    if(auto const *stmt = dre.getExprStmt()) {
        auto qn = TCCKey {
            String(context, dre),
            std::string(manifest.currentFn())
            //getContainerFunction(context, *stmt)
        };
        TCC_DEBUG(logKey, "DRE is a stmt without decl, stringifying dre; qn = '{}'", String(qn));
        return qn;
    }

    auto errQn = TCCKey {
        "~dre~err(" + String(context, dre) + ")",
        std::string(manifest.currentFn())
        //getContainerFunction(context, dre)
    };
    TCC_ERROR(logKey, "DRE does not have any valid decl or stmt! qn = '{}'", String(errQn));
    return errQn;
}

// TO FIX
auto qualifiedNameFromPossibleFptrCall(TCCManifest &manifest,
        CallExpr const &call,
        unsigned paramPos)
    -> TCCKey {

    auto &context = manifest.context();
    auto logKey = String(context, call);
    TCC_DEBUG_FN(logKey);

    if(auto const *fptr = getFptrFromFptrCall(context, call)) {
        TCC_DEBUG(logKey, "Found fptr from callexpr: {}", String(context, *fptr));
        auto dreqn = qualifiedNameDRE(manifest, *fptr);
        TCC_DEBUG(logKey, "Fptr qn: {}", String(dreqn));
        auto dreid = dreqn.id();
        TCCKey ek {
            "$" + std::to_string(paramPos),
            String(dreqn)
        };
        TCC_DEBUG(logKey, "Fptr parm qn = '{}'", String(ek));
        return ek;
    }

    TCC_DEBUG(logKey, "No fptr in call expr, using callee expr");
    if(auto const *fn = call.getCallee()) {
        TCC_DEBUG(logKey, "Found callee: {}", String(context, *fn));
        // TODO chk
        //auto qn = qualifiedNameForParm(context, *fn, paramPos);
        auto qn = TCCKey {
            "$" + std::to_string(paramPos),
            String(context, *fn)
        };
        TCC_DEBUG(logKey, "qn = '{}'", String(qn));
        return qn;
    }

    TCC_DEBUG(logKey, "Call expr has no callee, stringifying call");
    auto qn = TCCKey {
        String(context, call),
        std::string(manifest.currentFn())
        //getContainerFunction(context, call)
    };
    TCC_DEBUG(logKey, "qn = '{}'", String(qn));
    return qn;
}
// END FIX

auto makeTCCNodeForParam(TCCManifest &manifest,
        ParmVarDecl const &param)
    -> TCCNode {

    auto const &context = manifest.context();
    auto const logKey = String(context, param);
    TCC_DEBUG_FN(logKey);
    TCC_DEBUG(logKey, "Building TCCNode for ParmVarDecl type: {}",
            Typename(context, param));

    return {
        {&context, &param},
        qualifiedNameValueDecl(manifest, param),
        String(context, param),
        Typename(context, param),
        TypeCategory(param),
        sourceLocation(param).printToString(context.getSourceManager()),
        makeTypeMetadata(context, param)
    };
}

auto makeTCCNodeForParamFromMaybeFptrCall(TCCManifest &manifest,
        CallExpr const &call,
        unsigned paramPos)
    -> TCCNode {

    auto &context = manifest.context();
    auto const logKey = String(context, call) + "@" + std::to_string(paramPos);
    TCC_DEBUG_FN(logKey);
    TCC_DEBUG(logKey, "Building TCCNode for maybe fptr call's parameter");

    return TCCNode {
        {}, // No parameter decl
        qualifiedNameFromPossibleFptrCall(manifest, call, paramPos),
        String(context, call, paramPos),
        "", // No decl from callee => no type
        "",
        sourceLocation(call).printToString(context.getSourceManager()),
        {}  // No decl from callee => no type
    };
}

auto makeTCCNodeForParamFromCall(TCCManifest &manifest,
        CallExpr const &call,
        unsigned paramPos)
    -> TCCNode {

    auto const &context = manifest.context();
    auto const logKey = String(context, call) + "@" + std::to_string(paramPos);
    TCC_DEBUG_FN(logKey);
    TCC_DEBUG(logKey, "Building TCCNode for Call Param (index: {})", paramPos);

    auto const *param = getParamDecl(context, call, paramPos);
    if(param) {
        TCC_DEBUG(logKey, "Found param decl");
        return makeTCCNodeForParam(manifest, *param);
    }

    TCC_DEBUG(logKey, "No param decl found");
    auto const *fn = call.getCallee();  // getCalleDecl() will not work (used in getParamDecl)
    if(fn) {
        // it's possible that call is fptr since getparamdecl failed
        return makeTCCNodeForParamFromMaybeFptrCall(manifest, call, paramPos);
    }

    TCC_DEBUG(logKey, "Callee in call is also nullptr");
    return TCCNode {
        {},
        TCCKey {std::to_string(paramPos), "~param~ERR:ResolveFunctionFromCallexpr_.$"},
        String(context, call) + ".$" + std::to_string(paramPos),
        "", //Typename(context, *arg),
        "", //TypeCategory(context, *arg),
        sourceLocation(call).printToString(context.getSourceManager()),
        {} //makeTypeMetadata(context, *arg) // TODO Why? type is not defined clearly
    };
}

auto makeTCCNodeForVarDecl(TCCManifest &manifest,
        VarDecl const &var)
    -> TCCNode {

    auto const &context = manifest.context();
    auto logKey = String(context, var);
    TCC_DEBUG_FN(logKey);
    TCC_DEBUG(logKey, "Building TCCNode for VarDecl type: {}",
            Typename(context, var));
    return {
        {&context, &var},
        qualifiedNameValueDecl(manifest, var),
        String(context, var),
        Typename(context, var),
        TypeCategory(var),
        sourceLocation(var).printToString(context.getSourceManager()),
        makeTypeMetadata(context, var)
    };
}

auto makeTCCNodeForDREDecl(TCCManifest &manifest,
        DeclRefExpr const &dre,
        ValueDecl const &decl)
    -> TCCNode {

    auto const &context = manifest.context();
    auto logKey = String(context, decl);
    TCC_DEBUG_FN(logKey);
    TCC_DEBUG(logKey, "Building TCCNode for Valuedecl type: {}",
            Typename(context, decl));

    return {
        {&context, &decl},
        qualifiedNameValueDecl(manifest, decl),
        String(context, decl),
        Typename(context, decl),
        TypeCategory(decl),
        sourceLocation(decl).printToString(context.getSourceManager()),
        makeTypeMetadata(context, decl)
    };
}

// originalExpr is only used for location in case decl is not found
auto makeTCCNodeForDRE(TCCManifest &manifest,
        Expr const &originalExpr,
        DeclRefExpr const &dre)
    -> TCCNode {

    auto &context = manifest.context();
    auto const logKey = String(context, originalExpr);
    TCC_DEBUG_FN(logKey);
    TCC_DEBUG(logKey, "Building TCCNode for DRE in expr with type: {}",
            Typename(context, originalExpr));

    auto const *refd = dre.getReferencedDeclOfCallee();
    if(refd) {
        TCC_DEBUG(logKey, "Building from reference decl");
        auto const *vd = dyn_cast<VarDecl>(refd);
        if(vd) {
            TCC_DEBUG(logKey, "ReferenceDecl is a var decl");
            return makeTCCNodeForVarDecl(manifest, *vd);
        }
        TCC_ERROR(logKey, "Reference decl has no var decls");
    }

    if(auto const *decl = dre.getDecl()) {
        TCC_DEBUG(logKey, "Building from dre decl"); // Should be same as referenced decl?
        auto const *vd = dyn_cast<VarDecl>(refd);
        if(vd) {
            TCC_DEBUG(logKey, "DreDecl is a var decl");
            return makeTCCNodeForVarDecl(manifest, *vd);
        }
        TCC_DEBUG(logKey, "Dre decl has no var decls; checking for fptr");
        if(auto const *fp = decl->getAsFunction()) {
            TCC_DEBUG(logKey, "Dre decl is an fptr: {}", String(context, *fp));
            return makeTCCNodeForDREDecl(manifest, dre, *fp);
        }
        TCC_DEBUG(logKey, "Dre decl is not fptr either");
        return makeTCCNodeForDREDecl(manifest, dre, *decl);
    }

    if(auto const *stmt_ = dre.getExprStmt()) {
        TCC_DEBUG(logKey, "Building TCCNode from DRE stmt");
        auto const &stmt = *stmt_;
        TCC_DEBUG(String(context, stmt), "Building TCCNode for DRE stmt type: {}",
                Typename(context, dre));

        return {
            {&context, &dre},
            TCCKey {String(context, stmt),
                std::string(manifest.currentFn())},
                //getContainerFunction(context, stmt)},
            String(context, dre),
            Typename(context, dre),
            TypeCategory(dre),
            sourceLocation(originalExpr).printToString(context.getSourceManager()), // <----
            makeTypeMetadata(context, dre)
        };
    }

    TCC_ERROR(logKey, "No decl or stmt in DRE");
    return TCCNode {
        {&context, &dre},
        qualifiedNameDRE(manifest, dre),
        String(context, dre),
        Typename(context, dre),
        TypeCategory(dre),
        sourceLocation(originalExpr).printToString(context.getSourceManager()),
        makeTypeMetadata(context, dre)
    };
}

auto makeTCCNodeForBinarySubExpr(TCCManifest &manifest,
        DeclRefExpr const &dre,
        SourceLocation const &opExprLoc)
    -> TCCNode {

    auto &context = manifest.context();
    auto const logKey = String(context, dre);
    TCC_DEBUG_FN(logKey);
    TCC_DEBUG(logKey, "Building TCCNode for Binary op lhs/rhs with type: {}",
            Typename(context, dre));

    auto const *refd = dre.getReferencedDeclOfCallee();
    if(refd) {
        TCC_DEBUG(logKey, "Building from reference decl");
        auto const *vd = dyn_cast<VarDecl>(refd);
        if(vd) {
            TCC_DEBUG(logKey, "ReferenceDecl is a var decl");
            return makeTCCNodeForVarDecl(manifest, *vd);
        }
        TCC_ERROR(logKey, "Reference decl has no var decls");
    }

    if(auto const *decl = dre.getDecl()) {
        TCC_DEBUG(logKey, "Building from dre decl"); // Should be same as referenced decl?
        auto const *vd = dyn_cast<VarDecl>(refd);
        if(vd) {
            TCC_DEBUG(logKey, "DreDecl is a var decl");
            return makeTCCNodeForVarDecl(manifest, *vd);
        }
        TCC_DEBUG(logKey, "Dre decl has no var decls; checking for fptr");
        if(auto const *fp = decl->getAsFunction()) {
            TCC_DEBUG(logKey, "Dre decl is an fptr: {}", String(context, *fp));
            return makeTCCNodeForDREDecl(manifest, dre, *fp);
        }
        TCC_DEBUG(logKey, "Dre decl is not fptr either");
        return makeTCCNodeForDREDecl(manifest, dre, *decl);
    }

    if(auto const *stmt_ = dre.getExprStmt()) {
        TCC_DEBUG(logKey, "Building TCCNode from DRE stmt");
        auto const &stmt = *stmt_;
        TCC_DEBUG(String(context, stmt), "Building TCCNode for DRE stmt type: {}",
                Typename(context, dre));

        return {
            {&context, &dre},
            TCCKey {String(context, stmt),
                std::string(manifest.currentFn())},
                //getContainerFunction(context, stmt)},
            String(context, dre),
            Typename(context, dre),
            TypeCategory(dre),
            opExprLoc.printToString(context.getSourceManager()), // <----
            makeTypeMetadata(context, dre)
        };
    }

    TCC_ERROR(logKey, "No decl or stmt in DRE");
    return TCCNode {
        {&context, &dre},
        qualifiedNameDRE(manifest, dre),
        String(context, dre),
        Typename(context, dre),
        TypeCategory(dre),
        opExprLoc.printToString(context.getSourceManager()),
        makeTypeMetadata(context, dre)
    };
}

auto makeTCCNodeForMemberExpr(TCCManifest &manifest,
        MemberExpr const &mex,
        ValueDecl const &member)
    -> TCCNode {

    auto &context = manifest.context();
    auto const logKey = String(context, member);
    TCC_DEBUG_FN(logKey);
    TCC_DEBUG(logKey, "Building TCCNode for member of type: {}",
            Typename(context, member));

    auto nameUT = [&](RecordType const *rt) -> std::string {
        auto const *record = rt->getDecl();
        std::string fields;
        char sep;
        char suffix;
        if(record->isUnion()) {
            fields = "(";
            sep = '|';
            suffix = ')';
        }
        else {
            fields = "{";
            sep = ',';
            suffix = '}';
        }
        for(auto const *f: record->fields()) {
            fields += f->getNameAsString() + sep;
        }
        if(fields.size() > 1) {
            fields.back() = suffix;
        }
        else {
            fields += suffix;
        }
        return fields;
    };

    // To be updated once the member type is identified
    auto qn = qualifiedNameExpr(manifest, mex);
    if(auto const *field = dyn_cast<FieldDecl>(&member)) {
        TCC_DEBUG(logKey, "Member decl is also a field decl => Nested member");
        auto qt = field->getType();
        if(auto const *record = qt->getAs<RecordType>()) {
            if(field->isAnonymousStructOrUnion()) {
                TCC_DEBUG(logKey, "Member is an anonymous record");
                std::string ut = nameUT(record);
                qn = TCCKey{qn.id() + "::" + ut, qn.scope(), qn.operation()};
            }
            else {
                auto recordName = record->getDecl()->getNameAsString();
                TCC_DEBUG(logKey, "Member record name: {}", recordName);
                qn = TCCKey{qn.id() + "::" + recordName, qn.scope(), qn.operation()};
            }
        }
    }
    else {
        TCC_ERROR(logKey, "Cannot make TCC member node from non-member");
    }
    TCC_DEBUG(logKey, "Member qn: {}", String(qn));

    // TODO check mex vs member type info; additional overload for record types may be needed
    return {
        {&context, &member},
        qn,
        qn.id(),
        Typename(context, mex),
        TypeCategory(mex),
        sourceLocation(mex).printToString(context.getSourceManager()),
        makeTypeMetadata(context, member)
    };
}

auto makeTCCNodeForExpr(TCCManifest &manifest,
        Expr const &e)
    -> TCCNode {

    auto &context = manifest.context();
    auto const logKey = String(context, e);
    TCC_DEBUG_FN(logKey);
    TCC_DEBUG(logKey, "Building TCCNode for expr of type: {}",
            Typename(context, e));

    return {
        {&context, &e},
        qualifiedNameExpr(manifest, e),
        String(context, e),
        Typename(context, e),
        TypeCategory(e),
        sourceLocation(e).printToString(context.getSourceManager()), // check if castexpr is needed
        makeTypeMetadata(context, e)
    };
}

//----------------------

#endif // end TCCCENSUSVISITOR_H
