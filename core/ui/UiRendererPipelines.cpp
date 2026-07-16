// Pipeline and render pass setup for UiRenderer. Kept in a separate
// translation unit so UiRenderer.cpp stays focused on frame management and
// draw calls. All functions here are private methods of UiRenderer.

#include "UiRenderer.h"
#include "UiVulkanUtils.h"

#include "generated_shaders/ui.vert.h"
#include "generated_shaders/ui_solid.frag.h"
#include "generated_shaders/ui_text.frag.h"
#include "generated_shaders/ui_image.frag.h"
#include "generated_shaders/ui_image_rounded.frag.h"
#include "generated_shaders/screen_pattern.frag.h"

#include <stdexcept>

namespace
{
    void CheckVk(VkResult result, const char *what)
    {
        if (result != VK_SUCCESS)
            throw std::runtime_error(std::string("Vulkan call failed: ") + what + " (" + std::to_string(result) + ")");
    }
} // namespace

VkRenderPass UiRenderer::GetOrCreateRenderPass(VkFormat format)
{
    if (m_renderPass != VK_NULL_HANDLE && m_renderPassFormat == format)
        return m_renderPass;
    if (m_renderPass != VK_NULL_HANDLE)
        throw std::runtime_error("UiRenderer: swapchain format changed after first use - not supported");

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // OpenXR hands color swapchain images over in COLOR_ATTACHMENT_OPTIMAL
    // and requires them back in it - declaring UNDEFINED here breaks
    // SteamVR's shared-image interop (device loss).
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkRenderPassCreateInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    CheckVk(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass), "vkCreateRenderPass (ui)");
    m_renderPassFormat = format;
    return m_renderPass;
}

VkRenderPass UiRenderer::GetOrCreateOffscreenRenderPass(VkFormat format)
{
    if (m_offscreenRenderPass != VK_NULL_HANDLE && m_offscreenRenderPassFormat == format)
        return m_offscreenRenderPass;
    if (m_offscreenRenderPass != VK_NULL_HANDLE)
        throw std::runtime_error("UiRenderer: offscreen render texture format changed after first use - not supported");

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // Ends in shader-readable layout - this target gets sampled by
    // DrawImageRounded's compositing draw in the next render pass.
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkSubpassDependency dependencies[2]{};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = 0;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    // Downstream sampling must wait for the color attachment write to finish.
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 2;
    renderPassInfo.pDependencies = dependencies;
    CheckVk(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_offscreenRenderPass),
            "vkCreateRenderPass (ui offscreen)");
    m_offscreenRenderPassFormat = format;
    return m_offscreenRenderPass;
}

