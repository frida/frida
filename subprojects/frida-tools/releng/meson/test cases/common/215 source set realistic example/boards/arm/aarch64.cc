#include "common.h"
#include <iostream>

void initialize_target()
{
    std::cout << ANSI_START << "some " << THE_TARGET
              << " initialization" << ANSI_END << std::endl;
}
