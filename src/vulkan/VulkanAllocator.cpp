#include "VulkanAllocator.hpp"

#include <stdexcept>

void VulkanAllocator::initialize(
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    VkDevice device)
{
    if (m_allocator != VK_NULL_HANDLE)
    {
        throw std::runtime_error(
            "VulkanAllocator is already initialized."
        );
    }

    VmaAllocatorCreateInfo allocatorInfo{};

    allocatorInfo.instance = instance;
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;

    allocatorInfo.vulkanApiVersion =
        VK_API_VERSION_1_3;

    VkResult result =
        vmaCreateAllocator(
            &allocatorInfo,
            &m_allocator
        );

    if (result != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Failed to create VMA allocator."
        );
    }
}

void VulkanAllocator::shutdown()
{
    if (m_allocator != VK_NULL_HANDLE)
    {
        vmaDestroyAllocator(m_allocator);
        m_allocator = VK_NULL_HANDLE;
    }
}