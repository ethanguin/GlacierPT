#include "VulkanGeo.hpp"

#include <algorithm>

void VulkanGeometry::initialize(VulkanContext& context, VulkanCommands& commands, const asset::Model& model) {
    m_context = &context;
    m_commands = &commands;

    // Vertex/index buffers are BLAS build inputs AND storage buffers read by the closest-hit shader.
    const VkBufferUsageFlags geometryUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                             VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

    m_vertexBuffer = uploadDeviceLocal(model.vertices().data(), model.vertices().size() * sizeof(asset::Vertex), geometryUsage);

    m_indexBuffer = uploadDeviceLocal(model.indices().data(), model.indices().size() * sizeof(uint32_t), geometryUsage);

    m_meshBuffer = uploadDeviceLocal(model.meshes().data(), model.meshes().size() * sizeof(asset::Mesh), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    m_materialBuffer =
        uploadDeviceLocal(model.materials().data(), model.materials().size() * sizeof(asset::Material), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    m_vertexAddress = bufferAddress(m_vertexBuffer.buffer);
    m_indexAddress = bufferAddress(m_indexBuffer.buffer);
}

void VulkanGeometry::shutdown() {
    if (m_context == nullptr) {
        return;
    }

    VulkanAllocator& allocator = m_context->allocator();

    allocator.destroyBuffer(m_vertexBuffer);
    allocator.destroyBuffer(m_indexBuffer);
    allocator.destroyBuffer(m_meshBuffer);
    allocator.destroyBuffer(m_materialBuffer);

    m_vertexAddress = 0;
    m_indexAddress = 0;

    m_context = nullptr;
    m_commands = nullptr;
}

AllocatedBuffer VulkanGeometry::uploadDeviceLocal(const void* data, VkDeviceSize size, VkBufferUsageFlags usage) {
    VulkanAllocator& allocator = m_context->allocator();

    // Never create a zero-sized buffer: descriptors must always point at something valid,
    // even for a scene with (for example) no triangle geometry at all.
    const VkDeviceSize allocationSize = std::max<VkDeviceSize>(size, 16);

    AllocatedBuffer destination = allocator.createBuffer(allocationSize, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    if (size == 0) {
        return destination;
    }

    AllocatedBuffer staging = allocator.createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    allocator.uploadBuffer(staging, data, size);

    VkCommandBuffer cmd = m_commands->beginSingleTimeCommands();

    VkBufferCopy region{};
    region.srcOffset = 0;
    region.dstOffset = 0;
    region.size = size;

    vkCmdCopyBuffer(cmd, staging.buffer, destination.buffer, 1, &region);

    // endSingleTimeCommands waits for the queue to go idle, so staging is safe to free afterwards.
    m_commands->endSingleTimeCommands(cmd, m_context->graphicsQueue());

    allocator.destroyBuffer(staging);

    return destination;
}

VkDeviceAddress VulkanGeometry::bufferAddress(VkBuffer buffer) const {
    VkBufferDeviceAddressInfo addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addressInfo.buffer = buffer;

    return vkGetBufferDeviceAddress(m_context->device(), &addressInfo);
}
