#ifndef TCCUTILS_H
#define TCCUTILS_H

//module;

#include "logger.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclBase.h>
#include <clang/AST/ParentMapContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>
#include <clang/Basic/SourceManager.h>

#include <optional>
#include <algorithm>

//export module tcc:utils;

using namespace clang;

//export namespace tcc {
namespace tcc {
    auto getDeclFromFunctionPtr(ASTContext const &, CallExpr const &) -> FunctionDecl const *;

    auto getCalleeDecl(ASTContext const &context, CallExpr const &call) -> FunctionDecl const *;

    auto getParameterIndex(FunctionDecl const &fn,
            DeclarationName const &identifier)
        -> std::optional<unsigned> {

        auto const logKey = fn.getNameAsString();
        TCC_DEBUG_FN(logKey);

        unsigned parmPos = 0;
        auto match = std::find_if(fn.param_begin(), fn.param_end(),
                [&](auto const &parm) -> bool {
                    parmPos++;
                    TCC_DEBUG(logKey, "parmIndex == {}", parmPos);
                    return identifier == parm->getDeclName();
                });

        if(match == fn.param_end() || parmPos > fn.getNumParams()) {
            TCC_DEBUG(logKey, "Identifier did not match with any parm");
            return std::nullopt;
        }
        TCC_DEBUG(logKey, "found parmIndex: {}", parmPos - 1);
        return parmPos - 1;
    }

    auto getParameterIndex(FunctionDecl const &fn,
            DeclarationNameInfo const &identifierInfo)
        -> std::optional<unsigned> {
        return getParameterIndex(fn, identifierInfo.getName());
    }

    template<typename TargetType>
    auto getChild(Decl const &decl) -> TargetType const * {
        if(auto *target = dyn_cast<TargetType>(&decl)) {
            return target;
        }

        if(auto const *dc = dyn_cast<DeclContext>(&decl)) {
            for(auto const *c: dc->decls()) {
                if(auto t = getChild<TargetType>(*c)) {
                    return t;
                }
            }
        }
        return nullptr;
    }

    template<typename TargetType>
    auto getChildFromSub(Expr const *e) -> TargetType const *;

    template<typename TargetType>
    auto getChild(Stmt const &stmt) -> TargetType const * {
        if(auto *target = dyn_cast<TargetType>(&stmt)) {
            return target;
        }

        if(auto const *e = dyn_cast<UnaryOperator>(&stmt)) {
            return getChildFromSub<TargetType>(e->getSubExpr());
        }

        if(auto const *e = dyn_cast<MemberExpr>(&stmt)) {
            return getChildFromSub<TargetType>(e->getBase());
        }

        if(auto const *e = dyn_cast<ArraySubscriptExpr>(&stmt)) {
            return getChildFromSub<TargetType>(e->getBase());
        }

        if(auto const *e = dyn_cast<CastExpr>(&stmt)) {
            return getChildFromSub<TargetType>(e->getSubExpr());
        }

        if(auto const *e = dyn_cast<ParenExpr>(&stmt)) {
            return getChildFromSub<TargetType>(e->getSubExpr());
        }

        return nullptr;
    }

    template<typename TargetType>
    auto getChildFromSub(Expr const *e) -> TargetType const * {
        if(e) {
            return getChild<TargetType>(*e);
        }
        return nullptr;
    }

    auto sourceLocation(Decl const &d) -> SourceLocation {
        return d.getLocation();
    }

    auto sourceLocation(Expr const &e) -> SourceLocation {
        return e.getExprLoc();
    }

    // Useless (only for getContainerFunction(stmt) to compile
    auto sourceLocation(Stmt const &s) -> SourceLocation {
        return s.getBeginLoc();
    }

    auto Typename(ASTContext const &context,
            QualType qt)
        -> std::string {
        auto policy = context.getLangOpts();
        return qt.getAsString(policy);
    }

    template<typename ExprOrValueDecl>
    auto Typename(ASTContext const &context,
            ExprOrValueDecl const &node)
        -> std::string {
        return Typename(context, node.getType());
    }

    auto TypeCategory(QualType const &qt) -> std::string {
        if(qt->isFunctionPointerType()) {
            return "FunctionPoiner";
        }
        return qt->getTypeClassName();
    }

