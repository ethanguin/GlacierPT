#include "VulkanAccelerationStructure.hpp"

#include <stdexcept>

void VulkanAccelerationStructure::initialize(VulkanContext& context, VulkanCommands& commands) {
    m_context = &context;
    m_commands = &commands;

    m_device = context.device();

    loadFunctions();
}

void VulkanAccelerationStructure::shutdown() {
    if (m_device == VK_NULL_HANDLE)
        return;

    if (m_tlas != VK_NULL_HANDLE) {
        m_vkDestroyAccelerationStructureKHR(m_device, m_tlas, nullptr);

        m_tlas = VK_NULL_HANDLE;
    }

    m_context->allocator().destroyBuffer(m_tlasBuffer);

    m_context->allocator().destroyBuffer(m_tlasInstanceBuffer);

    for (auto& blas : m_blas) {
        if (blas.accelerationStructure != VK_NULL_HANDLE) {
            m_vkDestroyAccelerationStructureKHR(m_device, blas.accelerationStructure, nullptr);

            blas.accelerationStructure = VK_NULL_HANDLE;
        }

        m_context->allocator().destroyBuffer(blas.backingBuffer);

        m_context->allocator().destroyBuffer(blas.aabbBuffer);
    }

    m_blas.clear();
}

void VulkanAccelerationStructure::loadFunctions() {
    m_vkCreateAccelerationStructureKHR =
        reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(m_device, "vkCreateAccelerationStructureKHR"));

    m_vkDestroyAccelerationStructureKHR =
        reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(m_device, "vkDestroyAccelerationStructureKHR"));

    m_vkGetAccelerationStructureBuildSizesKHR =
        reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(m_device, "vkGetAccelerationStructureBuildSizesKHR"));

    m_vkCmdBuildAccelerationStructuresKHR =
        reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(m_device, "vkCmdBuildAccelerationStructuresKHR"));

    m_vkGetAccelerationStructureDeviceAddressKHR =
        reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(m_device, "vkGetAccelerationStructureDeviceAddressKHR"));

    if (!m_vkCreateAccelerationStructureKHR || !m_vkDestroyAccelerationStructureKHR || !m_vkGetAccelerationStructureBuildSizesKHR ||
        !m_vkCmdBuildAccelerationStructuresKHR || !m_vkGetAccelerationStructureDeviceAddressKHR) {

        throw std::runtime_error("Failed to load Vulkan acceleration structure functions.");
    }
}

void VulkanAccelerationStructure::buildBLAS() {
    m_blas.clear();

    const VkAabbPositionsKHR aabbs[] = {
        {-2.5f, -1.0f, -1.0f, -0.5f, 1.0f, 1.0f}, {-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f}, {0.5f, -1.0f, -1.0f, 2.5f, 1.0f, 1.0f}};

    for (const auto& aabb : aabbs) {
        createBLAS(aabb);
    }
}

