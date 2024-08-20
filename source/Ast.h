#ifndef EXPR_H
#define EXPR_H
#include <format>
#include <variant>
#include <vector>
#include <algorithm>

#include "parser/Token.h"
#include "Value.h"
#include "base/bitflags.h"
#include "base/box.h"


// Reference: https://lesleylai.info/en/ast-in-cpp-part-1-variant/
// https://www.foonathan.net/2022/05/recursive-variant-box/
/**
* Container to attach additional metadata to ast nodes
*/
template <typename T>
class AstNode {
public:
    explicit AstNode(T&& value, const std::int64_t id) : id(id),
                                                         ptr(new T(std::move(value))) {}

    AstNode(const AstNode& value) = delete;
    AstNode& operator=(const AstNode&) = delete;

    T& operator*() { return *ptr; }
    const T& operator*() const { return *ptr; }

    T* operator->() { return ptr.get(); }
    const T* operator->() const { return ptr.get(); }

    AstNode(AstNode&& other) noexcept = default;
    AstNode& operator=(AstNode&& other) noexcept = default;

    ~AstNode() = default;

    std::int64_t id;

private:
    std::unique_ptr<T> ptr;
};

using Expr = std::variant<AstNode<struct LiteralExpr>, AstNode<struct StringLiteral>, AstNode<struct UnaryExpr>, AstNode
                          <struct BinaryExpr>, AstNode<struct VariableExpr>, AstNode<struct CallExpr>, AstNode<struct
                              GetPropertyExpr>, AstNode<struct SuperExpr>, AstNode<struct BlockExpr>, AstNode<struct
                              IfExpr>, AstNode<struct LoopExpr>, AstNode<struct BreakExpr>, AstNode<struct ContinueExpr>
                          , AstNode<struct WhileExpr>, AstNode<struct ForExpr>, AstNode<struct ReturnExpr>, AstNode<
                              struct ThisExpr>, AstNode<struct ObjectExpr>, AstNode<struct InvalidExpr>>;

using Stmt = std::variant<AstNode<struct VarStmt>, AstNode<struct ExprStmt>, AstNode<struct FunctionStmt>, AstNode<
                              struct ClassStmt>, AstNode<struct NativeStmt>, AstNode<struct ObjectStmt>, AstNode<struct
                              TraitStmt>, AstNode<struct UsingStmt>, AstNode<struct InvalidStmt>>;

// TODO: this feels wrong...
using ExprPtr = std::variant<AstNode<LiteralExpr>*, AstNode<StringLiteral>*, AstNode<UnaryExpr>*, AstNode<BinaryExpr>*,
                             AstNode<VariableExpr>*, AstNode<CallExpr>*, AstNode<GetPropertyExpr>*, AstNode<SuperExpr>*,
                             AstNode<BlockExpr>*, AstNode<IfExpr>*, AstNode<LoopExpr>*, AstNode<BreakExpr>*, AstNode<
                                 ContinueExpr>*, AstNode<WhileExpr>*, AstNode<ForExpr>*, AstNode<ReturnExpr>*, AstNode<
                                 ThisExpr>*, AstNode<ObjectExpr>*, AstNode<InvalidExpr>*>;

using StmtPtr = std::variant<AstNode<VarStmt>*, AstNode<ExprStmt>*, AstNode<FunctionStmt>*, AstNode<ClassStmt>*, AstNode
                             <NativeStmt>*, AstNode<ObjectStmt>*, AstNode<TraitStmt>*, AstNode<UsingStmt>*, AstNode<
                                 InvalidStmt>*>;

struct LocalDeclarationInfo {
    StmtPtr declaration;
    std::int64_t idx;
    bool is_captured;
};

struct GlobalDeclarationInfo {
    StmtPtr declaration;
    StringTable::Handle name;
};

using DeclarationInfo = std::variant<LocalDeclarationInfo, GlobalDeclarationInfo>;



using Binding = std::variant<struct NoBinding, struct LocalBinding, struct GlobalBinding, struct UpvalueBinding, struct ParameterBinding, struct MemberBinding,
                             struct PropertyBinding, struct SuperBinding, struct ClassObjectBinding>;

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


