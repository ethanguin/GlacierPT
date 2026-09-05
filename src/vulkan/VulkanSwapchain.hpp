#pragma once

#include <vulkan/vulkan.h>

#include <vector>

class VulkanSwapchain
{
public:
    void initialize(
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        VkSurfaceKHR surface,
        uint32_t width,
        uint32_t height
    );

    void shutdown();

    VkSwapchainKHR swapchain() const { return m_swapchain; }

    VkFormat imageFormat() const { return m_imageFormat; }
    VkExtent2D extent() const { return m_extent; }

    const std::vector<VkImage>& images() const
    {
        return m_images;
    }

    const std::vector<VkImageView>& imageViews() const
    {
        return m_imageViews;
    }

private:
    VkDevice m_device = VK_NULL_HANDLE;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;

    VkFormat m_imageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_extent{};

    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
};