#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include "Astral/Renderer/Passes/DeferredLightingPass.hpp"
#include "PassUtils.hpp"
#include "Astral/Renderer/ShaderInterop.hpp"
#include <array>

namespace Astral {

std::vector<vk::DescriptorSetLayoutBinding> DeferredLightingPass::GetBindingDescriptions() {
    std::vector<vk::DescriptorSetLayoutBinding> bindings(11);
    for (uint32_t i = 0; i < 5; ++i) {
        bindings[i] = {i, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute};
    }
    for (uint32_t i = 5; i < 8; ++i) {
        bindings[i] = {i, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eCompute};
    }
    for (uint32_t i = 8; i < 11; ++i) {
        bindings[i] = {i, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute};
    }
    return bindings;
}

DeferredLightingPass::DeferredLightingPass(vk::Device device, const std::string& spvPath)
    : m_Device(device) {
    ComputeProgramDesc desc;
    desc.shaderPath = spvPath;
    desc.pushConstantBytes = sizeof(DeferredLightingPushConstants);
    desc.bindings = GetBindingDescriptions();

    m_Program = std::make_unique<ComputeProgram>(m_Device, desc);
    m_DescriptorPool = CreateDescriptorPoolFromBindings(m_Device, desc.bindings, 1);

    auto layout = m_Program->GetSetLayout();
    vk::DescriptorSetAllocateInfo allocInfo(m_DescriptorPool.get(), 1, &layout);
    auto sets = m_Device.allocateDescriptorSets(allocInfo);
    m_DescriptorSet = sets[0];
}

void DeferredLightingPass::BindResources(const LightingInputs& inputs) {
    m_Inputs = inputs;

    vk::DescriptorImageInfo albedoInfo(nullptr, inputs.gbuffer.albedo.view, vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo normalInfo(nullptr, inputs.gbuffer.normal.view, vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo materialInfo(nullptr, inputs.gbuffer.material.view, vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo depthInfo(nullptr, inputs.gbuffer.depth.view, vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo outColorInfo(nullptr, inputs.output.view, vk::ImageLayout::eGeneral);

    auto irradianceInfo = inputs.irradiance;
    auto prefilteredInfo = inputs.prefiltered;
    auto brdfLutInfo = inputs.brdf;

    auto lightBufInfo = inputs.scene.lights;
    auto editBufInfo = inputs.scene.primitives;
    auto gridBufInfo = inputs.scene.grid;

    std::array<vk::WriteDescriptorSet, 11> writeSets{};
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
    writeSets[4].pImageInfo = &outColorInfo;

    writeSets[5].dstSet = m_DescriptorSet;
    writeSets[5].dstBinding = 5;
    writeSets[5].descriptorCount = 1;
    writeSets[5].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writeSets[5].pImageInfo = &irradianceInfo;

    writeSets[6].dstSet = m_DescriptorSet;
    writeSets[6].dstBinding = 6;
    writeSets[6].descriptorCount = 1;
    writeSets[6].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writeSets[6].pImageInfo = &prefilteredInfo;

    writeSets[7].dstSet = m_DescriptorSet;
    writeSets[7].dstBinding = 7;
    writeSets[7].descriptorCount = 1;
    writeSets[7].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writeSets[7].pImageInfo = &brdfLutInfo;

    writeSets[8].dstSet = m_DescriptorSet;
    writeSets[8].dstBinding = 8;
    writeSets[8].descriptorCount = 1;
    writeSets[8].descriptorType = vk::DescriptorType::eStorageBuffer;
    writeSets[8].pBufferInfo = &lightBufInfo;

    writeSets[9].dstSet = m_DescriptorSet;
    writeSets[9].dstBinding = 9;
    writeSets[9].descriptorCount = 1;
    writeSets[9].descriptorType = vk::DescriptorType::eStorageBuffer;
    writeSets[9].pBufferInfo = &editBufInfo;

    writeSets[10].dstSet = m_DescriptorSet;
    writeSets[10].dstBinding = 10;
    writeSets[10].descriptorCount = 1;
    writeSets[10].descriptorType = vk::DescriptorType::eStorageBuffer;
    writeSets[10].pBufferInfo = &gridBufInfo;

    m_Device.updateDescriptorSets(static_cast<uint32_t>(writeSets.size()), writeSets.data(), 0, nullptr);
}

void DeferredLightingPass::Record(vk::CommandBuffer cmd, const RenderFrameSettings& settings) {
    if (!settings.camera) {
        return;
    }

    const auto& camera = *settings.camera;
    uint32_t activeCount = (settings.activePrimitiveCount > 0) ? settings.activePrimitiveCount : m_Inputs.scene.primitiveCount;
    glm::vec4 gridParams = (settings.gridParams != glm::vec4(0.0f)) ? settings.gridParams : m_Inputs.scene.gridParams;

    DeferredLightingPushConstants defPush{};
    defPush.cameraRight = glm::vec4(camera.right, camera.projection[1][1] * 0.5f);
    defPush.cameraUp = glm::vec4(camera.up, settings.useGrid ? 1.0f : 0.0f);
    defPush.camPos = glm::vec4(camera.position, static_cast<float>(m_Inputs.prefilteredMipLevels));
    defPush.camDir = glm::vec4(camera.forward, 1.0f); // xyz: dir, w: exposure = 1.0
    defPush.screenRes = glm::vec4(
        static_cast<float>(settings.width),
        static_cast<float>(settings.height),
        1.0f, // z: iblIntensity = 1.0
        static_cast<float>(activeCount)
    );
    defPush.rayParams = glm::vec4(
        settings.jitter.x,
        settings.jitter.y,
        settings.quality.shadowMaxDistance,
        settings.quality.surfaceBias
    );
    defPush.shadowAOParams = glm::vec4(
        static_cast<float>(settings.quality.shadowMaxSteps),
        settings.quality.shadowK,
        static_cast<float>(settings.quality.aoSamples),
        settings.quality.aoRadius
    );
    float shadowFlag = (settings.optimizedShadows && settings.quality.enableShadows) ? 1.0f : 0.0f;
    defPush.qualityParams = glm::vec4(shadowFlag, gridParams.x, gridParams.y, gridParams.w);

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
        sizeof(DeferredLightingPushConstants),
        &defPush
    );

    uint32_t groupX = (settings.width + 7u) / 8u;
    uint32_t groupY = (settings.height + 7u) / 8u;
    cmd.dispatch(groupX, groupY, 1);
}

} // namespace Astral
