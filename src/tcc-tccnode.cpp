#ifndef TCC_TCCNODE_H
#define TCC_TCCNODE_H

//module;
#include "tcc-utils.cpp"

#include "logger.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>
#include <clang/AST/RecordLayout.h>

#include <llvm/ADT/PointerUnion.h>
//#include <llvm/ADT/StringMap.h>
#include <llvm/ADT/SmallVector.h>

#include <unordered_map>
#include <string>
#include <optional>

//export module tcc:tccnode;

//import :utils;


/*
struct TCCRecordTypeFieldInfo {
    std::string name_;
    std::string location_;
    CharUnits recordSize_;
    bool isUnion_ = false;
};

struct TCCRecordTypeInfo {
    //using Fields = std::vector<TCCRecordTypeFieldInfo>;
    using Fields = std::vector<TCCRecordTypeInfo>;

    std::string name_;
    std::string location_;
    Fields fields_;
    CharUnits recordSize_;
    bool isUnion_ = false;
};
*/

using namespace clang;

//export namespace tcc {
namespace tcc {

    struct UncheckedASTNodeLink {
        ASTContext const * context_;
        llvm::PointerUnion<Decl const*, Stmt const*> node_;
    };

    struct TypeMetadata {
        bool isPointerType_;
        bool isVoidPointerType_;
        bool isUnionType_;
        std::optional<std::string> fptrType_;
        std::optional<std::string> pointeeType_;
        std::optional<std::string> numericType_;
        std::optional<std::string> charType_;
        std::string unqualifiedType_;
    };

    class TCCKey {
    public:
        explicit TCCKey(std::string id, std::optional<std::string> prefix = {}):
            id_(id),
            prefix_(prefix.value_or("")) {
                //hash_ = prefix_ + "." + tail();
                hash_ = prefix_ + tail();
            }

        /*
        operator std::string_view() const noexcept {
            return hash_;
        }
        */

        auto id() const -> std::string {
            return id_;
        }

        auto prefix() const -> std::string {
            return prefix_;
        }

        auto tail() const -> std::string {
            return "." + id_;
        }

        friend auto operator==(TCCKey const &a, TCCKey const &b) -> bool {
            return a.hash_ == b.hash_;
        }

        friend auto operator<=>(TCCKey const &a, TCCKey const &b) {
            return a.hash_ <=> b.hash_;
        }

        friend auto operator==(TCCKey const &a, std::string const &b) -> bool {
            return a.hash_ == b;
        }

        friend auto operator<=>(TCCKey const &a, std::string const &b) {
            return a.hash_ <=> b;
        }

    private:
        std::string id_ {"BIGBUG"};
        std::string prefix_ {"BUG"};
        std::string hash_ {"BUG2"};
    };

    auto String(TCCKey const &k) -> std::string {
        return k.prefix() + k.tail();
    }

    struct TCCNode {
        using Key = TCCKey;
        using KeyRef = std::string;

        UncheckedASTNodeLink ast_;

        Key id_;
        std::string body_;
        std::string type_;
        std::string category_;
        std::string location_;
        TypeMetadata tmd_;

        auto key() const -> TCCKey const & {
            return id_;
        }

        auto id() const -> KeyRef {
            return String(id_);
        }
    };

    auto String(TCCNode const &n) -> std::string  {
        return std::string(n.id()) + "(" + n.type_ + ")";
    }

    auto getPointedAtType(ASTContext const &context,
            QualType qt,
            unsigned indirections = 0)
        -> std::pair<QualType, unsigned> {

        auto const logKey = Typename(context, qt);
        TCC_DEBUG_FN(logKey);
        if(qt->isPointerType()) {
            TCC_DEBUG(logKey, "Pointer type found");
            return getPointedAtType(context, qt->getPointeeType(), indirections + 1);
        }

        if(qt->isArrayType()) {
            TCC_DEBUG(logKey, "Array type found");
            auto const * arrayt = qt->getAsArrayTypeUnsafe();
            if(!arrayt) {
                TCC_WARN(logKey, "Cannot get array type pointer from array type");
                return {qt, indirections};
            }
            TCC_DEBUG(logKey, "Found array type pointer, indirection + 1");
            return getPointedAtType(context, arrayt->getElementType(), indirections + 1);
        }

        return {qt, indirections};
    }

