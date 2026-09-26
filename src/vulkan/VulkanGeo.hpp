#pragma once

#include "asset/Model.hpp"
#include "VulkanCommands.hpp"
#include "VulkanContext.hpp"

#include <vulkan/vulkan.h>

// Uploads an asset::Model verbatim into four device-local buffers. This class holds
// handles only; the data model lives in asset/Model.hpp and knows nothing about Vulkan.
class VulkanGeometry {
public:
    void initialize(VulkanContext& context, VulkanCommands& commands, const asset::Model& model);
    void shutdown();

    VkBuffer vertexBuffer() const {
        return m_vertexBuffer.buffer;
    }
    VkBuffer indexBuffer() const {
        return m_indexBuffer.buffer;
    }
    VkBuffer meshBuffer() const {
        return m_meshBuffer.buffer;
    }
    VkBuffer materialBuffer() const {
        return m_materialBuffer.buffer;
    }

    // Device addresses of the pooled vertex/index buffers (used as BLAS build input).
    VkDeviceAddress vertexAddress() const {
        return m_vertexAddress;
    }
    VkDeviceAddress indexAddress() const {
        return m_indexAddress;
    }

private:
    AllocatedBuffer uploadDeviceLocal(const void* data, VkDeviceSize size, VkBufferUsageFlags usage);

    VkDeviceAddress bufferAddress(VkBuffer buffer) const;

    VulkanContext* m_context = nullptr;
    VulkanCommands* m_commands = nullptr;

    AllocatedBuffer m_vertexBuffer{};
    AllocatedBuffer m_indexBuffer{};
    AllocatedBuffer m_meshBuffer{};
    AllocatedBuffer m_materialBuffer{};

    VkDeviceAddress m_vertexAddress = 0;
    VkDeviceAddress m_indexAddress = 0;
};
