#ifndef PARSER_H
#define PARSER_H
#include <istream>

#include "Token.h"


/**
 * Takes an istream and produces tokens.\n
 * Source must be kept alive for whole Lexer exectution!
 */
class Lexer {
public:
    explicit Lexer(const std::string_view source) : source(source) {};

    Token::Type keyword_or_identifier(char c);

    Token next_token();

    bool end();
private:
    // source traversal functions
    char next();
    char peek() const;
    char peek_next();
    bool match(char c);
    void skip_whitespace();

    inline static bool is_identifier_character(char c);
    void consume_identifier();
    int source_position = 0;
    int line = 1;
    int line_offset = 0;

    std::string_view source;
};


#endif //PARSER_H
