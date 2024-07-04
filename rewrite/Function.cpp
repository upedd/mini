#include "Function.h"

int Function::add_constant(const Value &value) {
    constants.push_back(value);
    return constants.size() - 1;
}

Value Function::get_constant(int idx) {
    return constants[idx];
}

std::vector<Value> & Function::get_constants() {
    return constants;
}
