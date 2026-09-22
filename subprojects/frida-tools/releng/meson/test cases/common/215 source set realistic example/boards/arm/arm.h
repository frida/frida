#ifndef ARM_H
#define ARM_H 1

#include "common.h"

struct ARMBoard: Board {
    const char *target();
    void some_arm_thing();
};


#endif
