#pragma once

#include "scene/Scene.hpp"
#include <vulkan/vulkan.h>
#include "VulkanContext.hpp"
#include "VulkanAccelerationStructure.hpp"

// explicit GPU sphere structure so it doesn't have to be connected to attributes for the scene sphere and add its own padding to map to HLSL
struct GPUSphere {
    glm::vec4 positionRadius;
    glm::vec4 color;
};
static_assert(sizeof(GPUSphere) == 32);

class VulkanRTResources {
public:
    void initialize(VulkanContext& context, VulkanAccelerationStructure& accelerationStructure, const Scene& scene, VkImageView outputImageView);

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

    // TODO add actual geo buffer
    AllocatedBuffer m_sphereBuffer{};
};