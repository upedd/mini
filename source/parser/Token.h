#ifndef TOKEN_H
#define TOKEN_H
#include "../Diagnostics.h"
#include "../shared/StringTable.h"

struct Token {
    // todo: organize
    enum class Type : std::uint8_t {
        IDENTIFIER,
        NUMBER,
        INTEGER,
        STRING,
        EQUAL,
        EQUAL_EQUAL,
        BANG_EQUAL,
        LESS_EQUAL,
        GREATER_EQUAL,
        PLUS_EQUAL,
        MINUS_EQUAL,
        STAR_EQUAL,
        PERCENT_EQUAL,
        SLASH_EQUAL,
        SLASH_SLASH_EQUAL,
        AND_EQUAL,
        BAR_EQUAL,
        CARET_EQUAL,
        LESS_LESS_EQUAL,
        GREATER_GREATER_EQUAL,
        LESS,
        GREATER,
        LEFT_PAREN,
        RIGHT_PAREN,
        LEFT_BRACE,
        RIGHT_BRACE,
        LEFT_BRACKET,
        RIGHT_BRACKET,
        PLUS,
        MINUS,
        STAR,
        SLASH,
        SLASH_SLASH,
        PERCENT,
        AND,
        AND_AND,
        BAR,
        BAR_BAR,
        BANG,
        DOT,
        DOT_DOT,
        DOT_DOT_DOT,
        LESS_LESS,
        GREATER_GREATER,
        CARET,
        TRUE,
        FALSE,
        LOOP,
        IF,
        ELSE,
        IN,
        IS,
        CLASS,
        FUN,
        GET,
        SET,
        NIL,
        THIS,
        COMMA,
        SEMICOLON,
        BREAK,
        CONTINUE,
        NONE,
        MATCH,
        RETURN,
        SUPER,
        LET,
        WHILE,
        TILDE,
        FOR,
        LABEL,
        COLON,
        PRIVATE,
        ABSTRACT,
        OVERRDIE,
        OBJECT,
        TRAIT,
        USING,
        AS,
        EXCLUDE,
        IMPORT,
        FROM,
        MODULE,
        COLON_COLON,
        QUESTION_DOT,
        QUESTION_QUESTION,
        QUESTION_QUESTION_EQUAL,
        QUESTION_LEFT_PAREN,
        OPERATOR,
        END // NOTE: Type END must always be last element in this enum.
    };

    Type type = Type::NONE;
    bite::SourceSpan span;
    StringTable::Handle string;

    static std::string type_to_string(Type type);
    static std::string type_to_display(Type type);
    /**
     *
     * @return string representation for logging purposes.
     */
    [[nodiscard]] std::string to_string() const {
        return "Token(type=" + type_to_string(type) + ", lexeme='" + (string ? *string : "") + "')";
    }
};

