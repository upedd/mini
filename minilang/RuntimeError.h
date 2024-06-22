//
// Created by MiłoszK on 21.06.2024.
//

#ifndef RUNTIMEERROR_H
#define RUNTIMEERROR_H
#include <stdexcept>
#include <utility>

#include "Token.h"

class RuntimeError : public std::runtime_error {
public:
    RuntimeError(Token token, std::string_view message) : runtime_error(message.data()), token(std::move(token)) {}
    Token token;
};

#endif //RUNTIMEERROR_H
