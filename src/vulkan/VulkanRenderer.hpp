#pragma once

#include "VulkanContext.hpp"
#include "VulkanSwapchain.hpp"
#include "VulkanCommands.hpp"
#include "VulkanFrame.hpp"
#include "VulkanPipeline.hpp"

#include <array>
#include <vector>

class VulkanRenderer {
public:
    void initialize(GLFWwindow* window);
    void shutdown();

    void drawFrame();

    ~VulkanRenderer() {
        shutdown();
    }

private:
    void createFrames();
    void destroyFrames();

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    void recreateSwapchain();

    void submitFrame(VulkanFrame& frame, uint32_t imageIndex);

    void createTriangleVertexBuffer();

    void drawTriangle(VkCommandBuffer cmd);

    GLFWwindow* m_window = nullptr;

    VulkanContext m_context;
    VulkanSwapchain m_swapchain;
    VulkanCommands m_commands;
    VulkanPipeline m_pipeline;

    std::vector<VkSemaphore> m_renderFinished;

    std::array<VulkanFrame, MAX_FRAMES_IN_FLIGHT> m_frames;

    uint32_t m_currentFrame = 0;

    AllocatedBuffer m_triangleVertexBuffer;
};