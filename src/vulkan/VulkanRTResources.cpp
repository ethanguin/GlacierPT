#include "VulkanRTResources.hpp"

#include <stdexcept>

void VulkanRTResources::initialize(VulkanContext& context, VulkanAccelerationStructure& accelerationStructure, const Scene& scene,
                                   VkImageView outputImageView) {
    m_context = &context;

    VkDevice device = context.device();

    // Create GPU sphere buffer
    // TODO add/replace with actual geometry buffers

    std::vector<GPUSphere> gpuSpheres;

    gpuSpheres.reserve(scene.spheres().size());

    for (const SceneSphere& sphere : scene.spheres()) {
        gpuSpheres.push_back({sphere.position, sphere.radius, sphere.color, 0.0f});
    }

    if (!gpuSpheres.empty()) {
        m_sphereBuffer = m_context->allocator().createBuffer(sizeof(GPUSphere) * gpuSpheres.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                             VMA_MEMORY_USAGE_CPU_TO_GPU);

        m_context->allocator().uploadBuffer(m_sphereBuffer, gpuSpheres.data(), sizeof(GPUSphere) * gpuSpheres.size());
    }

    // Descriptor set layout

    VkDescriptorSetLayoutBinding tlasBinding{};
    tlasBinding.binding = 0;
    tlasBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    tlasBinding.descriptorCount = 1;
    tlasBinding.stageFlags =
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;

    VkDescriptorSetLayoutBinding imageBinding{};
    imageBinding.binding = 1;
    imageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    imageBinding.descriptorCount = 1;
    imageBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding sphereBinding{};
    sphereBinding.binding = 2;
    sphereBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    sphereBinding.descriptorCount = 1;
    sphereBinding.stageFlags = VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    VkDescriptorSetLayoutBinding bindings[] = {tlasBinding, imageBinding, sphereBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

    layoutInfo.bindingCount = 3;
    layoutInfo.pBindings = bindings;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ray tracing descriptor set layout.");
    }

    // Descriptor pool

    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1}, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}};

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;

    poolInfo.maxSets = 1;

    poolInfo.poolSizeCount = 3;
    poolInfo.pPoolSizes = poolSizes;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);

        m_descriptorSetLayout = VK_NULL_HANDLE;

        throw std::runtime_error("Failed to create ray tracing descriptor pool.");
    }

    // Allocate descriptor set

    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

    allocateInfo.descriptorPool = m_descriptorPool;

    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &m_descriptorSetLayout;

    if (vkAllocateDescriptorSets(device, &allocateInfo, &m_descriptorSet) != VK_SUCCESS) {
        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);

        vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);

        m_descriptorPool = VK_NULL_HANDLE;
        m_descriptorSetLayout = VK_NULL_HANDLE;

        throw std::runtime_error("Failed to allocate ray tracing descriptor set.");
    }

    // TLAS descriptor

    VkAccelerationStructureKHR tlas = accelerationStructure.tlas();

    VkWriteDescriptorSetAccelerationStructureKHR tlasInfo{};
    tlasInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;

    tlasInfo.accelerationStructureCount = 1;
    tlasInfo.pAccelerationStructures = &tlas;

    VkWriteDescriptorSet tlasWrite{};
    tlasWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

    tlasWrite.pNext = &tlasInfo;

    tlasWrite.dstSet = m_descriptorSet;
    tlasWrite.dstBinding = 0;
    tlasWrite.dstArrayElement = 0;

    tlasWrite.descriptorCount = 1;
    tlasWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    // Storage image descriptor

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageView = outputImageView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet imageWrite{};
    imageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

    imageWrite.dstSet = m_descriptorSet;
    imageWrite.dstBinding = 1;
    imageWrite.dstArrayElement = 0;

    imageWrite.descriptorCount = 1;
    imageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

    imageWrite.pImageInfo = &imageInfo;

    // Sphere buffer descriptor

    VkDescriptorBufferInfo sphereBufferInfo{};
    sphereBufferInfo.buffer = m_sphereBuffer.buffer;
    sphereBufferInfo.offset = 0;
    sphereBufferInfo.range = sizeof(GPUSphere) * scene.spheres().size();

    VkWriteDescriptorSet sphereWrite{};
    sphereWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

    sphereWrite.dstSet = m_descriptorSet;
    sphereWrite.dstBinding = 2;
    sphereWrite.dstArrayElement = 0;

    sphereWrite.descriptorCount = 1;
    sphereWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    sphereWrite.pBufferInfo = &sphereBufferInfo;

    // Write descriptors

    VkWriteDescriptorSet writes[] = {tlasWrite, imageWrite, sphereWrite};

    vkUpdateDescriptorSets(device, 3, writes, 0, nullptr);
}

void VulkanRTResources::shutdown() {
    if (m_context == nullptr) {
        return;
    }

    m_context->allocator().destroyBuffer(m_sphereBuffer);

    VkDevice device = m_context->device();

    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);

        m_descriptorPool = VK_NULL_HANDLE;
        m_descriptorSet = VK_NULL_HANDLE;
    }

    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);

        m_descriptorSetLayout = VK_NULL_HANDLE;
    }

    m_context = nullptr;
}