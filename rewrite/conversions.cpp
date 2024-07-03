#include "conversions.h"

#include <algorithm>

#include "common.h"

std::unexpected<ConversionError> make_conversion_error(const std::string& what) {
    return std::unexpected(ConversionError(what));
}

std::expected<bite_int, ConversionError> wrapped_stoll(const std::string& string, int base) {
    try {
        return std::stoll(string, nullptr, base);
    } catch (const std::out_of_range&) {
        return make_conversion_error("Literal value is too big.");
    } catch (const std::invalid_argument&) {
        return make_conversion_error("Literal parsing failed unexpectedly.");
    }
}

std::string remove_digit_separator(const std::string& string) {
    std::string result;
    for (char c : string) {
        if (c != '_') result += c;
    }
    return result;
}


std::optional<ConversionError> validate_decimal_string(const std::string& string) {
    for (char c : string) {
        if (!is_digit(c)) return ConversionError(std::string("Expected decimal digit but got '") + c + "'.");
    }
    return {};
}
std::optional<ConversionError> validate_binary_string(const std::string& string) {
    for (char c : string) {
        if (!is_binary_digit(c)) return ConversionError(std::string("Expected binary digit but got '") + c + "'.");
    }
    return {};
}
std::optional<ConversionError> validate_hex_string(const std::string& string) {
    for (char c : string) {
        if (!is_hex_digit(c)) return ConversionError(std::string("Expected hex digit but got '") + c + "'.");
    }
    return {};
}
std::optional<ConversionError> validate_octal_string(const std::string& string) {
    for (char c : string) {
        if (!is_octal_digit(c)) return ConversionError(std::string("Expected octal digit but got '") + c + "'.");
    }
    return {};
}

std::expected<bite_int, ConversionError> string_to_int(const std::string &string) {
    std::string number;
    int base = 10;
    if (string[0] == '0') {
        if (string.size() == 1) return 0;
        if (to_upper(string[1]) == 'X') { // hex
            number = remove_digit_separator(string.substr(2));
            auto error = validate_hex_string(number);
            if (error) return std::unexpected(*error);
            base = 16;
        } else if (to_upper(string[1]) == 'B') { // binary
            number = remove_digit_separator(string.substr(2));
            auto error = validate_binary_string(number);
            if (error) return std::unexpected(*error);
            base = 2;
        } else { // octal
            number = remove_digit_separator(string.substr(1));
            auto error = validate_octal_string(number);
            if (error) return std::unexpected(*error);
            base = 8;
        }
    } else { // decimal
        number = remove_digit_separator(string);
        auto error = validate_decimal_string(number);
        if (error) return std::unexpected(*error);
    }
    return wrapped_stoll(number, base);
}

std::expected<double, ConversionError> string_to_floating(const std::string &string) {
    // todo: validate number!
    std::string number;
    for (char c : string) {
        number += c;
    }

    try {
        return std::stod(number);
    } catch (const std::out_of_range&) {
        return std::unexpected(
            ConversionError(std::string("Literal value is too big."))
        );
    } catch (const std::invalid_argument&) {
        return std::unexpected(
            ConversionError(std::string("Literal parsing failed unexpectedly."))
        );
    }
}
