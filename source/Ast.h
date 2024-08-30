#ifndef EXPR_H
#define EXPR_H
#include <algorithm>
#include <format>
#include <vector>
#include <utility>

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
    V(SafeCallExpr, safe_call_expr) \
    V(LiteralExpr, literal_expr) \
    V(StringExpr, string_expr) \
    V(VariableExpr, variable_expr) \
    V(GetPropertyExpr, get_property_expr) \
    V(SafeGetPropertyExpr, safe_get_property_expr) \
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
    V(AnonymousFunctionExpr, anonymous_function_expr) \
    V(FunctionDeclaration, function_declaration) \
    V(VariableDeclaration, variable_declaration) \
    V(ExprStmt, expr_stmt) \
    V(ClassDeclaration, class_declaration) \
    V(TraitDeclaration, trait_declaration) \
    V(ObjectDeclaration, object_declaration) \
    V(ImportStmt, import_stmt) \
    V(ModuleStmt, module_stmt) \
    V(ModuleResolutionExpr, module_resolution_expr) \
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
    explicit AstNode(const bite::SourceSpan& span) : span(span) {}
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
    LocalDeclarationInfo* info;
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

class Declaration;

struct Local {
    Declaration* declaration;
    StringTable::Handle name;
};

struct Locals {
    int64_t locals_count = 0;
    std::vector<std::vector<Local>> scopes;
};

struct Global {
    Declaration* declaration = nullptr;
    bool is_defined = false;
};


struct GlobalEnviroment {
    bite::unordered_dense::map<StringTable::Handle, Global> globals;
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
    std::vector<std::pair<StringTable::Handle, bite::SourceSpan>> parameters;
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
    bite::SourceSpan parent_span;
    std::optional<bite::SourceSpan> inclusion_span;
};

struct ClassEnviroment {
    StringTable::Handle class_name; // TODO: temporary!
    bite::unordered_dense::map<StringTable::Handle, MemberInfo> members;
    ClassEnviroment* metaobject_enviroment = nullptr;
};

struct TraitEnviroment {
    bite::unordered_dense::map<StringTable::Handle, MemberInfo> members;
    bite::unordered_dense::map<StringTable::Handle, MemberInfo> requirements;
};


class Stmt : public AstNode {
public:
    explicit Stmt(const bite::SourceSpan& span) : AstNode(span) {}
};

class Expr : public AstNode {
public:
    explicit Expr(const bite::SourceSpan& span) : AstNode(span) {}
};

class Declaration : public Stmt {
public:
    Token name;
    DeclarationInfo info;

    explicit Declaration(const bite::SourceSpan& span, const Token& name) : Stmt(span),
                                                                            name(name) {}
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

    UnaryExpr(const bite::SourceSpan& span, std::unique_ptr<Expr> expr, Token::Type op) : Expr(span),
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

    BinaryExpr(const bite::SourceSpan& span, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right, Token::Type op) :
        Expr(span),
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
    std::vector<std::unique_ptr<Expr>> arguments;

    CallExpr(const bite::SourceSpan& span, std::unique_ptr<Expr> callee, std::vector<std::unique_ptr<Expr>> arguments) :
        Expr(span),
        callee(std::move(callee)),
        arguments(std::move(arguments)) {}
};

class SafeCallExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::safe_call_expr;
    }

    std::unique_ptr<Expr> callee;
    std::vector<std::unique_ptr<Expr>> arguments;

    SafeCallExpr(
        const bite::SourceSpan& span,
        std::unique_ptr<Expr> callee,
        std::vector<std::unique_ptr<Expr>> arguments
    ) : Expr(span),
        callee(std::move(callee)),
        arguments(std::move(arguments)) {}
};

class LiteralExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::literal_expr;
    }

    Value value;

    LiteralExpr(const bite::SourceSpan& span, Value value) : Expr(span),
                                                             value(std::move(value)) {}
};

class StringExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::string_expr;
    }

    std::string string;

    StringExpr(const bite::SourceSpan& span, std::string string) : Expr(span),
                                                                   string(std::move(string)) {}
};

class VariableExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::variable_expr;
    }

    Token identifier;
    Binding binding;

    VariableExpr(const bite::SourceSpan& span, const Token& identifier) : Expr(span),
                                                                          identifier(identifier) {}
};

class GetPropertyExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::get_property_expr;
    }

    std::unique_ptr<Expr> left;
    Token property;

    GetPropertyExpr(const bite::SourceSpan& span, std::unique_ptr<Expr> left, const Token& property) : Expr(span),
        left(std::move(left)),
        property(property) {}
};

class SafeGetPropertyExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::safe_get_property_expr;
    }

    std::unique_ptr<Expr> left;
    Token property;

    SafeGetPropertyExpr(const bite::SourceSpan& span, std::unique_ptr<Expr> left, const Token& property) : Expr(span),
        left(std::move(left)),
        property(property) {}
};


class SuperExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::super_expr;
    }

    Token method;

    SuperExpr(const bite::SourceSpan& span, const Token& method) : Expr(span),
                                                                   method(method) {}
};

class BlockExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::block_expr;
    }

    std::vector<std::unique_ptr<Stmt>> stmts;
    std::unique_ptr<Expr> expr;
    std::optional<Token> label;

    BlockExpr(
        const bite::SourceSpan& span,
        std::vector<std::unique_ptr<Stmt>> stmts,
        std::unique_ptr<Expr> expr = {},
        const std::optional<Token>& label = {}
    ) : Expr(span),
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
    std::unique_ptr<Expr> else_expr;

    IfExpr(
        const bite::SourceSpan& span,
        std::unique_ptr<Expr> condition,
        std::unique_ptr<Expr> then_expr,
        std::unique_ptr<Expr> else_expr = {}
    ) : Expr(span),
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

    LoopExpr(const bite::SourceSpan& span, std::unique_ptr<BlockExpr> body, const std::optional<Token>& label = {}) :
        Expr(span),
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
        const bite::SourceSpan& span,
        std::unique_ptr<Expr> condition,
        std::unique_ptr<BlockExpr> body,
        const std::optional<Token>& label = {}
    ) : Expr(span),
        condition(std::move(condition)),
        body(std::move(body)),
        label(label) {}
};

class BreakExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::break_expr;
    }

    std::unique_ptr<Expr> expr;
    std::optional<Token> label;

    explicit BreakExpr(const bite::SourceSpan& span, std::unique_ptr<Expr> expr, const std::optional<Token>& label) :
        Expr(span),
        expr(std::move(expr)),
        label(label) {}
};

class ContinueExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::continue_expr;
    }

    std::optional<Token> label;

    ContinueExpr(const bite::SourceSpan& span, const std::optional<Token>& label) : Expr(span),
        label(label) {}
};

class ForExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::for_expr;
    }

    std::unique_ptr<VariableDeclaration> name;
    std::unique_ptr<Expr> iterable;
    std::unique_ptr<BlockExpr> body;
    std::optional<Token> label;

    ForExpr(
        const bite::SourceSpan& span,
        std::unique_ptr<VariableDeclaration> name,
        std::unique_ptr<Expr> iterable,
        std::unique_ptr<BlockExpr> body,
        const std::optional<Token>& label = {}
    ) : Expr(span),
        name(std::move(name)),
        iterable(std::move(iterable)),
        body(std::move(body)),
        label(label) {}
};

class ReturnExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::return_expr;
    }

    std::unique_ptr<Expr> value;

    explicit ReturnExpr(const bite::SourceSpan& span, std::unique_ptr<Expr> value) : Expr(span),
        value(std::move(value)) {}
};

class ThisExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::this_expr;
    }

    explicit ThisExpr(const bite::SourceSpan& span) : Expr(span) {}
};

// TODO: refactor!

class AnonymousFunctionExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::anonymous_function_expr;
    }

    std::unique_ptr<FunctionDeclaration> function;

    AnonymousFunctionExpr(const bite::SourceSpan& span, std::unique_ptr<FunctionDeclaration> function) : Expr(span),
        function(std::move(function)) {}
};


class FunctionDeclaration final : public Declaration {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::function_declaration;
    }

    std::vector<Token> params;
    std::unique_ptr<Expr> body;
    FunctionEnviroment enviroment {};

    FunctionDeclaration(
        const bite::SourceSpan& span,
        const Token& name,
        std::vector<Token> params,
        std::unique_ptr<Expr> body = {}
    ) : Declaration(span, name),
        params(std::move(params)),
        body(std::move(body)) {}
};

class VariableDeclaration final : public Declaration {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::variable_declaration;
    }

    std::unique_ptr<Expr> value;

    VariableDeclaration(const bite::SourceSpan& span, const Token& name, std::unique_ptr<Expr> value = {}) :
        Declaration(span, name),
        value(std::move(value)) {}
};

class ExprStmt final : public Stmt {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::expr_stmt;
    }

    std::unique_ptr<Expr> value;

    ExprStmt(const bite::SourceSpan& span, std::unique_ptr<Expr> expr) : Stmt(span),
                                                                         value(std::move(expr)) {}
};

struct Field {
    bitflags<ClassAttributes> attributes;
    std::unique_ptr<VariableDeclaration> variable;
    bite::SourceSpan span;
};

struct Method {
    bitflags<ClassAttributes> attributes;
    std::unique_ptr<FunctionDeclaration> function;
    bite::SourceSpan span;
};

struct SuperConstructorCall {
    std::vector<std::unique_ptr<Expr>> arguments;
    bite::SourceSpan span;
};