    auto TypenamePointedAt(ASTContext const &context,
            QualType qt)
        -> std::string {

        auto const logKey = Typename(context, qt);
        TCC_DEBUG_FN(logKey);
        auto [pointeeType, starCount] = getPointedAtType(context, qt);
        std::string stars (starCount, '*');

        auto type = Typename(context, pointeeType.getUnqualifiedType());
        TCC_DEBUG(logKey, "Unqualified pointed-at type: {} {}", type, stars);
        if(stars.empty()) {
            TCC_DEBUG(logKey, "Type: {}", type);
            return type;
        }
        type.append(" " + stars);
        TCC_DEBUG(logKey, "Type: {}", type);
        return type;
    }

    auto getNumericType(ASTContext const &context,
            QualType qt)
        -> std::optional<std::string> {

        auto const logKey = Typename(context, qt);
        TCC_DEBUG_FN(logKey);

        auto [ft, starCount] = getPointedAtType(context, qt);
        std::string stars(starCount, '*');
        TCC_DEBUG(logKey, "Pointed-at type: {} {}", Typename(context, ft), stars);

        if((ft->isIntegerType() || ft->isRealFloatingType())
                && (!ft->isAnyCharacterType())) {
            std::string type = "Number";
            TCC_DEBUG(logKey, "Pointed-at type is number type: Number {}", stars);
            if(stars.empty()) {
                TCC_DEBUG(logKey, "Type: {}", type);
                return type;
            }

            type.append(" " + stars);
            TCC_DEBUG(logKey, "Type: {}", type);
            return type;
        }

        TCC_DEBUG(logKey, "Type is not numeric");
        return {};
    }

    auto getCharType(ASTContext const &context,
            QualType qt)
        -> std::optional<std::string> {

        auto const logKey = Typename(context, qt);
        TCC_DEBUG_FN(logKey);

        auto [ft, starCount] = getPointedAtType(context, qt);
        std::string stars(starCount, '*');
        TCC_DEBUG(logKey, "Pointed-at type: {} {}", Typename(context, ft), stars);

        if(ft->isAnyCharacterType()) {
            std::string type = "Char";
            TCC_DEBUG(logKey, "Pointed-at type is character type: Char {}", stars);
            if(stars.empty()) {
                TCC_DEBUG(logKey, "Type: {}", type);
                return type;
            }

            type.append(" " + stars);
            TCC_DEBUG(logKey, "Type: {}", type);
            return type;
        }
        TCC_DEBUG(logKey, "Type is not char-type");
        return {};
    }

    auto getFunctionPointeeType(ASTContext const &context,
            QualType qt)
        -> std::optional<std::string> {

        auto const logKey = Typename(context, qt);
        TCC_DEBUG_FN(logKey);
        if(qt->isFunctionPointerType()) {
            TCC_DEBUG(logKey, "Function pointer type found");
            return {Typename(context, qt->getPointeeType())};
        }

        if(qt->isFunctionType() || qt->isFunctionProtoType()) {
            TCC_DEBUG(logKey, "Function/Function prototype found");
            return {Typename(context, qt)};
        }

        TCC_DEBUG(logKey, "Not a function type");
        return {};
    }

    auto makeTypeMetadata(ASTContext const &context,
            QualType qt)
        -> TypeMetadata {

        TCC_DEBUG_FN("<qt>");
        return {
            (qt->isPointerType() || qt->isArrayType()),
            qt->isVoidPointerType(),
            qt->isUnionType(),
            getFunctionPointeeType(context, qt),
            TypenamePointedAt(context, qt),
            getNumericType(context, qt),
            getCharType(context, qt),
            Typename(context, qt.getUnqualifiedType())
            //makeTypeInfo(context, qt)
        };
    }

