#include "VulkanTextures.hpp"

#include <stdexcept>

AllocatedImage uploadTexture2D(VulkanContext& context, VulkanCommands& commands, uint32_t width, uint32_t height, VkFormat format,
                               const void* pixels, VkDeviceSize byteSize) {
    VulkanAllocator& allocator = context.allocator();

    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = format;
    info.extent = {width, height, 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    AllocatedImage image = allocator.createImage(info, VMA_MEMORY_USAGE_GPU_ONLY);

    AllocatedBuffer staging = allocator.createBuffer(byteSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    allocator.uploadBuffer(staging, pixels, byteSize);

    VkCommandBuffer cmd = commands.beginSingleTimeCommands();

    VkImageMemoryBarrier2 toTransfer{};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    toTransfer.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
    toTransfer.srcAccessMask = VK_ACCESS_2_NONE;
    toTransfer.dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
    toTransfer.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = image.image;
    toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkDependencyInfo dependency{};
    dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers = &toTransfer;

    vkCmdPipelineBarrier2(cmd, &dependency);

    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(cmd, staging.buffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    VkImageMemoryBarrier2 toShaderRead = toTransfer;
    toShaderRead.srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
    toShaderRead.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    toShaderRead.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    toShaderRead.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    toShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toShaderRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    dependency.pImageMemoryBarriers = &toShaderRead;

    vkCmdPipelineBarrier2(cmd, &dependency);

    // Waits for the queue to go idle, so the staging buffer can be freed right after.
    commands.endSingleTimeCommands(cmd, context.graphicsQueue());

    allocator.destroyBuffer(staging);

    return image;
}

VkImageView createTextureView(VkDevice device, VkImage image, VkFormat format) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkImageView view = VK_NULL_HANDLE;

    if (vkCreateImageView(device, &viewInfo, nullptr, &view) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture image view.");
    }

    return view;
}

void VulkanTextures::initialize(VulkanContext& context, VulkanCommands& commands, const asset::Model& model) {
    m_context = &context;

    const auto& images = model.images();

    if (images.size() > kMaxTextures) {
        throw std::runtime_error("Model has more textures than VulkanTextures::kMaxTextures.");
    }

    m_images.reserve(images.size());
    m_views.reserve(images.size());

    for (const asset::Image& image : images) {
        const VkFormat format = image.srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

        m_images.push_back(uploadTexture2D(context, commands, image.width, image.height, format, image.pixels.data(), image.pixels.size()));
        m_views.push_back(createTextureView(context.device(), m_images.back().image, format));
    }

    // Single mip, LOD 0 only: ray tracing has no screen-space derivatives, and the per-frame
    // sub-pixel jitter + accumulation already box-filters the texture.
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(context.device(), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture sampler.");
    }
}

void VulkanTextures::shutdown() {
    if (m_context == nullptr) {
        return;
    }

    VkDevice device = m_context->device();

    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }

    for (VkImageView view : m_views) {
        vkDestroyImageView(device, view, nullptr);
    }
    m_views.clear();

    for (AllocatedImage& image : m_images) {
        m_context->allocator().destroyImage(image);
    }
    m_images.clear();

    m_context = nullptr;
}
