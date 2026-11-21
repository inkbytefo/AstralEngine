#include "Vk/Pipelines.hpp"
#include "Vk/Device.hpp"

#include "Astral/Render/RenderTypes.hpp"

#include <cstddef>
#include <utility>

namespace Astral::Render::Detail {
std::span<const uint32_t> FindEmbeddedShader(std::string_view name); // generated
}

namespace Astral::Render::Vk {

std::span<const uint32_t> FindShader(std::string_view name) {
    auto code = Detail::FindEmbeddedShader(name);
    if (code.empty()) throw std::runtime_error("embedded shader not found: " + std::string(name));
    return code;
}

namespace {
vk::ShaderModule CreateModule(vk::Device device, const std::string& name) {
    auto code = FindShader(name);
    return device.createShaderModule({{}, code.size_bytes(), code.data()});
}
} // namespace

Pipeline::~Pipeline() { Release(); }
Pipeline::Pipeline(Pipeline&& o) noexcept { *this = std::move(o); }

Pipeline& Pipeline::operator=(Pipeline&& o) noexcept {
    if (this != &o) {
        Release();
        m_Device = std::exchange(o.m_Device, nullptr);
        m_Pipeline = std::exchange(o.m_Pipeline, nullptr);
        m_Layout = std::exchange(o.m_Layout, nullptr);
        m_SetLayout = std::exchange(o.m_SetLayout, nullptr);
        m_BindPoint = o.m_BindPoint;
    }
    return *this;
}

void Pipeline::Release() {
    if (!m_Device) return;
    vk::Device device = m_Device->Get();
    vk::Pipeline pipeline = m_Pipeline;
    vk::PipelineLayout layout = m_Layout;
    vk::DescriptorSetLayout setLayout = m_SetLayout;
    m_Device->Retire([=] {
        device.destroyPipeline(pipeline);
        device.destroyPipelineLayout(layout);
        device.destroyDescriptorSetLayout(setLayout);
    });
    m_Device = nullptr;
}

void Pipeline::CreateLayout(Device& device, const PipelineLayoutDesc& desc, vk::DescriptorSetLayout bindless) {
    m_Device = &device;
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    for (const Binding& b : desc.bindings)
        bindings.emplace_back(b.binding, b.type, b.count, vk::ShaderStageFlagBits::eAll);

    vk::DescriptorSetLayoutCreateInfo setInfo(vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR, bindings);
    m_SetLayout = device.Get().createDescriptorSetLayout(setInfo);

    std::vector<vk::DescriptorSetLayout> sets = {m_SetLayout};
    if (desc.bindlessTextures) sets.push_back(bindless);

    vk::PushConstantRange range(vk::ShaderStageFlagBits::eAll, 0, desc.pushConstantSize);
    vk::PipelineLayoutCreateInfo layoutInfo({}, sets);
    if (desc.pushConstantSize) {
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &range;
    }
    m_Layout = device.Get().createPipelineLayout(layoutInfo);
}

Pipeline Pipeline::Compute(Device& device, const std::string& shader, const PipelineLayoutDesc& layout,
                           vk::DescriptorSetLayout bindless) {
    Pipeline p;
    p.CreateLayout(device, layout, bindless);
    p.m_BindPoint = vk::PipelineBindPoint::eCompute;

    vk::ShaderModule module = CreateModule(device.Get(), shader);
    vk::ComputePipelineCreateInfo info;
    info.stage = vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eCompute, module, "main");
    info.layout = p.m_Layout;
    auto result = device.Get().createComputePipeline(nullptr, info);
    device.Get().destroyShaderModule(module);
    if (result.result != vk::Result::eSuccess) throw std::runtime_error("compute pipeline failed: " + shader);
    p.m_Pipeline = result.value;
    device.SetDebugName(vk::ObjectType::ePipeline, reinterpret_cast<uint64_t>(static_cast<VkPipeline>(p.m_Pipeline)), shader);
    return p;
}

Pipeline Pipeline::Graphics(Device& device, const GraphicsDesc& desc, const PipelineLayoutDesc& layout,
                            vk::DescriptorSetLayout bindless) {
    Pipeline p;
    p.CreateLayout(device, layout, bindless);
    p.m_BindPoint = vk::PipelineBindPoint::eGraphics;

    std::vector<vk::PipelineShaderStageCreateInfo> stages;
    vk::ShaderModule vs = CreateModule(device.Get(), desc.vertexShader);
    stages.push_back({{}, vk::ShaderStageFlagBits::eVertex, vs, "main"});
    vk::ShaderModule fs;
    if (!desc.fragmentShader.empty()) {
        fs = CreateModule(device.Get(), desc.fragmentShader);
        stages.push_back({{}, vk::ShaderStageFlagBits::eFragment, fs, "main"});
    }

    vk::VertexInputBindingDescription binding(0, sizeof(Vertex), vk::VertexInputRate::eVertex);
    std::array<vk::VertexInputAttributeDescription, 4> attributes{{
        {0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)},
        {1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal)},
        {2, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Vertex, tangent)},
        {3, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, uv)},
    }};
    vk::PipelineVertexInputStateCreateInfo vertexInput;
    if (desc.meshVertexInput) vertexInput = vk::PipelineVertexInputStateCreateInfo({}, binding, attributes);

    vk::PipelineInputAssemblyStateCreateInfo assembly({}, vk::PrimitiveTopology::eTriangleList);
    vk::PipelineViewportStateCreateInfo viewport({}, 1, nullptr, 1, nullptr);
    vk::PipelineRasterizationStateCreateInfo raster;
    raster.polygonMode = vk::PolygonMode::eFill;
    raster.cullMode = desc.cull;
    raster.frontFace = vk::FrontFace::eCounterClockwise;
    raster.lineWidth = 1.0f;
    raster.depthClampEnable = desc.depthClamp && device.DepthClampSupported();
    raster.depthBiasEnable = desc.depthBias;
    vk::PipelineMultisampleStateCreateInfo multisample;
    vk::PipelineDepthStencilStateCreateInfo depth({}, desc.depthTest, desc.depthWrite, desc.depthCompare);

    std::vector<vk::PipelineColorBlendAttachmentState> blends(desc.colorFormats.size());
    for (auto& b : blends) b.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    vk::PipelineColorBlendStateCreateInfo blend({}, false, {}, blends);

    std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    if (desc.depthBias) dynamicStates.push_back(vk::DynamicState::eDepthBias);
    vk::PipelineDynamicStateCreateInfo dynamic({}, dynamicStates);

    vk::PipelineRenderingCreateInfo rendering({}, desc.colorFormats, desc.depthFormat);

    vk::GraphicsPipelineCreateInfo info;
    info.pNext = &rendering;
    info.stageCount = static_cast<uint32_t>(stages.size());
    info.pStages = stages.data();
    info.pVertexInputState = &vertexInput;
    info.pInputAssemblyState = &assembly;
    info.pViewportState = &viewport;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &multisample;
    info.pDepthStencilState = &depth;
    info.pColorBlendState = &blend;
    info.pDynamicState = &dynamic;
    info.layout = p.m_Layout;

    auto result = device.Get().createGraphicsPipeline(nullptr, info);
    device.Get().destroyShaderModule(vs);
    if (fs) device.Get().destroyShaderModule(fs);
    if (result.result != vk::Result::eSuccess) throw std::runtime_error("graphics pipeline failed: " + desc.vertexShader);
    p.m_Pipeline = result.value;
    return p;
}

