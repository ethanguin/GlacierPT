#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

struct AllocatedBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
};

struct AllocatedImage {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
};

class VulkanAllocator {
public:
    void initialize(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);

    void shutdown();

    AllocatedBuffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    void destroyBuffer(AllocatedBuffer& buffer);

    AllocatedImage createImage(const VkImageCreateInfo& createInfo, VmaMemoryUsage memoryUsage);
    void destroyImage(AllocatedImage& image);

    VmaAllocator allocator() const {
        return m_allocator;
    }

private:
    VmaAllocator m_allocator = VK_NULL_HANDLE;
};