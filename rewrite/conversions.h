#ifndef CONVERSIONS_H
#define CONVERSIONS_H
#include <expected>
#include <stdexcept>

#include "types.h"

class ConversionError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// todo: comments!
std::expected<bite_int, ConversionError> string_to_int(const std::string& string);

std::expected<double, ConversionError> string_to_floating(const std::string& string);

#endif //CONVERSIONS_H
