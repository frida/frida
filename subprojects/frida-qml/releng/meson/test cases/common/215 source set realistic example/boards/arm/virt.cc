#include <iostream>
#include "common.h"
#include "arm.h"

struct VirtBoard: ARMBoard {
    void say_hello();
};

void VirtBoard::say_hello()
{
    some_arm_thing();
    std::cout << ANSI_START << "I am the virt board"
              << ANSI_END << std::endl;
}

static VirtBoard virt;
