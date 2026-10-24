#pragma once

#include "VulkanCommands.hpp"
#include "VulkanContext.hpp"
#include "asset/Model.hpp"

#include <vulkan/vulkan.h>

#include <vector>

// Uploads a tightly packed single-mip 2D image to a device-local image and leaves it in
// SHADER_READ_ONLY_OPTIMAL. Blocks until the copy has finished (load-time helper).
AllocatedImage uploadTexture2D(VulkanContext& context, VulkanCommands& commands, uint32_t width, uint32_t height, VkFormat format,
                               const void* pixels, VkDeviceSize byteSize);

VkImageView createTextureView(VkDevice device, VkImage image, VkFormat format);

// All material textures of a model, exposed as one bindless array (binding 13) plus one shared sampler (binding 14).
class VulkanTextures {
public:
    static constexpr uint32_t kMaxTextures = 512; // must match MAX_TEXTURES in raycommon.hlsli

    void initialize(VulkanContext& context, VulkanCommands& commands, const asset::Model& model);
    void shutdown();

    const std::vector<VkImageView>& views() const {
        return m_views;
    }

    VkSampler sampler() const {
        return m_sampler;
    }

private:
    VulkanContext* m_context = nullptr;

    std::vector<AllocatedImage> m_images;
    std::vector<VkImageView> m_views;
    VkSampler m_sampler = VK_NULL_HANDLE;
};
