#pragma once

#include <vulkan/vulkan.h>

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

struct VulkanFrame {
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

    VkSemaphore imageAvailable = VK_NULL_HANDLE;
    VkSemaphore renderFinished = VK_NULL_HANDLE;

    VkFence renderFence = VK_NULL_HANDLE;
};