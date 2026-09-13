#include "VulkanRenderer.hpp"

void VulkanRenderer::initialize(GLFWwindow* window) {
    m_window = window;

    m_context.initialize(window);

    int width;
    int height;

    glfwGetFramebufferSize(window, &width, &height);

    m_swapchain.initialize(m_context.physicalDevice(), m_context.device(), m_context.surface(), width, height);

    m_commands.initialize(m_context.device(), m_context.graphicsQueueFamilyIndex());

    createFrames();
}

void VulkanRenderer::shutdown() {
    VkDevice device = m_context.device();

    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
    }

    destroyFrames();
    m_commands.shutdown();
    m_swapchain.shutdown();
    m_context.shutdown();

    m_window = nullptr;
}

void VulkanRenderer::drawFrame() {
    VulkanFrame& frame = m_frames[m_currentFrame];

    VkDevice device = m_context.device();

    vkWaitForFences(device, 1, &frame.renderFence, VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;

    VkResult result = m_swapchain.acquireNextImage(frame.imageAvailable, imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image.");
    }

    vkResetFences(device, 1, &frame.renderFence);

    if (vkResetCommandBuffer(frame.commandBuffer, 0) != VK_SUCCESS) {
        throw std::runtime_error("Failed to reset command buffer.");
    }

    recordCommandBuffer(frame.commandBuffer, imageIndex);

    submitFrame(frame);

    result = m_swapchain.present(m_context.presentQueue(), imageIndex, frame.renderFinished);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image.");
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::createFrames() {
    VkDevice device = m_context.device();

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (auto& frame : m_frames) {
        frame.commandBuffer = m_commands.allocateCommandBuffer();

        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frame.imageAvailable) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frame.renderFinished) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &frame.renderFence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create frame synchronization objects.");
        }
    }
}

void VulkanRenderer::destroyFrames() {
    VkDevice device = m_context.device();

    for (auto& frame : m_frames) {
        if (frame.commandBuffer != VK_NULL_HANDLE) {
            m_commands.freeCommandBuffer(frame.commandBuffer);
            frame.commandBuffer = VK_NULL_HANDLE;
        }

        if (frame.imageAvailable != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, frame.imageAvailable, nullptr);
            frame.imageAvailable = VK_NULL_HANDLE;
        }

        if (frame.renderFinished != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, frame.renderFinished, nullptr);
            frame.renderFinished = VK_NULL_HANDLE;
        }

        if (frame.renderFence != VK_NULL_HANDLE) {
            vkDestroyFence(device, frame.renderFence, nullptr);
            frame.renderFence = VK_NULL_HANDLE;
        }
    }
}

void VulkanRenderer::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin command buffer.");
    }

    VkImage image = m_swapchain.images()[imageIndex];

    // UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL

    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;

    barrier.srcAccessMask = VK_ACCESS_2_NONE;

    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    barrier.image = image;

    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;

    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(cmd, &dependencyInfo);

    // DYNAMIC RENDERING

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

    colorAttachment.imageView = m_swapchain.imageViews()[imageIndex];

    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    colorAttachment.clearValue.color = {{0.05f, 0.05f, 0.05f, 1.0f}};

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;

    renderingInfo.renderArea.offset = {0, 0};

    renderingInfo.renderArea.extent = m_swapchain.extent();

    renderingInfo.layerCount = 1;

    renderingInfo.colorAttachmentCount = 1;

    renderingInfo.pColorAttachments = &colorAttachment;

    vkCmdBeginRendering(cmd, &renderingInfo);

    // Actual drawing will eventually go here.

    vkCmdEndRendering(cmd);

    // PRESENTATION

    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

    barrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;

    barrier.dstAccessMask = VK_ACCESS_2_NONE;

    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    vkCmdPipelineBarrier2(cmd, &dependencyInfo);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer.");
    }
}

void VulkanRenderer::recreateSwapchain() {
    int width = 0;
    int height = 0;

    glfwGetFramebufferSize(m_window, &width, &height);

    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(m_window, &width, &height);
        glfwWaitEvents();
    }

    VkDevice device = m_context.device();

    vkDeviceWaitIdle(device);

    m_swapchain.recreate(m_context.physicalDevice(), device, m_context.surface(), width, height);
}

void VulkanRenderer::submitFrame(VulkanFrame& frame) {
    VkSemaphore waitSemaphores[] = {frame.imageAvailable};

    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame.commandBuffer;

    VkSemaphore signalSemaphores[] = {frame.renderFinished};

    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(m_context.graphicsQueue(), 1, &submitInfo, frame.renderFence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer.");
    }
}

VkCommandBuffer VulkanCommands::allocateCommandBuffer() {
    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = m_commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

    if (vkAllocateCommandBuffers(m_device, &allocateInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffer.");
    }

    return commandBuffer;
}

void VulkanCommands::freeCommandBuffer(VkCommandBuffer commandBuffer) {
    if (commandBuffer != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);
    }
}
