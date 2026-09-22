#pragma once

#include "scene/Scene.hpp"
#include "scene/Camera.hpp"
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
    void initialize(GLFWwindow* window, const Scene& scene, const Camera& camera, uint32_t renderWidth, uint32_t renderHeight);
    void shutdown();

    void drawFrame();

    void setCamera(const Camera& camera) {
        m_camera = camera;
        m_rayTracingResources.updateCamera(m_camera);
    }

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

    GLFWwindow* m_window = nullptr;
    VkExtent2D m_renderExtent{};
    Camera m_camera;
    uint32_t m_lightCount = 0;

    VulkanContext m_context;
    VulkanSwapchain m_swapchain;
    VulkanCommands m_commands;
    VulkanAccelerationStructure m_accelerationStructure;
    VulkanRTResources m_rayTracingResources;
    VulkanRTPipeline m_rayTracingPipeline;
    VulkanPresentationPipeline m_presentationPipeline;

    std::vector<VkSemaphore> m_renderFinished;

    std::array<VulkanFrame, MAX_FRAMES_IN_FLIGHT> m_frames;

    uint32_t m_currentFrame = 0;

    bool m_rayTracingImageInitialized = false;
    AllocatedImage m_rayTracingImage;
    VkImageView m_rayTracingImageView = VK_NULL_HANDLE;

    VulkanScreenshot m_screenshot;
};