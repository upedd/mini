#include <cassert>
#include <iostream>

#include "Mini.h"

int main(int argc, char** argv) {
    assert(argc <= 2);
    Mini mini;
    if (argc == 2) {
        mini.run_file(argv[1]);
    } else {
        mini.run_prompt();
    }
    return 0;
}
