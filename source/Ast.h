#ifndef EXPR_H
#define EXPR_H
#include <algorithm>
#include <format>
#include <forward_list>
#include <utility>
#include <vector>

#include "Diagnostics.h"
#include "Value.h"
#include "base/bitflags.h"
#include "base/box.h"
#include "parser/Token.h"


// Reference: https://lesleylai.info/en/ast-in-cpp-part-1-variant/
// https://www.foonathan.net/2022/05/recursive-variant-box/
/**
* Container to attach additional metadata to ast nodes
*/

// ###
// Big refactor!
// ###

// TODO: switch to forward lists
// TODO: rethink optionl of unique ptrs

// ### Definitions ###
// AST - abstract syntax tree
// Expr - expression
// Stmt - statment

// Inspiration: https://github.com/v8/v8/blob/main/src/ast/ast.h


#define AST_NODES(V) \
    V(UnaryExpr, unary_expr) \
    V(BinaryExpr, binary_expr) \
    V(CallExpr, call_expr) \
    V(LiteralExpr, literal_expr) \
    V(StringExpr, string_expr) \
    V(VariableExpr, variable_expr) \
    V(GetPropertyExpr, get_property_expr) \
    V(SuperExpr, super_expr) \
    V(BlockExpr, block_expr) \
    V(IfExpr, if_expr) \
    V(LoopExpr, loop_expr) \
    V(WhileExpr, while_expr) \
    V(ForExpr, for_expr) \
    V(ContinueExpr, continue_expr) \
    V(BreakExpr, break_expr) \
    V(ReturnExpr, return_expr) \
    V(ThisExpr, this_expr) \
    V(ObjectExpr, object_expr) \
    V(FunctionDeclaration, function_declaration) \
    V(VariableDeclaration, variable_declaration) \
    V(ExprStmt, expr_stmt) \
    V(ClassDeclaration, class_declaration) \
    V(NativeDeclaration, native_declaration) \
    V(TraitDeclaration, trait_declaration) \
    V(ObjectDeclaration, object_declaration) \
    V(InvalidStmt, invalid_stmt) \
    V(InvalidExpr, invalid_expr)
#define DEFINE_TYPE_ENUM(class_name, type_name) type_name,

enum class NodeKind : std::uint8_t {
    AST_NODES(DEFINE_TYPE_ENUM)
};

#define DECLARE_NODE(class_name, type_name) class class_name;

AST_NODES(DECLARE_NODE)

class AstNode {
public:
    [[nodiscard]] virtual NodeKind kind() const = 0;
    virtual ~AstNode() = default;

    #define TYPE_METHODS(class_name, type_name) \
        [[nodiscard]] virtual bool is_##type_name() const { return kind() == NodeKind::type_name; } \
        [[nodiscard]] class_name* as_##type_name();

    AST_NODES(TYPE_METHODS)

    bite::SourceSpan span;
    explicit AstNode(bite::SourceSpan span) : span(std::move(span)) {}
};

struct LocalDeclarationInfo {
    AstNode* declaration;
    std::int64_t idx;
    bool is_captured;
};

struct GlobalDeclarationInfo {
    AstNode* declaration;
    StringTable::Handle name;
};

using DeclarationInfo = std::variant<LocalDeclarationInfo, GlobalDeclarationInfo>;


using Binding = std::variant<struct NoBinding, struct LocalBinding, struct GlobalBinding, struct UpvalueBinding, struct
                             ParameterBinding, struct MemberBinding, struct PropertyBinding, struct SuperBinding, struct
                             ClassObjectBinding>;

struct NoBinding {};

struct LocalBinding {
    LocalDeclarationInfo* info;
};

struct GlobalBinding {
    GlobalDeclarationInfo* info;
};

struct UpvalueBinding {
    std::int64_t idx;
};

struct ParameterBinding {
    std::int64_t idx;
};

struct MemberBinding {
    StringTable::Handle name;
};

struct ClassObjectBinding {
    bite::box<Binding> class_binding; // performance overhead?
    StringTable::Handle name;
};

struct PropertyBinding {
    StringTable::Handle property;
};

struct SuperBinding {
    StringTable::Handle property;
};


struct Local {
    LocalDeclarationInfo* declaration;
    StringTable::Handle name;
};

struct Locals {
    int64_t locals_count = 0;
    std::vector<std::vector<Local>> scopes;
};

struct GlobalEnviroment {
    bite::unordered_dense::map<StringTable::Handle, GlobalDeclarationInfo*> globals;
    Locals locals;
};

struct UpValue {
    int64_t idx;
    bool local;