    template<typename ExprOrValueDecl>
    auto makeTypeMetadata(ASTContext const &context,
            ExprOrValueDecl const &node)
        -> TypeMetadata {
        TCC_DEBUG_FN(String(context, node));
        return makeTypeMetadata(context, node.getType());
    }

    auto qualifiedNameForParm(std::string const &fn,
            unsigned parmIndex)
        -> TCCKey {

        auto const logKey = fn + ".$" + std::to_string(parmIndex);
        TCC_DEBUG_FN(logKey);
        return TCCKey("$" + std::to_string(parmIndex), fn);
    }

    auto qualifiedNameForParm(ASTContext const &context,
            FunctionDecl const &fn,
            unsigned parmIndex)
        -> TCCKey {

        return qualifiedNameForParm(String(context, fn), parmIndex);
    }

    auto qualifiedNameValueDecl(ASTContext &context, ValueDecl const &vd)
        -> TCCKey;
    auto qualifiedNameValueDecl(ASTContext &context, ValueDecl const &vd, DeclarationNameInfo const &nameInfo)
        -> TCCKey;
    auto qualifiedNameDRE(ASTContext &context, DeclRefExpr const &dre)
        -> TCCKey;
    auto qualifiedNameExpr(ASTContext &context, Expr const &e)
        -> TCCKey;
    auto qualifiedNameFromPossibleFptrCall(ASTContext &context, CallExpr const &call, unsigned paramPos)
        -> TCCKey;

    auto qualifiedNameValueDecl(ASTContext &context,
            ValueDecl const &vd)
            //DeclarationName const &name)
        -> TCCKey {

        auto logKey = String(context, vd);
        TCC_DEBUG_FN(logKey);

        auto name = vd.getDeclName();
        if(!name) {
            TCC_ERROR(logKey, "Bad name!");
            llvm::errs() << "Bad name for " << String(context, vd) << "\n";
            return TCCKey("~tcckErr(" + String(context, vd) + ")");
        }
        TCC_DEBUG(logKey, "Name: {}", name.getAsString());
        logKey += name.getAsString();

        auto const *fn = getContainerFunctionDecl(context, vd);
        if(!fn) {
            TCC_DEBUG(logKey, "Container function is null");
            return TCCKey {name.getAsString(), "::"};
        }

        if(auto parmIndex = getParameterIndex(*fn, name)) {
            auto qn = qualifiedNameForParm(context, *fn, *parmIndex);
            TCC_DEBUG(logKey, "parmIndex: {}; {}", parmIndex.value(), String(qn));
            return qn;
        }

        TCC_DEBUG(logKey, "parmIndex is nullopt (identifier is not a parm)");
        return TCCKey {
            name.getAsString(),
            getContainerFunction(context, vd)
        };
    }

    auto qualifiedNameValueDecl(ASTContext &context,
            ValueDecl const &vd,
            DeclarationNameInfo const &nameInfo)
        -> TCCKey {
        return qualifiedNameValueDecl(context, vd);
    }

