#pragma once

#include <vulkan/vulkan.h>

class VulkanCommands {
public:
    void initialize(VkDevice device, uint32_t graphicsQueueFamilyIndex);

    void shutdown();

    VkCommandPool commandPool() const;

    VkCommandBuffer allocateCommandBuffer();

    void freeCommandBuffer(VkCommandBuffer commandBuffer);

    VkCommandBuffer beginSingleTimeCommands();

    void endSingleTimeCommands(VkCommandBuffer commandBuffer, VkQueue graphicsQueue);

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
};