    bool operator==(const UpValue& other) const {
        return this->idx == other.idx && this->local == other.local;
    }
};

struct FunctionEnviroment {
    Locals locals;
    std::vector<UpValue> upvalues;
    std::vector<StringTable::Handle> parameters;
};


enum class ClassAttributes: std::uint8_t {
    PRIVATE,
    OVERRIDE,
    ABSTRACT,
    GETTER,
    SETTER,
    OPERATOR,
    size // tracks ClassAttributes size. Must be at end!
};

struct MemberInfo {
    bitflags<ClassAttributes> attributes;
    bite::SourceSpan decl_span;
};

struct ClassEnviroment {
    StringTable::Handle class_name; // TODO: temporary!
    bite::unordered_dense::map<StringTable::Handle, MemberInfo> members;
    ClassEnviroment* class_object_enviroment = nullptr;
};

struct TraitEnviroment {
    bite::unordered_dense::map<StringTable::Handle, MemberInfo> members;
    bite::unordered_dense::map<StringTable::Handle, MemberInfo> requirements;
};


class Stmt : public AstNode {
public:
    explicit Stmt(bite::SourceSpan span) : AstNode(std::move(span)) {}
};

class Expr : public AstNode {
public:
    explicit Expr(bite::SourceSpan span) : AstNode(std::move(span)) {}
};

class Ast {
public:
    std::vector<std::unique_ptr<Stmt>> stmts;
    GlobalEnviroment enviroment;
};


class UnaryExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::unary_expr;
    }

    std::unique_ptr<Expr> expr;
    Token::Type op;

    UnaryExpr(bite::SourceSpan span, std::unique_ptr<Expr> expr, Token::Type op) : Expr(std::move(span)),
        expr(std::move(expr)),
        op(op) {}
};

class BinaryExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::binary_expr;
    }

    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
    Token::Type op;
    Binding binding;

    BinaryExpr(bite::SourceSpan span, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right, Token::Type op) :
        Expr(std::move(span)),
        left(std::move(left)),
        right(std::move(right)),
        op(op) {}
};

class CallExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::call_expr;
    }

    std::unique_ptr<Expr> callee;
    std::forward_list<std::unique_ptr<Expr>> arguments;

    CallExpr(bite::SourceSpan span, std::unique_ptr<Expr> callee, std::forward_list<std::unique_ptr<Expr>> arguments) :
        Expr(std::move(span)),
        callee(std::move(callee)),
        arguments(std::move(arguments)) {}
};

class LiteralExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::literal_expr;
    }

    Value value;

    LiteralExpr(bite::SourceSpan span, Value value) : Expr(std::move(span)),
                                                      value(std::move(value)) {}
};

class StringExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::string_expr;
    }

    std::string string;

    StringExpr(bite::SourceSpan span, std::string string) : Expr(std::move(span)),
                                                            string(std::move(string)) {}
};

class VariableExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::variable_expr;
    }

    Token identifier;
    Binding binding;

    VariableExpr(bite::SourceSpan span, const Token& identifier) : Expr(std::move(span)),
                                                                   identifier(identifier) {}
};

class GetPropertyExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::get_property_expr;
    }

    std::unique_ptr<Expr> left;
    Token property;

    GetPropertyExpr(bite::SourceSpan span, std::unique_ptr<Expr> left, const Token& property) : Expr(std::move(span)),
        left(std::move(left)),
        property(property) {}
};

class SuperExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::super_expr;
    }

    Token method;

    SuperExpr(bite::SourceSpan span, const Token& method) : Expr(std::move(span)),
                                                            method(method) {}
};

class BlockExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::block_expr;
    }

    std::forward_list<std::unique_ptr<Stmt>> stmts;
    std::optional<std::unique_ptr<Expr>> expr;
    std::optional<Token> label;

    BlockExpr(
        bite::SourceSpan span,
        std::forward_list<std::unique_ptr<Stmt>> stmts,
        std::optional<std::unique_ptr<Expr>> expr = {},
        const std::optional<Token>& label = {}
    ) : Expr(std::move(span)),
        stmts(std::move(stmts)),
        expr(std::move(expr)),
        label(label) {}
};

class IfExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::if_expr;
    }

    std::unique_ptr<Expr> condition;
    std::unique_ptr<Expr> then_expr;
    std::optional<std::unique_ptr<Expr>> else_expr;

    IfExpr(
        bite::SourceSpan span,
        std::unique_ptr<Expr> condition,
        std::unique_ptr<Expr> then_expr,
        std::optional<std::unique_ptr<Expr>> else_expr = {}
    ) : Expr(std::move(span)),
        condition(std::move(condition)),
        then_expr(std::move(then_expr)),
        else_expr(std::move(else_expr)) {}
};

class LoopExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::loop_expr;
    }

    std::unique_ptr<BlockExpr> body;
    std::optional<Token> label;

    LoopExpr(bite::SourceSpan span, std::unique_ptr<BlockExpr> body, const std::optional<Token>& label = {}) :
        Expr(std::move(span)),
        body(std::move(body)),
        label(label) {}
};

class WhileExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::while_expr;
    }

    std::unique_ptr<Expr> condition;
    std::unique_ptr<BlockExpr> body;
    std::optional<Token> label;

    WhileExpr(
        bite::SourceSpan span,
        std::unique_ptr<Expr> condition,
        std::unique_ptr<BlockExpr> body,
        const std::optional<Token>& label = {}
    ) : Expr(std::move(span)),
        condition(std::move(condition)),
        body(std::move(body)),
        label(label) {}
};

class BreakExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::break_expr;
    }

    std::optional<std::unique_ptr<Expr>> expr;
    std::optional<Token> label;
    bite::SourceSpan label_span;

    explicit BreakExpr(
        bite::SourceSpan span,
        std::optional<std::unique_ptr<Expr>> expr,
        const std::optional<Token>& label,
        bite::SourceSpan label_span
    ) : Expr(std::move(span)),
        expr(std::move(expr)),
        label(label),
        label_span(std::move(label_span)) {}
};

class ContinueExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::continue_expr;
    }

    std::optional<Token> label;
    bite::SourceSpan label_span;

    ContinueExpr(bite::SourceSpan span, const std::optional<Token>& label, bite::SourceSpan label_span) :
        Expr(std::move(span)),
        label(label),
        label_span(std::move(label_span)) {}
};

class ForExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::for_expr;
    }

    Token name;
    std::unique_ptr<Expr> iterable;
    std::unique_ptr<BlockExpr> body;
    std::optional<Token> label;
    DeclarationInfo info; // For iterator variable

    ForExpr(
        bite::SourceSpan span,
        const Token& name,
        std::unique_ptr<Expr> iterable,
        std::unique_ptr<BlockExpr> body,
        const std::optional<Token>& label = {}
    ) : Expr(std::move(span)),
        name(name),
        iterable(std::move(iterable)),
        body(std::move(body)),
        label(label) {}
};

class ReturnExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::return_expr;
    }

    std::optional<std::unique_ptr<Expr>> value;

    explicit ReturnExpr(bite::SourceSpan span, std::optional<std::unique_ptr<Expr>> value) : Expr(std::move(span)),
        value(std::move(value)) {}
};

class ThisExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::this_expr;
    }

    explicit ThisExpr(bite::SourceSpan span) : Expr(std::move(span)) {}
};


class FunctionDeclaration final : public Stmt {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::function_declaration;
    }

    Token name;
    std::vector<Token> params;
    std::optional<std::unique_ptr<Expr>> body;
    DeclarationInfo info;
    FunctionEnviroment enviroment {};

    FunctionDeclaration(
        bite::SourceSpan span,
        const Token& name,
        std::vector<Token> params,
        std::optional<std::unique_ptr<Expr>> body = {}
    ) : Stmt(std::move(span)),
        name(name),
        params(std::move(params)),
        body(std::move(body)) {}
};

class VariableDeclaration final : public Stmt {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::variable_declaration;
    }

    Token name;
    std::optional<std::unique_ptr<Expr>> value;
    DeclarationInfo info;

    VariableDeclaration(bite::SourceSpan span, const Token& name, std::optional<std::unique_ptr<Expr>> value = {}) :
        Stmt(std::move(span)),
        name(name),
        value(std::move(value)) {}
};

class ExprStmt final : public Stmt {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::expr_stmt;
    }

    std::unique_ptr<Expr> value;

    ExprStmt(bite::SourceSpan span, std::unique_ptr<Expr> expr) : Stmt(std::move(span)),
                                                                  value(std::move(expr)) {}
};

// TODO: rethink those
struct Field {
    std::unique_ptr<VariableDeclaration> variable;
    bitflags<ClassAttributes> attributes;
    bite::SourceSpan span;
};

struct Method {
    std::unique_ptr<FunctionDeclaration> function;
    bitflags<ClassAttributes> attributes;
    bite::SourceSpan decl_span;
};