inline std::string Token::type_to_string(const Type type) {
    // maybe do this using some macros magic
    switch (type) {
        case Type::IDENTIFIER: return "IDENTIFIER";
        case Type::NUMBER: return "NUMBER";
        case Type::INTEGER: return "INTEGER";
        case Type::STRING: return "STRING";
        case Type::EQUAL: return "EQUAL";
        case Type::EQUAL_EQUAL: return "EQUAL_EQUAL";
        case Type::BANG_EQUAL: return "BANG_EQUAL";
        case Type::LESS_EQUAL: return "LESS_EQUAL";
        case Type::GREATER_EQUAL: return "GREATER_EQUAL";
        case Type::PLUS_EQUAL: return "PLUS_EQUAL";
        case Type::MINUS_EQUAL: return "MINUS_EQUAL";
        case Type::STAR_EQUAL: return "STAR_EQUAL";
        case Type::PERCENT_EQUAL: return "PERCENT_EQUAL";
        case Type::SLASH_EQUAL: return "SLASH_EQUAL";
        case Type::SLASH_SLASH_EQUAL: return "SLASH_SLASH_EQUAL";
        case Type::AND_EQUAL: return "AND_EQUAL";
        case Type::BAR_EQUAL: return "BAR_EQUAL";
        case Type::CARET_EQUAL: return "CARET_EQUAL";
        case Type::LESS_LESS_EQUAL: return "LESS_LESS_EQUAL";
        case Type::GREATER_GREATER_EQUAL: return "GREATER_GREATER_EQUAL";
        case Type::LESS: return "LESS";
        case Type::GREATER: return "GREATER";
        case Type::LEFT_PAREN: return "LEFT_PAREN";
        case Type::RIGHT_PAREN: return "RIGHT_PAREN";
        case Type::LEFT_BRACE: return "LEFT_BRACE";
        case Type::RIGHT_BRACE: return "RIGHT_BRACE";
        case Type::LEFT_BRACKET: return "LEFT_BRACKET";
        case Type::RIGHT_BRACKET: return "RIGHT_BRACKET";
        case Type::PLUS: return "PLUS";
        case Type::MINUS: return "MINUS";
        case Type::STAR: return "STAR";
        case Type::SLASH: return "SLASH";
        case Type::SLASH_SLASH: return "SLASH_SLASH";
        case Type::PERCENT: return "PERCENT";
        case Type::AND: return "AND";
        case Type::AND_AND: return "AND_AND";
        case Type::BAR: return "BAR";
        case Type::BAR_BAR: return "BAR_BAR";
        case Type::BANG: return "BANG";
        case Type::DOT: return "DOT";
        case Type::DOT_DOT: return "DOT_DOT";
        case Type::DOT_DOT_DOT: return "DOT_DOT_DOT";
        case Type::LESS_LESS: return "LESS_LESS";
        case Type::GREATER_GREATER: return "GREATER_GREATER";
        case Type::CARET: return "CARET";
        case Type::TRUE: return "TRUE";
        case Type::FALSE: return "FALSE";
        case Type::LOOP: return "LOOP";
        case Type::IF: return "IF";
        case Type::ELSE: return "ELSE";
        case Type::IN: return "IN";
        case Type::IS: return "IS";
        case Type::CLASS: return "CLASS";
        case Type::FUN: return "FUN";
        case Type::GET: return "GET";
        case Type::SET: return "SET";
        case Type::NIL: return "NIL";
        case Type::END: return "END";
        case Type::THIS: return "THIS";
        case Type::COMMA: return "COMMA";
        case Type::SEMICOLON: return "SEMICOLON";
        case Type::BREAK: return "BREAK";
        case Type::CONTINUE: return "CONTINUE";
        case Type::NONE: return "NONE";
        case Type::MATCH: return "MATCH";
        case Type::RETURN: return "RETURN";
        case Type::SUPER: return "SUPER";
        case Type::LET: return "LET";
        case Type::WHILE: return "WHILE";
        case Type::TILDE: return "TILDE";
        case Type::FOR: return "FOR";
        case Type::LABEL: return "LABEL";
        case Type::COLON: return "COLON";
        case Type::OVERRDIE: return "OVERRIDE";
        case Type::OBJECT: return "OBJECT";
        case Type::TRAIT: return "TRAIT";
        case Type::USING: return "USING";
        case Type::PRIVATE: return "PRIVATE";
        case Type::ABSTRACT: return "ABSTRACT";
        case Type::AS: return "AS";
        case Type::EXCLUDE: return "EXCLUDE";
        case Type::IMPORT: return "IMPORT";
        case Type::FROM: return "FROM";
        case Type::MODULE: return "MODULE";
        case Type::COLON_COLON: return "COLON_COLON";
        case Type::QUESTION_DOT: return "QUESTION_DOT";
        case Type::QUESTION_QUESTION: return "QUESTION_QUESTION";
        case Type::QUESTION_QUESTION_EQUAL: return "QUESTION_QUESTION_EQAUL";
        case Type::QUESTION_LEFT_PAREN: return "LEFT_PAREN";
        case Type::OPERATOR: return "OPERATOR";
    }
    return "INVALID_TOKEN";
}

