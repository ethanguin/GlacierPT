#include "VulkanAllocator.hpp"

#include <stdexcept>

void VulkanAllocator::initialize(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device) {
    if (m_allocator != VK_NULL_HANDLE) {
        throw std::runtime_error("VulkanAllocator is already initialized.");
    }

    VmaAllocatorCreateInfo allocatorInfo{};

    allocatorInfo.instance = instance;
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;

    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;

    VkResult result = vmaCreateAllocator(&allocatorInfo, &m_allocator);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create VMA allocator.");
    }
}

void VulkanAllocator::shutdown() {
    if (m_allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(m_allocator);
        m_allocator = VK_NULL_HANDLE;
    }
}

AllocatedBuffer VulkanAllocator::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;

    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = memoryUsage;

    AllocatedBuffer result{};

    VkResult vkResult = vmaCreateBuffer(m_allocator, &bufferInfo, &allocationInfo, &result.buffer, &result.allocation, nullptr);

    if (vkResult != VK_SUCCESS) {
        throw std::runtime_error("Failed to create VMA buffer");
    }

    return result;
}

void VulkanAllocator::destroyBuffer(AllocatedBuffer& buffer) {
    if (buffer.buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(m_allocator, buffer.buffer, buffer.allocation);

        buffer.buffer = VK_NULL_HANDLE;
        buffer.allocation = VK_NULL_HANDLE;
    }
}

AllocatedImage VulkanAllocator::createImage(const VkImageCreateInfo& createInfo, VmaMemoryUsage memoryUsage) {
    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = memoryUsage;

    AllocatedImage result{};

    VkResult vkResult = vmaCreateImage(m_allocator, &createInfo, &allocationInfo, &result.image, &result.allocation, nullptr);

    if (vkResult != VK_SUCCESS) {
        throw std::runtime_error("Failed to create VMA image");
    }

    return result;
}

void VulkanAllocator::destroyImage(AllocatedImage& image) {
    if (image.image != VK_NULL_HANDLE) {
        vmaDestroyImage(m_allocator, image.image, image.allocation);

        image.image = VK_NULL_HANDLE;
        image.allocation = VK_NULL_HANDLE;
    }
}