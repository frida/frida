#include <vulkan/vulkan.h>
#include <stdio.h>

int main(void)
{
    VkInstanceCreateInfo instance_create_info = {
            VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            NULL,
            0,
            NULL,
            0,
            NULL,
            0,
            NULL,
    };

    // we don't actually require instance creation to succeed since
    // we cannot expect test environments to have a vulkan driver installed.
    // As long as this does not produce as segmentation fault or similar,
    // everything's alright.
    VkInstance instance;
    if(vkCreateInstance(&instance_create_info, NULL, &instance) == VK_SUCCESS)
        vkDestroyInstance(instance, NULL);

    return 0;
}
