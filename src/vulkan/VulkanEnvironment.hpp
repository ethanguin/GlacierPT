#pragma once

#include "VulkanTextures.hpp"

#include <filesystem>

// HDRI environment: equirectangular RGBA16F image (binding 15), its sampler (16) and a uniform
// buffer (17) holding the intensity / rotation and 9 pre-convolved SH irradiance coefficients.
// With no HDRI a 1x1 black image is bound and `enabled` is 0, so the shaders keep using the
// constant AmbientLight.
class VulkanEnvironment {
public:
    void initialize(VulkanContext& context, VulkanCommands& commands, const std::filesystem::path& hdrPath, float intensity,
                    float rotationRadians);
    void shutdown();

    bool enabled() const {
        return m_enabled;
    }
    VkImageView view() const {
        return m_view;
    }
    VkSampler sampler() const {
        return m_sampler;
    }
    VkBuffer paramsBuffer() const {
        return m_params.buffer;
    }

private:
    VulkanContext* m_context = nullptr;

    AllocatedImage m_image{};
    VkImageView m_view = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    AllocatedBuffer m_params{};

    bool m_enabled = false;
};
