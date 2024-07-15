#ifndef TOKEN_H
#define TOKEN_H

struct Token {
    // todo: organize
    enum class Type {
        IDENTIFIER,
        NUMBER,
        INTEGER,
        STRING,
        EQUAL,
        EQUAL_EQUAL,
        BANG_EQUAL,
        COLON_EQUAL,
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
        RETURN, SUPER, LET, WHILE, TILDE, NATIVE ,FOR,
        END // NOTE: Type END must always be last element in this enum.
    };

    Type type = Type::NONE;
    int source_offset = 0;
    int length = 0;

    static std::string type_to_string(Type type);

    /**
     *
     * @param source string used by lexer to produce this token.
     * @return lexeme of this token.
     */
    [[nodiscard]] std::string get_lexeme(std::string_view source) const {
        return std::string(source.substr(source_offset, length));
    }

    /**
     *
     * @param source string used by lexer to produce this token.
     * @return string representation for logging purposes.
     */
    [[nodiscard]] std::string to_string(std::string_view source) const {
        return "Token(type=" + type_to_string(type) + ", lexeme='" + get_lexeme(source) + "')";
    }
};

inline std::string Token::type_to_string(Type type) {
    // maybe do this using some macros magic
    switch (type) {
        case Type::IDENTIFIER:
            return "IDENTIFIER";
        case Type::NUMBER:
            return "NUMBER";
        case Type::INTEGER:
            return "INTEGER";
        case Type::STRING:
            return "STRING";
        case Type::EQUAL:
            return "EQUAL";
        case Type::EQUAL_EQUAL:
            return "EQUAL_EQUAL";
        case Type::BANG_EQUAL:
            return "BANG_EQUAL";
        case Type::COLON_EQUAL:
            return "COLON_EQUAL";
        case Type::LESS_EQUAL:
            return "LESS_EQUAL";
        case Type::GREATER_EQUAL:
            return "GREATER_EQUAL";
        case Type::PLUS_EQUAL:
            return "PLUS_EQUAL";
        case Type::MINUS_EQUAL:
            return "MINUS_EQUAL";
        case Type::STAR_EQUAL:
            return "STAR_EQUAL";
        case Type::PERCENT_EQUAL:
            return "PERCENT_EQUAL";
        case Type::SLASH_EQUAL:
            return "SLASH_EQUAL";
        case Type::SLASH_SLASH_EQUAL:
            return "SLASH_SLASH_EQUAL";
        case Type::AND_EQUAL:
            return "AND_EQUAL";
        case Type::BAR_EQUAL:
            return "BAR_EQUAL";
        case Type::CARET_EQUAL:
            return "CARET_EQUAL";
        case Type::LESS_LESS_EQUAL:
            return "LESS_LESS_EQUAL";
        case Type::GREATER_GREATER_EQUAL:
            return "GREATER_GREATER_EQUAL";
        case Type::LESS:
            return "LESS";
        case Type::GREATER:
            return "GREATER";
        case Type::LEFT_PAREN:
            return "LEFT_PAREN";
        case Type::RIGHT_PAREN:
            return "RIGHT_PAREN";
        case Type::LEFT_BRACE:
            return "LEFT_BRACE";
        case Type::RIGHT_BRACE:
            return "RIGHT_BRACE";
        case Type::LEFT_BRACKET:
            return "LEFT_BRACKET";
        case Type::RIGHT_BRACKET:
            return "RIGHT_BRACKET";
        case Type::PLUS:
            return "PLUS";
        case Type::MINUS:
            return "MINUS";
        case Type::STAR:
            return "STAR";
        case Type::SLASH:
            return "SLASH";
        case Type::SLASH_SLASH:
            return "SLASH_SLASH";
        case Type::PERCENT:
            return "PERCENT";
        case Type::AND:
            return "AND";
        case Type::AND_AND:
            return "AND_AND";
        case Type::BAR:
            return "BAR";
        case Type::BAR_BAR:
            return "BAR_BAR";
        case Type::BANG:
            return "BANG";
        case Type::DOT:
            return "DOT";
        case Type::DOT_DOT:
            return "DOT_DOT";
        case Type::DOT_DOT_DOT:
            return "DOT_DOT_DOT";
        case Type::LESS_LESS:
            return "LESS_LESS";
        case Type::GREATER_GREATER:
            return "GREATER_GREATER";
        case Type::CARET:
            return "CARET";
        case Type::TRUE:
            return "TRUE";
        case Type::FALSE:
            return "FALSE";
        case Type::LOOP:
            return "LOOP";
        case Type::IF:
            return "IF";
        case Type::ELSE:
            return "ELSE";
        case Type::IN:
            return "IN";
        case Type::IS:
            return "IS";
        case Type::CLASS:
            return "CLASS";
        case Type::FUN:
            return "FUN";
        case Type::GET:
            return "GET";
        case Type::SET:
            return "SET";
        case Type::NIL:
            return "NIL";
        case Type::END:
            return "END";
        case Type::THIS:
            return "THIS";
        case Type::COMMA:
            return "COMMA";
        case Type::SEMICOLON:
            return "SEMICOLON";
        case Type::BREAK:
            return "BREAK";
        case Type::CONTINUE:
            return "CONTINUE";
        case Type::NONE:
            return "NONE";
        case Type::MATCH:
            return "MATCH";
        case Type::RETURN:
            return "RETURN";
        case Type::SUPER:
            return "SUPER";
        case Type::LET:
            return "LET";
        case Type::WHILE:
            return "WHILE";
        case Type::TILDE:
            return "TILDE";
        case Type::NATIVE:
            return "NATIVE";
        case Type::FOR:
            return "FOR";
    }
    return "INVALID_TOKEN";
}

#endif //TOKEN_H
