#ifndef STRINGTABLE_H
#define STRINGTABLE_H
#include <string>

#include "../base/unordered_dense.h"

class StringTable {
public:
    using Handle = std::string const*;

    Handle intern(const std::string& string) {
        // mess
        return &*strings.insert(string).first;
    }

private:
    bite::unordered_dense::segmented_set<std::string> strings;
};

#endif //STRINGTABLE_H
