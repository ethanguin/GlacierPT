#pragma once

#include "VulkanContext.hpp"
#include "VulkanCommands.hpp"

#include <vulkan/vulkan.h>
#include <vector>

class VulkanAccelerationStructure {
public:
    void initialize(VulkanContext& context, VulkanCommands& commands);
    void shutdown();

    void buildBLAS();
    void buildTLAS();

    VkAccelerationStructureKHR tlas() const {
        return m_tlas;
    }

private:
    struct BLAS {
        VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;

        AllocatedBuffer backingBuffer{};
        AllocatedBuffer aabbBuffer{};

        VkDeviceAddress deviceAddress = 0;
    };

    VkDeviceAddress getBufferDeviceAddress(VkBuffer buffer);

    VkDeviceAddress getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR accelerationStructure);

    void loadFunctions();

    void createBLAS(const VkAabbPositionsKHR& aabb);

    VulkanContext* m_context = nullptr;
    VulkanCommands* m_commands = nullptr;

    VkDevice m_device = VK_NULL_HANDLE;

    std::vector<BLAS> m_blas;

    VkAccelerationStructureKHR m_tlas = VK_NULL_HANDLE;

    AllocatedBuffer m_tlasBuffer{};
    AllocatedBuffer m_tlasInstanceBuffer{};

    PFN_vkCreateAccelerationStructureKHR m_vkCreateAccelerationStructureKHR = nullptr;

    PFN_vkDestroyAccelerationStructureKHR m_vkDestroyAccelerationStructureKHR = nullptr;

    PFN_vkGetAccelerationStructureBuildSizesKHR m_vkGetAccelerationStructureBuildSizesKHR = nullptr;

    PFN_vkCmdBuildAccelerationStructuresKHR m_vkCmdBuildAccelerationStructuresKHR = nullptr;

    PFN_vkGetAccelerationStructureDeviceAddressKHR m_vkGetAccelerationStructureDeviceAddressKHR = nullptr;
};