#include "core_module.h"

#include "shared/SharedContext.h"
#include "Object.h"
#include "VM.h"

// TODO: refactor
// TODO: error handling!
// TODO: missing functionality
// TODO: refactor ints and floats

void apply_core(VM* vm, SharedContext* context) {
    auto* int_class = new Class("Int");
    vm->int_class = int_class;

    auto* int_add = new ForeginFunctionObject(
        new ForeignFunction {
            .arity = 1,
            .name = context->intern("add"),
            .function = [](FunctionContext ctx) {
                return ctx.get_instance().get<bite_int>() + ctx.get_arg(0).get<bite_int>();
            }
        }
    );
    int_class->methods["add"] = ClassValue { .value = int_add, .attributes = {}, .is_computed = false };

    auto* int_multiply = new ForeginFunctionObject(
        new ForeignFunction {
            .arity = 1,
            .name = context->intern("multiply"),
            .function = [](FunctionContext ctx) {
                return ctx.get_instance().get<bite_int>() * ctx.get_arg(0).get<bite_int>();
            }
        }
    );
    int_class->methods["multiply"] = ClassValue { .value = int_multiply, .attributes = {}, .is_computed = false };

    auto* int_subtract = new ForeginFunctionObject(
        new ForeignFunction {
            .arity = 1,
            .name = context->intern("subtract"),
            .function = [](FunctionContext ctx) {
                return ctx.get_instance().get<bite_int>() - ctx.get_arg(0).get<bite_int>();
            }
        }
    );
    int_class->methods["subtract"] = ClassValue { .value = int_subtract, .attributes = {}, .is_computed = false };

    auto* int_divide = new ForeginFunctionObject(
        new ForeignFunction {
            .arity = 1,
            .name = context->intern("divide"),
            .function = [](FunctionContext ctx) {
                return static_cast<double>(ctx.get_instance().get<bite_int>()) / ctx.get_arg(0).get<bite_int>();
            }
        }
    );
    int_class->methods["divide"] = ClassValue { .value = int_divide, .attributes = {}, .is_computed = false };

    auto* int_floor_div = new ForeginFunctionObject(
        new ForeignFunction {
            .arity = 1,
            .name = context->intern("floor_div"),
            .function = [](FunctionContext ctx) {
                return ctx.get_instance().get<bite_int>() / ctx.get_arg(0).get<bite_int>();
            }
        }
    );
    int_class->methods["floor_div"] = ClassValue { .value = int_floor_div, .attributes = {}, .is_computed = false };

    auto* int_modulo = new ForeginFunctionObject(
        new ForeignFunction {
            .arity = 1,
            .name = context->intern("modulo"),
            .function = [](FunctionContext ctx) {
                return ctx.get_instance().get<bite_int>() % ctx.get_arg(0).get<bite_int>();
            }
        }
    );
    int_class->methods["modulo"] = ClassValue { .value = int_modulo, .attributes = {}, .is_computed = false };

    auto* int_binary_not = new ForeginFunctionObject(
        new ForeignFunction {
            .arity = 0,
            .name = context->intern("binary_not"),
            .function = [](FunctionContext ctx) {
                return ~ctx.get_instance().get<bite_int>();
            }
        }
    );
    int_class->methods["binary_not"] = ClassValue { .value = int_binary_not, .attributes = {}, .is_computed = false };

    auto* int_equals = new ForeginFunctionObject(
        new ForeignFunction {
            .arity = 1,
            .name = context->intern("equals"),
            .function = [](FunctionContext ctx) {
                return ctx.get_instance().get<bite_int>() == ctx.get_arg(0).get<bite_int>();
            }
        }
    );
    int_class->methods["equals"] = ClassValue { .value = int_equals, .attributes = {}, .is_computed = false };

    auto* int_not_equals = new ForeginFunctionObject(
        new ForeignFunction {
            .arity = 1,
            .name = context->intern("not_equals"),
            .function = [](FunctionContext ctx) {
                return ctx.get_instance().get<bite_int>() != ctx.get_arg(0).get<bite_int>();
            }
        }
    );
    int_class->methods["not_equals"] = ClassValue { .value = int_not_equals, .attributes = {}, .is_computed = false };

    auto* int_less = new ForeginFunctionObject(
        new ForeignFunction {
            .arity = 1,
            .name = context->intern("less"),
            .function = [](FunctionContext ctx) {
                return ctx.get_instance().get<bite_int>() < ctx.get_arg(0).get<bite_int>();
            }
        }
    );
    int_class->methods["less"] = ClassValue { .value = int_less, .attributes = {}, .is_computed = false };

    auto* int_less_equal = new ForeginFunctionObject(
            new ForeignFunction {
                .arity = 1,
                .name = context->intern("less_equal"),
                .function = [](FunctionContext ctx) {
                    return ctx.get_instance().get<bite_int>() <= ctx.get_arg(0).get<bite_int>();
                }
            }
        );
    int_class->methods["int_less_equal"] = ClassValue { .value = int_less_equal, .attributes = {}, .is_computed = false };

    auto* int_greater = new ForeginFunctionObject(
            new ForeignFunction {
                .arity = 1,
                .name = context->intern("greater"),
                .function = [](FunctionContext ctx) {
                    return ctx.get_instance().get<bite_int>() > ctx.get_arg(0).get<bite_int>();
                }
            }
        );
    int_class->methods["greater"] = ClassValue { .value = int_greater, .attributes = {}, .is_computed = false };

    auto* int_greater_equal = new ForeginFunctionObject(
            new ForeignFunction {
                .arity = 1,
                .name = context->intern("greater_equal"),
                .function = [](FunctionContext ctx) {
                    return ctx.get_instance().get<bite_int>() >= ctx.get_arg(0).get<bite_int>();
                }
            }
        );
    int_class->methods["greater_equal"] = ClassValue { .value = int_greater_equal, .attributes = {}, .is_computed = false };

    auto* int_binary_and = new ForeginFunctionObject(
        new ForeignFunction {
            .arity = 1,
            .name = context->intern("binary_and"),
            .function = [](FunctionContext ctx) {
                return ctx.get_instance().get<bite_int>() & ctx.get_arg(0).get<bite_int>();
            }
        }
    );


    int_class->methods["binary_and"] = ClassValue { .value = int_binary_and, .attributes = {}, .is_computed = false };

    auto* int_binary_or = new ForeginFunctionObject(
        new ForeignFunction {
            .arity = 1,
            .name = context->intern("binary_or"),
            .function = [](FunctionContext ctx) {
                return ctx.get_instance().get<bite_int>() | ctx.get_arg(0).get<bite_int>();
            }
        }
    );
    int_class->methods["binary_or"] = ClassValue { .value = int_binary_or, .attributes = {}, .is_computed = false };

    auto* int_shift_left = new ForeginFunctionObject(
        new ForeignFunction {
            .arity = 1,
            .name = context->intern("shift_left"),
            .function = [](FunctionContext ctx) {
                return ctx.get_instance().get<bite_int>() << ctx.get_arg(0).get<bite_int>();
            }
        }
    );
    int_class->methods["shift_left"] = ClassValue { .value = int_shift_left, .attributes = {}, .is_computed = false };

    auto* int_shift_right = new ForeginFunctionObject(
        new ForeignFunction {
            .arity = 1,
            .name = context->intern("shift_right"),
            .function = [](FunctionContext ctx) {
                return ctx.get_instance().get<bite_int>() >> ctx.get_arg(0).get<bite_int>();
            }
        }
    );
    int_class->methods["shift_right"] = ClassValue { .value = int_shift_right, .attributes = {}, .is_computed = false };

    auto* int_binary_xor = new ForeginFunctionObject(
        new ForeignFunction {
            .arity = 1,
            .name = context->intern("binary_xor"),
            .function = [](FunctionContext ctx) {
                return ctx.get_instance().get<bite_int>() + ctx.get_arg(0).get<bite_int>();
            }
        }
    );
    int_class->methods["binary_xor"] = ClassValue { .value = int_binary_xor, .attributes = {}, .is_computed = false };

    int_class->methods["add"] = ClassValue { .value = int_add, .attributes = {}, .is_computed = false };

    auto* string_class = new Class("String");
    vm->string_class = string_class;

    auto* string_add = new ForeginFunctionObject(
            new ForeignFunction {
                .arity = 1,
                .name = context->intern("add"),
                .function = [](FunctionContext ctx) {
                    return ctx.get_instance().get<std::string>() + ctx.get_arg(0).get<std::string>();
                }
            }
        );
    string_class->methods["add"] = ClassValue { .value = string_add, .attributes = {}, .is_computed = false };

    vm->allocate(
        {
            int_add,
            int_class,
            int_binary_and,
            int_binary_not,
            int_binary_or,
            int_binary_xor,
            int_divide,
            int_equals,
            int_floor_div,
            int_less,
            int_modulo,
            int_multiply,
            int_shift_left,
            int_shift_right,
            int_subtract,
            int_less_equal,
            int_greater,
            int_greater_equal,
            int_not_equals,
            string_class,
            string_add
        }
    );
}