struct Constructor {
    bool has_super;
    std::forward_list<std::unique_ptr<Expr>> super_arguments;
    std::unique_ptr<FunctionDeclaration> function;
    bite::SourceSpan superconstructor_call_span;
    bite::SourceSpan decl_span;
};

class ObjectExpr;

// TODO: rethink:
// TODO: insane...
// find a better way to write this
struct UsingStmtMemeberDeclaration {
    StringTable::Handle original_name;
    StringTable::Handle aliased_name;
    bitflags<ClassAttributes> attributes;
};

struct UsingStmtItem {
    Token name;
    bite::SourceSpan span;
    std::vector<Token> exclusions;
    std::vector<std::pair<Token, Token>> aliases;
    std::vector<UsingStmtMemeberDeclaration> declarations; // TODO
    Binding binding;
};

// Is not actually a statement?
struct UsingStmt {
    std::vector<UsingStmtItem> items;
};

/**
 * Shared fields between ObjectExpr and ClassStmt
 */
struct StructureBody {
    std::vector<Method> methods;
    std::vector<Field> fields;
    std::optional<std::unique_ptr<ObjectExpr>> class_object;
    std::vector<UsingStmt> using_statements;
    std::optional<Constructor> constructor; // TODO: remove this optional
};

class ClassDeclaration final : public Stmt {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::class_declaration;
    }

    Token name;
    bite::SourceSpan name_span; // TODO: temp
    std::optional<Token> super_class;
    bite::SourceSpan super_class_span; // TODO: temp
    StructureBody body;
    bool is_abstract = false;
    DeclarationInfo info;
    ClassEnviroment enviroment;
    Binding superclass_binding;

    ClassDeclaration(
        bite::SourceSpan span,
        const Token& name,
        bite::SourceSpan name_span,
        const std::optional<Token>& super_class,
        bite::SourceSpan super_class_span,
        StructureBody body,
        bool is_abstract
    ) : Stmt(std::move(span)),
        name(name),
        name_span(std::move(name_span)),
        super_class(super_class),
        super_class_span(std::move(super_class_span)),
        body(std::move(body)),
        is_abstract(is_abstract) {}
};

class ObjectExpr : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::object_expr;
    }

    StructureBody body;
    std::optional<Token> super_class;
    std::forward_list<std::unique_ptr<Expr>> superclass_arguments;
    bite::SourceSpan name_span; // TODO: temp
    bite::SourceSpan super_class_span; // TODO: temp
    ClassEnviroment class_enviroment;
    Binding superclass_binding;

    ObjectExpr(
        bite::SourceSpan span,
        StructureBody body,
        const std::optional<Token>& super_class,
        std::forward_list<std::unique_ptr<Expr>> superclass_arguments,
        bite::SourceSpan name_span,
        bite::SourceSpan super_class_span
    ) : Expr(std::move(span)),
        body(std::move(body)),
        super_class(super_class),
        superclass_arguments(std::move(superclass_arguments)),
        name_span(std::move(name_span)),
        super_class_span(std::move(super_class_span)) {}
};

class NativeDeclaration final : public Stmt {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::native_declaration;
    }

    Token name;
    DeclarationInfo info;

    NativeDeclaration(bite::SourceSpan span, const Token& name) : Stmt(std::move(span)),
                                                                  name(name) {}
};

class ObjectDeclaration final : public Stmt {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::object_declaration;
    }

    Token name;
    std::unique_ptr<Expr> object;
    DeclarationInfo info;

    ObjectDeclaration(bite::SourceSpan span, const Token& name, std::unique_ptr<Expr> object) : Stmt(std::move(span)),
        name(name),
        object(std::move(object)) {}
};

class TraitDeclaration final : public Stmt {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::trait_declaration;
    }

    Token name;
    std::vector<Method> methods;
    std::vector<Field> fields;
    std::vector<UsingStmt> using_stmts;
    DeclarationInfo info;
    TraitEnviroment enviroment;

    TraitDeclaration(
        bite::SourceSpan span,
        const Token& name,
        std::vector<Method> methods,
        std::vector<Field> fields,
        std::vector<UsingStmt> using_stmts
    ) : Stmt(std::move(span)),
        name(name),
        methods(std::move(methods)),
        fields(std::move(fields)),
        using_stmts(std::move(using_stmts)) {}
};

class InvalidStmt final : public Stmt {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::invalid_stmt;
    }

    explicit InvalidStmt(bite::SourceSpan span) : Stmt(std::move(span)) {}
};

class InvalidExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::invalid_expr;
    }

    explicit InvalidExpr(bite::SourceSpan span) : Expr(std::move(span)) {}
};

