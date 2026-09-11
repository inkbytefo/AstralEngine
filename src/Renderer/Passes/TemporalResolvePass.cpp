#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include "Astral/Renderer/Passes/TemporalResolvePass.hpp"
#include "PassUtils.hpp"
#include "Astral/Renderer/ShaderInterop.hpp"
#include <array>

namespace Astral {

std::vector<vk::DescriptorSetLayoutBinding> TemporalResolvePass::GetBindingDescriptions() {
    std::vector<vk::DescriptorSetLayoutBinding> bindings(11);
    bindings[0] = {0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute};
    bindings[1] = {1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eCompute};
    bindings[2] = {2, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute};
    bindings[3] = {3, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute};
    for (uint32_t i = 4; i < 11; ++i) {
        bindings[i] = {i, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute};
    }
    return bindings;
}

TemporalResolvePass::TemporalResolvePass(vk::Device device, const std::string& spvPath)
    : m_Device(device) {
    ComputeProgramDesc desc;
    desc.shaderPath = spvPath;
    desc.pushConstantBytes = sizeof(TAAPushConstants);
    desc.bindings = GetBindingDescriptions();

    m_Program = std::make_unique<ComputeProgram>(m_Device, desc);
    m_DescriptorPool = CreateDescriptorPoolFromBindings(m_Device, desc.bindings, 2);

    auto layout = m_Program->GetSetLayout();
    for (uint32_t i = 0; i < 2; ++i) {
        vk::DescriptorSetAllocateInfo allocInfo(m_DescriptorPool.get(), 1, &layout);
        auto sets = m_Device.allocateDescriptorSets(allocInfo);
        m_DescriptorSets[i] = sets[0];
    }
}

void TemporalResolvePass::BindResources(const ResolveInputs& inputs) {
    m_Inputs = inputs;

    for (uint32_t i = 0; i < 2; ++i) {
        uint32_t readIdx = i;
        uint32_t writeIdx = 1 - i;

        vk::DescriptorImageInfo taaCurrInfo(nullptr, inputs.rawColor.view, vk::ImageLayout::eGeneral);
        vk::DescriptorImageInfo taaHistInfo(inputs.historySampler, inputs.historyColor[readIdx].view, vk::ImageLayout::eGeneral);
        vk::DescriptorImageInfo taaOutInfo(nullptr, inputs.output.view, vk::ImageLayout::eGeneral);
        vk::DescriptorImageInfo taaOutHistInfo(nullptr, inputs.historyColor[writeIdx].view, vk::ImageLayout::eGeneral);
        vk::DescriptorImageInfo taaMotionInfo(nullptr, inputs.gbuffer.motion.view, vk::ImageLayout::eGeneral);

        vk::DescriptorImageInfo depthInfo(nullptr, inputs.gbuffer.depth.view, vk::ImageLayout::eGeneral);
        vk::DescriptorImageInfo normalInfo(nullptr, inputs.gbuffer.normal.view, vk::ImageLayout::eGeneral);
        vk::DescriptorImageInfo materialInfo(nullptr, inputs.gbuffer.material.view, vk::ImageLayout::eGeneral);
        vk::DescriptorImageInfo albedoInfo(nullptr, inputs.gbuffer.albedo.view, vk::ImageLayout::eGeneral);
        vk::DescriptorImageInfo histExtraReadInfo(nullptr, inputs.historyExtra[readIdx].view, vk::ImageLayout::eGeneral);
        vk::DescriptorImageInfo histExtraWriteInfo(nullptr, inputs.historyExtra[writeIdx].view, vk::ImageLayout::eGeneral);

        std::array<vk::WriteDescriptorSet, 11> taaWriteSets{};
        taaWriteSets[0].dstSet = m_DescriptorSets[i];
        taaWriteSets[0].dstBinding = 0;
        taaWriteSets[0].descriptorCount = 1;
        taaWriteSets[0].descriptorType = vk::DescriptorType::eStorageImage;
        taaWriteSets[0].pImageInfo = &taaCurrInfo;

        taaWriteSets[1].dstSet = m_DescriptorSets[i];
        taaWriteSets[1].dstBinding = 1;
        taaWriteSets[1].descriptorCount = 1;
        taaWriteSets[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        taaWriteSets[1].pImageInfo = &taaHistInfo;

        taaWriteSets[2].dstSet = m_DescriptorSets[i];
        taaWriteSets[2].dstBinding = 2;
        taaWriteSets[2].descriptorCount = 1;
        taaWriteSets[2].descriptorType = vk::DescriptorType::eStorageImage;
        taaWriteSets[2].pImageInfo = &taaOutInfo;

        taaWriteSets[3].dstSet = m_DescriptorSets[i];
        taaWriteSets[3].dstBinding = 3;
        taaWriteSets[3].descriptorCount = 1;
        taaWriteSets[3].descriptorType = vk::DescriptorType::eStorageImage;
        taaWriteSets[3].pImageInfo = &taaOutHistInfo;

        taaWriteSets[4].dstSet = m_DescriptorSets[i];
        taaWriteSets[4].dstBinding = 4;
        taaWriteSets[4].descriptorCount = 1;
        taaWriteSets[4].descriptorType = vk::DescriptorType::eStorageImage;
        taaWriteSets[4].pImageInfo = &taaMotionInfo;

        taaWriteSets[5] = taaWriteSets[4];
        taaWriteSets[5].dstBinding = 5;
        taaWriteSets[5].pImageInfo = &depthInfo;

        taaWriteSets[6] = taaWriteSets[4];
        taaWriteSets[6].dstBinding = 6;
        taaWriteSets[6].pImageInfo = &normalInfo;

        taaWriteSets[7] = taaWriteSets[4];
        taaWriteSets[7].dstBinding = 7;
        taaWriteSets[7].pImageInfo = &materialInfo;

        taaWriteSets[8] = taaWriteSets[4];
        taaWriteSets[8].dstBinding = 8;
        taaWriteSets[8].pImageInfo = &albedoInfo;

        taaWriteSets[9] = taaWriteSets[4];
        taaWriteSets[9].dstBinding = 9;
        taaWriteSets[9].pImageInfo = &histExtraReadInfo;

        taaWriteSets[10] = taaWriteSets[4];
        taaWriteSets[10].dstBinding = 10;
        taaWriteSets[10].pImageInfo = &histExtraWriteInfo;

        m_Device.updateDescriptorSets(static_cast<uint32_t>(taaWriteSets.size()), taaWriteSets.data(), 0, nullptr);
    }
}

void TemporalResolvePass::Record(vk::CommandBuffer cmd, const RenderFrameSettings& settings,
                                 const TemporalPlan& temporalPlan, const SDFChangeSet& changeSet) {
    uint32_t readIdx = temporalPlan.readIndex % 2;
    auto descriptorSet = m_DescriptorSets[readIdx];

    TAAPushConstants taaPush{};
    taaPush.colorParams.x = settings.exposure;
    uint32_t numChangedRects = 0;
    std::array<glm::vec4, 4> packedChangedRects{};
    changeSet.GetPackedRects(packedChangedRects, numChangedRects);
    taaPush.colorParams.y = static_cast<float>(numChangedRects);
    taaPush.colorParams.z = changeSet.IsGlobalChange() ? 1.0f : 0.0f;
    taaPush.changedRect0 = packedChangedRects[0];
    taaPush.changedRect1 = packedChangedRects[1];
    taaPush.changedRect2 = packedChangedRects[2];
    taaPush.changedRect3 = packedChangedRects[3];

    bool isDebugActive = (settings.debugMode != 0);
    bool isFirstHistory = !temporalPlan.useHistory || (settings.frameId == 0) || !settings.taaEnabled || isDebugActive;

    float blendAlpha = settings.quality.taaBlendAlpha;
    if (isDebugActive) {
        blendAlpha = -1.0f;
    } else if (!settings.taaEnabled) {
        blendAlpha = 1.0f;
    }

    taaPush.screenRes = glm::vec4(
        static_cast<float>(settings.width),
        static_cast<float>(settings.height),
        (isFirstHistory || !settings.taaEnabled || isDebugActive) ? 0.0f : static_cast<float>(settings.frameId),
        blendAlpha
    );

    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_Program->GetPipeline());
    cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute,
        m_Program->GetLayout(),
        0,
        1, &descriptorSet,
        0, nullptr
    );
    cmd.pushConstants(
        m_Program->GetLayout(),
        vk::ShaderStageFlagBits::eCompute,
        0,
        sizeof(TAAPushConstants),
        &taaPush
    );

    uint32_t groupX = (settings.width + 7u) / 8u;
    uint32_t groupY = (settings.height + 7u) / 8u;
    cmd.dispatch(groupX, groupY, 1);
}

} // namespace Astral
