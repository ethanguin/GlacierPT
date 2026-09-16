#pragma once

#include "VulkanContext.hpp"

#include <vulkan/vulkan.h>

class VulkanPresentationPipeline {
public:
    void initialize(VulkanContext& context, VkFormat colorFormat);
    void shutdown();

    VkPipeline pipeline() const {
        return m_pipeline;
    }

    VkPipelineLayout layout() const {
        return m_pipelineLayout;
    }

    VkDescriptorSetLayout descriptorSetLayout() const {
        return m_descriptorSetLayout;
    }

    VkDescriptorSet descriptorSet() const {
        return m_descriptorSet;
    }

    VkSampler sampler() const {
        return m_sampler;
    }

    void updateDescriptorSet(VkImageView imageView);

private:
    VulkanContext* m_context = nullptr;

    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
};