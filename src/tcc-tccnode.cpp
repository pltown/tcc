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

//class TCCManifest;

    struct UncheckedASTNodeLink {
        ASTContext const * context_;
        llvm::PointerUnion<ValueDecl const*, Stmt const*> node_;
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
        explicit TCCKey(std::string id,
                std::optional<std::string> container = {},
                std::optional<std::string> operation = {}):
            id_(id),
            container_(container.value_or("")),
            operation_(operation.value_or("")) {
                hash_ = operation_ + container_ + tail();
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
            if(operation_.empty()) {
                return container_;
            }
            return ".__" + operation_ + "__." + container_;
        }

        auto scope() const -> std::string {
            return container_;
        }

        auto operation() const -> std::string {
            return operation_;
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
        std::string container_ {"BUG"};
        std::string hash_ {"BUG2"};
        std::string operation_{"BUG3"};
    };

    auto String(TCCKey const &k) -> std::string {
        if(k.prefix().empty()) {
            return k.id();
        }
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
        return std::string(n.id()) + ":" + n.type_;
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