#define DECLARE_AS_METHOD(class_name, type_name) inline class_name* AstNode::as_##type_name() { return static_cast<class_name*>(this);}
AST_NODES(DECLARE_AS_METHOD)

// template <typename T>
// class AstNode {
// public:
//     explicit AstNode(T&& value, const std::int64_t id, const bite::SourceSpan& span) : id(id),
//         span(span),
//         ptr(new T(std::move(value))) {}
//
//     AstNode(const AstNode& value) = delete;
//     AstNode& operator=(const AstNode&) = delete;
//
//     T& operator*() { return *ptr; }
//     const T& operator*() const { return *ptr; }
//
//     T* operator->() { return ptr.get(); }
//     const T* operator->() const { return ptr.get(); }
//
//     AstNode(AstNode&& other) noexcept = default;
//     AstNode& operator=(AstNode&& other) noexcept = default;
//
//     ~AstNode() = default;
//
//     std::int64_t id;
//     bite::SourceSpan span;
//
// private:
//     std::unique_ptr<T> ptr;
// };
//
// using Expr = std::variant<AstNode<struct LiteralExpr>, AstNode<struct StringLiteral>, AstNode<struct UnaryExpr>, AstNode
//                           <struct BinaryExpr>, AstNode<struct VariableExpr>, AstNode<struct CallExpr>, AstNode<struct
//                               GetPropertyExpr>, AstNode<struct SuperExpr>, AstNode<struct BlockExpr>, AstNode<struct
//                               IfExpr>, AstNode<struct LoopExpr>, AstNode<struct BreakExpr>, AstNode<struct ContinueExpr>
//                           , AstNode<struct WhileExpr>, AstNode<struct ForExpr>, AstNode<struct ReturnExpr>, AstNode<
//                               struct ThisExpr>, AstNode<struct ObjectExpr>, AstNode<struct InvalidExpr>>;
//
// using Stmt = std::variant<AstNode<struct VarStmt>, AstNode<struct ExprStmt>, AstNode<struct FunctionStmt>, AstNode<
//                               struct ClassStmt>, AstNode<struct NativeStmt>, AstNode<struct ObjectStmt>, AstNode<struct
//                               TraitStmt>, AstNode<struct UsingStmt>, AstNode<struct InvalidStmt>>;
//
// // TODO: this feels wrong...
// using ExprPtr = std::variant<AstNode<LiteralExpr>*, AstNode<StringLiteral>*, AstNode<UnaryExpr>*, AstNode<BinaryExpr>*,
//                              AstNode<VariableExpr>*, AstNode<CallExpr>*, AstNode<GetPropertyExpr>*, AstNode<SuperExpr>*,
//                              AstNode<BlockExpr>*, AstNode<IfExpr>*, AstNode<LoopExpr>*, AstNode<BreakExpr>*, AstNode<
//                                  ContinueExpr>*, AstNode<WhileExpr>*, AstNode<ForExpr>*, AstNode<ReturnExpr>*, AstNode<
//                                  ThisExpr>*, AstNode<ObjectExpr>*, AstNode<InvalidExpr>*>;
//
// using StmtPtr = std::variant<AstNode<VarStmt>*, AstNode<ExprStmt>*, AstNode<FunctionStmt>*, AstNode<ClassStmt>*, AstNode
//                              <NativeStmt>*, AstNode<ObjectStmt>*, AstNode<TraitStmt>*, AstNode<UsingStmt>*, AstNode<
//                                  InvalidStmt>*>;
//
// using Node = std::variant<StmtPtr, ExprPtr>;
//
// struct LocalDeclarationInfo {
//     Node declaration;
//     std::int64_t idx;
//     bool is_captured;
// };
//
// struct GlobalDeclarationInfo {
//     Node declaration;
//     StringTable::Handle name;
// };
//
// using DeclarationInfo = std::variant<LocalDeclarationInfo, GlobalDeclarationInfo>;
//
//
// using Binding = std::variant<struct NoBinding, struct LocalBinding, struct GlobalBinding, struct UpvalueBinding, struct
//                              ParameterBinding, struct MemberBinding, struct PropertyBinding, struct SuperBinding, struct
//                              ClassObjectBinding>;
//
// struct NoBinding {};
//
// struct LocalBinding {
//     LocalDeclarationInfo* info;
// };
//
// struct GlobalBinding {
//     GlobalDeclarationInfo* info;
// };
//
// struct UpvalueBinding {
//     std::int64_t idx;
// };
//
// struct ParameterBinding {
//     std::int64_t idx;
// };
//
// struct MemberBinding {
//     StringTable::Handle name;
// };
//
// struct ClassObjectBinding {
//     bite::box<Binding> class_binding; // performance overhead?
//     StringTable::Handle name;
// };
//
// struct PropertyBinding {
//     StringTable::Handle property;
// };
//
// struct SuperBinding {
//     StringTable::Handle property;
// };
//
//
// struct Local {
//     LocalDeclarationInfo* declaration;
//     StringTable::Handle name;
// };
//
// struct Locals {
//     int64_t locals_count = 0;
//     std::vector<std::vector<Local>> scopes;
// };
//
// struct GlobalEnviroment {
//     bite::unordered_dense::map<StringTable::Handle, GlobalDeclarationInfo*> globals;
//     Locals locals;
// };
//
// struct UpValue {
//     int64_t idx;
//     bool local;
//
//     bool operator==(const UpValue& other) const {
//         return this->idx == other.idx && this->local == other.local;
//     }
// };
//
// struct FunctionEnviroment {
//     Locals locals;
//     std::vector<UpValue> upvalues;
//     std::vector<StringTable::Handle> parameters;
// };
//
//
// enum class ClassAttributes: std::uint8_t {
//     PRIVATE,
//     OVERRIDE,
//     ABSTRACT,
//     GETTER,
//     SETTER,
//     OPERATOR,
//     size // tracks ClassAttributes size. Must be at end!
// };
//
// struct MemberInfo {
//     bitflags<ClassAttributes> attributes;
//     bite::SourceSpan decl_span;
// };
//
// struct ClassEnviroment {
//     StringTable::Handle class_name; // TODO: temporary!
//     bite::unordered_dense::map<StringTable::Handle, MemberInfo> members;
//     ClassEnviroment* class_object_enviroment = nullptr;
// };
//
// struct TraitEnviroment {
//     bite::unordered_dense::map<StringTable::Handle, MemberInfo> members;
//     bite::unordered_dense::map<StringTable::Handle, MemberInfo> requirements;
// };
//
// class Ast {
// public:
//     std::vector<Stmt> statements;
//     GlobalEnviroment enviroment;
//     std::size_t current_id = 0;
//
//     template <typename T, typename... Args>
//     AstNode<T> make_node(const bite::SourceSpan& span, Args&&... args) {
//         return AstNode<T>(T(std::forward<Args>(args)...), current_id++, span);
//     }
// };
//
//
// struct UnaryExpr {
//     Expr expr;
//     Token::Type op;
// };
//
// struct BinaryExpr {
//     Expr left;
//     Expr right;
//     Token::Type op;
//     Binding binding; // TODO: temp
// };
//
// struct CallExpr {
//     Expr callee;
//     std::vector<Expr> arguments;
// };
//
// struct LiteralExpr {
//     Value literal;
// };
//
// struct StringLiteral {
//     std::string string;
// };
//
// struct VariableExpr {
//     Token identifier;
//     Binding binding = NoBinding();
// };
//
// struct GetPropertyExpr {
//     Expr left; // Todo: better name?
//     Token property;
// };
//
// struct SuperExpr {
//     Token method;
// };
//
// struct BlockExpr {
//     std::vector<Stmt> stmts;
//     std::optional<Expr> expr;
//     std::optional<Token> label;
// };
//
// struct IfExpr {
//     Expr condition;
//     Expr then_expr;
//     std::optional<Expr> else_expr;
// };
//
// struct LoopExpr {
//     AstNode<BlockExpr> body;
//     std::optional<Token> label;
// };
//
// struct WhileExpr {
//     Expr condition;
//     AstNode<BlockExpr> body;
//     std::optional<Token> label;
// };
//
// struct BreakExpr {
//     std::optional<Expr> expr;
//     std::optional<Token> label;
//     bite::SourceSpan label_span;
// };
//
// struct ContinueExpr {
//     std::optional<Token> label;
//     bite::SourceSpan label_span;
// };
//
// struct ForExpr {
//     Token name;
//     Expr iterable;
//     AstNode<BlockExpr> body;
//     std::optional<Token> label;
//     DeclarationInfo info; // For iterator variable
// };
//
// struct ReturnExpr {
//     std::optional<Expr> value;
// };
//
//
// struct ThisExpr {};
//
//
// struct FunctionStmt {
//     Token name;
//     std::vector<Token> params;
//     std::optional<Expr> body;
//     DeclarationInfo info;
//     FunctionEnviroment enviroment {};
// };
//
// struct VarStmt {
//     Token name;
//     std::optional<Expr> value;
//     DeclarationInfo info;
// };
//
// struct ExprStmt {
//     Expr expr;
// };
//
// struct Field {
//     AstNode<VarStmt> variable;
//     bitflags<ClassAttributes> attributes;
//     bite::SourceSpan span;
// };
//
// struct Method {
//     AstNode<FunctionStmt> function;
//     bitflags<ClassAttributes> attributes;
//     bite::SourceSpan decl_span;
// };
//
// struct Constructor {
//     bool has_super;
//     std::vector<Expr> super_arguments;
//     AstNode<FunctionStmt> function;
//     bite::SourceSpan superconstructor_call_span;
//     bite::SourceSpan decl_span;
// };
//
// /**
//  * Shared fields between ObjectExpr and ClassStmt
//  */
// struct StructureBody {
//     std::vector<Method> methods;
//     std::vector<Field> fields;
//     std::optional<AstNode<ObjectExpr>> class_object;
//     std::vector<AstNode<UsingStmt>> using_statements;
//     std::optional<Constructor> constructor; // TODO: remove this optional
// };
//
// struct ObjectExpr {
//     StructureBody body;
//     std::optional<Token> super_class;
//     std::vector<Expr> superclass_arguments;
//     bite::SourceSpan name_span; // TODO: temp
//     bite::SourceSpan super_class_span; // TODO: temp
//     ClassEnviroment class_enviroment;
//     Binding superclass_binding = NoBinding();
// };
//
// struct ClassStmt {
//     Token name;
//     bite::SourceSpan name_span; // TODO: temp
//     std::optional<Token> super_class;
//     bite::SourceSpan super_class_span; // TODO: temp
//     StructureBody body;
//     bool is_abstract = false;
//     DeclarationInfo info;
//     ClassEnviroment enviroment;
//     Binding superclass_binding = NoBinding();
// };
//
// struct NativeStmt {
//     Token name;
//     DeclarationInfo info;
// };
//
// struct ObjectStmt {
//     Token name;
//     Expr object;
//     DeclarationInfo info;
// };
//
// struct TraitStmt {
//     Token name;
//     std::vector<Method> methods;
//     std::vector<Field> fields;
//     std::vector<AstNode<UsingStmt>> using_stmts;
//     DeclarationInfo info;
//     TraitEnviroment enviroment;
// };
//
//
// // TODO: insane...
// // find a better way to write this
// struct UsingStmtMemeberDeclaration {
//     StringTable::Handle original_name;
//     StringTable::Handle aliased_name;
//     bitflags<ClassAttributes> attributes;
// };
//
// struct UsingStmtItem {
//     Token name;
//     bite::SourceSpan span;
//     std::vector<Token> exclusions;
//     std::vector<std::pair<Token, Token>> aliases;
//     std::vector<UsingStmtMemeberDeclaration> declarations; // TODO
//     Binding binding = NoBinding();
// };
//
// struct UsingStmt {
//     std::vector<UsingStmtItem> items;
// };
//
// // markers for invalid ast nodes
// struct InvalidExpr {};
//
// struct InvalidStmt {};
//
// inline bite::SourceSpan get_span(const Stmt& statment) {
//     return std::visit(overloaded { [](const auto& stmt) { return stmt.span; } }, statment);
// }
//
// inline bite::SourceSpan get_span(const Expr& expression) {
//     return std::visit(overloaded { [](const auto& expr) { return expr.span; } }, expression);
// }
//
// inline bite::SourceSpan get_span(const StmtPtr& statment) {
//     return std::visit(overloaded { [](const auto* stmt) { return stmt->span; } }, statment);
// }
//
// inline bite::SourceSpan get_span(const ExprPtr& expression) {
//     return std::visit(overloaded { [](const auto* expr) { return expr->span; } }, expression);
// }
//
// inline bite::SourceSpan get_span(const Node& node) {
//     return std::holds_alternative<StmtPtr>(node)
//                ? get_span(std::get<StmtPtr>(node))
//                : get_span(std::get<ExprPtr>(node));
// }

