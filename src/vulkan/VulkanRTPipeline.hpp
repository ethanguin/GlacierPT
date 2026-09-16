#pragma once

#include "VulkanContext.hpp"
#include "VulkanRTResources.hpp"

#include <vulkan/vulkan.h>

class VulkanRTPipeline {
public:
    void initialize(VulkanContext& context, VulkanRTResources& resources);

    void shutdown();

    VkPipeline pipeline() const {
        return m_pipeline;
    }

    VkPipelineLayout layout() const {
        return m_pipelineLayout;
    }

    VkStridedDeviceAddressRegionKHR raygenRegion() const {
        return m_raygenRegion;
    }

    VkStridedDeviceAddressRegionKHR missRegion() const {
        return m_missRegion;
    }

    VkStridedDeviceAddressRegionKHR hitRegion() const {
        return m_hitRegion;
    }

    PFN_vkCmdTraceRaysKHR traceRaysFunction() const {
        return m_vkCmdTraceRaysKHR;
    }

private:
    VkShaderModule loadShaderModule(const char* path);

    void createShaderBindingTable();

    AllocatedBuffer m_shaderBindingTable{};

    VkStridedDeviceAddressRegionKHR m_raygenRegion{};
    VkStridedDeviceAddressRegionKHR m_missRegion{};
    VkStridedDeviceAddressRegionKHR m_hitRegion{};

    VulkanContext* m_context = nullptr;

    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;

    VkShaderModule m_raygenShader = VK_NULL_HANDLE;
    VkShaderModule m_missShader = VK_NULL_HANDLE;
    VkShaderModule m_intersectionShader = VK_NULL_HANDLE;
    VkShaderModule m_closestHitShader = VK_NULL_HANDLE;

    PFN_vkCreateRayTracingPipelinesKHR m_vkCreateRayTracingPipelinesKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR m_vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkCmdTraceRaysKHR m_vkCmdTraceRaysKHR = nullptr;
};