#ifndef ASTVISITOR_H
#define ASTVISITOR_H
#include "Ast.h"

class AstVisitor {
    template <typename Self>
    // ReSharper disable once CppMemberFunctionMayBeStatic
    void visit(this Self&& self, const AstNode& node) { // NOLINT(*-function-cognitive-complexity, *-missing-std-forward)
        #define STR(str) ##str
        #define VISIT_CASE(class_name, type_name) \
            case NodeKind::##type_name: \
                self.##type_name(static_cast<const STR(class_name)&>(node)); \
                return;
        switch (node.kind()) {
            AST_NODES(VISIT_CASE) // NOLINT(*-pro-type-static-cast-downcast)
        }
    }
};

class MutatingAstVisitor {
    template <typename Self>
    // ReSharper disable once CppMemberFunctionMayBeStatic
    void visit(this Self&& self, AstNode& node) { // NOLINT(*-function-cognitive-complexity, *-missing-std-forward)
        #define STR(str) ##str
        #define VISIT_CASE(class_name, type_name) \
        case NodeKind::##type_name: \
        self.##type_name(static_cast<STR(class_name)&>(node)); \
        return;
        switch (node.kind()) {
            AST_NODES(VISIT_CASE) // NOLINT(*-pro-type-static-cast-downcast)
        }
    }
};

#endif //ASTVISITOR_H
