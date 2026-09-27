#include "VulkanRTResources.hpp"

#include <stdexcept>

void VulkanRTResources::initialize(VulkanContext& context, VulkanAccelerationStructure& accelerationStructure, VulkanGeometry& geometry,
                                   const Scene& scene, const Camera& camera, VkImageView outputImageView) {
    m_context = &context;

    VkDevice device = context.device();

    // TODO add/replace with actual geometry buffers

    // Create GPU sphere buffer
    std::vector<GPUSphere> gpuSpheres;

    for (const SceneSphere& sphere : scene.spheres()) {
        gpuSpheres.push_back({{sphere.position, sphere.radius}, {sphere.color, 1.0f}});
    }

    // Always create the buffer, even if empty, so the descriptor is never null.
    const size_t sphereCount = std::max<size_t>(gpuSpheres.size(), 1);

    m_sphereBuffer =
        m_context->allocator().createBuffer(sizeof(GPUSphere) * sphereCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    if (!gpuSpheres.empty()) {
        m_context->allocator().uploadBuffer(m_sphereBuffer, gpuSpheres.data(), sizeof(GPUSphere) * gpuSpheres.size());
    }

    // Create light buffers

    std::vector<GPULight> gpuLights;
    for (const SceneLight& light : scene.lights()) {
        gpuLights.push_back(light.gpuData());
    }

    const size_t lightCount = std::max<size_t>(gpuLights.size(), 1);
    m_lightBuffer =
        m_context->allocator().createBuffer(sizeof(GPULight) * lightCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    if (!gpuLights.empty()) {
        m_context->allocator().uploadBuffer(m_lightBuffer, gpuLights.data(), sizeof(GPULight) * gpuLights.size());
    }

    m_ambLightBuffer = m_context->allocator().createBuffer(sizeof(GPUAmbientLight), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    GPUAmbientLight ambientData = scene.ambientLight().gpuData();
    m_context->allocator().uploadBuffer(m_ambLightBuffer, &ambientData, sizeof(GPUAmbientLight));

    // Create Camera buffer

    m_cameraBuffer = m_context->allocator().createBuffer(sizeof(GPUCamera), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    GPUCamera gpuCamera{camera.position(), camera.focalLength(), camera.forward(), camera.sensorWidth()};
    m_context->allocator().uploadBuffer(m_cameraBuffer, &gpuCamera, sizeof(GPUCamera));

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

    VkDescriptorSetLayoutBinding lightBinding{};
    lightBinding.binding = 3;
    lightBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    lightBinding.descriptorCount = 1;
    lightBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding ambientBinding{};
    ambientBinding.binding = 4;
    ambientBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ambientBinding.descriptorCount = 1;
    ambientBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding cameraBinding{};
    cameraBinding.binding = 5;
    cameraBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cameraBinding.descriptorCount = 1;
    cameraBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding vertexBinding{};
    vertexBinding.binding = 6;
    vertexBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    vertexBinding.descriptorCount = 1;
    vertexBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkDescriptorSetLayoutBinding indexBinding{};
    indexBinding.binding = 7;
    indexBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    indexBinding.descriptorCount = 1;
    indexBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkDescriptorSetLayoutBinding meshBinding{};
    meshBinding.binding = 8;
    meshBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    meshBinding.descriptorCount = 1;
    meshBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkDescriptorSetLayoutBinding materialBinding{};
    materialBinding.binding = 9;
    materialBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    materialBinding.descriptorCount = 1;
    materialBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkDescriptorSetLayoutBinding bindings[] = {tlasBinding,   imageBinding,  sphereBinding, lightBinding, ambientBinding,
                                               cameraBinding, vertexBinding, indexBinding,  meshBinding,  materialBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 10;
    layoutInfo.pBindings = bindings;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ray tracing descriptor set layout.");
    }

    // Descriptor pool

    VkDescriptorPoolSize poolSizes[] = {{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
                                        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
                                        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6}, // was 2
                                        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2}};

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 4;
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

    // Light buffer descriptor

    VkDescriptorBufferInfo lightBufferInfo{};
    lightBufferInfo.buffer = m_lightBuffer.buffer;
    lightBufferInfo.offset = 0;
    lightBufferInfo.range = scene.lights().empty() ? sizeof(GPULight) : sizeof(GPULight) * scene.lights().size();

    VkWriteDescriptorSet lightWrite{};
    lightWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

    lightWrite.dstSet = m_descriptorSet;
    lightWrite.dstBinding = 3;
    lightWrite.dstArrayElement = 0;

    lightWrite.descriptorCount = 1;
    lightWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    lightWrite.pBufferInfo = &lightBufferInfo;

    // Ambient light buffer descriptor

    VkDescriptorBufferInfo ambientBufferInfo{};
    ambientBufferInfo.buffer = m_ambLightBuffer.buffer;
    ambientBufferInfo.offset = 0;
    ambientBufferInfo.range = sizeof(GPUAmbientLight);

    VkWriteDescriptorSet ambientWrite{};
    ambientWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

    ambientWrite.dstSet = m_descriptorSet;
    ambientWrite.dstBinding = 4;
    ambientWrite.dstArrayElement = 0;

    ambientWrite.descriptorCount = 1;
    ambientWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ambientWrite.pBufferInfo = &ambientBufferInfo;

    // Camera buffer descriptor

    VkDescriptorBufferInfo cameraBufferInfo{};
    cameraBufferInfo.buffer = m_cameraBuffer.buffer;
    cameraBufferInfo.offset = 0;
    cameraBufferInfo.range = sizeof(GPUCamera);

    VkWriteDescriptorSet cameraWrite{};
    cameraWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

    cameraWrite.dstSet = m_descriptorSet;
    cameraWrite.dstBinding = 5;
    cameraWrite.dstArrayElement = 0;

    cameraWrite.descriptorCount = 1;
    cameraWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cameraWrite.pBufferInfo = &cameraBufferInfo;

    // mesh buffers w material

    VkDescriptorBufferInfo vertexBufferInfo{.buffer = geometry.vertexBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};
    VkWriteDescriptorSet vertexWrite{};
    vertexWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    vertexWrite.dstSet = m_descriptorSet;
    vertexWrite.dstBinding = 6;
    vertexWrite.descriptorCount = 1;
    vertexWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    vertexWrite.pBufferInfo = &vertexBufferInfo;

    VkDescriptorBufferInfo indexBufferInfo{.buffer = geometry.indexBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};
    VkWriteDescriptorSet indexWrite{};
    indexWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    indexWrite.dstSet = m_descriptorSet;
    indexWrite.dstBinding = 7;
    indexWrite.descriptorCount = 1;
    indexWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    indexWrite.pBufferInfo = &indexBufferInfo;

    VkDescriptorBufferInfo meshBufferInfo{.buffer = geometry.meshBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};
    VkWriteDescriptorSet meshWrite{};
    meshWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    meshWrite.dstSet = m_descriptorSet;
    meshWrite.dstBinding = 8;
    meshWrite.descriptorCount = 1;
    meshWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    meshWrite.pBufferInfo = &meshBufferInfo;

    VkDescriptorBufferInfo materialBufferInfo{.buffer = geometry.materialBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};
    VkWriteDescriptorSet materialWrite{};
    materialWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    materialWrite.dstSet = m_descriptorSet;
    materialWrite.dstBinding = 9;
    materialWrite.descriptorCount = 1;
    materialWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    materialWrite.pBufferInfo = &materialBufferInfo;

    // Write descriptors

    VkWriteDescriptorSet writes[] = {tlasWrite,   imageWrite,  sphereWrite, lightWrite, ambientWrite,
                                     cameraWrite, vertexWrite, indexWrite,  meshWrite,  materialWrite};
    vkUpdateDescriptorSets(device, 10, writes, 0, nullptr);
}

void VulkanRTResources::shutdown() {
    if (m_context == nullptr) {
        return;
    }

    m_context->allocator().destroyBuffer(m_sphereBuffer);
    m_context->allocator().destroyBuffer(m_lightBuffer);
    m_context->allocator().destroyBuffer(m_ambLightBuffer);
    m_context->allocator().destroyBuffer(m_cameraBuffer);

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

void VulkanRTResources::updateCamera(const Camera& camera) {
    GPUCamera gpuCamera{camera.position(), camera.focalLength(), camera.forward(), camera.sensorWidth()};
    m_context->allocator().uploadBuffer(m_cameraBuffer, &gpuCamera, sizeof(GPUCamera));
}