#include "vk_device.hpp"

#include <string>
#include <vector>

namespace astral::gpu {

namespace {

VkShaderModule create_module(Device::Impl& s, const ShaderBlob& blob) {
    if (blob.spirv.empty()) return VK_NULL_HANDLE;
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize = blob.spirv.size_bytes();
    info.pCode = blob.spirv.data();
    VkShaderModule module = VK_NULL_HANDLE;
    vkCreateShaderModule(s.device, &info, nullptr, &module);
    return module;
}

VkCompareOp to_vk(CompareOp op) {
    switch (op) {
    case CompareOp::Never: return VK_COMPARE_OP_NEVER;
    case CompareOp::Less: return VK_COMPARE_OP_LESS;
    case CompareOp::LessEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
    case CompareOp::Greater: return VK_COMPARE_OP_GREATER;
    case CompareOp::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case CompareOp::Equal: return VK_COMPARE_OP_EQUAL;
    case CompareOp::Always: return VK_COMPARE_OP_ALWAYS;
    }
    return VK_COMPARE_OP_LESS;
}

VkPrimitiveTopology to_vk(Topology t) {
    switch (t) {
    case Topology::TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case Topology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case Topology::LineList: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case Topology::PointList: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    }
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

VkCullModeFlags to_vk(CullMode c) {
    switch (c) {
    case CullMode::None: return VK_CULL_MODE_NONE;
    case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
    case CullMode::Back: return VK_CULL_MODE_BACK_BIT;
    }
    return VK_CULL_MODE_NONE;
}

} // namespace

PipelineHandle Device::create_compute_pipeline(const ComputePipelineDesc& desc) {
    auto& s = *impl_;
    VkShaderModule module = create_module(s, desc.shader);
    if (!module) {
        s.log_error("create_compute_pipeline: invalid SPIR-V");
        return {};
    }
    const std::string entry(desc.shader.entry);
    VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    info.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    info.stage.module = module;
    info.stage.pName = entry.c_str();
    info.layout = s.pipeline_layout;
    PipelineRecord record{VK_NULL_HANDLE, VK_PIPELINE_BIND_POINT_COMPUTE};
    const VkResult result = vkCreateComputePipelines(s.device, VK_NULL_HANDLE, 1, &info, nullptr, &record.pipeline);
    vkDestroyShaderModule(s.device, module, nullptr);
    if (result != VK_SUCCESS) {
        s.log_error("create_compute_pipeline: vkCreateComputePipelines failed");
        return {};
    }
    s.name_object(VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<std::uint64_t>(record.pipeline), desc.debug_name);
    return s.pipelines.create(record);
}

PipelineHandle Device::create_graphics_pipeline(const GraphicsPipelineDesc& desc) {
    auto& s = *impl_;
    VkShaderModule vs = create_module(s, desc.vertex);
    VkShaderModule fs = create_module(s, desc.fragment);
    if (!vs || (!desc.fragment.spirv.empty() && !fs)) {
        if (vs) vkDestroyShaderModule(s.device, vs, nullptr);
        if (fs) vkDestroyShaderModule(s.device, fs, nullptr);
        s.log_error("create_graphics_pipeline: invalid SPIR-V");
        return {};
    }
    const std::string vs_entry(desc.vertex.entry);
    const std::string fs_entry(desc.fragment.entry);
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                 VK_SHADER_STAGE_VERTEX_BIT, vs, vs_entry.c_str(), nullptr};
    stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                 VK_SHADER_STAGE_FRAGMENT_BIT, fs, fs_entry.c_str(), nullptr};

    VkPipelineVertexInputStateCreateInfo vertex_input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    assembly.topology = to_vk(desc.topology);
    VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = to_vk(desc.cull);
    raster.frontFace = desc.front_counter_clockwise ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
    raster.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    const bool has_depth = desc.depth.format != Format::Undefined;
    depth.depthTestEnable = has_depth && desc.depth.test;
    depth.depthWriteEnable = has_depth && desc.depth.write;
    depth.depthCompareOp = to_vk(desc.depth.compare);

    std::vector<VkPipelineColorBlendAttachmentState> blends(desc.color_formats.size());
    for (auto& b : blends) {
        b.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        if (desc.alpha_blend) {
            b.blendEnable = VK_TRUE;
            b.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            b.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            b.colorBlendOp = VK_BLEND_OP_ADD;
            b.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            b.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            b.alphaBlendOp = VK_BLEND_OP_ADD;
        }
    }
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend.attachmentCount = static_cast<std::uint32_t>(blends.size());
    blend.pAttachments = blends.data();

    const VkDynamicState dynamic_states[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamic_states;

    std::vector<VkFormat> color_formats;
    for (Format f : desc.color_formats) color_formats.push_back(to_vk(f));
    VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rendering.colorAttachmentCount = static_cast<std::uint32_t>(color_formats.size());
    rendering.pColorAttachmentFormats = color_formats.data();
    rendering.depthAttachmentFormat = to_vk(desc.depth.format);

    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &rendering};
    info.stageCount = fs ? 2u : 1u;
    info.pStages = stages;
    info.pVertexInputState = &vertex_input;
    info.pInputAssemblyState = &assembly;
    info.pViewportState = &viewport;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &multisample;
    info.pDepthStencilState = &depth;
    info.pColorBlendState = &blend;
    info.pDynamicState = &dynamic;
    info.layout = s.pipeline_layout;

    PipelineRecord record{VK_NULL_HANDLE, VK_PIPELINE_BIND_POINT_GRAPHICS};
    const VkResult result = vkCreateGraphicsPipelines(s.device, VK_NULL_HANDLE, 1, &info, nullptr, &record.pipeline);
    vkDestroyShaderModule(s.device, vs, nullptr);
    if (fs) vkDestroyShaderModule(s.device, fs, nullptr);
    if (result != VK_SUCCESS) {
        s.log_error("create_graphics_pipeline: vkCreateGraphicsPipelines failed");
        return {};
    }
    s.name_object(VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<std::uint64_t>(record.pipeline), desc.debug_name);
    return s.pipelines.create(record);
}

void Device::destroy(PipelineHandle pipeline) {
    auto& s = *impl_;
    const PipelineRecord* r = s.pipelines.get(pipeline);
    if (!r) return;
    const VkPipeline native = r->pipeline;
    s.pipelines.destroy(pipeline);
    s.retire([&s, native] { vkDestroyPipeline(s.device, native, nullptr); });
}

} // namespace astral::gpu
