//
// Created by MiłoszK on 22.06.2024.
//

#ifndef RETURN_H
#define RETURN_H
#include <any>
#include <utility>

class Return {
public:
    explicit Return(std::any value) : value(std::move(value)) {};

    std::any value;

};



#endif //RETURN_H