// ----------------------------------------------------------------------------- Bindings

Bindings& Bindings::StorageImage(uint32_t binding, vk::ImageView view) {
    m_Images.emplace_back(nullptr, view, vk::ImageLayout::eGeneral);
    m_Pending.push_back({binding, vk::DescriptorType::eStorageImage, true, m_Images.size() - 1});
    return *this;
}

Bindings& Bindings::SampledImage(uint32_t binding, vk::ImageView view, vk::Sampler sampler, vk::ImageLayout layout) {
    m_Images.emplace_back(sampler, view, layout);
    m_Pending.push_back({binding, vk::DescriptorType::eCombinedImageSampler, true, m_Images.size() - 1});
    return *this;
}

Bindings& Bindings::StorageBuffer(uint32_t binding, vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize size) {
    m_Buffers.emplace_back(buffer, offset, size);
    m_Pending.push_back({binding, vk::DescriptorType::eStorageBuffer, false, m_Buffers.size() - 1});
    return *this;
}

Bindings& Bindings::UniformBuffer(uint32_t binding, vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize size) {
    m_Buffers.emplace_back(buffer, offset, size);
    m_Pending.push_back({binding, vk::DescriptorType::eUniformBuffer, false, m_Buffers.size() - 1});
    return *this;
}

void Bindings::Push(vk::CommandBuffer cmd, const Pipeline& pipeline) {
    m_Writes.clear();
    for (const Pending& p : m_Pending) {
        vk::WriteDescriptorSet w;
        w.dstBinding = p.binding;
        w.descriptorCount = 1;
        w.descriptorType = p.type;
        if (p.image) w.pImageInfo = &m_Images[p.index];
        else w.pBufferInfo = &m_Buffers[p.index];
        m_Writes.push_back(w);
    }
    cmd.pushDescriptorSetKHR(pipeline.BindPoint(), pipeline.Layout(), 0, m_Writes);
}

// ----------------------------------------------------------------------------- Bindless

BindlessTextures::BindlessTextures(Device& device) : m_Device(device) {
    vk::DescriptorPoolSize size(vk::DescriptorType::eCombinedImageSampler, kCapacity);
    m_Pool = device.Get().createDescriptorPool({vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind, 1, size});

    vk::DescriptorSetLayoutBinding binding(0, vk::DescriptorType::eCombinedImageSampler, kCapacity, vk::ShaderStageFlagBits::eAll);
    vk::DescriptorBindingFlags flags = vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind;
    vk::DescriptorSetLayoutBindingFlagsCreateInfo flagsInfo(flags);
    vk::DescriptorSetLayoutCreateInfo layoutInfo(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool, binding);
    layoutInfo.pNext = &flagsInfo;
    m_Layout = device.Get().createDescriptorSetLayout(layoutInfo);
    m_Set = device.Get().allocateDescriptorSets({m_Pool, m_Layout})[0];
}

BindlessTextures::~BindlessTextures() {
    m_Device.Get().destroyDescriptorPool(m_Pool);
    m_Device.Get().destroyDescriptorSetLayout(m_Layout);
}

uint32_t BindlessTextures::Allocate(vk::ImageView view, vk::Sampler sampler) {
    uint32_t slot;
    if (!m_Free.empty()) {
        slot = m_Free.back();
        m_Free.pop_back();
    } else {
        if (m_Next >= kCapacity) throw std::runtime_error("bindless texture table is full");
        slot = m_Next++;
    }
    vk::DescriptorImageInfo image(sampler, view, vk::ImageLayout::eShaderReadOnlyOptimal);
    vk::WriteDescriptorSet write(m_Set, 0, slot, 1, vk::DescriptorType::eCombinedImageSampler, &image);
    m_Device.Get().updateDescriptorSets(write, {});
    return slot;
}

void BindlessTextures::Free(uint32_t slot) {
    // The descriptor may still be used by in-flight frames; recycle the slot later.
    m_Device.Retire([this, slot] { m_Free.push_back(slot); });
}

} // namespace Astral::Render::Vk
