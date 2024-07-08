#ifndef GARBAGECOLLECTOR_H
#define GARBAGECOLLECTOR_H
#include <list>
#include <queue>
#include "Object.h"

class GarbageCollector {
public:
    void collect();
    void mark(Object* object);
    void mark(const Value& value);
    void add_object(Object* object);

    [[nodiscard]] std::size_t get_memory_used() const {
        return memory_used;
    }
private:
    void trace_references();
    void blacken_object(Object* object);
    void sweep();

    std::list<Object*> objects;
    std::queue<Object*> grey_objects;
    std::size_t memory_used = 0;
};



#endif //GARBAGECOLLECTOR_H
