#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include "Astral/Renderer/Passes/GBufferPass.hpp"
#include "PassUtils.hpp"
#include "Astral/Renderer/ShaderInterop.hpp"
#include <array>

namespace Astral {

std::vector<vk::DescriptorSetLayoutBinding> GBufferPass::GetBindingDescriptions() {
    std::vector<vk::DescriptorSetLayoutBinding> bindings(10);
    for (uint32_t i = 0; i < 5; ++i) {
        bindings[i] = {i, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute};
    }
    for (uint32_t i = 5; i < 8; ++i) {
        bindings[i] = {i, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute};
    }
    bindings[8] = {8, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eCompute};
    bindings[9] = {9, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute};
    return bindings;
}

GBufferPass::GBufferPass(vk::Device device, const std::string& spvPath)
    : m_Device(device) {
    ComputeProgramDesc desc;
    desc.shaderPath = spvPath;
    desc.pushConstantBytes = sizeof(SDFPushConstants);
    desc.bindings = GetBindingDescriptions();

    m_Program = std::make_unique<ComputeProgram>(m_Device, desc);
    m_DescriptorPool = CreateDescriptorPoolFromBindings(m_Device, desc.bindings, 1);

    auto layout = m_Program->GetSetLayout();
    vk::DescriptorSetAllocateInfo allocInfo(m_DescriptorPool.get(), 1, &layout);
    auto sets = m_Device.allocateDescriptorSets(allocInfo);
    m_DescriptorSet = sets[0];
}

void GBufferPass::BindResources(const GBufferInputs& inputs) {
    m_Inputs = inputs;

    vk::DescriptorImageInfo albedoInfo(nullptr, inputs.targets.albedo.view, vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo normalInfo(nullptr, inputs.targets.normal.view, vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo materialInfo(nullptr, inputs.targets.material.view, vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo depthInfo(nullptr, inputs.targets.depth.view, vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo motionInfo(nullptr, inputs.targets.motion.view, vk::ImageLayout::eGeneral);

    auto editBufInfo = inputs.scene.primitives;
    auto gridBufInfo = inputs.scene.grid;
    auto selBufInfo = inputs.selection;
    auto camBufInfo = inputs.camera;
    auto histBufInfo = inputs.scene.previousTransforms;

    std::array<vk::WriteDescriptorSet, 10> writeSets{};
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
    writeSets[5].descriptorType = vk::DescriptorType::eStorageBuffer;
    writeSets[5].pBufferInfo = &editBufInfo;

    writeSets[6].dstSet = m_DescriptorSet;
    writeSets[6].dstBinding = 6;
    writeSets[6].descriptorCount = 1;
    writeSets[6].descriptorType = vk::DescriptorType::eStorageBuffer;
    writeSets[6].pBufferInfo = &gridBufInfo;

    writeSets[7].dstSet = m_DescriptorSet;
    writeSets[7].dstBinding = 7;
    writeSets[7].descriptorCount = 1;
    writeSets[7].descriptorType = vk::DescriptorType::eStorageBuffer;
    writeSets[7].pBufferInfo = &selBufInfo;

    writeSets[8].dstSet = m_DescriptorSet;
    writeSets[8].dstBinding = 8;
    writeSets[8].descriptorCount = 1;
    writeSets[8].descriptorType = vk::DescriptorType::eUniformBuffer;
    writeSets[8].pBufferInfo = &camBufInfo;

    writeSets[9].dstSet = m_DescriptorSet;
    writeSets[9].dstBinding = 9;
    writeSets[9].descriptorCount = 1;
    writeSets[9].descriptorType = vk::DescriptorType::eStorageBuffer;
    writeSets[9].pBufferInfo = &histBufInfo;

    m_Device.updateDescriptorSets(static_cast<uint32_t>(writeSets.size()), writeSets.data(), 0, nullptr);
}

void GBufferPass::Record(vk::CommandBuffer cmd, const RenderFrameSettings& settings) {
    if (!settings.camera) {
        return;
    }

    const auto& camera = *settings.camera;
    uint32_t activeCount = (settings.activePrimitiveCount > 0) ? settings.activePrimitiveCount : m_Inputs.scene.primitiveCount;
    glm::vec4 gridParams = (settings.gridParams != glm::vec4(0.0f)) ? settings.gridParams : m_Inputs.scene.gridParams;

    SDFPushConstants pushConstants{};
    pushConstants.camPos = glm::vec4(camera.position, camera.nearClip);
    pushConstants.camDir = glm::vec4(camera.forward, static_cast<float>(settings.normalMode));
    pushConstants.cameraRight = glm::vec4(camera.right, camera.projection[1][1] * 0.5f);
    pushConstants.cameraUp = glm::vec4(camera.up, camera.farClip);

    pushConstants.screenRes = glm::vec4(
        static_cast<float>(settings.width),
        static_cast<float>(settings.height),
        static_cast<float>(activeCount),
        settings.useGrid ? 1.0f : 0.0f
    );
    pushConstants.gridParams = gridParams;
    pushConstants.gridParams.z = static_cast<float>(settings.quality.primaryRayMaxSteps);

    pushConstants.taaParams = glm::vec4(
        settings.jitter.x,
        settings.jitter.y,
        settings.taaEnabled ? 1.0f : 0.0f,
        settings.quality.taaBlendAlpha
    );

    pushConstants.mouseParams = glm::vec4(
        static_cast<float>(settings.pickMouseX),
        static_cast<float>(settings.pickMouseY),
        settings.pickFlag,
        static_cast<float>(settings.selectedHitIndex)
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
        sizeof(SDFPushConstants),
        &pushConstants
    );

    uint32_t groupX = (settings.width + 7u) / 8u;
    uint32_t groupY = (settings.height + 7u) / 8u;
    cmd.dispatch(groupX, groupY, 1);
}

} // namespace Astral
