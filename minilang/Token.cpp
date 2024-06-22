//
// Created by MiłoszK on 19.06.2024.
//

#include "Token.h"

std::string Token::to_string() const {
    // TODO: in craftring interpreters also prints literal but not sure how to do it in c++
    return token_type_strings[static_cast<int>(type)] + ' ' + lexeme;
}
