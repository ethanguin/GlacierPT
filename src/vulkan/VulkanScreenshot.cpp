#include "VulkanScreenshot.hpp"
#include "VulkanContext.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <cstdint>
#include <stdexcept>
#include <vector>
#include <fstream>

void VulkanScreenshot::initialize(VulkanContext& context, uint32_t width, uint32_t height, VkFormat format) {
    m_context = &context;
    m_width = width;
    m_height = height;
    m_format = format;

    // We copy the swapchain image into a 4-byte-per-pixel buffer.
    m_bufferSize = static_cast<VkDeviceSize>(m_width) * static_cast<VkDeviceSize>(m_height) * 4;

    createStagingBuffer();
}

void VulkanScreenshot::shutdown() {
    destroyStagingBuffer();

    m_context = nullptr;
    m_requested = false;
    m_copyRecorded = false;
}

void VulkanScreenshot::createStagingBuffer() {
    m_stagingBuffer = m_context->allocator().createBuffer(m_bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU);
}

void VulkanScreenshot::destroyStagingBuffer() {
    if (m_stagingBuffer.buffer != VK_NULL_HANDLE) {
        m_context->allocator().destroyBuffer(m_stagingBuffer);
        m_stagingBuffer = {};
    }
}

void VulkanScreenshot::request(const std::string& filename) {
    // Only allow one screenshot to be in flight.
    if (m_requested || m_copyRecorded) {
        return;
    }

    m_filename = filename;
    m_requested = true;
}

bool VulkanScreenshot::recordCopy(VkCommandBuffer cmd, VkImage swapchainImage, uint32_t frameSlot) {
    if (!m_requested) {
        return false;
    }

    m_captureFrameSlot = frameSlot;

    VkImageMemoryBarrier2 toTransfer{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                     .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                     .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                     .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                     .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
                                     .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                     .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                     .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                     .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                     .image = swapchainImage,
                                     .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    VkDependencyInfo dependencyInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &toTransfer};

    vkCmdPipelineBarrier2(cmd, &dependencyInfo);

    VkBufferImageCopy region{.bufferOffset = 0,
                             .bufferRowLength = 0,
                             .bufferImageHeight = 0,
                             .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                             .imageOffset = {0, 0, 0},
                             .imageExtent = {m_width, m_height, 1}};

    vkCmdCopyImageToBuffer(cmd, swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_stagingBuffer.buffer, 1, &region);

    // Put the swapchain image back into the layout expected by present.
    VkImageMemoryBarrier2 toPresent{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                    .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                    .srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
                                    .dstStageMask = VK_PIPELINE_STAGE_2_NONE,
                                    .dstAccessMask = VK_ACCESS_2_NONE,
                                    .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                    .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                    .image = swapchainImage,
                                    .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    dependencyInfo = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &toPresent};

    vkCmdPipelineBarrier2(cmd, &dependencyInfo);

    m_requested = false;
    m_copyRecorded = true;

    return true;
}

void VulkanScreenshot::saveIfReady(uint32_t frameSlot) {
    if (!m_copyRecorded) {
        return;
    }

    // The fence for this exact frame slot has just been waited on,
    // so the GPU has finished writing the staging buffer.
    if (frameSlot != m_captureFrameSlot) {
        return;
    }

    writePNG();

    m_copyRecorded = false;
    m_filename.clear();
}

