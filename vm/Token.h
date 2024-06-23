#ifndef TOKEN_H
#define TOKEN_H
#include <string_view>

class Token {
public:
    enum class Type {
        LEFT_PAREN, RIGHT_PAREN, LEFT_BRACE, RIGHT_BRACE,
        COMMA, DOT, MINUS, PLUS, SEMICOLON, SLASH, STAR,

        BANG, BANG_EQUAL,
        EQUAL, EQUAL_EQUAL,
        GREATER, GREATER_EQUAL,
        LESS, LESS_EQUAL,

        IDENTIFIER, STRING, NUMBER,

        AND, CLASS, ELSE, FALSE, FUN, FOR, IF, NIL, OR,
        PRINT, RETURN, SUPER, THIS, TRUE, VAR, WHILE,

        ERROR, END
    };

    Token(Type type, std::string_view lexeme, int line) : type(type), lexeme(lexeme), line(line) {}

    Type type;
    std::string_view lexeme;
    int line;
};

#endif