    // TO CHECK TODO
    auto qualifiedNameExpr(ASTContext &context,
            Expr const &e)
        -> TCCKey {

        auto const logKey = String(context, e);
        TCC_DEBUG_FN(logKey);

        auto const *dre = getChild<DeclRefExpr>(e);
        if(!dre) {
            TCC_DEBUG(logKey, "No DRE in expression, stringifying expr for qn");
            return TCCKey{"#tcck(" + String(context, e) + ")"};
        }

        TCC_DEBUG(logKey, "DRE in expression: {}", String(context, *dre));

        // Qualified name of dre should be replaced if the expr contains dre
        if(auto const *vd = dyn_cast<VarDecl>(dre->getFoundDecl())) {
            TCC_DEBUG(logKey, "Decl from DRE: {}", String(context, *vd));
            if(vd->isFunctionPointerType() || vd->isFunctionOrFunctionTemplate()) {
                TCC_DEBUG(logKey, "DRE is fptr type: {}", String(context, *dre));
                // Any operation on function/fptr are non-type changing => qualified name of dre suffices
                return qualifiedNameDRE(context, *dre);
            }
            TCC_DEBUG(logKey, "DRE is not fptr type: {}", String(context, *dre));
        }

        //TCC_DEBUG(logKey, "No decl in dre: {}", String(context, *dre));
        auto dreKey = qualifiedNameDRE(context, *dre);
        auto dreQn = dreKey.id(); //String(dreKey);
        auto dreStr = String(context, *dre);
        TCC_DEBUG(logKey, "DRE: qn({}); str({})", dreQn, dreStr);
        // replace dre name in expr string with dreqn
        auto expr = String(context, e);
        if(auto pos = expr.find(dreStr); pos != std::string::npos) {
            TCC_DEBUG(logKey, "Found identifier '{}' in expr", dreStr);
            expr.replace(pos, dreStr.size(), dreQn);
            TCC_DEBUG(logKey, "Replaced identifier '{}' with qn '{}'", dreStr, dreQn);
        }

        TCC_DEBUG(logKey, "Expr: '{}'", expr);
        TCCKey ek{
            expr,
            dreKey.prefix(), //getContainerFunction(context, e)
        };
        TCC_DEBUG(logKey, "QN: {}", String(ek));
        return ek;
    }

    auto qualifiedNameDRE(ASTContext &context,
            DeclRefExpr const &dre)
        -> TCCKey {

        auto const logKey = String(context, dre);
        TCC_DEBUG_FN(logKey);
        if(auto const *refd = dre.getReferencedDeclOfCallee()) {
            TCC_DEBUG(logKey, "Found referenced decl: {}", String(context, *refd));
            if(auto const *vd = dyn_cast<ValueDecl>(refd)) {
                TCC_DEBUG(logKey, "Referenced decl is value decl");
                return qualifiedNameValueDecl(context, *vd);
            }
        }


        if(auto const *decl = dre.getFoundDecl()) {
            TCC_DEBUG(logKey, "Found decl from dre: {}", String(context, *decl));
            if(auto const *vd = dyn_cast<VarDecl>(decl)) {
                TCC_DEBUG(logKey, "Decl is vardecl");
                return qualifiedNameValueDecl(context, *vd);
            }

            TCC_DEBUG(logKey, "No vardecl in dre decl; checking fptr");
            if(decl->getFunctionType()) {
                auto qn = TCCKey {
                    String(context, *decl),
                    getContainerFunction(context, *decl)
                };
                TCC_DEBUG(logKey, "DRE decl is a function type; qn = fn-name({})", String(qn));
                return qn;
            }

            TCC_WARN(logKey, "DRE decl is neither vardecl nor fptr!");
            auto qn = TCCKey {
                String(context, dre),
                getContainerFunction(context, *decl)
            };
            TCC_WARN(logKey, "Stringified qn: {}", String(qn));
            return qn;
        }

        if(auto const *stmt = dre.getExprStmt()) {
            auto qn = TCCKey {
                String(context, dre),
                getContainerFunction(context, *stmt)
            };
            TCC_DEBUG(logKey, "DRE is a stmt without decl, stringifying dre; qn = {}", String(qn));
            return qn;
        }

        auto errQn = TCCKey {
            "#tcckerr(" + String(context, dre) + ")",
            getContainerFunction(context, dre)
        };
        TCC_ERROR(logKey, "DRE does not have any valid decl or stmt! qn = {}", String(errQn));
        return errQn;
    }