    template<typename ExprOrValueDecl>
    auto TypeCategory(ExprOrValueDecl const &node) -> std::string {
        return TypeCategory(node.getType());
    }

    void prettyPrint(Stmt const &s,
            llvm::raw_ostream &os,
            PrintingPolicy const &policy) {
        s.printPretty(os, nullptr, policy);
    }

    void prettyPrint(Decl const &d,
            llvm::raw_ostream &os,
            PrintingPolicy const &policy) {
        d.print(os, policy, 0, true);
    }

    void prettyPrint(NamedDecl const &d,
            llvm::raw_ostream &os,
            PrintingPolicy const &policy) {
        os << d.getNameAsString();
    }

    void prettyPrint(DeclStmt const &ds,
            llvm::raw_ostream &os,
            PrintingPolicy const &policy) {
        if(ds.isSingleDecl()) {
            auto const *d = ds.getSingleDecl();
            if(auto const *nd = dyn_cast<NamedDecl>(d)) {
                return prettyPrint(*nd, os, policy);
            }
        }
        // unlikely
        for(auto const *d: ds.decls()) {
            os << "<";
            if(auto const *nd = dyn_cast<NamedDecl>(d)) {
                os << nd->getNameAsString() << ", ";
            }
            os << ">";
        }
    }

    void prettyPrint(FunctionProtoType const &proto,
            llvm::raw_ostream &os,
            PrintingPolicy const &policy) {
        os << proto.getReturnType().getAsString(policy);
        os << "()(";
        for(auto const &p: proto.param_types()) {
            os << p.getAsString(policy) << ", ";
        }
        os << ")";
    }

    // Clang throws error if this is defined after templated counterpart for String() on prototypes
    // However, according to overload resolution rules non-template overload should take priority
    auto stringLocation(ASTContext const &context,
            FunctionProtoType const &proto)
        -> std::string {
        return "@{unk}";
    }

    template<typename StmtOrDecl>
    auto stringLocation(ASTContext const &context,
            StmtOrDecl const &node)
        -> std::string {

        auto loc = sourceLocation(node);
        auto const &sm = context.getSourceManager();
        //return sourceLocation.printToString(sm);

        std::string locs;
        std::string fname (sm.getFilename(loc));
        auto line = sm.getSpellingLineNumber(loc);
        auto col = sm.getSpellingColumnNumber(loc);
        locs = fname.substr(fname.find_last_of('/') + 1) + ":"
            + std::to_string(line) + ":" + std::to_string(col);

        return locs;
    }

    template<typename StmtOrDecl>
    auto String(ASTContext const &context,
            StmtOrDecl const &node)
        -> std::string {

        LangOptions defaultOps;
        std::string dump;
        llvm::raw_string_ostream stream(dump);
        auto policy = context.getLangOpts();
        prettyPrint(node, stream, policy);
        //dump += "@[" + stringLocation(context, node) + "]";
        return dump;
    }

    auto String(ASTContext const &context,
            NamedDecl const &decl)
        -> std::string {
        auto str = decl.getNameAsString();
        //str += "@[" + stringLocation(context, decl) + "]";
        return str;
    }

    auto String(ASTContext const &context,
            DeclStmt const &ds)
        -> std::string {

        if(ds.isSingleDecl()) {
            auto const *decl = ds.getSingleDecl();
            if(auto const *nd = dyn_cast<NamedDecl const>(decl)) {
                return String(context, *nd);
            }
            else {
                return String(context, *decl);
            }
        }
        return "(DeclStmt has more than one decl!)";
    }

    auto String(ASTContext const &context,
            FunctionDecl const &fn,
            unsigned parmIndex)
        -> std::string {

        std::string dump;
        dump.reserve(64);
        dump = fn.getNameAsString() + ".$" + std::to_string(parmIndex);

        if(fn.getNumParams() == 0) {
            dump += "()";
            return dump;
        }

        if(auto const * parm = fn.getParamDecl(parmIndex)) {
            auto const parmType = parm->getOriginalType();
            dump += ": " + Typename(context, parmType);

            if(auto const *parmId = parm->getIdentifier()) {
                dump += " " + parmId->getName().str();
                return dump;
            }
            dump += " (@Unknown parm name!)";
            return dump;
        }

        dump += ":(@Unknown parm!)";
        return dump;
    }

