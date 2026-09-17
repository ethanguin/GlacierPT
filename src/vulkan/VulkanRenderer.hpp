#pragma once

#include "scene/Scene.hpp"
#include "VulkanContext.hpp"
#include "VulkanSwapchain.hpp"
#include "VulkanCommands.hpp"
#include "VulkanFrame.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanRTPipeline.hpp"
#include "VulkanPresentationPipeline.hpp"
#include "VulkanAccelerationStructure.hpp"
#include "VulkanRTResources.hpp"
#include "VulkanScreenshot.hpp"

#include <array>
#include <vector>

class VulkanRenderer {
public:
    void initialize(GLFWwindow* window, const Scene& scene);
    void shutdown();

    void drawFrame();

    void requestScreenshot(const std::string& filename) {
        m_screenshot.request(filename);
    }

    ~VulkanRenderer() {
        shutdown();
    }

private:
    void createFrames();
    void destroyFrames();

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    void recreateSwapchain();

    void submitFrame(VulkanFrame& frame, uint32_t imageIndex);

    void createRayTracingImage();
    void destroyRayTracingImage();

    void createTriangleVertexBuffer();

    void drawTriangle(VkCommandBuffer cmd);

    GLFWwindow* m_window = nullptr;

    VulkanContext m_context;
    VulkanSwapchain m_swapchain;
    VulkanCommands m_commands;
    VulkanPipeline m_pipeline;
    VulkanAccelerationStructure m_accelerationStructure;
    VulkanRTResources m_rayTracingResources;
    VulkanRTPipeline m_rayTracingPipeline;
    VulkanPresentationPipeline m_presentationPipeline;

    std::vector<VkSemaphore> m_renderFinished;

    std::array<VulkanFrame, MAX_FRAMES_IN_FLIGHT> m_frames;

    uint32_t m_currentFrame = 0;

    AllocatedBuffer m_triangleVertexBuffer;

    bool m_rayTracingImageInitialized = false;
    AllocatedImage m_rayTracingImage;
    VkImageView m_rayTracingImageView = VK_NULL_HANDLE;

    VulkanScreenshot m_screenshot;
};