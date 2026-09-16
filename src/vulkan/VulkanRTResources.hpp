#pragma once
#include <vulkan/vulkan.h>
#include "VulkanContext.hpp"
#include "VulkanAccelerationStructure.hpp"

class VulkanRTResources {
public:
    void initialize(VulkanContext& context, VulkanAccelerationStructure& accelerationStructure, VkImageView outputImageView);

    void shutdown();

    VkDescriptorSetLayout descriptorSetLayout() const {
        return m_descriptorSetLayout;
    }

    VkDescriptorSet descriptorSet() const {
        return m_descriptorSet;
    }

private:
    VulkanContext* m_context = nullptr;

    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
};