    // TO FIX
    auto qualifiedNameFromPossibleFptrCall(ASTContext &context,
            CallExpr const &call,
            unsigned paramPos)
        -> TCCKey {

        // TODO: add param pos (return + ".$" + to_string(paramPos)
        auto logKey = String(context, call);
        TCC_DEBUG_FN(logKey);

        if(auto const *fptr = getFptrFromFptrCall(context, call)) {
            TCC_DEBUG(logKey, "Found fptr from callexpr: {}", String(context, *fptr));
            auto qn1 = TCCKey {
                "$" + std::to_string(paramPos),
                getContainerFunction(context, *fptr) + "." + String(context, *fptr)
            };
            auto qn2 = TCCKey {
                "$" + std::to_string(paramPos),
                String(context, *fptr)
            };

            TCC_DEBUG(logKey, "qn1 = {}", String(qn1));
            TCC_DEBUG(logKey, "qn2 = {}", String(qn2));
            return qn2;
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
            TCC_DEBUG(logKey, "qn = {}", String(qn));
            return qn;
        }

        TCC_DEBUG(logKey, "Call expr has no callee, stringifying call");
        auto qn = TCCKey {
            String(context, call),
            getContainerFunction(context, call)
        };
        TCC_DEBUG(logKey, "qn = {}", String(qn));
        return qn;
    }
    // END FIX

    auto makeTCCNodeForParam(ASTContext &context,
            ParmVarDecl const &param)
        -> TCCNode {

        TCC_DEBUG(String(context, param), "Building TCCNode for ParmVarDecl type: {}",
                Typename(context, param));

        return {
            {&context, &param},
            qualifiedNameValueDecl(context, param),
            String(context, param),
            Typename(context, param),
            TypeCategory(param),
            sourceLocation(param).printToString(context.getSourceManager()),
            makeTypeMetadata(context, param)
        };
    }

    auto makeTCCNodeForParamFromMaybeFptrCall(ASTContext &context,
            CallExpr const &call,
            unsigned paramPos)
        -> TCCNode {

        auto const logKey = String(context, call) + "@" + std::to_string(paramPos);
        TCC_DEBUG(logKey, "Building TCCNode for maybe fptr call's parameter");

        return TCCNode {
            {}, // No parameter decl
            qualifiedNameFromPossibleFptrCall(context, call, paramPos),
            String(context, call, paramPos),
            "", // No decl from callee => no type
            "",
            sourceLocation(call).printToString(context.getSourceManager()),
            {}  // No decl from callee => no type
        };
    }

    auto makeTCCNodeForParamFromCall(ASTContext &context,
            CallExpr const &call,
            unsigned paramPos)
        -> TCCNode {

        auto const logKey = String(context, call) + "@" + std::to_string(paramPos);
        TCC_DEBUG(logKey, "Building TCCNode for Call Param (index: {})", paramPos);

        auto const *param = getParamDecl(context, call, paramPos);
        if(param) {
            TCC_DEBUG(logKey, "Found param decl");
            return makeTCCNodeForParam(context, *param);
        }

        TCC_DEBUG(logKey, "No param decl found");
        auto const *fn = call.getCallee();  // getCalleDecl() will not work (used in getParamDecl)
        if(fn) {
            // it's possible that call is fptr since getparamdecl failed
            return makeTCCNodeForParamFromMaybeFptrCall(context, call, paramPos);
        }

        TCC_DEBUG(logKey, "Callee in call is also nullptr");
        return TCCNode {
            {},
            TCCKey {std::to_string(paramPos), "ERR:ResolveFunctionFromCallexpr_.$"},
            String(context, call) + ".$" + std::to_string(paramPos),
            "", //Typename(context, *arg),
            "", //TypeCategory(context, *arg),
            sourceLocation(call).printToString(context.getSourceManager()),
            {} //makeTypeMetadata(context, *arg) // TODO Why? type is not defined clearly
        };
    }

    auto makeTCCNodeForVarDecl(ASTContext &context,
            VarDecl const &var)
        -> TCCNode {

        TCC_DEBUG(String(context, var), "Building TCCNode for VarDecl type: {}",
                Typename(context, var));
        return {
            {&context, &var},
            qualifiedNameValueDecl(context, var),
            String(context, var),
            Typename(context, var),
            TypeCategory(var),
            sourceLocation(var).printToString(context.getSourceManager()),
            makeTypeMetadata(context, var)
        };
    }

    auto makeTCCNodeForVarDecl(ASTContext &context,
            ValueDecl const &vald)
        -> TCCNode {

        TCC_DEBUG(String(context, vald), "Building TCCNode for ValueDecl type: {}",
                Typename(context, vald));

        return {
            {&context, &vald},
            qualifiedNameValueDecl(context, vald),
            String(context, vald),
            Typename(context, vald),
            TypeCategory(vald),
            sourceLocation(vald).printToString(context.getSourceManager()),
            makeTypeMetadata(context, vald)
        };
    }