inline std::string Token::type_to_display(const Type type) {
    switch (type) {
        case Type::IDENTIFIER: return "identifier";
        case Type::NUMBER: return "number";
        case Type::INTEGER: return "integer";
        case Type::STRING: return "string";
        case Type::EQUAL: return "=";
        case Type::EQUAL_EQUAL: return "==";
        case Type::BANG_EQUAL: return "!=";
        case Type::LESS_EQUAL: return "<=";
        case Type::GREATER_EQUAL: return ">=";
        case Type::PLUS_EQUAL: return "<=";
        case Type::MINUS_EQUAL: return "-=";
        case Type::STAR_EQUAL: return "*=";
        case Type::PERCENT_EQUAL: return "%=";
        case Type::SLASH_EQUAL: return "/=";
        case Type::SLASH_SLASH_EQUAL: return "//=";
        case Type::AND_EQUAL: return "&=";
        case Type::BAR_EQUAL: return "|=";
        case Type::CARET_EQUAL: return "^=";
        case Type::LESS_LESS_EQUAL: return "<<=";
        case Type::GREATER_GREATER_EQUAL: return ">>=";
        case Type::LESS: return "<";
        case Type::GREATER: return ">";
        case Type::LEFT_PAREN: return "(";
        case Type::RIGHT_PAREN: return ")";
        case Type::LEFT_BRACE: return "{";
        case Type::RIGHT_BRACE: return "}";
        case Type::LEFT_BRACKET: return "[";
        case Type::RIGHT_BRACKET: return "]";
        case Type::PLUS: return "+";
        case Type::MINUS: return "-";
        case Type::STAR: return "*";
        case Type::SLASH: return "/";
        case Type::SLASH_SLASH: return "//";
        case Type::PERCENT: return "%";
        case Type::AND: return "&";
        case Type::AND_AND: return "&&";
        case Type::BAR: return "|";
        case Type::BAR_BAR: return "||";
        case Type::BANG: return "!";
        case Type::DOT: return ".";
        case Type::DOT_DOT: return "..";
        case Type::DOT_DOT_DOT: return "...";
        case Type::LESS_LESS: return "<<";
        case Type::GREATER_GREATER: return ">>";
        case Type::CARET: return "^";
        case Type::TRUE: return "true";
        case Type::FALSE: return "false";
        case Type::LOOP: return "loop";
        case Type::IF: return "if";
        case Type::ELSE: return "else";
        case Type::IN: return "in";
        case Type::IS: return "is";
        case Type::CLASS: return "class";
        case Type::FUN: return "fun";
        case Type::GET: return "get";
        case Type::SET: return "set";
        case Type::NIL: return "nil";
        case Type::THIS: return "this";
        case Type::COMMA: return ",";
        case Type::SEMICOLON: return ";";
        case Type::BREAK: return "break";
        case Type::CONTINUE: return "continue";
        case Type::NONE: return "none";
        case Type::MATCH: return "match";
        case Type::RETURN: return "return";
        case Type::SUPER: return "super";
        case Type::LET: return "let";
        case Type::WHILE: return "while";
        case Type::TILDE: return "~";
        case Type::FOR: return "for";
        case Type::LABEL: return "label";
        case Type::COLON: return ":";
        case Type::PRIVATE: return "private";
        case Type::ABSTRACT: return "abstract";
        case Type::OVERRDIE: return "override";
        case Type::OBJECT: return "object";
        case Type::TRAIT: return "trait";
        case Type::USING: return "using";
        case Type::AS: return "as";
        case Type::EXCLUDE: return "exclude";
        case Type::END: return "end";
        case Type::IMPORT: return "import";
        case Type::FROM: return "from";
        case Type::MODULE: return "module";
        case Type::COLON_COLON: return "::";
        case Type::QUESTION_DOT: return "?.";
        case Type::QUESTION_QUESTION: return "??";
        case Type::QUESTION_QUESTION_EQUAL: return "??=";
        case Type::QUESTION_LEFT_PAREN: return "?(";
        case Type::OPERATOR: return "operator";
    }
}

#endif //TOKEN_H
