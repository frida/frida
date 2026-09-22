#include <iostream>
#include "common.h"
#include "arm.h"

struct VersatilePBBoard: ARMBoard {
    void say_hello();
};

void VersatilePBBoard::say_hello()
{
    some_arm_thing();
    std::cout << ANSI_START << "I am the versatilepb board"
              << ANSI_END << std::endl;
}

static VersatilePBBoard versatilepb;
