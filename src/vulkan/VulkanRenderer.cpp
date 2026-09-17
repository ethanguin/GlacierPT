#include "VulkanRenderer.hpp"
#include "Vertex.hpp"

void VulkanRenderer::initialize(GLFWwindow* window, const Scene& scene) {
    m_window = window;

    m_context.initialize(window);

    int width;
    int height;

    glfwGetFramebufferSize(window, &width, &height);

    m_swapchain.initialize(m_context.physicalDevice(), m_context.device(), m_context.surface(), width, height);

    m_screenshot.initialize(m_context, m_swapchain.extent().width, m_swapchain.extent().height, m_swapchain.imageFormat());

    m_pipeline.initialize(m_context.device(), m_swapchain.imageFormat());

    createTriangleVertexBuffer();

    m_commands.initialize(m_context.device(), m_context.graphicsQueueFamilyIndex());

    m_accelerationStructure.initialize(m_context, m_commands);

    m_accelerationStructure.buildBLAS(scene);

    m_accelerationStructure.buildTLAS(scene);

    createRayTracingImage();

    m_rayTracingResources.initialize(m_context, m_accelerationStructure, scene, m_rayTracingImageView);

    PFN_vkCmdTraceRaysKHR m_vkCmdTraceRaysKHR = nullptr;

    m_vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(m_context.device(), "vkCmdTraceRaysKHR"));

    if (m_vkCmdTraceRaysKHR == nullptr) {
        throw std::runtime_error("Failed to load vkCmdTraceRaysKHR.");
    }

    m_rayTracingPipeline.initialize(m_context, m_rayTracingResources);

    m_presentationPipeline.initialize(m_context, m_swapchain.imageFormat());

    m_presentationPipeline.updateDescriptorSet(m_rayTracingImageView);

    createFrames();
}

void VulkanRenderer::shutdown() {
    if (m_context.device() == VK_NULL_HANDLE) {
        return;
    }

    VkDevice device = m_context.device();

    vkDeviceWaitIdle(device);

    m_screenshot.shutdown();

    destroyFrames();

    m_rayTracingPipeline.shutdown();

    m_presentationPipeline.shutdown();

    m_rayTracingResources.shutdown();

    m_accelerationStructure.shutdown();

    destroyRayTracingImage();

    m_context.allocator().destroyBuffer(m_triangleVertexBuffer);

    m_pipeline.shutdown(device);

    m_commands.shutdown();
    m_swapchain.shutdown();
    m_context.shutdown();

    m_window = nullptr;
}

void VulkanRenderer::drawFrame() {
    VulkanFrame& frame = m_frames[m_currentFrame];

    VkDevice device = m_context.device();

    vkWaitForFences(device, 1, &frame.renderFence, VK_TRUE, UINT64_MAX);

    m_screenshot.saveIfReady(m_currentFrame);

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

    submitFrame(frame, imageIndex);

    result = m_swapchain.present(m_context.presentQueue(), imageIndex, m_renderFinished[imageIndex]);

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

    // One image-available semaphore and command buffer per frame in flight.
    for (auto& frame : m_frames) {
        frame.commandBuffer = m_commands.allocateCommandBuffer();

        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frame.imageAvailable) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image-available semaphore.");
        }

        if (vkCreateFence(device, &fenceInfo, nullptr, &frame.renderFence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render fence.");
        }
    }

    // One render-finished semaphore per swapchain image.
    m_renderFinished.resize(m_swapchain.images().size());

    for (VkSemaphore& semaphore : m_renderFinished) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render-finished semaphore.");
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

        if (frame.renderFence != VK_NULL_HANDLE) {
            vkDestroyFence(device, frame.renderFence, nullptr);
            frame.renderFence = VK_NULL_HANDLE;
        }
    }

    for (VkSemaphore semaphore : m_renderFinished) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, semaphore, nullptr);
        }
    }

    m_renderFinished.clear();
}

