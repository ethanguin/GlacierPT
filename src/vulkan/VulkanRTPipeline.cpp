#include "VulkanRTPipeline.hpp"

#include <fstream>
#include <stdexcept>
#include <vector>

void VulkanRTPipeline::initialize(VulkanContext& context, VulkanRTResources& resources) {
    m_context = &context;

    VkDevice device = context.device();

    m_vkCreateRayTracingPipelinesKHR =
        reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR"));

    if (m_vkCreateRayTracingPipelinesKHR == nullptr) {
        throw std::runtime_error("Failed to load vkCreateRayTracingPipelinesKHR.");
    }

    m_vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR"));

    if (m_vkCmdTraceRaysKHR == nullptr) {
        throw std::runtime_error("Failed to load vkCmdTraceRaysKHR.");
    }

    m_raygenShader = loadShaderModule("shaders/raygen.rgen.spv");

    m_missShader = loadShaderModule("shaders/miss.rmiss.spv");

    m_missShadowShader = loadShaderModule("shaders/missshadow.rmiss.spv");

    m_intersectionShader = loadShaderModule("shaders/intersection.rint.spv");

    m_closestHitShader = loadShaderModule("shaders/closesthit.rchit.spv");

    VkDescriptorSetLayout descriptorSetLayout = resources.descriptorSetLayout();

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(RaygenPushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &descriptorSetLayout;

    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ray tracing pipeline layout.");
    }

    // SHADER STAGES

    VkPipelineShaderStageCreateInfo raygenStage{};
    raygenStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    raygenStage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    raygenStage.module = m_raygenShader;
    raygenStage.pName = "RayGen";

    VkPipelineShaderStageCreateInfo missStage{};
    missStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    missStage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    missStage.module = m_missShader;
    missStage.pName = "Miss";

    VkPipelineShaderStageCreateInfo missShadowStage{};
    missShadowStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    missShadowStage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    missShadowStage.module = m_missShadowShader;
    missShadowStage.pName = "MissShadow";

    VkPipelineShaderStageCreateInfo intersectionStage{};
    intersectionStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    intersectionStage.stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
    intersectionStage.module = m_intersectionShader;
    intersectionStage.pName = "Intersection";

    VkPipelineShaderStageCreateInfo closestHitStage{};
    closestHitStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    closestHitStage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    closestHitStage.module = m_closestHitShader;
    closestHitStage.pName = "ClosestHit";

    VkPipelineShaderStageCreateInfo shaderStages[] = {raygenStage, missStage, missShadowStage, intersectionStage, closestHitStage};

    // RAYGEN

    VkRayTracingShaderGroupCreateInfoKHR raygenGroup{};
    raygenGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    raygenGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    raygenGroup.generalShader = 0;
    raygenGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
    raygenGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    raygenGroup.intersectionShader = VK_SHADER_UNUSED_KHR;

    // MISS

    VkRayTracingShaderGroupCreateInfoKHR missGroup{};
    missGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    missGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    missGroup.generalShader = 1;
    missGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
    missGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    missGroup.intersectionShader = VK_SHADER_UNUSED_KHR;

    // MISS SHADOW

    VkRayTracingShaderGroupCreateInfoKHR missShadowGroup{};
    missShadowGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    missShadowGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    missShadowGroup.generalShader = 2;
    missShadowGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
    missShadowGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    missShadowGroup.intersectionShader = VK_SHADER_UNUSED_KHR;

    // HIT (gets AABB procedurally)

    VkRayTracingShaderGroupCreateInfoKHR hitGroup{};
    hitGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    hitGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
    hitGroup.generalShader = VK_SHADER_UNUSED_KHR;
    hitGroup.closestHitShader = 4;
    hitGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    hitGroup.intersectionShader = 3;

    VkRayTracingShaderGroupCreateInfoKHR shaderGroups[] = {raygenGroup, missGroup, missShadowGroup, hitGroup};

    // 0 = RayGen
    // 1 = Miss
    // 2 = Shadow Miss
    // 3 = Hit

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;

    pipelineInfo.stageCount = static_cast<uint32_t>(std::size(shaderStages));

    pipelineInfo.pStages = shaderStages;

    pipelineInfo.groupCount = static_cast<uint32_t>(std::size(shaderGroups));

    pipelineInfo.pGroups = shaderGroups;

    pipelineInfo.maxPipelineRayRecursionDepth = 1;

    pipelineInfo.layout = m_pipelineLayout;

    VkResult result = m_vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline);

    m_vkGetRayTracingShaderGroupHandlesKHR =
        reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR"));

    if (m_vkGetRayTracingShaderGroupHandlesKHR == nullptr) {
        throw std::runtime_error("Failed to load vkGetRayTracingShaderGroupHandlesKHR.");
    }

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ray tracing pipeline.");
    }

    createShaderBindingTable();
}

