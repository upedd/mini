//
// Created by MiłoszK on 23.06.2024.
//

#ifndef MINIINSTANCE_H
#define MINIINSTANCE_H
#include <utility>

#include "MiniClass.h"


class MiniInstance {
public:
    explicit MiniInstance(MiniClass klass) : klass(std::move(klass)) {}

    std::any get(const Token& token);

    void set(const Token &name, const std::any &value);

private:
    std::unordered_map<std::string, std::any> fields;
    MiniClass klass;




};



#endif //MINIINSTANCE_H