void UiRenderer::EnsurePipelines(VkFormat format)
{
    GetOrCreateRenderPass(format);
    if (m_solidPipeline != VK_NULL_HANDLE)
        return;

    // --- Shared vertex state ---
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(float) * 2;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attr{0, 0, VK_FORMAT_R32G32_SFLOAT, 0};

    VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 1;
    vertexInput.pVertexAttributeDescriptions = &attr;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Standard alpha blending for semi-transparent UI elements.
    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.blendEnable = VK_TRUE;
    blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &blendAttachment;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkPushConstantRange pushConstant{};
    // Fragment access is only actually used by screen_pattern.frag (reading
    // its patternColors tail directly - see PushConstants' doc comment);
    // granting it on every pipeline layout is harmless for the shaders that
    // don't declare a push_constant block of their own. DrawUnitQuad's
    // single vkCmdPushConstants call must pass this same stage mask for
    // every pipeline, since it's shared across all of them.
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(PushConstants);

    VkShaderModuleCreateInfo vertModuleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    vertModuleInfo.codeSize = g_ui_vert_size;
    vertModuleInfo.pCode = g_ui_vert;
    VkShaderModule vertModule;
    CheckVk(vkCreateShaderModule(m_device, &vertModuleInfo, nullptr, &vertModule), "vkCreateShaderModule (ui.vert)");

    // --- Solid-color pipeline (no descriptor set) ---
    {
        VkShaderModuleCreateInfo fragModuleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        fragModuleInfo.codeSize = g_ui_solid_frag_size;
        fragModuleInfo.pCode = g_ui_solid_frag;
        VkShaderModule fragModule;
        CheckVk(vkCreateShaderModule(m_device, &fragModuleInfo, nullptr, &fragModule), "vkCreateShaderModule (ui_solid.frag)");

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertModule;
        stages[0].pName = "main";
        stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragModule;
        stages[1].pName = "main";

        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstant;
        CheckVk(vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_solidPipelineLayout),
                "vkCreatePipelineLayout (ui solid)");

        VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pColorBlendState = &colorBlend;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_solidPipelineLayout;
        pipelineInfo.renderPass = m_renderPass;
        pipelineInfo.subpass = 0;
        CheckVk(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_solidPipeline),
                "vkCreateGraphicsPipelines (ui solid)");
        vkDestroyShaderModule(m_device, fragModule, nullptr);
    }

    // --- Text pipeline (R8 glyph atlas, one combined-image-sampler) ---
    // m_textDescriptorSetLayout is created eagerly in Initialize() so
    // LoadFont can allocate descriptor sets before the first BeginFrame.
    {
        VkShaderModuleCreateInfo fragModuleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        fragModuleInfo.codeSize = g_ui_text_frag_size;
        fragModuleInfo.pCode = g_ui_text_frag;
        VkShaderModule fragModule;
        CheckVk(vkCreateShaderModule(m_device, &fragModuleInfo, nullptr, &fragModule), "vkCreateShaderModule (ui_text.frag)");

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertModule;
        stages[0].pName = "main";
        stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragModule;
        stages[1].pName = "main";

        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &m_textDescriptorSetLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstant;
        CheckVk(vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_textPipelineLayout),
                "vkCreatePipelineLayout (ui text)");

        VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pColorBlendState = &colorBlend;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_textPipelineLayout;
        pipelineInfo.renderPass = m_renderPass;
        pipelineInfo.subpass = 0;
        CheckVk(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_textPipeline),
                "vkCreateGraphicsPipelines (ui text)");
        vkDestroyShaderModule(m_device, fragModule, nullptr);
    }

    // --- Image pipeline (RGBA textures) ---
    // Reuses m_textPipelineLayout - same descriptor shape (one sampler at binding 0).
    {
        VkShaderModuleCreateInfo fragModuleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        fragModuleInfo.codeSize = g_ui_image_frag_size;
        fragModuleInfo.pCode = g_ui_image_frag;
        VkShaderModule fragModule;
        CheckVk(vkCreateShaderModule(m_device, &fragModuleInfo, nullptr, &fragModule), "vkCreateShaderModule (ui_image.frag)");

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertModule;
        stages[0].pName = "main";
        stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragModule;
        stages[1].pName = "main";

        VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pColorBlendState = &colorBlend;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_textPipelineLayout;
        pipelineInfo.renderPass = m_renderPass;
        pipelineInfo.subpass = 0;
        CheckVk(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_imagePipeline),
                "vkCreateGraphicsPipelines (ui image)");
        vkDestroyShaderModule(m_device, fragModule, nullptr);
    }

    // --- Rounded-image pipeline (composites a pre-rendered buffer with rounded corners) ---
    {
        VkShaderModuleCreateInfo fragModuleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        fragModuleInfo.codeSize = g_ui_image_rounded_frag_size;
        fragModuleInfo.pCode = g_ui_image_rounded_frag;
        VkShaderModule fragModule;
        CheckVk(vkCreateShaderModule(m_device, &fragModuleInfo, nullptr, &fragModule),
                "vkCreateShaderModule (ui_image_rounded.frag)");

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertModule;
        stages[0].pName = "main";
        stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragModule;
        stages[1].pName = "main";

        VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pColorBlendState = &colorBlend;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_textPipelineLayout;
        pipelineInfo.renderPass = m_renderPass;
        pipelineInfo.subpass = 0;
        CheckVk(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_imageRoundedPipeline),
                "vkCreateGraphicsPipelines (ui image rounded)");
        vkDestroyShaderModule(m_device, fragModule, nullptr);
    }

    // --- Screen pattern pipeline (recolors the VB screen texture via a
    // multi-hue gradient instead of a flat tint) ---
    // Reuses m_textPipelineLayout - same descriptor shape as the image
    // pipeline above (one sampler at binding 0).
    {
        VkShaderModuleCreateInfo fragModuleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        fragModuleInfo.codeSize = g_screen_pattern_frag_size;
        fragModuleInfo.pCode = g_screen_pattern_frag;
        VkShaderModule fragModule;
        CheckVk(vkCreateShaderModule(m_device, &fragModuleInfo, nullptr, &fragModule),
                "vkCreateShaderModule (screen_pattern.frag)");

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertModule;
        stages[0].pName = "main";
        stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragModule;
        stages[1].pName = "main";

        VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pColorBlendState = &colorBlend;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_textPipelineLayout;
        pipelineInfo.renderPass = m_renderPass;
        pipelineInfo.subpass = 0;
        CheckVk(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_screenPatternPipeline),
                "vkCreateGraphicsPipelines (screen pattern)");
        vkDestroyShaderModule(m_device, fragModule, nullptr);
    }

    vkDestroyShaderModule(m_device, vertModule, nullptr);
}

UiRenderer::RenderTarget &UiRenderer::GetOrCreateRenderTarget(VkImage image, VkFormat format, uint32_t width,
                                                              uint32_t height, VkRenderPass renderPass)
{
    auto it = m_renderTargets.find(image);
    if (it != m_renderTargets.end())
        return it->second;

    RenderTarget target;
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    CheckVk(vkCreateImageView(m_device, &viewInfo, nullptr, &target.view), "vkCreateImageView (ui target)");

    VkFramebufferCreateInfo fbInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fbInfo.renderPass = renderPass;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments = &target.view;
    fbInfo.width = width;
    fbInfo.height = height;
    fbInfo.layers = 1;
    CheckVk(vkCreateFramebuffer(m_device, &fbInfo, nullptr, &target.framebuffer), "vkCreateFramebuffer (ui target)");

    auto [inserted, ok] = m_renderTargets.emplace(image, target);
    return inserted->second;
}