void VulkanRTPipeline::shutdown() {
    if (m_context == nullptr) {
        return;
    }

    VkDevice device = m_context->device();

    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_pipeline, nullptr);

        m_pipeline = VK_NULL_HANDLE;
    }

    m_context->allocator().destroyBuffer(m_shaderBindingTable);

    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);

        m_pipelineLayout = VK_NULL_HANDLE;
    }

    if (m_raygenShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_raygenShader, nullptr);

        m_raygenShader = VK_NULL_HANDLE;
    }

    if (m_missShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_missShader, nullptr);

        m_missShader = VK_NULL_HANDLE;
    }

    if (m_missShadowShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_missShadowShader, nullptr);

        m_missShadowShader = VK_NULL_HANDLE;
    }

    if (m_intersectionShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_intersectionShader, nullptr);

        m_intersectionShader = VK_NULL_HANDLE;
    }

    if (m_closestHitShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_closestHitShader, nullptr);

        m_closestHitShader = VK_NULL_HANDLE;
    }

    m_vkCreateRayTracingPipelinesKHR = nullptr;
    m_context = nullptr;
}

VkShaderModule VulkanRTPipeline::loadShaderModule(const char* path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error(std::string("Failed to open shader: ") + path);
    }

    const size_t fileSize = static_cast<size_t>(file.tellg());

    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

    createInfo.codeSize = buffer.size();

    createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

    VkShaderModule shaderModule = VK_NULL_HANDLE;

    if (vkCreateShaderModule(m_context->device(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error(std::string("Failed to create shader module: ") + path);
    }

    return shaderModule;
}

void VulkanRTPipeline::createShaderBindingTable() {
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties{};
    rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 properties2{};
    properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

    properties2.pNext = &rayTracingProperties;

    vkGetPhysicalDeviceProperties2(m_context->physicalDevice(), &properties2);

    const uint32_t handleSize = rayTracingProperties.shaderGroupHandleSize;

    const uint32_t handleAlignment = rayTracingProperties.shaderGroupHandleAlignment;

    const uint32_t baseAlignment = rayTracingProperties.shaderGroupBaseAlignment;

    auto alignUp = [](VkDeviceSize value, VkDeviceSize alignment) { return (value + alignment - 1) & ~(alignment - 1); };

    // Each shader record's stride
    const VkDeviceSize raygenStride = alignUp(handleSize, handleAlignment);

    const VkDeviceSize missStride = alignUp(handleSize, handleAlignment);

    const VkDeviceSize hitStride = alignUp(handleSize, handleAlignment);

    // One record in each region, so size == stride
    const VkDeviceSize raygenSize = raygenStride;
    const VkDeviceSize missSize = missStride * 2; // multiply by 1) miss shader 2) shadow miss shader
    const VkDeviceSize hitSize = hitStride;

    // Each region must start on shaderGroupBaseAlignment
    const VkDeviceSize raygenOffset = 0;

    const VkDeviceSize missOffset = alignUp(raygenOffset + raygenSize, baseAlignment);

    const VkDeviceSize hitOffset = alignUp(missOffset + missSize, baseAlignment);

    const VkDeviceSize totalSize = hitOffset + hitSize;

    // Get shader group handles
    const unsigned int shaderGroupAmt = 4;
    std::vector<uint8_t> handles(handleSize * shaderGroupAmt);

    VkResult result = m_vkGetRayTracingShaderGroupHandlesKHR(m_context->device(), m_pipeline, 0, shaderGroupAmt, handles.size(), handles.data());

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to get ray tracing shader group handles.");
    }

    // Build the SBT data with required padding between regions
    std::vector<uint8_t> sbtData(totalSize);

    auto* raygenData = sbtData.data() + raygenOffset;
    auto* missData = sbtData.data() + missOffset;
    auto* hitData = sbtData.data() + hitOffset;

    std::memcpy(raygenData, handles.data() + handleSize * 0, handleSize);
    std::memcpy(missData, handles.data() + handleSize * 1, handleSize);
    std::memcpy(missData + missStride, handles.data() + handleSize * 2, handleSize);
    std::memcpy(hitData, handles.data() + handleSize * 3, handleSize);

    // Create SBT buffer
    m_shaderBindingTable = m_context->allocator().createBuffer(
        totalSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    m_context->allocator().uploadBuffer(m_shaderBindingTable, sbtData.data(), totalSize);

    // Get SBT buffer device address
    VkBufferDeviceAddressInfo addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;

    addressInfo.buffer = m_shaderBindingTable.buffer;

    VkDeviceAddress sbtAddress = vkGetBufferDeviceAddress(m_context->device(), &addressInfo);

    // RayGen region
    m_raygenRegion.deviceAddress = sbtAddress + raygenOffset;

    m_raygenRegion.stride = raygenStride;

    m_raygenRegion.size = raygenSize;

    // Miss region
    m_missRegion.deviceAddress = sbtAddress + missOffset;

    m_missRegion.stride = missStride;

    m_missRegion.size = missSize;

    // Hit region
    m_hitRegion.deviceAddress = sbtAddress + hitOffset;

    m_hitRegion.stride = hitStride;

    m_hitRegion.size = hitSize;
}