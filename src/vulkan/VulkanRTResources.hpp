#pragma once

#include "scene/Scene.hpp"
#include "scene/Camera.hpp"
#include <vulkan/vulkan.h>
#include "VulkanContext.hpp"
#include "VulkanAccelerationStructure.hpp"

class VulkanRTResources {
public:
    void initialize(VulkanContext& context, VulkanAccelerationStructure& accelerationStructure, const Scene& scene, const Camera& camera,
                    VkImageView outputImageView);

    void shutdown();

    void updateCamera(const Camera& camera);

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
    AllocatedBuffer m_lightBuffer{};
    AllocatedBuffer m_ambLightBuffer{};
    AllocatedBuffer m_cameraBuffer{};
};