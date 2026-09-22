#include <iostream>
#include "common.h"
#include "virtio.h"

struct VirtioMMIODevice: VirtioDevice {
    void say_hello();
};

void VirtioMMIODevice::say_hello()
{
    some_virtio_thing();
    std::cout << ANSI_START << "virtio-mmio is available"
              << ANSI_END << std::endl;
}

static VirtioMMIODevice virtio_mmio;
