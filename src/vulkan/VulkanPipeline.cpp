#include "VulkanPipeline.hpp"

#include "Vertex.hpp"

#include <fstream>
#include <stdexcept>
#include <vector>
#include <cstddef>

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

VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule = VK_NULL_HANDLE;

    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {

        throw std::runtime_error("Failed to create shader module");
    }

    return shaderModule;
}

} // namespace

void VulkanPipeline::initialize(VkDevice device, VkFormat colorFormat) {
    const auto vertCode = readFile("shaders/triangle.vert.spv");

    const auto fragCode = readFile("shaders/triangle.frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(device, vertCode);

    VkShaderModule fragShaderModule = createShaderModule(device, fragCode);

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertShaderModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragShaderModule;
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertStage, fragStage};

    // VERTEX INPUT

    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributeDescriptions[] = {
        {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, position)},
        {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, color)}};

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDescription;

    vertexInput.vertexAttributeDescriptionCount = 2;
    vertexInput.pVertexAttributeDescriptions = attributeDescriptions;

    // INPUT ASSEMBLY

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;

    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // VIEWPORT

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;

    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // These are set dynamically in VulkanRenderer.

    // RASTERIZER

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;

    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;

    rasterizer.lineWidth = 1.0f;

    // Disable culling for the first triangle.
    rasterizer.cullMode = VK_CULL_MODE_NONE;

    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    // MULTISAMPLING

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;

    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    multisampling.sampleShadingEnable = VK_FALSE;

    // COLOR BLENDING

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable = VK_FALSE;

    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

    colorBlending.logicOpEnable = VK_FALSE;

    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // DYNAMIC STATE

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    // PIPELINE LAYOUT

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    layoutInfo.setLayoutCount = 0;
    layoutInfo.pSetLayouts = nullptr;

    layoutInfo.pushConstantRangeCount = 0;
    layoutInfo.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {

        vkDestroyShaderModule(device, fragShaderModule, nullptr);

        vkDestroyShaderModule(device, vertShaderModule, nullptr);

        throw std::runtime_error("Failed to create pipeline layout");
    }

    // DYNAMIC RENDERING

    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &colorFormat;

    // GRAPHICS PIPELINE

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    pipelineInfo.pNext = &renderingInfo;

    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;

    pipelineInfo.pVertexInputState = &vertexInput;

    pipelineInfo.pInputAssemblyState = &inputAssembly;

    pipelineInfo.pViewportState = &viewportState;

    pipelineInfo.pRasterizationState = &rasterizer;

    pipelineInfo.pMultisampleState = &multisampling;

    pipelineInfo.pDepthStencilState = nullptr;

    pipelineInfo.pColorBlendState = &colorBlending;

    pipelineInfo.pDynamicState = &dynamicState;

    pipelineInfo.layout = m_pipelineLayout;

    pipelineInfo.renderPass = VK_NULL_HANDLE;

    pipelineInfo.subpass = 0;

    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {

        vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);

        m_pipelineLayout = VK_NULL_HANDLE;

        vkDestroyShaderModule(device, fragShaderModule, nullptr);

        vkDestroyShaderModule(device, vertShaderModule, nullptr);

        throw std::runtime_error("Failed to create graphics pipeline");
    }

    // Shader modules are only needed during pipeline creation.
    vkDestroyShaderModule(device, fragShaderModule, nullptr);

    vkDestroyShaderModule(device, vertShaderModule, nullptr);
}

void VulkanPipeline::shutdown(VkDevice device) {
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_pipeline, nullptr);

        m_pipeline = VK_NULL_HANDLE;
    }

    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);

        m_pipelineLayout = VK_NULL_HANDLE;
    }
}