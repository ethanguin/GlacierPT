#include "VulkanAccelerationStructure.hpp"
#include "VulkanGeo.hpp"
#include "asset/Model.hpp"

#include <algorithm>
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

    destroyMeshBLAS();
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

// Procedural (sphere) BLAS
void VulkanAccelerationStructure::buildBLAS(const Scene& scene) {
    m_blas.clear();

    // TODO replace spheres with actual triangle mesh

    for (const SceneSphere& sphere : scene.spheres()) {
        VkAabbPositionsKHR aabb{};

        aabb.minX = sphere.position.x - sphere.radius;
        aabb.minY = sphere.position.y - sphere.radius;
        aabb.minZ = sphere.position.z - sphere.radius;

        aabb.maxX = sphere.position.x + sphere.radius;
        aabb.maxY = sphere.position.y + sphere.radius;
        aabb.maxZ = sphere.position.z + sphere.radius;

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

// -----------------------------------------------------------------------------
// Triangle mesh BLAS
// -----------------------------------------------------------------------------

void VulkanAccelerationStructure::buildMeshBLAS(const asset::Model& model, const VulkanGeometry& gpuGeometry) {
    destroyMeshBLAS();

    const std::vector<asset::Mesh>& meshes = model.meshes();
    const size_t meshCount = meshes.size();

    if (meshCount == 0) {
        return;
    }

    std::vector<VkAccelerationStructureGeometryKHR> geometries(meshCount);
    std::vector<VkAccelerationStructureBuildGeometryInfoKHR> buildInfos(meshCount);
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges(meshCount);

    m_meshBlas.resize(meshCount);

    VkDeviceSize maxScratchSize = 0;

    // Pass 1: describe each BLAS, query its size, create the backing buffer + acceleration structure.

    for (size_t i = 0; i < meshCount; ++i) {
        const asset::Mesh& mesh = meshes[i];

        VkAccelerationStructureGeometryKHR& geo = geometries[i];
        geo = {};
        geo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geo.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

        VkAccelerationStructureGeometryTrianglesDataKHR& triangles = geo.geometry.triangles;
        triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        triangles.vertexData.deviceAddress = gpuGeometry.vertexAddress() + static_cast<VkDeviceSize>(mesh.firstVertex) * sizeof(asset::Vertex);
        triangles.vertexStride = sizeof(asset::Vertex);
        triangles.maxVertex = mesh.vertexCount - 1;
        triangles.indexType = VK_INDEX_TYPE_UINT32;
        triangles.indexData.deviceAddress = gpuGeometry.indexAddress() + static_cast<VkDeviceSize>(mesh.firstIndex) * sizeof(uint32_t);

        VkAccelerationStructureBuildGeometryInfoKHR& buildInfo = buildInfos[i];
        buildInfo = {};
        buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &geo;

        const uint32_t primitiveCount = mesh.indexCount / 3;

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
        sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

        m_vkGetAccelerationStructureBuildSizesKHR(m_device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primitiveCount, &sizeInfo);

        BLAS& blas = m_meshBlas[i];

        blas.backingBuffer = m_context->allocator().createBuffer(
            sizeInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY);

        VkAccelerationStructureCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createInfo.buffer = blas.backingBuffer.buffer;
        createInfo.size = sizeInfo.accelerationStructureSize;
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

        if (m_vkCreateAccelerationStructureKHR(m_device, &createInfo, nullptr, &blas.accelerationStructure) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create mesh BLAS.");
        }

        buildInfo.dstAccelerationStructure = blas.accelerationStructure;

        ranges[i] = {};
        ranges[i].primitiveCount = primitiveCount;

        maxScratchSize = std::max(maxScratchSize, sizeInfo.buildScratchSize);
    }

    // One scratch buffer shared by every build (sequential builds, barrier in between).

    AllocatedBuffer scratchBuffer = m_context->allocator().createBuffer(
        maxScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    const VkDeviceAddress scratchAddress = getBufferDeviceAddress(scratchBuffer.buffer);

    for (auto& buildInfo : buildInfos) {
        buildInfo.scratchData.deviceAddress = scratchAddress;
    }

    // Pass 2: record all builds into a single command buffer / single submit.

    VkMemoryBarrier2 scratchBarrier{};
    scratchBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    scratchBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
    scratchBarrier.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    scratchBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
    scratchBarrier.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;

    VkDependencyInfo dependency{};
    dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.memoryBarrierCount = 1;
    dependency.pMemoryBarriers = &scratchBarrier;

    VkCommandBuffer cmd = m_commands->beginSingleTimeCommands();

    for (size_t i = 0; i < meshCount; ++i) {
        const VkAccelerationStructureBuildRangeInfoKHR* rangePtr = &ranges[i];

        m_vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfos[i], &rangePtr);

        vkCmdPipelineBarrier2(cmd, &dependency);
    }

    m_commands->endSingleTimeCommands(cmd, m_context->graphicsQueue());

    m_context->allocator().destroyBuffer(scratchBuffer);

    for (BLAS& blas : m_meshBlas) {
        blas.deviceAddress = getAccelerationStructureDeviceAddress(blas.accelerationStructure);
    }
}

void VulkanAccelerationStructure::destroyMeshBLAS() {
    for (BLAS& blas : m_meshBlas) {
        if (blas.accelerationStructure != VK_NULL_HANDLE) {
            m_vkDestroyAccelerationStructureKHR(m_device, blas.accelerationStructure, nullptr);
            blas.accelerationStructure = VK_NULL_HANDLE;
        }

        m_context->allocator().destroyBuffer(blas.backingBuffer);
    }

    m_meshBlas.clear();
}

// TLAS (covers both procedural and mesh BLAS)

void VulkanAccelerationStructure::buildTLAS(const Scene& scene) {
    const std::vector<asset::MeshInstance>& meshInstances = scene.geometry().instances();

    if (m_blas.size() != scene.spheres().size()) {
        throw std::runtime_error("BLAS count does not match scene sphere count.");
    }

    if (m_meshBlas.size() != scene.geometry().meshes().size()) {
        throw std::runtime_error("Mesh BLAS count does not match scene mesh count.");
    }

    if (m_blas.empty() && meshInstances.empty()) {
        throw std::runtime_error("Cannot build TLAS without instances");
    }

    std::vector<VkAccelerationStructureInstanceKHR> instances;
    instances.reserve(m_blas.size() + meshInstances.size());

    // Procedural spheres -> SBT hit record 0 (intersection + closest hit). customIndex = sphere index.
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

        instances.push_back(instance);
    }

    // Triangle meshes -> SBT hit record 1. customIndex = mesh index (indexes the Meshes table in the shader).
    for (const asset::MeshInstance& meshInstance : meshInstances) {
        VkAccelerationStructureInstanceKHR instance{};

        // glm is column-major, VkTransformMatrixKHR is a row-major 3x4.
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 4; ++col) {
                instance.transform.matrix[row][col] = meshInstance.transform[col][row];
            }
        }

        instance.instanceCustomIndex = meshInstance.mesh;
        instance.mask = 0xFF;
        instance.instanceShaderBindingTableRecordOffset = 1;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference = m_meshBlas[meshInstance.mesh].deviceAddress;

        instances.push_back(instance);
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