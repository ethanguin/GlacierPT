#pragma once

#include "VulkanAllocator.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>

class VulkanContext;

class VulkanScreenshot {
public:
    void initialize(VulkanContext& context, uint32_t width, uint32_t height, VkFormat format);

    void shutdown();

    void request(const std::string& filename);

    // Called while recording the frame's command buffer.
    bool recordCopy(VkCommandBuffer cmd, VkImage swapchainImage, uint32_t frameSlot);

    // Called after waiting for the corresponding frame fence.
    void saveIfReady(uint32_t frameSlot);

    bool pending() const {
        return m_requested || m_copyRecorded;
    }

private:
    void createStagingBuffer();
    void destroyStagingBuffer();
    void writePPM();
    void writePNG();

    VulkanContext* m_context = nullptr;

    AllocatedBuffer m_stagingBuffer{};

    VkDeviceSize m_bufferSize = 0;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    VkFormat m_format = VK_FORMAT_UNDEFINED;

    std::string m_filename;

    bool m_requested = false;
    bool m_copyRecorded = false;

    // The frame slot whose fence protects the staging buffer.
    uint32_t m_captureFrameSlot = 0;
};