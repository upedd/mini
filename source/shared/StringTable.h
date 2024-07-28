#ifndef STRINGTABLE_H
#define STRINGTABLE_H
#include <iostream>
#include <unordered_set>
#include <string>

class StringTable {
public:
    using Handle = std::string const*;

    Handle intern(const std::string& string) {
        std::cout << string << '\n';
        return new std::string("abc");
        // return &(*strings.insert(string).first);
    }

private:
    // move to segmented set
    std::unordered_set<std::string> strings;
};

#endif //STRINGTABLE_H