    auto makeTCCNodeForDREDecl(ASTContext &context,
            DeclRefExpr const &dre,
            ValueDecl const &decl)
        -> TCCNode {

        TCC_DEBUG(String(context, dre), "Building TCCNode for Valuedecl type: {}",
                Typename(context, decl));

        return {
            {&context, &decl},
            qualifiedNameValueDecl(context, decl),
            String(context, decl),
            Typename(context, decl),
            TypeCategory(decl),
            sourceLocation(decl).printToString(context.getSourceManager()),
            makeTypeMetadata(context, decl)
        };
    }

    // originalExpr is only used for location in case decl is not found
    auto makeTCCNodeForDRE(ASTContext &context,
            Expr const &originalExpr,
            DeclRefExpr const &dre)
        -> TCCNode {

        auto const logKey = String(context, originalExpr);
        TCC_DEBUG(logKey, "Building TCCNode for DRE in expr with type: {}",
                Typename(context, originalExpr));

        auto const *refd = dre.getReferencedDeclOfCallee();
        if(refd) {
            TCC_DEBUG(logKey, "Building from reference decl");
            auto const *vd = dyn_cast<VarDecl>(refd);
            if(vd) {
                TCC_DEBUG(logKey, "ReferenceDecl is a var decl");
                return makeTCCNodeForVarDecl(context, *vd);
            }
            TCC_ERROR(logKey, "Reference decl has no var decls");
        }

        if(auto const *decl = dre.getDecl()) {
            TCC_DEBUG(logKey, "Building from dre decl"); // Should be same as referenced decl?
            auto const *vd = dyn_cast<VarDecl>(refd);
            if(vd) {
                TCC_DEBUG(logKey, "DreDecl is a var decl");
                return makeTCCNodeForVarDecl(context, *vd);
            }
            TCC_DEBUG(logKey, "Dre decl has no var decls; checking for fptr");
            if(auto const *fp = decl->getAsFunction()) {
                TCC_DEBUG(logKey, "Dre decl is an fptr: {}", String(context, *fp));
                return makeTCCNodeForDREDecl(context, dre, *fp);
            }
            TCC_DEBUG(logKey, "Dre decl is not fptr either");
            return makeTCCNodeForDREDecl(context, dre, *decl);
        }

        if(auto const *stmt_ = dre.getExprStmt()) {
            TCC_DEBUG(logKey, "Building TCCNode from DRE stmt");
            auto const &stmt = *stmt_;
            TCC_DEBUG(String(context, stmt), "Building TCCNode for DRE stmt type: {}",
                    Typename(context, dre));

            return {
                {&context, &dre},
                TCCKey {String(context, stmt), getContainerFunction(context, stmt)},
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
            qualifiedNameDRE(context, dre),
            String(context, dre),
            Typename(context, dre),
            TypeCategory(dre),
            sourceLocation(originalExpr).printToString(context.getSourceManager()),
            makeTypeMetadata(context, dre)
        };
    }

    auto makeTCCNodeForBinarySubExpr(ASTContext &context,
            DeclRefExpr const &dre,
            SourceLocation const &opExprLoc)
        -> TCCNode {

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
                return makeTCCNodeForVarDecl(context, *vd);
            }
            TCC_ERROR(logKey, "Reference decl has no var decls");
        }

        if(auto const *decl = dre.getDecl()) {
            TCC_DEBUG(logKey, "Building from dre decl"); // Should be same as referenced decl?
            auto const *vd = dyn_cast<VarDecl>(refd);
            if(vd) {
                TCC_DEBUG(logKey, "DreDecl is a var decl");
                return makeTCCNodeForVarDecl(context, *vd);
            }
            TCC_DEBUG(logKey, "Dre decl has no var decls; checking for fptr");
            if(auto const *fp = decl->getAsFunction()) {
                TCC_DEBUG(logKey, "Dre decl is an fptr: {}", String(context, *fp));
                return makeTCCNodeForDREDecl(context, dre, *fp);
            }
            TCC_DEBUG(logKey, "Dre decl is not fptr either");
            return makeTCCNodeForDREDecl(context, dre, *decl);
        }