struct ClassEnviroment {
    StringTable::Handle class_name; // TODO: temporary!
    bite::unordered_dense::map<StringTable::Handle, bitflags<ClassAttributes>> members;
    ClassEnviroment* class_object_enviroment = nullptr;
};

class Ast {
public:
    std::vector<Stmt> statements;
    GlobalEnviroment enviroment;
    std::size_t current_id = 0;

    template <typename T, typename... Args>
    AstNode<T> make_node(Args&&... args) {
        return AstNode<T>(T(std::forward<Args>(args)...), current_id++);
    }
};


struct UnaryExpr {
    Expr expr;
    Token::Type op;
};

struct BinaryExpr {
    Expr left;
    Expr right;
    Token::Type op;
    Binding binding; // TODO: temp
};

struct CallExpr {
    Expr callee;
    std::vector<Expr> arguments;
};

struct LiteralExpr {
    Value literal;
};

struct StringLiteral {
    std::string string;
};

struct VariableExpr {
    Token identifier;
    Binding binding = NoBinding();
};

struct GetPropertyExpr {
    Expr left; // Todo: better name?
    Token property;
};

struct SuperExpr {
    Token method;
};

struct BlockExpr {
    std::vector<Stmt> stmts;
    std::optional<Expr> expr;
    std::optional<Token> label;
};

struct IfExpr {
    Expr condition;
    Expr then_expr;
    std::optional<Expr> else_expr;
};

struct LoopExpr {
    AstNode<BlockExpr> body;
    std::optional<Token> label;
};

struct WhileExpr {
    Expr condition;
    AstNode<BlockExpr> body;
    std::optional<Token> label;
};

struct BreakExpr {
    std::optional<Expr> expr;
    std::optional<Token> label;
};

struct ContinueExpr {
    std::optional<Token> label;
};

struct ForExpr {
    Token name;
    Expr iterable;
    AstNode<BlockExpr> body;
    std::optional<Token> label;
};

struct ReturnExpr {
    std::optional<Expr> value;
};


struct ThisExpr {};


struct FunctionStmt {
    Token name;
    std::vector<Token> params;
    std::optional<Expr> body;
    DeclarationInfo info;
    FunctionEnviroment enviroment {};
};

struct VarStmt {
    Token name;
    std::optional<Expr> value;
    DeclarationInfo info;
};

struct ExprStmt {
    Expr expr;
};

struct Field {
    AstNode<VarStmt> variable;
    bitflags<ClassAttributes> attributes;
};

struct Method {
    AstNode<FunctionStmt> function;
    bitflags<ClassAttributes> attributes;
};

struct Constructor {
    bool has_super;
    std::vector<Expr> super_arguments;
    AstNode<FunctionStmt> function;
};

/**
 * Shared fields between ObjectExpr and ClassStmt
 */
struct StructureBody {
    std::vector<Method> methods;
    std::vector<Field> fields;
    std::optional<AstNode<ObjectExpr>> class_object;
    std::vector<AstNode<UsingStmt>> using_statements;
    std::optional<Constructor> constructor; // TODO: remove this optional
};

struct ObjectExpr {
    StructureBody body;
    std::optional<Token> super_class;
    std::vector<Expr> superclass_arguments;
    ClassEnviroment class_enviroment;
    Binding superclass_binding = NoBinding();
};

struct ClassStmt {
    Token name;
    std::optional<Token> super_class;
    StructureBody body;
    bool is_abstract = false;
    DeclarationInfo info;
    ClassEnviroment enviroment;
    Binding superclass_binding = NoBinding();
};

struct NativeStmt {
    Token name;
    DeclarationInfo info;
};

struct ObjectStmt {
    Token name;
    Expr object;
    DeclarationInfo info;
};

struct TraitStmt {
    Token name;
    std::vector<Method> methods;
    std::vector<Field> fields;
    std::vector<AstNode<UsingStmt>> using_stmts;
    DeclarationInfo info;
};

struct UsingStmtItem {
    Token name;
    std::vector<Token> exclusions;
    std::vector<std::pair<Token, Token>> aliases;
};

struct UsingStmt {
    std::vector<UsingStmtItem> items;
};

// markers for invalid ast nodes
struct InvalidExpr {};

struct InvalidStmt {};

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
