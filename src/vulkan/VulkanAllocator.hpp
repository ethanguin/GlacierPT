#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

class VulkanAllocator
{
public:
    void initialize(
        VkInstance instance,
        VkPhysicalDevice physicalDevice,
        VkDevice device);

    void shutdown();

    VmaAllocator allocator() const
    {
        return m_allocator;
    }

private:
    VmaAllocator m_allocator = VK_NULL_HANDLE;
};