struct Constructor {
    std::optional<SuperConstructorCall> super_arguments_call;
    std::unique_ptr<FunctionDeclaration> function;
    bite::SourceSpan span;
};

struct TraitUsageDeclaration {
    StringTable::Handle original_name;
    StringTable::Handle aliased_name;
    bitflags<ClassAttributes> attributes;
};

struct TraitUsage {
    Token trait;
    std::vector<Token> exclusions;
    std::vector<std::pair<Token, Token>> aliases;
    bite::SourceSpan span;
    std::vector<TraitUsageDeclaration> declarations; // TODO: elimnate this
    Binding binding = NoBinding();
};

class ObjectExpr;

struct ClassObject {
    std::unique_ptr<ObjectExpr> metaobject;
    std::optional<Token> superclass;
    std::vector<Field> fields;
    std::vector<Method> methods;
    std::vector<TraitUsage> traits_used;
    Constructor constructor;
    Binding superclass_binding;
    ClassEnviroment enviroment;
};

class ClassDeclaration final : public Declaration {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::class_declaration;
    }

    ClassDeclaration(const bite::SourceSpan& span, bool is_abstract, const Token& name, ClassObject object) :
        Declaration(span, name),
        is_abstract(is_abstract),
        object(std::move(object)) {}

    bool is_abstract;
    ClassObject object;
};

class ObjectExpr final : public Expr {
public:
    bite::SourceSpan name_span; // TODO: temp?
    ObjectExpr(const bite::SourceSpan& span, ClassObject object, const bite::SourceSpan& name_span) : Expr(span),
        object(std::move(object)),
        name_span(name_span) {}

    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::object_expr;
    }

    ClassObject object;
};

class ObjectDeclaration final : public Declaration {
public:
    ObjectDeclaration(const bite::SourceSpan& span, const Token& name, std::unique_ptr<ObjectExpr> object) :
        Declaration(span, name),
        object(std::move(object)) {}

    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::object_declaration;
    }

    std::unique_ptr<ObjectExpr> object;
};

class TraitDeclaration final : public Declaration {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::trait_declaration;
    }

    std::vector<Method> methods;
    std::vector<Field> fields;
    std::vector<TraitUsage> using_stmts;
    TraitEnviroment enviroment;

    TraitDeclaration(
        const bite::SourceSpan& span,
        const Token& name,
        std::vector<Method> methods,
        std::vector<Field> fields,
        std::vector<TraitUsage> using_stmts
    ) : Declaration(span, name),
        methods(std::move(methods)),
        fields(std::move(fields)),
        using_stmts(std::move(using_stmts)) {}
};

class ImportStmt final : public Stmt {
public:
    // TODO: workaround?
    class Item final : public Declaration {
    public:
        [[nodiscard]] NodeKind kind() const override {
            return NodeKind::invalid_stmt;
        }

        Declaration* item_declaration = nullptr;
        std::unique_ptr<Expr> item; // better name?
        Binding binding = NoBinding();

        Item(const bite::SourceSpan& span, const Token& name, std::unique_ptr<Expr> item) : Declaration(span, name),
            item(std::move(item)) {}
    };

    ImportStmt(const bite::SourceSpan& span, std::vector<std::unique_ptr<Item>> items, std::unique_ptr<Expr> module) :
        Stmt(span),
        items(std::move(items)),
        module(std::move(module)) {}

    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::import_stmt;
    }

    std::vector<std::unique_ptr<Item>> items;
    std::unique_ptr<Expr> module;
};

class ModuleStmt final : public Declaration {
public:
    ModuleStmt(const bite::SourceSpan& span, const Token& name, std::vector<std::unique_ptr<Stmt>> stmts) :
        Declaration(span, name),
        stmts(std::move(stmts)) {}

    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::module_stmt;
    }

    std::vector<std::unique_ptr<Stmt>> stmts;
    bite::unordered_dense::map<StringTable::Handle, Declaration*> declarations;
};

class ModuleResolutionExpr final : public Expr {
public:
    ModuleResolutionExpr(const bite::SourceSpan& span, std::vector<Token> path) : Expr(span),
        path(std::move(path)) {}

    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::module_resolution_expr;
    }

    Binding binding = NoBinding();
    std::vector<Token> path;
};

class InvalidStmt final : public Stmt {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::invalid_stmt;
    }

    explicit InvalidStmt(const bite::SourceSpan& span) : Stmt(span) {}
};

class InvalidExpr final : public Expr {
public:
    [[nodiscard]] NodeKind kind() const override {
        return NodeKind::invalid_expr;
    }

    explicit InvalidExpr(const bite::SourceSpan& span) : Expr(span) {}
};

#define DECLARE_AS_METHOD(class_name, type_name) inline class_name* AstNode::as_##type_name() { return static_cast<class_name*>(this);}
AST_NODES(DECLARE_AS_METHOD)

#endif //EXPR_H