void VulkanScreenshot::writePPM() {
    void* mapped = nullptr;

    VmaAllocator allocator = m_context->allocator().allocator();

    VkResult result = vmaMapMemory(allocator, m_stagingBuffer.allocation, &mapped);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to map screenshot staging buffer");
    }

    vmaInvalidateAllocation(allocator, m_stagingBuffer.allocation, 0, VK_WHOLE_SIZE);

    const auto* src = static_cast<const uint8_t*>(mapped);

    std::vector<uint8_t> rgb(static_cast<size_t>(m_width) * static_cast<size_t>(m_height) * 3);

    for (uint32_t y = 0; y < m_height; ++y) {
        for (uint32_t x = 0; x < m_width; ++x) {
            const size_t srcIndex = (static_cast<size_t>(y) * m_width + x) * 4;

            const size_t dstIndex = (static_cast<size_t>(y) * m_width + x) * 3;

            switch (m_format) {
            case VK_FORMAT_R8G8B8A8_UNORM:
            case VK_FORMAT_R8G8B8A8_SRGB:
                rgb[dstIndex + 0] = src[srcIndex + 0];
                rgb[dstIndex + 1] = src[srcIndex + 1];
                rgb[dstIndex + 2] = src[srcIndex + 2];
                break;

            case VK_FORMAT_B8G8R8A8_UNORM:
            case VK_FORMAT_B8G8R8A8_SRGB:
                rgb[dstIndex + 0] = src[srcIndex + 2];
                rgb[dstIndex + 1] = src[srcIndex + 1];
                rgb[dstIndex + 2] = src[srcIndex + 0];
                break;

            default:
                vmaUnmapMemory(allocator, m_stagingBuffer.allocation);

                throw std::runtime_error("Unsupported swapchain format for PPM screenshot");
            }
        }
    }

    vmaUnmapMemory(allocator, m_stagingBuffer.allocation);

    m_filename += ".ppm";

    std::ofstream file(m_filename, std::ios::binary);

    if (!file) {
        throw std::runtime_error("Failed to open screenshot file: " + m_filename);
    }

    file << "P6\n"
         << m_width << ' ' << m_height << "\n"
         << "255\n";

    file.write(reinterpret_cast<const char*>(rgb.data()), static_cast<std::streamsize>(rgb.size()));

    if (!file) {
        throw std::runtime_error("Failed while writing screenshot: " + m_filename);
    }
}

void VulkanScreenshot::writePNG() {
    VmaAllocator allocator = m_context->allocator().allocator();

    void* mapped = nullptr;

    VkResult result = vmaMapMemory(allocator, m_stagingBuffer.allocation, &mapped);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to map screenshot staging buffer.");
    }

    vmaInvalidateAllocation(allocator, m_stagingBuffer.allocation, 0, VK_WHOLE_SIZE);

    const uint8_t* src = static_cast<const uint8_t*>(mapped);

    // stb_image_write expects tightly packed RGB/RGBA data.
    std::vector<uint8_t> rgba(static_cast<size_t>(m_width) * static_cast<size_t>(m_height) * 4);

    for (uint32_t y = 0; y < m_height; ++y) {
        for (uint32_t x = 0; x < m_width; ++x) {
            const size_t index = (static_cast<size_t>(y) * m_width + x) * 4;

            switch (m_format) {
            case VK_FORMAT_R8G8B8A8_UNORM:
            case VK_FORMAT_R8G8B8A8_SRGB:
                rgba[index + 0] = src[index + 0];
                rgba[index + 1] = src[index + 1];
                rgba[index + 2] = src[index + 2];
                rgba[index + 3] = src[index + 3];
                break;

            case VK_FORMAT_B8G8R8A8_UNORM:
            case VK_FORMAT_B8G8R8A8_SRGB:
                rgba[index + 0] = src[index + 2];
                rgba[index + 1] = src[index + 1];
                rgba[index + 2] = src[index + 0];
                rgba[index + 3] = src[index + 3];
                break;

            default:
                vmaUnmapMemory(allocator, m_stagingBuffer.allocation);

                throw std::runtime_error("Unsupported swapchain format for PNG screenshot.");
            }
        }
    }

    vmaUnmapMemory(allocator, m_stagingBuffer.allocation);

    // stride = width * 4 bytes per RGBA pixel.
    const int stride = static_cast<int>(m_width * 4);

    m_filename += ".png";

    if (!stbi_write_png(m_filename.c_str(), static_cast<int>(m_width), static_cast<int>(m_height), 4, rgba.data(), stride)) {
        throw std::runtime_error("Failed to write PNG screenshot: " + m_filename);
    }
}