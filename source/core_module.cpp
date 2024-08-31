#include "core_module.h"

#include "shared/SharedContext.h"
#include "Object.h"
#include "VM.h"

void apply_core(VM* vm, SharedContext* context) {
    auto* int_add = new ForeignFunction {
        .arity = 1,
        .name = context->intern("add"),
        .function = [](FunctionContext ctx) {
            // TODO: errors!
            BITE_ASSERT(std::holds_alternative<bite_int>(ctx.get_instance()));
            bite_int a = std::get<bite_int>(ctx.get_instance());
            bite_int b = std::get<bite_int>(ctx.get_instance());
            return a + b;
        }
    };

    auto* int_constructor = new ForeginFunctionObject(new ForeignFunction {
        .arity = 1,
        .name = context->intern("constructor"),
        .function = [](FunctionContext ctx) {
            // TODO: stub!
            return ctx.get_arg(0);
        }
    });

    auto* int_class = new Class("Int");
    int_class->constructor = int_constructor; // TODO?
    auto* int_add_object = new ForeginFunctionObject(int_add);
    int_class->methods["add"] = ClassValue {.value = int_add_object, .attributes = {}, .is_computed = false};
    vm->int_class = int_class;
    vm->allocate({int_add_object, int_constructor, int_class});
}
