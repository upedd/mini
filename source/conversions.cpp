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
        if (c != '_')
            result += c;
    }
    return result;
}


std::optional<ConversionError> validate_decimal_string(const std::string& string) {
    for (char c : string) {
        if (!is_digit(c))
            return ConversionError(std::string("Expected decimal digit but got '") + c + "'.");
    }
    return {};
}

std::optional<ConversionError> validate_binary_string(const std::string& string) {
    for (char c : string) {
        if (!is_binary_digit(c))
            return ConversionError(std::string("Expected binary digit but got '") + c + "'.");
    }
    return {};
}

std::optional<ConversionError> validate_hex_string(const std::string& string) {
    for (char c : string) {
        if (!is_hex_digit(c))
            return ConversionError(std::string("Expected hex digit but got '") + c + "'.");
    }
    return {};
}

std::optional<ConversionError> validate_octal_string(const std::string& string) {
    for (char c : string) {
        if (!is_octal_digit(c))
            return ConversionError(std::string("Expected octal digit but got '") + c + "'.");
    }
    return {};
}

std::expected<bite_int, ConversionError> string_to_int(const std::string& string) {
    std::string number;
    int base = 10;
    if (string[0] == '0') {
        if (string.size() == 1)
            return 0;
        if (to_upper(string[1]) == 'X') { // hex
            number = remove_digit_separator(string.substr(2));
            if (auto error = validate_hex_string(number))
                return std::unexpected(*error);
            base = 16;
        } else if (to_upper(string[1]) == 'B') { // binary
            number = remove_digit_separator(string.substr(2));
            if (auto error = validate_binary_string(number))
                return std::unexpected(*error);
            base = 2;
        } else { // octal
            number = remove_digit_separator(string.substr(1));
            if (auto error = validate_octal_string(number))
                return std::unexpected(*error);
            base = 8;
        }
    } else { // decimal
        number = remove_digit_separator(string);
        if (auto error = validate_decimal_string(number))
            return std::unexpected(*error);
    }
    return wrapped_stoll(number, base);
}

std::optional<ConversionError> validate_floating_string(const std::string& string) {
    bool is_hex = string.starts_with("0x") || string.starts_with("0X");
    std::string number = is_hex ? string.substr(2) : string;
    bool in_exponent = false;
    for (auto& c : number) {
        if (in_exponent) {
            if (is_hex && !is_hex_digit(c)) {
                return ConversionError(std::string("Expected hex digit in exponent but got '") + c + "'.");
            }
            if (!is_hex && !is_digit(c)) {
                return ConversionError(std::string("Expected decimal digit in exponent but got '") + c + "'.");
            }
        } else {
            if (c == '.')
                continue;
            // we don't need to validate if literal contains multiple dot separators as lexer guarantees only one will be present
            if ((is_hex && to_upper(c) == 'P') || (!is_hex && to_upper(c) == 'E')) {
                in_exponent = true;
                continue;
            }
            if (is_hex && !is_hex_digit(c)) {
                return ConversionError(std::string("Expected hex digit but got '") + c + "'.");
            }
            if (!is_hex && !is_digit(c)) {
                return ConversionError(std::string("Expected decimal digit but got '") + c + "'.");
            }
        }
    }
    return {};
}

std::expected<bite_float, ConversionError> string_to_floating(const std::string& string) {
    std::string number = remove_digit_separator(string);
    if (auto error = validate_floating_string(string))
        return std::unexpected(*error);

    try {
        return std::stod(number);
    } catch (const std::out_of_range&) {
        return make_conversion_error("Literal value is too big.");
    } catch (const std::invalid_argument&) {
        return make_conversion_error("Literal parsing failed unexpectedly.");
    }
}