void VulkanRenderer::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin command buffer.");
    }

    // TRANSITION TO RT

    VkImageMemoryBarrier2 rtImageBarrier{};
    rtImageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    if (m_rayTracingImageInitialized) {
        rtImageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

        rtImageBarrier.srcAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;

        rtImageBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    } else {
        rtImageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;

        rtImageBarrier.srcAccessMask = VK_ACCESS_2_NONE;

        rtImageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    rtImageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;

    rtImageBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

    rtImageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

    rtImageBarrier.image = m_rayTracingImage.image;

    rtImageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    rtImageBarrier.subresourceRange.baseMipLevel = 0;
    rtImageBarrier.subresourceRange.levelCount = 1;
    rtImageBarrier.subresourceRange.baseArrayLayer = 0;
    rtImageBarrier.subresourceRange.layerCount = 1;

    VkDependencyInfo rtDependency{};
    rtDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;

    rtDependency.imageMemoryBarrierCount = 1;
    rtDependency.pImageMemoryBarriers = &rtImageBarrier;

    vkCmdPipelineBarrier2(cmd, &rtDependency);

    m_rayTracingImageInitialized = true;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_rayTracingPipeline.pipeline());

    VkDescriptorSet descriptorSet = m_rayTracingResources.descriptorSet();

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_rayTracingPipeline.layout(), 0, 1, &descriptorSet, 0, nullptr);

    VkStridedDeviceAddressRegionKHR raygenRegion = m_rayTracingPipeline.raygenRegion();

    VkStridedDeviceAddressRegionKHR missRegion = m_rayTracingPipeline.missRegion();

    VkStridedDeviceAddressRegionKHR hitRegion = m_rayTracingPipeline.hitRegion();

    VkStridedDeviceAddressRegionKHR callableRegion{};

    m_rayTracingPipeline.traceRaysFunction()(cmd, &raygenRegion, &missRegion, &hitRegion, &callableRegion, m_swapchain.extent().width,
                                             m_swapchain.extent().height, 1);

    // RT IMAGE BARRIER

    VkImageMemoryBarrier2 rtReadBarrier{};
    rtReadBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    rtReadBarrier.srcStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;

    rtReadBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

    rtReadBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

    rtReadBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;

    rtReadBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;

    rtReadBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    rtReadBarrier.image = m_rayTracingImage.image;

    rtReadBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    rtReadBarrier.subresourceRange.baseMipLevel = 0;
    rtReadBarrier.subresourceRange.levelCount = 1;

    rtReadBarrier.subresourceRange.baseArrayLayer = 0;
    rtReadBarrier.subresourceRange.layerCount = 1;

    VkDependencyInfo rtReadDependency{};
    rtReadDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;

    rtReadDependency.imageMemoryBarrierCount = 1;
    rtReadDependency.pImageMemoryBarriers = &rtReadBarrier;

    vkCmdPipelineBarrier2(cmd, &rtReadDependency);

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

    // RENDER PIPELINE

    // Set up RT extents
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;

    viewport.width = static_cast<float>(m_swapchain.extent().width);

    viewport.height = static_cast<float>(m_swapchain.extent().height);

    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_swapchain.extent();

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_presentationPipeline.pipeline());

    VkDescriptorSet presentationDescriptorSet = m_presentationPipeline.descriptorSet();

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_presentationPipeline.layout(), 0, 1, &presentationDescriptorSet, 0, nullptr);

    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRendering(cmd);

    // PRESENTATION

    if (!m_screenshot.recordCopy(cmd, image, m_currentFrame)) {

        barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

        barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

        barrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;

        barrier.dstAccessMask = VK_ACCESS_2_NONE;

        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    }

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

void VulkanRenderer::submitFrame(VulkanFrame& frame, uint32_t imageIndex) {
    VkSemaphore waitSemaphores[] = {frame.imageAvailable};

    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame.commandBuffer;

    VkSemaphore signalSemaphores[] = {m_renderFinished[imageIndex]};

    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(m_context.graphicsQueue(), 1, &submitInfo, frame.renderFence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer.");
    }
}

void VulkanRenderer::createRayTracingImage() {
    VkExtent2D extent = m_swapchain.extent();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

    imageInfo.imageType = VK_IMAGE_TYPE_2D;

    imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;

    imageInfo.extent.width = extent.width;

    imageInfo.extent.height = extent.height;

    imageInfo.extent.depth = 1;

    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;

    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    m_rayTracingImage = m_context.allocator().createImage(imageInfo, VMA_MEMORY_USAGE_GPU_ONLY);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

    viewInfo.image = m_rayTracingImage.image;

    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

    viewInfo.format = imageInfo.format;

    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;

    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_context.device(), &viewInfo, nullptr, &m_rayTracingImageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ray tracing image view.");
    }
}

void VulkanRenderer::destroyRayTracingImage() {
    VkDevice device = m_context.device();

    if (m_rayTracingImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_rayTracingImageView, nullptr);

        m_rayTracingImageView = VK_NULL_HANDLE;
    }

    m_context.allocator().destroyImage(m_rayTracingImage);
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

void VulkanRenderer::createTriangleVertexBuffer() {
    const VkDeviceSize bufferSize = sizeof(TRIANGLE_VERTICES);

    m_triangleVertexBuffer = m_context.allocator().createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    m_context.allocator().uploadBuffer(m_triangleVertexBuffer, TRIANGLE_VERTICES, bufferSize);
}

void VulkanRenderer::drawTriangle(VkCommandBuffer cmd) {
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swapchain.extent().width);
    viewport.height = static_cast<float>(m_swapchain.extent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_swapchain.extent();

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.pipeline());

    VkDeviceSize offset = 0;

    vkCmdBindVertexBuffers(cmd, 0, 1, &m_triangleVertexBuffer.buffer, &offset);

    vkCmdDraw(cmd, 3, 1, 0, 0);
}