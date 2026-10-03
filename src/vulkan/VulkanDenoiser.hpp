#pragma once

#include "VulkanCommands.hpp"
#include "VulkanContext.hpp"

#include <vulkan/vulkan.h>

// Spatial-only à-trous wavelet denoiser (no history, so it is temporally stateless).
//
// Owns: ping-pong partner image B, and the two guide images (first non-delta hit
// position / normal) that raygen writes. The radiance image A is owned by the renderer.
//
// All images stay in VK_IMAGE_LAYOUT_GENERAL for their whole lifetime.
// After record(), the filtered result is back in image A (kPasses is even).
class VulkanDenoiser {
public:
    static constexpr int kPasses = 4; // step sizes 1,2,4,8 ; MUST be even
    static_assert(kPasses % 2 == 0, "Result must end in the original radiance image.");

    void initialize(VulkanContext& context, VulkanCommands& commands, VkExtent2D extent, VkImageView radianceView);
    void shutdown();

    // Records RT->compute barrier + all filter passes. The caller must still transition
    // the radiance image GENERAL -> SHADER_READ_ONLY afterwards (src = COMPUTE_SHADER).
    void record(VkCommandBuffer cmd) const;

    VkImageView guidePositionView() const {
        return m_guidePosView;
    }
    VkImageView guideNormalView() const {
        return m_guideNormalView;
    }

private:
    void createImage(AllocatedImage& image, VkImageView& view);
    void transitionAllToGeneral(VulkanCommands& commands);
    void writeSet(VkDescriptorSet set, VkImageView src, VkImageView dst);

    VulkanContext* m_context = nullptr;
    VkExtent2D m_extent{};

    AllocatedImage m_imageB{};
    AllocatedImage m_guidePos{};
    AllocatedImage m_guideNormal{};

    VkImageView m_imageBView = VK_NULL_HANDLE;
    VkImageView m_guidePosView = VK_NULL_HANDLE;
    VkImageView m_guideNormalView = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
    VkDescriptorSet m_sets[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE}; // [0] = A->B, [1] = B->A

    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};
