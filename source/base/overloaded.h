#ifndef OVERLOADED_H
#define OVERLOADED_H

// helper type for std::visit visitor
// source: https://en.cppreference.com/w/cpp/utility/variant/visit
template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

#endif //OVERLOADED_H