void VulkanAccelerationStructure::createBLAS(const VkAabbPositionsKHR& aabb) {
    // 1. Create/upload AABB buffer
    AllocatedBuffer aabbBuffer = m_context->allocator().createBuffer(
        sizeof(VkAabbPositionsKHR), VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VMA_MEMORY_USAGE_CPU_TO_GPU);

    m_context->allocator().uploadBuffer(aabbBuffer, &aabb, sizeof(VkAabbPositionsKHR));

    // 2. Get AABB buffer device address

    VkDeviceAddress aabbAddress = getBufferDeviceAddress(aabbBuffer.buffer);

    // 3. Describe VkAccelerationStructureGeometryKHR

    VkAccelerationStructureGeometryAabbsDataKHR aabbData{};
    aabbData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
    aabbData.stride = sizeof(VkAabbPositionsKHR);
    aabbData.data.deviceAddress = aabbAddress;

    // 4. Describe BLAS build

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.aabbs = aabbData;

    // 5. Query build sizes

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;

    uint32_t primitiveCount = 1;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    m_vkGetAccelerationStructureBuildSizesKHR(m_device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primitiveCount, &sizeInfo);

    // 6. Create BLAS backing buffer

    AllocatedBuffer backingBuffer = m_context->allocator().createBuffer(
        sizeInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    // 7. Create VkAccelerationStructureKHR

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = backingBuffer.buffer;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;

    VkResult result = m_vkCreateAccelerationStructureKHR(m_device, &createInfo, nullptr, &accelerationStructure);

    // 8. Create scratch buffer

    AllocatedBuffer scratchBuffer = m_context->allocator().createBuffer(
        sizeInfo.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    VkDeviceAddress scratchAddress = getBufferDeviceAddress(scratchBuffer.buffer);

    buildInfo.dstAccelerationStructure = accelerationStructure;
    buildInfo.scratchData.deviceAddress = scratchAddress;

    // 9. Build BLAS with vkCmdBuildAccelerationStructuresKHR

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount = 1;

    const VkAccelerationStructureBuildRangeInfoKHR* rangeInfoPtr = &rangeInfo;

    VkCommandBuffer cmd = m_commands->beginSingleTimeCommands();

    m_vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &rangeInfoPtr);

    m_commands->endSingleTimeCommands(cmd, m_context->graphicsQueue());

    // 10. Destroy temp scratch buffer

    m_context->allocator().destroyBuffer(scratchBuffer);

    // 11. Get BLAS device address

    VkDeviceAddress blasAddress = getAccelerationStructureDeviceAddress(accelerationStructure);

    BLAS blas{};
    blas.accelerationStructure = accelerationStructure;
    blas.backingBuffer = backingBuffer;
    blas.aabbBuffer = aabbBuffer;
    blas.deviceAddress = blasAddress;

    m_blas.push_back(blas);
}

void VulkanAccelerationStructure::buildTLAS() {
    if (m_blas.empty()) {
        throw std::runtime_error("Cannot build TLAS without BLAS");
    }

    std::vector<VkAccelerationStructureInstanceKHR> instances;
    instances.resize(m_blas.size());

    for (uint32_t i = 0; i < static_cast<uint32_t>(m_blas.size()); ++i) {
        VkAccelerationStructureInstanceKHR instance{};

        // Identity transform.
        instance.transform.matrix[0][0] = 1.0f;
        instance.transform.matrix[1][1] = 1.0f;
        instance.transform.matrix[2][2] = 1.0f;

        // Used later to identify the sphere/material.
        instance.instanceCustomIndex = i;

        // Ray visibility mask.
        instance.mask = 0xFF;

        // First SBT record offset.
        instance.instanceShaderBindingTableRecordOffset = 0;

        // Don't cull procedural geometry.
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

        // Reference the BLAS.
        instance.accelerationStructureReference = m_blas[i].deviceAddress;

        instances[i] = instance;
    }

    // Instance buffer

    VkDeviceSize instanceBufferSize = sizeof(VkAccelerationStructureInstanceKHR) * instances.size();

    m_tlasInstanceBuffer = m_context->allocator().createBuffer(
        instanceBufferSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VMA_MEMORY_USAGE_CPU_TO_GPU);

    m_context->allocator().uploadBuffer(m_tlasInstanceBuffer, instances.data(), instanceBufferSize);

    // Get instance buffer device address

    VkDeviceAddress instanceAddress = getBufferDeviceAddress(m_tlasInstanceBuffer.buffer);

    // Describe TLAS geometry

    VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
    instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;

    instancesData.arrayOfPointers = VK_FALSE;
    instancesData.data.deviceAddress = instanceAddress;

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;

    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;

    geometry.geometry.instances = instancesData;

    // Describe TLAS build

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;

    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;

    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;

    uint32_t primitiveCount = static_cast<uint32_t>(instances.size());

    // Query TLAS size

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    m_vkGetAccelerationStructureBuildSizesKHR(m_device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primitiveCount, &sizeInfo);

    // TLAS backing buffer

    m_tlasBuffer = m_context->allocator().createBuffer(
        sizeInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    // Create TLAS object

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;

    createInfo.buffer = m_tlasBuffer.buffer;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    VkResult result = m_vkCreateAccelerationStructureKHR(m_device, &createInfo, nullptr, &m_tlas);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create TLAS");
    }

    // Scratch buffer

    AllocatedBuffer scratchBuffer = m_context->allocator().createBuffer(
        sizeInfo.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    VkDeviceAddress scratchAddress = getBufferDeviceAddress(scratchBuffer.buffer);

    // Build TLAS

    buildInfo.dstAccelerationStructure = m_tlas;
    buildInfo.scratchData.deviceAddress = scratchAddress;

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount = primitiveCount;

    const VkAccelerationStructureBuildRangeInfoKHR* rangeInfoPtr = &rangeInfo;

    VkCommandBuffer cmd = m_commands->beginSingleTimeCommands();

    m_vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &rangeInfoPtr);

    m_commands->endSingleTimeCommands(cmd, m_context->graphicsQueue());

    // GPU is finished with the scratch buffer.
    m_context->allocator().destroyBuffer(scratchBuffer);
}

VkDeviceAddress VulkanAccelerationStructure::getBufferDeviceAddress(VkBuffer buffer) {
    VkBufferDeviceAddressInfo addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addressInfo.buffer = buffer;

    return vkGetBufferDeviceAddress(m_device, &addressInfo);
}

VkDeviceAddress VulkanAccelerationStructure::getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR accelerationStructure) {
    VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addressInfo.accelerationStructure = accelerationStructure;

    return m_vkGetAccelerationStructureDeviceAddressKHR(m_device, &addressInfo);
}