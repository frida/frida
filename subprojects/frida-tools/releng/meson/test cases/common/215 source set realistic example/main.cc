#include <iostream>
#include <vector>
#include "common.h"

Board* boards;
Device* devices;
Dependency* deps;

Board::Board() { this->next = boards; boards = this; }
Board::~Board() {}

Device::Device() { this->next = devices; devices = this; }
Device::~Device() {}

Dependency::Dependency() { this->next = deps; deps = this; }
Dependency::~Dependency() {}

int main(void)
{
    some_random_function();
    for (auto d = deps; d; d = d->next)
        d->initialize();

    initialize_target();
    for (auto b = boards; b; b = b->next) {
        std::cout << ANSI_START << b->target() << " - " << ANSI_END;
        b->say_hello();
    }

    for (auto d = devices; d; d = d->next)
        d->say_hello();
}
