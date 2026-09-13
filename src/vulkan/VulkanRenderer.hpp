#pragma once

#include "VulkanContext.hpp"
#include "VulkanSwapchain.hpp"
#include "VulkanCommands.hpp"
#include "VulkanFrame.hpp"

#include <array>

class VulkanRenderer {
public:
    void initialize(GLFWwindow* window);
    void shutdown();

    void drawFrame();

private:
    void createFrames();
    void destroyFrames();

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    void recreateSwapchain();

    void submitFrame(VulkanFrame& frame);

    GLFWwindow* m_window = nullptr;

    VulkanContext m_context;
    VulkanSwapchain m_swapchain;
    VulkanCommands m_commands;

    std::array<VulkanFrame, MAX_FRAMES_IN_FLIGHT> m_frames;

    uint32_t m_currentFrame = 0;
};