    auto getParamDecl(ASTContext const &context,
            CallExpr const &call,
            unsigned paramPos)
        -> ParmVarDecl const * {

        auto const logKey = String(context, call) + ".$" + std::to_string(paramPos);
        TCC_DEBUG_FN(logKey);

        auto const *fn = getCalleeDecl(context, call);
        if(!fn) {
            TCC_DEBUG(logKey, "Cannot find function decl");
            return nullptr;
        }

        if(fn->getNumParams() == 0) {
            TCC_WARN(logKey, "No parameters defined for function");
            return nullptr;
        }

        return fn->getParamDecl(paramPos);
    }

    auto getDeclFromFunctionPtrVar(ASTContext const &context,
            VarDecl const &fp)
        -> FunctionDecl const * {

        auto const logKey = String(context, fp);
        TCC_DEBUG_FN(logKey);
        auto const *init = fp.getInit();
        if(!init) {
            TCC_WARN(logKey, "FPtr initialized with nullptr");
            return nullptr;
        }

        auto const *refd = init->getReferencedDeclOfCallee();
        if(!refd) {
            TCC_WARN(logKey, "FPtr referenced decl is nullptr");
            return nullptr;
        }

        auto const *fn = dyn_cast<FunctionDecl>(refd);
        if(!fn) {
            TCC_WARN(logKey, "dyncast<FunctionDecl> on ref'd decl failed");
            return nullptr;
        }
        return fn;
    }

    auto getDeclFromFunctionPtr(ASTContext const &context,
            CallExpr const &call)
        -> FunctionDecl const * {

        auto const logKey = String(context, call);
        TCC_DEBUG_FN(logKey);
        auto const *callee = call.getCallee();
        if(!callee) {
            TCC_DEBUG(logKey, "Callee is nullptr");
            auto const *dcallee = call.getDirectCallee();
            if(!dcallee) {
                TCC_WARN(logKey, "Direct callee is also nullptr, cannot get fptr decl");
                return nullptr;
            }
            if(auto const *proto = dcallee->getType()
                                 ->getAs<FunctionProtoType>()) {
                TCC_DEBUG(logKey, "Callee was nullptr but confirmed to be func (found prototype: '{}')", String(context, *proto));
                return nullptr;
            }
        }

        auto const *fpdecl = callee->getReferencedDeclOfCallee();
        if(!fpdecl) {
            TCC_WARN(logKey, "Fptr decl (callee referenced decl) is nullptr");
            return nullptr;
        }

        // check for function decl instead? No, maybe a void* so decl from init is needed
        auto const *fp = dyn_cast<VarDecl>(fpdecl);
        if(!fp) {
            TCC_WARN(logKey, "dyncast<VarDecl> on fpdecl failed");
            return nullptr;
        }
        return getDeclFromFunctionPtrVar(context, *fp);
    }

    auto getCalleeDecl(ASTContext const &context,
            CallExpr const &call)
        -> FunctionDecl const * {

        auto const logKey = String(context, call);
        TCC_DEBUG_FN(logKey);
        auto const *fn = call.getDirectCallee();
        if(!fn) {
            TCC_DEBUG(logKey, "direct callee == nullptr");
            fn = getDeclFromFunctionPtr(context, call);
            if(!fn) {
                TCC_DEBUG(logKey, "Could not get callee from declFromFP");
                return nullptr;
            }
        }

        /*
        if(fn->isVariadic()) {
            TCC_DEBUG(logKey, "Callee is variadic and not yet supported!");
            return nullptr;
        }
        */

        return fn;
    }

    auto String(ASTContext const &context,
            CallExpr const &call,
            unsigned argIndex)
        -> std::string {

        auto const *fn = getCalleeDecl(context, call);
        if(!fn) {
            return "(Null function decl!)";
        }
        return String(context, *fn, argIndex);
    }

