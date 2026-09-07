#include "VulkanCommands.hpp"

#include <stdexcept>
#include <iostream>

void VulkanCommands::initialize(
    VkDevice device,
    uint32_t graphicsQueueFamilyIndex)
{
    m_device = device;

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType =
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

    poolInfo.queueFamilyIndex =
        graphicsQueueFamilyIndex;

    poolInfo.flags =
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(
            m_device,
            &poolInfo,
            nullptr,
            &m_commandPool) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Failed to create command pool."
        );
    }

    std::cout
        << "Vulkan command pool created.\n";
}

void VulkanCommands::shutdown()
{
    if (m_commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(
            m_device,
            m_commandPool,
            nullptr
        );

        m_commandPool = VK_NULL_HANDLE;
    }

    m_device = VK_NULL_HANDLE;
}

VkCommandBuffer VulkanCommands::beginSingleTimeCommands()
{
    VkCommandBufferAllocateInfo allocInfo{};

    allocInfo.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

    allocInfo.level =
        VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    allocInfo.commandPool =
        m_commandPool;

    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;

    if (vkAllocateCommandBuffers(
            m_device,
            &allocInfo,
            &commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Failed to allocate command buffer."
        );
    }

    VkCommandBufferBeginInfo beginInfo{};

    beginInfo.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    beginInfo.flags =
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(
            commandBuffer,
            &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Failed to begin command buffer."
        );
    }

    return commandBuffer;
}

void VulkanCommands::endSingleTimeCommands(
    VkCommandBuffer commandBuffer,
    VkQueue graphicsQueue)
{
    if (vkEndCommandBuffer(
            commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Failed to end command buffer."
        );
    }

    VkSubmitInfo submitInfo{};

    submitInfo.sType =
        VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submitInfo.commandBufferCount = 1;

    submitInfo.pCommandBuffers =
        &commandBuffer;

    if (vkQueueSubmit(
            graphicsQueue,
            1,
            &submitInfo,
            VK_NULL_HANDLE) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "Failed to submit command buffer."
        );
    }

    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(
        m_device,
        m_commandPool,
        1,
        &commandBuffer
    );
}