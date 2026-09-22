#include <iostream>
#include "common.h"
#include "arm.h"

struct XlnxZCU102Board: ARMBoard {
    void say_hello();
};

void XlnxZCU102Board::say_hello()
{
    some_arm_thing();
    std::cout << ANSI_START << "I am the xlnx_zcu102 board"
              << ANSI_END << std::endl;
}

static XlnxZCU102Board xlnx_zcu102;
