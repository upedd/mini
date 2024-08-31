/**
* Defines classes needed for bite runtime
*/

#ifndef CORE_MODULE_H
#define CORE_MODULE_H
#include "Object.h"

namespace bite::core_module {
    class Number final : public Class {
    public:
        explicit Number(std::string name) : Class(std::move(name)) {}

        std::size_t get_size() override {
            return sizeof(Number);
        }
        std::string to_string() override {
            return Class::to_string();
        }
        void mark_references(GarbageCollector& gc) override {
            return Class::mark_references(gc);
        }
    };

    inline void apply() {
    }
}

#endif //CORE_MODULE_H
