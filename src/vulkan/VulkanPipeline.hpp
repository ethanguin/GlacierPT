#pragma once

#include <vulkan/vulkan.h>

class VulkanPipeline {
public:
    void initialize(VkDevice device, VkFormat colorFormat);

    void shutdown(VkDevice device);

    VkPipeline pipeline() const {
        return m_pipeline;
    }

    VkPipelineLayout layout() const {
        return m_pipelineLayout;
    }

private:
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};