// inline std::string expr_to_string(const Expr& expr, std::string_view source);
//
// inline std::string stmt_to_string(const Stmt& stmt, std::string_view source) {
//     return std::visit(
//         overloaded {
//             [source](const VarStmt& stmt) {
//                 return std::format("(define {} {})", *stmt.name.string, expr_to_string(*stmt.value, source));
//             },
//             [source](const ExprStmt& stmt) {
//                 return expr_to_string(*stmt.expr, source);
//             },
//             [source](const FunctionStmt& stmt) {
//                 std::string parameters_string;
//                 for (auto& param : stmt.params) {
//                     parameters_string += *param.string;
//                     if (*param.string != *stmt.params.back().string) {
//                         parameters_string += " ";
//                     }
//                 }
//                 return std::format(
//                     "(fun {} ({}) {})",
//                     *stmt.name.string,
//                     parameters_string,
//                     expr_to_string(*stmt.body, source)
//                 );
//             },
//             [source](const ClassStmt& stmt) {
//                 return std::format("(class {})", *stmt.name.string);
//             },
//             [source](const NativeStmt& stmt) {
//                 return std::format("(native {})", *stmt.name.string);
//             },
//             [source](const MethodStmt&) {
//                 return std::string("method");
//             },
//             [source](const FieldStmt&) {
//                 return std::string("field");
//             },
//             [](const ConstructorStmt& stmt) {
//                 return std::string("constructor"); // TODO: implement!
//             },
//             [](const ObjectStmt& stmt) {
//                 return std::string("object"); // TODO: implement!
//             },
//             [](const TraitStmt& stmt) {
//                 return std::string("trait"); // TODO: implement!
//             },
//             [](const UsingStmt& stmt) {
//                 return std::string("using"); // TODO: implement!
//             }
//         },
//         stmt
//     );
// }
//
// inline std::string expr_to_string(const Expr& expr, std::string_view source) {
//     return std::visit(
//         overloaded {
//             [](const LiteralExpr& expr) {
//                 return expr.literal.to_string();
//             },
//             [source](const UnaryExpr& expr) {
//                 return std::format("({} {})", Token::type_to_string(expr.op), expr_to_string(*expr.expr, source));
//             },
//             [source](const BinaryExpr& expr) {
//                 return std::format(
//                     "({} {} {})",
//                     Token::type_to_string(expr.op),
//                     expr_to_string(*expr.left, source),
//                     expr_to_string(*expr.right, source)
//                 );
//             },
//             [](const StringLiteral& expr) {
//                 return std::format("\"{}\"", expr.string);
//             },
//             [source](const VariableExpr& expr) {
//                 return *expr.identifier.string;
//             },
//             [source](const CallExpr& expr) {
//                 std::string arguments_string;
//                 for (auto& arg : expr.arguments) {
//                     arguments_string += " ";
//                     arguments_string += expr_to_string(*arg, source);
//                 }
//                 return std::format("(call {}{})", expr_to_string(*expr.callee, source), arguments_string);
//             },
//             [source](const GetPropertyExpr& expr) {
//                 return std::format("({}.{})", expr_to_string(*expr.left, source), *expr.property.string);
//             },
//             [source](const SuperExpr& expr) {
//                 return std::format("super.{}", *expr.method.string);
//             },
//             [source](const BlockExpr& stmt) {
//                 std::string blocks;
//                 for (auto& s : stmt.stmts) {
//                     blocks += stmt_to_string(*s, source);
//                     if (s != stmt.stmts.back())
//                         blocks += " ";
//                 }
//                 return std::format("({})", blocks);
//             },
//             [source](const IfExpr& expr) {
//                 if (expr.else_expr != nullptr) {
//                     return std::format(
//                         "(if {} {} {})",
//                         expr_to_string(*expr.condition, source),
//                         expr_to_string(*expr.then_expr, source),
//                         expr_to_string(*expr.else_expr, source)
//                     );
//                 }
//                 return std::format(
//                     "(if {} {})",
//                     expr_to_string(*expr.condition, source),
//                     expr_to_string(*expr.then_expr, source)
//                 );
//             },
//             [source](const LoopExpr& expr) {
//                 return std::format("(loop {})", expr_to_string(*expr.body, source));
//             },
//             [](const BreakExpr& expr) {
//                 return std::string("break");
//             },
//             [](const ContinueExpr& expr) {
//                 return std::string("continue");
//             },
//             [source](const WhileExpr& expr) {
//                 return std::format(
//                     "(while {} {})",
//                     expr_to_string(*expr.condition, source),
//                     expr_to_string(*expr.body, source)
//                 );
//             },
//             [source](const ForExpr& expr) {
//                 return std::string("for"); // TODO: implement
//             },
//             [source](const ReturnExpr& stmt) {
//                 if (stmt.value != nullptr) {
//                     return std::format("(return {})", expr_to_string(*stmt.value, source));
//                 }
//                 return std::string("retrun");
//             },
//             [](const ThisExpr& expr) {
//                 return std::string("this");
//             },
//             [](const ObjectExpr& expr) {
//                 return std::string("object"); // TODO: implement!
//             },
//         },
//         expr
//     );
// }

#endif //EXPR_H
