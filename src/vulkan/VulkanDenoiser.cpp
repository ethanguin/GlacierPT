#include "VulkanDenoiser.hpp"

#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<char> readFile(const char* filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error(std::string("Failed to open shader file: ") + filename);
    }

    const std::streamsize fileSize = file.tellg();

    if (fileSize <= 0) {
        throw std::runtime_error(std::string("Shader file is empty: ") + filename);
    }

    std::vector<char> buffer(static_cast<size_t>(fileSize));

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    return buffer;
}

} // namespace

void VulkanDenoiser::initialize(VulkanContext& context, VulkanCommands& commands, VkExtent2D extent, VkImageView radianceView) {
    m_context = &context;
    m_extent = extent;

    VkDevice device = context.device();

    // IMAGES

    createImage(m_imageB, m_imageBView);
    createImage(m_guidePos, m_guidePosView);
    createImage(m_guideNormal, m_guideNormalView);

    transitionAllToGeneral(commands);

    // DESCRIPTOR SET LAYOUT: 0 = src, 1 = dst, 2 = guide position, 3 = guide normal

    VkDescriptorSetLayoutBinding bindings[4]{};

    for (uint32_t i = 0; i < 4; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo setLayoutInfo{};
    setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setLayoutInfo.bindingCount = 4;
    setLayoutInfo.pBindings = bindings;

    if (vkCreateDescriptorSetLayout(device, &setLayoutInfo, nullptr, &m_setLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create denoiser descriptor set layout.");
    }

    // DESCRIPTOR POOL + SETS

    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 8};

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 2;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create denoiser descriptor pool.");
    }

    VkDescriptorSetLayout layouts[2] = {m_setLayout, m_setLayout};

    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = m_pool;
    allocateInfo.descriptorSetCount = 2;
    allocateInfo.pSetLayouts = layouts;

    if (vkAllocateDescriptorSets(device, &allocateInfo, m_sets) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate denoiser descriptor sets.");
    }

    writeSet(m_sets[0], radianceView, m_imageBView); // A -> B
    writeSet(m_sets[1], m_imageBView, radianceView); // B -> A

    // PIPELINE LAYOUT (push constant: int stepSize)

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(int32_t);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_setLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create denoiser pipeline layout.");
    }

    // COMPUTE PIPELINE

    const auto code = readFile("shaders/atrous.comp.spv");

    VkShaderModuleCreateInfo moduleInfo{};
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.codeSize = code.size();
    moduleInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule = VK_NULL_HANDLE;

    if (vkCreateShaderModule(device, &moduleInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create denoiser shader module.");
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shaderModule;
    pipelineInfo.stage.pName = "CSMain";
    pipelineInfo.layout = m_pipelineLayout;

    VkResult result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline);

    vkDestroyShaderModule(device, shaderModule, nullptr);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create denoiser compute pipeline.");
    }
}

void VulkanDenoiser::shutdown() {
    if (m_context == nullptr) {
        return;
    }

    VkDevice device = m_context->device();
    VulkanAllocator& allocator = m_context->allocator();

    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }

    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }

    if (m_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_pool, nullptr);
        m_pool = VK_NULL_HANDLE;
        m_sets[0] = m_sets[1] = VK_NULL_HANDLE;
    }

    if (m_setLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_setLayout, nullptr);
        m_setLayout = VK_NULL_HANDLE;
    }

    for (VkImageView* view : {&m_imageBView, &m_guidePosView, &m_guideNormalView}) {
        if (*view != VK_NULL_HANDLE) {
            vkDestroyImageView(device, *view, nullptr);
            *view = VK_NULL_HANDLE;
        }
    }

    allocator.destroyImage(m_imageB);
    allocator.destroyImage(m_guidePos);
    allocator.destroyImage(m_guideNormal);

    m_context = nullptr;
}

void VulkanDenoiser::record(VkCommandBuffer cmd) const {
    // Used for RT->compute and compute->compute: no layout change, everything is GENERAL.
    VkMemoryBarrier2 rtToCompute{};
    rtToCompute.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    rtToCompute.srcStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    rtToCompute.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
    rtToCompute.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    rtToCompute.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

    VkDependencyInfo rtDependency{};
    rtDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    rtDependency.memoryBarrierCount = 1;
    rtDependency.pMemoryBarriers = &rtToCompute;

    vkCmdPipelineBarrier2(cmd, &rtDependency);

    VkMemoryBarrier2 computeToCompute = rtToCompute;
    computeToCompute.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

    VkDependencyInfo computeDependency = rtDependency;
    computeDependency.pMemoryBarriers = &computeToCompute;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

    const uint32_t groupsX = (m_extent.width + 7) / 8;
    const uint32_t groupsY = (m_extent.height + 7) / 8;

    for (int i = 0; i < kPasses; ++i) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &m_sets[i % 2], 0, nullptr);

        const int32_t stepSize = 1 << i;
        vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(stepSize), &stepSize);

        vkCmdDispatch(cmd, groupsX, groupsY, 1);

        if (i + 1 < kPasses) {
            vkCmdPipelineBarrier2(cmd, &computeDependency);
        }
    }
}

void VulkanDenoiser::createImage(AllocatedImage& image, VkImageView& view) {
    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    info.extent = {m_extent.width, m_extent.height, 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_STORAGE_BIT;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    image = m_context->allocator().createImage(info, VMA_MEMORY_USAGE_GPU_ONLY);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = info.format;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    if (vkCreateImageView(m_context->device(), &viewInfo, nullptr, &view) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create denoiser image view.");
    }
}

void VulkanDenoiser::transitionAllToGeneral(VulkanCommands& commands) {
    const VkImage images[3] = {m_imageB.image, m_guidePos.image, m_guideNormal.image};

    VkImageMemoryBarrier2 barriers[3]{};

    for (int i = 0; i < 3; ++i) {
        barriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barriers[i].srcStageMask = VK_PIPELINE_STAGE_2_NONE;
        barriers[i].srcAccessMask = VK_ACCESS_2_NONE;
        barriers[i].dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        barriers[i].dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
        barriers[i].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barriers[i].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[i].image = images[i];
        barriers[i].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    }

    VkDependencyInfo dependency{};
    dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.imageMemoryBarrierCount = 3;
    dependency.pImageMemoryBarriers = barriers;

    VkCommandBuffer cmd = commands.beginSingleTimeCommands();
    vkCmdPipelineBarrier2(cmd, &dependency);
    commands.endSingleTimeCommands(cmd, m_context->graphicsQueue());
}

void VulkanDenoiser::writeSet(VkDescriptorSet set, VkImageView src, VkImageView dst) {
    const VkImageView views[4] = {src, dst, m_guidePosView, m_guideNormalView};

    VkDescriptorImageInfo infos[4]{};
    VkWriteDescriptorSet writes[4]{};

    for (uint32_t i = 0; i < 4; ++i) {
        infos[i].imageView = views[i];
        infos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = set;
        writes[i].dstBinding = i;
        writes[i].dstArrayElement = 0;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[i].pImageInfo = &infos[i];
    }

    vkUpdateDescriptorSets(m_context->device(), 4, writes, 0, nullptr);
}