        if(auto const *stmt_ = dre.getExprStmt()) {
            TCC_DEBUG(logKey, "Building TCCNode from DRE stmt");
            auto const &stmt = *stmt_;
            TCC_DEBUG(String(context, stmt), "Building TCCNode for DRE stmt type: {}",
                    Typename(context, dre));

            return {
                {&context, &dre},
                TCCKey {String(context, stmt), getContainerFunction(context, stmt)},
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
            qualifiedNameDRE(context, dre),
            String(context, dre),
            Typename(context, dre),
            TypeCategory(dre),
            opExprLoc.printToString(context.getSourceManager()),
            makeTypeMetadata(context, dre)
        };
    }

    auto makeTCCNodeForMemberExpr(ASTContext &context,
            MemberExpr const &mex,
            ValueDecl const &member)
        -> TCCNode {

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
        auto qn = qualifiedNameExpr(context, mex);
        if(auto const *field = dyn_cast<FieldDecl>(&member)) {
            TCC_DEBUG(logKey, "Member decl is also a field decl => Nested member");
            auto qt = field->getType();
            if(auto const *record = qt->getAs<RecordType>()) {
                if(field->isAnonymousStructOrUnion()) {
                    TCC_DEBUG(logKey, "Member is an anonymous record");
                    std::string ut = nameUT(record);
                    qn = TCCKey{qn.id() + ut, qn.prefix()};
                }
                else {
                    auto recordName = record->getDecl()->getNameAsString();
                    TCC_DEBUG(logKey, "Member record name: {}", recordName);
                    qn = TCCKey{qn.id() + recordName, qn.prefix()};
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

    auto makeTCCNodeForExpr(ASTContext &context,
            Expr const &e)
        -> TCCNode {

        auto const logKey = String(context, e);
        TCC_DEBUG_FN(logKey);
        TCC_DEBUG(logKey, "Building TCCNode for expr of type: {}",
                Typename(context, e));

        return {
            {&context, &e},
            qualifiedNameExpr(context, e),
            String(context, e),
            Typename(context, e),
            TypeCategory(e),
            sourceLocation(e).printToString(context.getSourceManager()), // check if castexpr is needed
            makeTypeMetadata(context, e)
        };
    }

    class TCCNodesDB {
    public:
        using TCCNodeKeyRef = TCCNode::KeyRef;
        using TCCNodes = std::unordered_map<std::string, TCCNode>;

        auto add(TCCNode &&n) -> std::string {
            //if(nodes_.contains(n.id())) {
            //    update(std::move(n));
            //}
            auto key = n.id();
            nodes_.insert({key, std::move(n)});
            return key;
        }

        void append(TCCNodesDB const &db) {
            //nodes_.insert(std::begin(db.nodes_), std::end(db.nodes_));

            for(auto const &[key, val]: db.nodes_) {
                if(nodes_.contains(key)) {
                    // what to do? TODO
                }
                nodes_.insert({key, val});
            }
        }


        void update(TCCNode &&n) {
            // log difference and update vs create version?
        }

        auto contains(TCCNodeKeyRef id) const -> bool {
            return nodes_.contains(id);
        }

        auto get(TCCNodeKeyRef id) const -> TCCNode const & {
            return nodes_.at(id);
        }

        auto getTCCKey(TCCNodeKeyRef id) const -> TCCKey const & {
            return nodes_.at(id).key();
        }

        auto size() const -> std::size_t {
            return nodes_.size();
        }

        auto isUnionType(TCCNodeKeyRef id) const -> bool {
            return nodes_.at(id).tmd_.isUnionType_;
        }

    private:
        TCCNodes nodes_;
    };

} // namespace tcc end

#endif // end TCC_TCCNODE