    auto getFptrFromFptrCall(ASTContext const &context,
            CallExpr const &call)
        -> DeclRefExpr const * {

        auto logKey = String(context, call);
        TCC_DEBUG_FN(logKey);
        if(auto const *fptr = call.IgnoreImplicit()) {
            TCC_DEBUG(logKey, "Found fptr after ignore implicit on callexpr: {}",
                    String(context, *fptr));
            if(auto const *dre = dyn_cast<DeclRefExpr>(fptr)) {
                TCC_DEBUG(logKey, "Found fptr dre: {}", String(context, *dre));
                return dre;
            }
            TCC_DEBUG(logKey, "No DRE found for fptr: {}", String(context, *fptr));
        }
        else {
            TCC_DEBUG(logKey, "No Fptr after ignore implicit on callexpr, trying children.");
        }

        for(auto child: call.children()) {
            if(auto const *ce = dyn_cast<CastExpr>(child)) {
                TCC_DEBUG(logKey, "Cast expr in call child; {}", String(context, *ce));
                if(auto const *dre = dyn_cast<DeclRefExpr>(ce->getSubExpr())) {
                    TCC_DEBUG(logKey, "Found dre in castexpr: {}", String(context, *dre));
                    // maybe check if it is fptr type.
                    return dre;
                }
                TCC_DEBUG(logKey, "No dre in castexpr: {}", String(context, *ce));
            }
            else {
                TCC_DEBUG(logKey, "No cast expr in call child, trying for dre");
                if(auto const *dre = dyn_cast<DeclRefExpr>(child)) {
                    TCC_DEBUG(logKey, "Found dre in call child; {}", String(context, *dre));
                    // maybe check if it is fptr type.
                    return dre;
                }
                TCC_DEBUG(logKey, "No dre in call child");
            }
        }

        TCC_DEBUG(logKey, "Could not find fptr in call expr");
        return nullptr;
    }

    auto getContainerFunctionDecl(ASTContext &context,
            DeclContext const *dc)
        -> FunctionDecl const * {

        //auto const logKey = String(context, vd);
        constexpr auto logKey = "<DeclContext>";
        TCC_DEBUG_FN(logKey);

        TCC_DEBUG(logKey, "Checking if decl context is Function");
        while(dc) {
            if(auto const *fn = dyn_cast<FunctionDecl>(dc)) {
                TCC_DEBUG(logKey, "Found parent function: {}",
                        fn->getNameAsString());
                return fn;
            }
            TCC_DEBUG(logKey, "Checking parent of Decl Context");
            dc = dc->getParent();
        }
        TCC_DEBUG(logKey, "Did not find container function");
        return nullptr;
    }

    auto getContainerFunctionDecl(ASTContext &context,
            Decl const &decl)
        -> FunctionDecl const * {
        auto const logKey = String(context, decl);
        TCC_DEBUG_FN(logKey);
        return getContainerFunctionDecl(context, decl.getDeclContext());
    }

    auto getContainerFunctionDecl(ASTContext &context,
            Stmt const &st)
        -> FunctionDecl const * {
        auto const logKey = String(context, st);
        TCC_DEBUG_FN(logKey);

        auto parents = context.getParents(st);

        TCC_DEBUG(logKey, "Checking parents");
        // Get parent function
        while(parents.size() != 0
                && parents[0].get<FunctionDecl>() == nullptr) {
            if(auto const *stmt = parents[0].get<Stmt>()) {
                TCC_DEBUG(logKey, "Checking parents of: {}", String(context, *stmt));
                parents = context.getParents(*stmt);
            }
            else if(auto const *decl = parents[0].get<Decl>()) {
                TCC_DEBUG(logKey, "Checking parents of: {}", String(context, *decl));
                parents = context.getParents(*decl);
            }
            else {
                TCC_DEBUG(logKey, "Reached non-stmt, non-decl node; cannot continue");
                return nullptr;
            }
        }

        if(parents.empty()) {
            TCC_DEBUG(logKey, "Reached ast root, no parent function found.");
            return nullptr;
        }

        auto const *fn = parents[0].get<FunctionDecl>();
        TCC_DEBUG(logKey, "Parent function is {}", fn->getNameAsString());
        return fn;
    }

    template<typename T>
    auto getContainerFunction(ASTContext &context,
            T const &node)
        -> std::string {

        auto const logKey = String(context, node);
        TCC_DEBUG_FN(logKey);
        auto const *fn = getContainerFunctionDecl(context, node);
        if(!fn) {
            TCC_DEBUG(logKey, "Could not find container; assuming global");
            return "::";
        }

        TCC_DEBUG(logKey, "Found container function: {}", fn->getNameAsString());
        return fn->getNameAsString();
    }

} // namespace tcc end

#endif // end TCC_UTILS
