#include <iostream>
#include "common.h"
#include "virtio.h"

struct VirtioPCIDevice: VirtioDevice {
    void say_hello();
};

void VirtioPCIDevice::say_hello()
{
    some_virtio_thing();
    std::cout << ANSI_START << "virtio-pci is available"
              << ANSI_END << std::endl;
}

static VirtioPCIDevice virtio_pci;
