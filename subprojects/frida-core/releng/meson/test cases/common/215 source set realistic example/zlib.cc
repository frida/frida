#include <iostream>
#include "common.h"

struct ZLibDependency : Dependency {
    void initialize();
};

void ZLibDependency::initialize() {
    if (ZLIB && ANOTHER) {
        std::cout << ANSI_START << "hello from zlib"
                  << ANSI_END << std::endl;
    }
}

ZLibDependency zlib;
