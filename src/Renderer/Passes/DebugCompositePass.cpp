#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include "Astral/Renderer/Passes/DebugCompositePass.hpp"
#include "PassUtils.hpp"
#include "Astral/Renderer/ShaderInterop.hpp"
#include <array>

namespace Astral {

std::vector<vk::DescriptorSetLayoutBinding> DebugCompositePass::GetBindingDescriptions() {
    std::vector<vk::DescriptorSetLayoutBinding> bindings(6);
    for (uint32_t i = 0; i < 6; ++i) {
        bindings[i] = {i, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute};
    }
    return bindings;
}

DebugCompositePass::DebugCompositePass(vk::Device device, const std::string& spvPath)
    : m_Device(device) {
    ComputeProgramDesc desc;
    desc.shaderPath = spvPath;
    desc.pushConstantBytes = sizeof(DebugCompositePushConstants);
    desc.bindings = GetBindingDescriptions();

    m_Program = std::make_unique<ComputeProgram>(m_Device, desc);
    m_DescriptorPool = CreateDescriptorPoolFromBindings(m_Device, desc.bindings, 1);

    auto layout = m_Program->GetSetLayout();
    vk::DescriptorSetAllocateInfo allocInfo(m_DescriptorPool.get(), 1, &layout);
    auto sets = m_Device.allocateDescriptorSets(allocInfo);
    m_DescriptorSet = sets[0];
}

void DebugCompositePass::BindResources(const DebugInputs& inputs) {
    m_Inputs = inputs;

    vk::DescriptorImageInfo albedoInfo(nullptr, inputs.gbuffer.albedo.view, vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo normalInfo(nullptr, inputs.gbuffer.normal.view, vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo materialInfo(nullptr, inputs.gbuffer.material.view, vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo depthInfo(nullptr, inputs.gbuffer.depth.view, vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo motionInfo(nullptr, inputs.gbuffer.motion.view, vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo outInfo(nullptr, inputs.output.view, vk::ImageLayout::eGeneral);

    std::array<vk::WriteDescriptorSet, 6> writeSets{};
    writeSets[0].dstSet = m_DescriptorSet;
    writeSets[0].dstBinding = 0;
    writeSets[0].descriptorCount = 1;
    writeSets[0].descriptorType = vk::DescriptorType::eStorageImage;
    writeSets[0].pImageInfo = &albedoInfo;

    writeSets[1].dstSet = m_DescriptorSet;
    writeSets[1].dstBinding = 1;
    writeSets[1].descriptorCount = 1;
    writeSets[1].descriptorType = vk::DescriptorType::eStorageImage;
    writeSets[1].pImageInfo = &normalInfo;

    writeSets[2].dstSet = m_DescriptorSet;
    writeSets[2].dstBinding = 2;
    writeSets[2].descriptorCount = 1;
    writeSets[2].descriptorType = vk::DescriptorType::eStorageImage;
    writeSets[2].pImageInfo = &materialInfo;

    writeSets[3].dstSet = m_DescriptorSet;
    writeSets[3].dstBinding = 3;
    writeSets[3].descriptorCount = 1;
    writeSets[3].descriptorType = vk::DescriptorType::eStorageImage;
    writeSets[3].pImageInfo = &depthInfo;

    writeSets[4].dstSet = m_DescriptorSet;
    writeSets[4].dstBinding = 4;
    writeSets[4].descriptorCount = 1;
    writeSets[4].descriptorType = vk::DescriptorType::eStorageImage;
    writeSets[4].pImageInfo = &motionInfo;

    writeSets[5].dstSet = m_DescriptorSet;
    writeSets[5].dstBinding = 5;
    writeSets[5].descriptorCount = 1;
    writeSets[5].descriptorType = vk::DescriptorType::eStorageImage;
    writeSets[5].pImageInfo = &outInfo;

    m_Device.updateDescriptorSets(static_cast<uint32_t>(writeSets.size()), writeSets.data(), 0, nullptr);
}

void DebugCompositePass::Record(vk::CommandBuffer cmd, const RenderFrameSettings& settings) {
    DebugCompositePushConstants debugPush{};
    debugPush.screenRes = glm::vec4(
        static_cast<float>(settings.width),
        static_cast<float>(settings.height),
        static_cast<float>(settings.debugMode),
        0.0f
    );

    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_Program->GetPipeline());
    cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute,
        m_Program->GetLayout(),
        0,
        1, &m_DescriptorSet,
        0, nullptr
    );
    cmd.pushConstants(
        m_Program->GetLayout(),
        vk::ShaderStageFlagBits::eCompute,
        0,
        sizeof(DebugCompositePushConstants),
        &debugPush
    );

    uint32_t groupX = (settings.width + 7u) / 8u;
    uint32_t groupY = (settings.height + 7u) / 8u;
    cmd.dispatch(groupX, groupY, 1);
}

} // namespace Astral
