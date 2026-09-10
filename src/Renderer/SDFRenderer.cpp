#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include "Astral/Renderer/SDFRenderer.hpp"
#include "Astral/Renderer/VulkanContext.hpp"
#include "Astral/Renderer/ComputePipeline.hpp"
#include "Astral/Renderer/Buffer.hpp"
#include "Astral/Renderer/BrickGrid.hpp"

#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <array>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>

namespace Astral {



SDFRenderer::SDFRenderer(VulkanContext& context, const std::string& spvPath, int width, int height,
                         bool persistentMap, const std::string& taaSpvPath,
                         const std::string& gbufferSpvPath, const std::string& debugCompositeSpvPath,
                         const std::string& deferredLightingSpvPath)
    : m_Context(context),
      m_Device(context.GetDevice()),
      m_PhysicalDevice(context.GetPhysicalDevice()),
      m_Width(width),
      m_Height(height),
      m_GBufferSpvPath(gbufferSpvPath),
      m_DebugCompositeSpvPath(debugCompositeSpvPath),
      m_DeferredLightingSpvPath(deferredLightingSpvPath),
      m_TaaSpvPath(taaSpvPath) {

    std::filesystem::path p(spvPath);

    // Shader yollarini ComputeProgram::ResolveShaderPath ile tespit et
    m_TaaSpvPath = ComputeProgram::ResolveShaderPath(m_TaaSpvPath.empty() ? "TAAResolve.spv" : m_TaaSpvPath, p).string();
    m_GBufferSpvPath = ComputeProgram::ResolveShaderPath(m_GBufferSpvPath.empty() ? "SDFGBuffer.spv" : m_GBufferSpvPath, p).string();
    m_DebugCompositeSpvPath = ComputeProgram::ResolveShaderPath(m_DebugCompositeSpvPath.empty() ? "SDFDebugComposite.spv" : m_DebugCompositeSpvPath, p).string();
    m_DeferredLightingSpvPath = ComputeProgram::ResolveShaderPath(m_DeferredLightingSpvPath.empty() ? "DeferredLighting.spv" : m_DeferredLightingSpvPath, p).string();

    (void)spvPath;
    m_RenderTargets = std::make_unique<RenderTargets>(m_Context, static_cast<uint32_t>(m_Width), static_cast<uint32_t>(m_Height));
    m_SceneGpuData = std::make_unique<SceneGpuData>(m_Context, persistentMap);

    // PR-9: Selection Buffer (32 bayt, persistent mapped, host-visible & coherent)
    m_SelectionBuffer = std::make_unique<Buffer>(
        m_Context.GetAllocator(),
        m_Device,
        m_PhysicalDevice,
        sizeof(SelectionDataGPU),
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        true
    );
    if (m_SelectionBuffer->GetMappedData()) {
        SelectionDataGPU initData{};
        initData.hitIndex = -1;
        std::memcpy(m_SelectionBuffer->GetMappedData(), &initData, sizeof(SelectionDataGPU));
    }

    // Faz 2: IBL kurulumu (Isik tamponu SceneGpuData tarafindan yonetilir)
    m_IBLManager = std::make_unique<IBLManager>(m_Context);

    m_TemporalHistory = std::make_unique<SDFTemporalHistory>();

    // 1. ImGui Viewport Sampler ve TAA Linear Clamp Sampler
    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;
    samplerInfo.borderColor = vk::BorderColor::eFloatOpaqueBlack;
    m_ViewportSampler = m_Device.createSamplerUnique(samplerInfo);
    m_LinearClampSampler = m_Device.createSamplerUnique(samplerInfo);

    CreateTAAPipeline();
    CreateGBufferPipeline();
    CreateDebugCompositePipeline();
    CreateDeferredLightingPipeline();
    CreateDescriptorPoolAndSets();

    std::cout << "[Astral::SDFRenderer] SDF Renderer baslatildi (" << m_Width << "x" << m_Height 
              << ", EditBuffer: " << (MAX_EDITS * sizeof(SDFPrimitiveRecord)) / 1024 << " KB"
              << ", Two-Level BrickGrid, Deferred PBR & IBL Lighting Aktif).\n";
}

SDFRenderer::~SDFRenderer() {
    m_Device.waitIdle();
    m_ViewportSampler.reset();
    m_LinearClampSampler.reset();

    m_DescriptorPool.reset();
    m_DeferredLightingProgram.reset();
    m_DebugCompositeProgram.reset();
    m_GBufferProgram.reset();
    m_TaaProgram.reset();

    m_IBLManager.reset();
    m_SelectionBuffer.reset();
    m_SceneGpuData.reset();
    m_RenderTargets.reset();
}

void SDFRenderer::SetLights(const std::vector<LightGPU>& lights) {
    if (m_SceneGpuData) {
        m_SceneGpuData->SetLights(lights);
    }
}

void SDFRenderer::CreateTAAPipeline() {
    ComputeProgramDesc desc;
    desc.shaderPath = m_TaaSpvPath;
    desc.pushConstantBytes = sizeof(TAAPushConstants);
    desc.bindings.resize(11);

    desc.bindings[0] = {0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute};
    desc.bindings[1] = {1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eCompute};
    desc.bindings[2] = {2, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute};
    desc.bindings[3] = {3, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute};
    for (uint32_t i = 4; i < 11; ++i) {
        desc.bindings[i] = {i, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute};
    }

    m_TaaProgram = std::make_unique<ComputeProgram>(m_Device, desc);
}

void SDFRenderer::CreateGBufferPipeline() {
    ComputeProgramDesc desc;
    desc.shaderPath = m_GBufferSpvPath;
    desc.pushConstantBytes = sizeof(SDFPushConstants);
    desc.bindings.resize(10);
    for (uint32_t i = 0; i < 5; ++i) {
        desc.bindings[i] = {i, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute};
    }
    for (uint32_t i = 5; i < 8; ++i) {
        desc.bindings[i] = {i, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute};
    }
    desc.bindings[8] = {8, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eCompute};
    desc.bindings[9] = {9, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute};

    m_GBufferProgram = std::make_unique<ComputeProgram>(m_Device, desc);
}

void SDFRenderer::CreateDebugCompositePipeline() {
    ComputeProgramDesc desc;
    desc.shaderPath = m_DebugCompositeSpvPath;
    desc.pushConstantBytes = sizeof(DebugCompositePushConstants);
    desc.bindings.resize(6);
    for (uint32_t i = 0; i < 6; ++i) {
        desc.bindings[i] = {i, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute};
    }

    m_DebugCompositeProgram = std::make_unique<ComputeProgram>(m_Device, desc);
}

void SDFRenderer::CreateDeferredLightingPipeline() {
    ComputeProgramDesc desc;
    desc.shaderPath = m_DeferredLightingSpvPath;
    desc.pushConstantBytes = sizeof(DeferredLightingPushConstants);
    desc.bindings.resize(11);

    for (uint32_t i = 0; i < 5; ++i) {
        desc.bindings[i] = {i, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute};
    }
    for (uint32_t i = 5; i < 8; ++i) {
        desc.bindings[i] = {i, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eCompute};
    }
    for (uint32_t i = 8; i < 11; ++i) {
        desc.bindings[i] = {i, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute};
    }

    m_DeferredLightingProgram = std::make_unique<ComputeProgram>(m_Device, desc);
}

void SDFRenderer::CreateDescriptorPoolAndSets() {
    std::array<vk::DescriptorPoolSize, 4> poolSizes{};
    poolSizes[0].type = vk::DescriptorType::eStorageImage;
    poolSizes[0].descriptorCount = 64;
    poolSizes[1].type = vk::DescriptorType::eStorageBuffer;
    poolSizes[1].descriptorCount = 48;
    poolSizes[2].type = vk::DescriptorType::eUniformBuffer;
    poolSizes[2].descriptorCount = 8;
    poolSizes[3].type = vk::DescriptorType::eCombinedImageSampler;
    poolSizes[3].descriptorCount = 16;

    vk::DescriptorPoolCreateInfo poolInfo(
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        16,
        static_cast<uint32_t>(poolSizes.size()),
        poolSizes.data()
    );
    m_DescriptorPool = m_Device.createDescriptorPoolUnique(poolInfo);

    // 1. TAA Descriptor Sets (Ping-Pong 0 & 1)
    auto taaLayout = m_TaaProgram->GetSetLayout();
    for (uint32_t i = 0; i < 2; ++i) {
        vk::DescriptorSetAllocateInfo taaAllocInfo(m_DescriptorPool.get(), 1, &taaLayout);
        auto taaSets = m_Device.allocateDescriptorSets(taaAllocInfo);
        m_TaaDescriptorSet[i] = taaSets[0];
    }

    // 2. G-Buffer Descriptor Set
    auto gbufLayout = m_GBufferProgram->GetSetLayout();
    vk::DescriptorSetAllocateInfo gbufAllocInfo(m_DescriptorPool.get(), 1, &gbufLayout);
    auto gbufSets = m_Device.allocateDescriptorSets(gbufAllocInfo);
    m_GBufferDescriptorSet = gbufSets[0];

    // 3. DebugComposite Descriptor Set
    auto debugLayout = m_DebugCompositeProgram->GetSetLayout();
    vk::DescriptorSetAllocateInfo debugAllocInfo(m_DescriptorPool.get(), 1, &debugLayout);
    auto debugSets = m_Device.allocateDescriptorSets(debugAllocInfo);
    m_DebugCompositeDescriptorSet = debugSets[0];

    // 4. DeferredLighting Descriptor Set
    auto defLayout = m_DeferredLightingProgram->GetSetLayout();
    vk::DescriptorSetAllocateInfo defAllocInfo(m_DescriptorPool.get(), 1, &defLayout);
    auto defSets = m_Device.allocateDescriptorSets(defAllocInfo);
    m_DeferredLightingDescriptorSet = defSets[0];

    UpdateTAADescriptorSets();
    UpdateGBufferDescriptorSets();
    UpdateDebugCompositeDescriptorSets();
    UpdateDeferredLightingDescriptorSets();
}

void SDFRenderer::UpdateTAADescriptorSets() {
    if (!m_RenderTargets) return;
    auto gbuf = m_RenderTargets->GetGBuffer();
    auto rawColor = m_RenderTargets->GetRawColor();
    auto storage = m_RenderTargets->GetOutput();

    // TAA Setleri Guncelleme (Ping-Pong 0 ve 1)
    // TAA Set 0: curr=RawColor, hist=History[0] (Sampler), out=Storage, outHist=History[1], motion=GBufMotion
    // TAA Set 1: curr=RawColor, hist=History[1] (Sampler), out=Storage, outHist=History[0], motion=GBufMotion
    for (uint32_t i = 0; i < 2; ++i) {
        uint32_t readIdx = i;
        uint32_t writeIdx = 1 - i;

        vk::DescriptorImageInfo taaCurrInfo(nullptr, rawColor.view, vk::ImageLayout::eGeneral);
        vk::DescriptorImageInfo taaHistInfo(m_LinearClampSampler.get(), m_RenderTargets->GetHistoryColor(readIdx).view, vk::ImageLayout::eGeneral);
        vk::DescriptorImageInfo taaOutInfo(nullptr, storage.view, vk::ImageLayout::eGeneral);
        vk::DescriptorImageInfo taaOutHistInfo(nullptr, m_RenderTargets->GetHistoryColor(writeIdx).view, vk::ImageLayout::eGeneral);
        vk::DescriptorImageInfo taaMotionInfo(nullptr, gbuf.motion.view, vk::ImageLayout::eGeneral);

        vk::DescriptorImageInfo depthInfo(nullptr, gbuf.depth.view, vk::ImageLayout::eGeneral);
        vk::DescriptorImageInfo normalInfo(nullptr, gbuf.normal.view, vk::ImageLayout::eGeneral);
        vk::DescriptorImageInfo materialInfo(nullptr, gbuf.material.view, vk::ImageLayout::eGeneral);
        vk::DescriptorImageInfo albedoInfo(nullptr, gbuf.albedo.view, vk::ImageLayout::eGeneral);
        vk::DescriptorImageInfo histExtraReadInfo(nullptr, m_RenderTargets->GetHistoryExtra(readIdx).view, vk::ImageLayout::eGeneral);
        vk::DescriptorImageInfo histExtraWriteInfo(nullptr, m_RenderTargets->GetHistoryExtra(writeIdx).view, vk::ImageLayout::eGeneral);

        std::array<vk::WriteDescriptorSet, 11> taaWriteSets{};
        taaWriteSets[0].dstSet = m_TaaDescriptorSet[i];
        taaWriteSets[0].dstBinding = 0;
        taaWriteSets[0].dstArrayElement = 0;
        taaWriteSets[0].descriptorCount = 1;
        taaWriteSets[0].descriptorType = vk::DescriptorType::eStorageImage;
        taaWriteSets[0].pImageInfo = &taaCurrInfo;

        taaWriteSets[1].dstSet = m_TaaDescriptorSet[i];
        taaWriteSets[1].dstBinding = 1;
        taaWriteSets[1].dstArrayElement = 0;
        taaWriteSets[1].descriptorCount = 1;
        taaWriteSets[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        taaWriteSets[1].pImageInfo = &taaHistInfo;

        taaWriteSets[2].dstSet = m_TaaDescriptorSet[i];
        taaWriteSets[2].dstBinding = 2;
        taaWriteSets[2].dstArrayElement = 0;
        taaWriteSets[2].descriptorCount = 1;
        taaWriteSets[2].descriptorType = vk::DescriptorType::eStorageImage;
        taaWriteSets[2].pImageInfo = &taaOutInfo;

        taaWriteSets[3].dstSet = m_TaaDescriptorSet[i];
        taaWriteSets[3].dstBinding = 3;
        taaWriteSets[3].dstArrayElement = 0;
        taaWriteSets[3].descriptorCount = 1;
        taaWriteSets[3].descriptorType = vk::DescriptorType::eStorageImage;
        taaWriteSets[3].pImageInfo = &taaOutHistInfo;

        taaWriteSets[4].dstSet = m_TaaDescriptorSet[i];
        taaWriteSets[4].dstBinding = 4;
        taaWriteSets[4].dstArrayElement = 0;
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

void SDFRenderer::UpdateGBufferDescriptorSets() {
    if (!m_RenderTargets || !m_SceneGpuData) return;
    auto gbuf = m_RenderTargets->GetGBuffer();
    auto sceneViews = m_SceneGpuData->GetViews();

    vk::DescriptorImageInfo albedoInfo(nullptr, gbuf.albedo.view, vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo normalInfo(nullptr, gbuf.normal.view, vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo materialInfo(nullptr, gbuf.material.view, vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo depthInfo(nullptr, gbuf.depth.view, vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo motionInfo(nullptr, gbuf.motion.view, vk::ImageLayout::eGeneral);

    auto editBufInfo = sceneViews.primitives;
    auto gridBufInfo = sceneViews.grid;
    auto selBufInfo = m_SelectionBuffer->GetDescriptorInfo();
    auto camBufInfo = m_SceneGpuData->GetCameraDescriptor();
    auto histBufInfo = sceneViews.previousTransforms;

    std::array<vk::WriteDescriptorSet, 10> writeSets{};
    writeSets[0].dstSet = m_GBufferDescriptorSet;
    writeSets[0].dstBinding = 0;
    writeSets[0].descriptorCount = 1;
    writeSets[0].descriptorType = vk::DescriptorType::eStorageImage;
    writeSets[0].pImageInfo = &albedoInfo;

    writeSets[1].dstSet = m_GBufferDescriptorSet;
    writeSets[1].dstBinding = 1;
    writeSets[1].descriptorCount = 1;
    writeSets[1].descriptorType = vk::DescriptorType::eStorageImage;
    writeSets[1].pImageInfo = &normalInfo;

    writeSets[2].dstSet = m_GBufferDescriptorSet;
    writeSets[2].dstBinding = 2;
    writeSets[2].descriptorCount = 1;
    writeSets[2].descriptorType = vk::DescriptorType::eStorageImage;
    writeSets[2].pImageInfo = &materialInfo;

    writeSets[3].dstSet = m_GBufferDescriptorSet;
    writeSets[3].dstBinding = 3;
    writeSets[3].descriptorCount = 1;
    writeSets[3].descriptorType = vk::DescriptorType::eStorageImage;
    writeSets[3].pImageInfo = &depthInfo;

    writeSets[4].dstSet = m_GBufferDescriptorSet;
    writeSets[4].dstBinding = 4;
    writeSets[4].descriptorCount = 1;
    writeSets[4].descriptorType = vk::DescriptorType::eStorageImage;
    writeSets[4].pImageInfo = &motionInfo;

    writeSets[5].dstSet = m_GBufferDescriptorSet;
    writeSets[5].dstBinding = 5;
    writeSets[5].descriptorCount = 1;
    writeSets[5].descriptorType = vk::DescriptorType::eStorageBuffer;
    writeSets[5].pBufferInfo = &editBufInfo;

    writeSets[6].dstSet = m_GBufferDescriptorSet;
    writeSets[6].dstBinding = 6;
    writeSets[6].descriptorCount = 1;
    writeSets[6].descriptorType = vk::DescriptorType::eStorageBuffer;
    writeSets[6].pBufferInfo = &gridBufInfo;

    writeSets[7].dstSet = m_GBufferDescriptorSet;
    writeSets[7].dstBinding = 7;
    writeSets[7].descriptorCount = 1;
    writeSets[7].descriptorType = vk::DescriptorType::eStorageBuffer;
    writeSets[7].pBufferInfo = &selBufInfo;

    writeSets[8].dstSet = m_GBufferDescriptorSet;
    writeSets[8].dstBinding = 8;
    writeSets[8].descriptorCount = 1;
    writeSets[8].descriptorType = vk::DescriptorType::eUniformBuffer;
    writeSets[8].pBufferInfo = &camBufInfo;

    writeSets[9].dstSet = m_GBufferDescriptorSet;
    writeSets[9].dstBinding = 9;
    writeSets[9].descriptorCount = 1;
    writeSets[9].descriptorType = vk::DescriptorType::eStorageBuffer;
    writeSets[9].pBufferInfo = &histBufInfo;

    m_Device.updateDescriptorSets(static_cast<uint32_t>(writeSets.size()), writeSets.data(), 0, nullptr);
}

void SDFRenderer::UpdateDebugCompositeDescriptorSets() {
    if (!m_RenderTargets) return;
    auto gbuf = m_RenderTargets->GetGBuffer();

    vk::DescriptorImageInfo albedoInfo(nullptr, gbuf.albedo.view, vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo normalInfo(nullptr, gbuf.normal.view, vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo materialInfo(nullptr, gbuf.material.view, vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo depthInfo(nullptr, gbuf.depth.view, vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo motionInfo(nullptr, gbuf.motion.view, vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo outInfo(nullptr, m_RenderTargets->GetRawColor().view, vk::ImageLayout::eGeneral);

    std::array<vk::WriteDescriptorSet, 6> writeSets{};
    writeSets[0].dstSet = m_DebugCompositeDescriptorSet;
    writeSets[0].dstBinding = 0;
    writeSets[0].descriptorCount = 1;
    writeSets[0].descriptorType = vk::DescriptorType::eStorageImage;
    writeSets[0].pImageInfo = &albedoInfo;

    writeSets[1].dstSet = m_DebugCompositeDescriptorSet;
    writeSets[1].dstBinding = 1;
    writeSets[1].descriptorCount = 1;
    writeSets[1].descriptorType = vk::DescriptorType::eStorageImage;
    writeSets[1].pImageInfo = &normalInfo;

    writeSets[2].dstSet = m_DebugCompositeDescriptorSet;
    writeSets[2].dstBinding = 2;
    writeSets[2].descriptorCount = 1;
    writeSets[2].descriptorType = vk::DescriptorType::eStorageImage;
    writeSets[2].pImageInfo = &materialInfo;

    writeSets[3].dstSet = m_DebugCompositeDescriptorSet;
    writeSets[3].dstBinding = 3;
    writeSets[3].descriptorCount = 1;
    writeSets[3].descriptorType = vk::DescriptorType::eStorageImage;
    writeSets[3].pImageInfo = &depthInfo;

    writeSets[4].dstSet = m_DebugCompositeDescriptorSet;
    writeSets[4].dstBinding = 4;
    writeSets[4].descriptorCount = 1;
    writeSets[4].descriptorType = vk::DescriptorType::eStorageImage;
    writeSets[4].pImageInfo = &motionInfo;

    writeSets[5].dstSet = m_DebugCompositeDescriptorSet;
    writeSets[5].dstBinding = 5;
    writeSets[5].descriptorCount = 1;
    writeSets[5].descriptorType = vk::DescriptorType::eStorageImage;
    writeSets[5].pImageInfo = &outInfo;

    m_Device.updateDescriptorSets(static_cast<uint32_t>(writeSets.size()), writeSets.data(), 0, nullptr);
}

void SDFRenderer::UpdateDeferredLightingDescriptorSets() {
    if (!m_DeferredLightingDescriptorSet || !m_RenderTargets) return;
    auto gbuf = m_RenderTargets->GetGBuffer();

    vk::DescriptorImageInfo albedoInfo(nullptr, gbuf.albedo.view, vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo normalInfo(nullptr, gbuf.normal.view, vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo materialInfo(nullptr, gbuf.material.view, vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo depthInfo(nullptr, gbuf.depth.view, vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo outColorInfo(nullptr, m_RenderTargets->GetRawColor().view, vk::ImageLayout::eGeneral);

    vk::DescriptorImageInfo irradianceInfo(
        m_IBLManager->GetCubemapSampler(),
        m_IBLManager->GetIrradianceView(),
        vk::ImageLayout::eShaderReadOnlyOptimal
    );
    vk::DescriptorImageInfo prefilteredInfo(
        m_IBLManager->GetCubemapSampler(),
        m_IBLManager->GetPrefilteredView(),
        vk::ImageLayout::eShaderReadOnlyOptimal
    );
    vk::DescriptorImageInfo brdfLutInfo(
        m_IBLManager->GetBRDFLutSampler(),
        m_IBLManager->GetBRDFLutView(),
        vk::ImageLayout::eShaderReadOnlyOptimal
    );

    if (!m_RenderTargets || !m_SceneGpuData) return;
    auto sceneViews = m_SceneGpuData->GetViews();
    auto lightBufInfo = sceneViews.lights;
    auto editBufInfo = sceneViews.primitives;
    auto gridBufInfo = sceneViews.grid;

    std::array<vk::WriteDescriptorSet, 11> writeSets{};

    writeSets[0].dstSet = m_DeferredLightingDescriptorSet;
    writeSets[0].dstBinding = 0;
    writeSets[0].descriptorCount = 1;
    writeSets[0].descriptorType = vk::DescriptorType::eStorageImage;
    writeSets[0].pImageInfo = &albedoInfo;

    writeSets[1].dstSet = m_DeferredLightingDescriptorSet;
    writeSets[1].dstBinding = 1;
    writeSets[1].descriptorCount = 1;
    writeSets[1].descriptorType = vk::DescriptorType::eStorageImage;
    writeSets[1].pImageInfo = &normalInfo;

    writeSets[2].dstSet = m_DeferredLightingDescriptorSet;
    writeSets[2].dstBinding = 2;
    writeSets[2].descriptorCount = 1;
    writeSets[2].descriptorType = vk::DescriptorType::eStorageImage;
    writeSets[2].pImageInfo = &materialInfo;

    writeSets[3].dstSet = m_DeferredLightingDescriptorSet;
    writeSets[3].dstBinding = 3;
    writeSets[3].descriptorCount = 1;
    writeSets[3].descriptorType = vk::DescriptorType::eStorageImage;
    writeSets[3].pImageInfo = &depthInfo;

    writeSets[4].dstSet = m_DeferredLightingDescriptorSet;
    writeSets[4].dstBinding = 4;
    writeSets[4].descriptorCount = 1;
    writeSets[4].descriptorType = vk::DescriptorType::eStorageImage;
    writeSets[4].pImageInfo = &outColorInfo;

    writeSets[5].dstSet = m_DeferredLightingDescriptorSet;
    writeSets[5].dstBinding = 5;
    writeSets[5].descriptorCount = 1;
    writeSets[5].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writeSets[5].pImageInfo = &irradianceInfo;

    writeSets[6].dstSet = m_DeferredLightingDescriptorSet;
    writeSets[6].dstBinding = 6;
    writeSets[6].descriptorCount = 1;
    writeSets[6].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writeSets[6].pImageInfo = &prefilteredInfo;

    writeSets[7].dstSet = m_DeferredLightingDescriptorSet;
    writeSets[7].dstBinding = 7;
    writeSets[7].descriptorCount = 1;
    writeSets[7].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writeSets[7].pImageInfo = &brdfLutInfo;

    writeSets[8].dstSet = m_DeferredLightingDescriptorSet;
    writeSets[8].dstBinding = 8;
    writeSets[8].descriptorCount = 1;
    writeSets[8].descriptorType = vk::DescriptorType::eStorageBuffer;
    writeSets[8].pBufferInfo = &lightBufInfo;

    writeSets[9].dstSet = m_DeferredLightingDescriptorSet;
    writeSets[9].dstBinding = 9;
    writeSets[9].descriptorCount = 1;
    writeSets[9].descriptorType = vk::DescriptorType::eStorageBuffer;
    writeSets[9].pBufferInfo = &editBufInfo;

    writeSets[10].dstSet = m_DeferredLightingDescriptorSet;
    writeSets[10].dstBinding = 10;
    writeSets[10].descriptorCount = 1;
    writeSets[10].descriptorType = vk::DescriptorType::eStorageBuffer;
    writeSets[10].pBufferInfo = &gridBufInfo;

    m_Device.updateDescriptorSets(static_cast<uint32_t>(writeSets.size()), writeSets.data(), 0, nullptr);
}

void SDFRenderer::UpdateEdits(const std::vector<LegacySDFEdit>& edits, bool useLegacyMapUnmap) {
    size_t activeCount = std::min(edits.size(), MAX_EDITS);
    std::vector<SDFPrimitiveRecord> records;
    records.reserve(activeCount);
    for (size_t i = 0; i < activeCount; ++i) {
        records.push_back(edits[i].ToPrimitiveRecord(static_cast<uint32_t>(i), static_cast<uint32_t>(i + 1)));
    }
    UpdateEdits(std::span<const SDFPrimitiveRecord>(records.data(), records.size()), useLegacyMapUnmap);
}

void SDFRenderer::UpdateEdits(std::span<const SDFPrimitiveRecord> records, bool useLegacyMapUnmap) {
    if (m_SceneGpuData) {
        m_SceneGpuData->Upload(records, m_CurrentChangeSet, useLegacyMapUnmap);
    }
}

void SDFRenderer::UpdateEdits(const SDFSceneSnapshot& snapshot, bool useLegacyMapUnmap) {
    if (m_PreviousSnapshot.GetRecordCount() > 0) {
        glm::mat4 prevVP = m_CameraMatricesInitialized ? m_PrevViewProj :
                           (m_RenderCamera ? m_RenderCamera->projection * m_RenderCamera->view : glm::mat4(1.0f));
        glm::mat4 currVP = m_CameraMatricesInitialized ? m_CurrViewProj :
                           (m_RenderCamera ? m_RenderCamera->projection * m_RenderCamera->view : prevVP);
        m_CurrentChangeSet = SDFChangeSet::Compare(m_PreviousSnapshot, snapshot, prevVP, currVP);
    }
    m_PreviousSnapshot = snapshot;
    if (m_RenderCamera) {
        m_PrevCameraViewProj = m_CameraMatricesInitialized ? m_CurrViewProj : (m_RenderCamera->projection * m_RenderCamera->view);
        m_HasPrevCameraViewProj = true;
    }
    UpdateEdits(std::span<const SDFPrimitiveRecord>(snapshot.GetRecords().data(), snapshot.GetRecordCount()), useLegacyMapUnmap);
}

void SDFRenderer::Resize(int width, int height) {
    if (width <= 0 || height <= 0 || (width == m_Width && height == m_Height)) return;

    m_Device.waitIdle();

    // Aday RenderTargets olustur (hata olursa mevcut durum bozulmaz)
    auto candidateTargets = std::make_unique<RenderTargets>(m_Context, static_cast<uint32_t>(width), static_cast<uint32_t>(height));

    m_Width = width;
    m_Height = height;
    m_RenderTargets = std::move(candidateTargets);

    UpdateTAADescriptorSets();
    UpdateGBufferDescriptorSets();
    UpdateDebugCompositeDescriptorSets();
    UpdateDeferredLightingDescriptorSets();
    m_HistoryInitialized = false;
    m_HasPrevCameraViewProj = false;
    m_PreviousSnapshot = SDFSceneSnapshot{};
    if (m_TemporalHistory) m_TemporalHistory->Reset();
    m_CurrentChangeSet = SDFChangeSet{};
}

void SDFRenderer::Render(vk::CommandBuffer cmd, float time, uint32_t normalMode, int width, int height,
                         bool useGrid, bool optShadow, bool enableTAA, uint32_t frameIndex,
                         const QualitySettings& qualitySettings) {
    (void)time;
    (void)width;
    (void)height;

    RenderFrameSettings settings;
    settings.camera = m_RenderCamera;
    settings.jitter = m_CurrJitter;
    settings.quality = qualitySettings;
    settings.frameId = frameIndex;
    settings.width = static_cast<uint32_t>(m_Width);
    settings.height = static_cast<uint32_t>(m_Height);
    settings.normalMode = normalMode;
    settings.debugMode = m_DebugMode;
    settings.selectedHitIndex = m_SelectedHitIndex;
    settings.useGrid = useGrid;
    settings.optimizedShadows = optShadow;
    settings.taaEnabled = enableTAA;
    settings.exposure = m_Exposure;

    Render(cmd, settings);
}

void SDFRenderer::Render(vk::CommandBuffer cmd, const RenderFrameSettings& settings) {
    uint32_t renderWidth = (settings.width > 0) ? settings.width : static_cast<uint32_t>(m_Width);
    uint32_t renderHeight = (settings.height > 0) ? settings.height : static_cast<uint32_t>(m_Height);

    if (settings.width > 0 && settings.height > 0 &&
        (settings.width != static_cast<uint32_t>(m_Width) || settings.height != static_cast<uint32_t>(m_Height))) {
        std::cerr << "[Astral::SDFRenderer] WARNING: RenderFrameSettings dimensions ("
                  << settings.width << "x" << settings.height
                  << ") do not match allocated render target dimensions ("
                  << m_Width << "x" << m_Height << "). Using allocated dimensions. Call Resize() before Render() to change resolution.\n";
        renderWidth = static_cast<uint32_t>(m_Width);
        renderHeight = static_cast<uint32_t>(m_Height);
    }

    uint32_t groupX = static_cast<uint32_t>(std::ceil(static_cast<float>(renderWidth) / 8.0f));
    uint32_t groupY = static_cast<uint32_t>(std::ceil(static_cast<float>(renderHeight) / 8.0f));

    // Determine effective camera: settings.camera takes precedence over m_RenderCamera
    std::optional<RenderCamera> effectiveCamera = settings.camera ? settings.camera : m_RenderCamera;

    if (settings.camera.has_value() && (!m_RenderCamera || settings.camera->entity != m_RenderCamera->entity ||
        settings.camera->sceneInstance != m_RenderCamera->sceneInstance ||
        settings.camera->projection != m_RenderCamera->projection ||
        settings.camera->view != m_RenderCamera->view ||
        settings.jitter != m_CurrJitter)) {
        SetCamera(settings.camera, settings.jitter);
        effectiveCamera = m_RenderCamera;
    }

    if (!effectiveCamera) {
        // No implicit camera: discard the previous frame and produce opaque black.
        vk::ImageMemoryBarrier barrier{};
        barrier.oldLayout = vk::ImageLayout::eUndefined;
        barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_RenderTargets->GetOutput().image;
        barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
            vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);
        cmd.clearColorImage(m_RenderTargets->GetOutput().image, vk::ImageLayout::eTransferDstOptimal,
            vk::ClearColorValue(std::array<float, 4>{0, 0, 0, 1}), barrier.subresourceRange);
        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout = vk::ImageLayout::eGeneral;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eTransferRead;
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, barrier);
        m_HistoryInitialized = false;
        if (m_TemporalHistory) m_TemporalHistory->Reset();
        m_PickingRequested = false;
        m_PickPendingRead = false;
        ClearSelectionResult();
        return;
    }

    if (m_PreviousTAAEnabled != settings.taaEnabled) m_HistoryInitialized = false;
    m_PreviousTAAEnabled = settings.taaEnabled;
    const auto& camera = *effectiveCamera;

    // Quality settings fallback rule: shadowMaxSteps > 0 selects settings.quality, otherwise fallback to m_QualitySettings
    const auto& qs = (settings.quality.shadowMaxSteps > 0) ? settings.quality : m_QualitySettings;

    // Debug mode precedence
    int effectiveDebugMode = settings.debugMode;
    if (settings.debugMode != m_DebugMode) {
        ResetTemporalHistory();
        m_DebugMode = settings.debugMode;
    }
    bool isDebugActive = (effectiveDebugMode != 0);

    // Selected hit index precedence
    int effectiveHitIndex = (settings.selectedHitIndex != -1) ? settings.selectedHitIndex : m_SelectedHitIndex;

    // Exposure precedence
    float effectiveExposure = (settings.exposure > 0.0f) ? settings.exposure : m_Exposure;

    // Jitter precedence
    glm::vec2 jitter = settings.taaEnabled ? ((settings.jitter != glm::vec2(0.0f)) ? settings.jitter : m_CurrJitter) : glm::vec2(0.0f);
    
    // =========================================================================
    // 1. G-Buffer Compute Pass (SDFGBuffer.glsl)
    // =========================================================================
    auto gbuf = m_RenderTargets->GetGBuffer();
    auto rawColor = m_RenderTargets->GetRawColor();
    auto outputImg = m_RenderTargets->GetOutput();

    auto makeGBufBarrier = [](VkImage img) {
        vk::ImageMemoryBarrier b{};
        b.oldLayout = vk::ImageLayout::eUndefined;
        b.newLayout = vk::ImageLayout::eGeneral;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = img;
        b.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
        b.srcAccessMask = {};
        b.dstAccessMask = vk::AccessFlagBits::eShaderWrite;
        return b;
    };

    std::array<vk::ImageMemoryBarrier, 5> gbufBarriers = {
        makeGBufBarrier(gbuf.albedo.image),
        makeGBufBarrier(gbuf.normal.image),
        makeGBufBarrier(gbuf.material.image),
        makeGBufBarrier(gbuf.depth.image),
        makeGBufBarrier(gbuf.motion.image)
    };

    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eComputeShader,
        {},
        0, nullptr,
        0, nullptr,
        static_cast<uint32_t>(gbufBarriers.size()), gbufBarriers.data()
    );

    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_GBufferProgram->GetPipeline());
    cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute,
        m_GBufferProgram->GetLayout(),
        0,
        1, &m_GBufferDescriptorSet,
        0, nullptr
    );

    SDFPushConstants pushConstants{};
    pushConstants.camPos = glm::vec4(camera.position, camera.nearClip);
    pushConstants.camDir = glm::vec4(camera.forward, static_cast<float>(settings.normalMode));
    pushConstants.cameraRight = glm::vec4(camera.right, camera.projection[1][1] * 0.5f);
    pushConstants.cameraUp = glm::vec4(camera.up, camera.farClip);
    uint32_t activeCount = m_SceneGpuData ? m_SceneGpuData->GetActiveEditCount() : 0;
    auto sceneViews = m_SceneGpuData ? m_SceneGpuData->GetViews() : SceneBufferViews{};

    pushConstants.screenRes = glm::vec4(
        static_cast<float>(renderWidth),
        static_cast<float>(renderHeight),
        static_cast<float>(activeCount),
        settings.useGrid ? 1.0f : 0.0f
    );
    pushConstants.gridParams = sceneViews.gridParams;
    pushConstants.gridParams.z = static_cast<float>(qs.primaryRayMaxSteps);

    pushConstants.taaParams = glm::vec4(jitter.x, jitter.y, settings.taaEnabled ? 1.0f : 0.0f, qs.taaBlendAlpha);

    pushConstants.mouseParams = glm::vec4(
        static_cast<float>(m_PickingMouseX),
        static_cast<float>(m_PickingMouseY),
        m_PickingRequested ? 1.0f : 0.0f,
        static_cast<float>(effectiveHitIndex)
    );

    if (m_PickingRequested && m_SelectionBuffer && m_SelectionBuffer->GetMappedData()) {
        auto* data = static_cast<SelectionDataGPU*>(m_SelectionBuffer->GetMappedData());
        data->hitIndex = -1;
    }

    cmd.pushConstants(
        m_GBufferProgram->GetLayout(),
        vk::ShaderStageFlagBits::eCompute,
        0,
        sizeof(SDFPushConstants),
        &pushConstants
    );

    cmd.dispatch(groupX, groupY, 1);

    if (m_PickingRequested) {
        vk::BufferMemoryBarrier barrier{};
        barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eHostRead;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = m_SelectionBuffer->GetBuffer();
        barrier.offset = 0;
        barrier.size = sizeof(SelectionDataGPU);

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eHost,
            {},
            0, nullptr,
            1, &barrier,
            0, nullptr
        );

        m_PickingRequested = false;
        m_PickPendingRead = true;
    }

    // =========================================================================
    // 2. G-Buffer -> Composite/Lighting Barriers
    // =========================================================================
    auto makeGBufReadBarrier = [](VkImage img) {
        vk::ImageMemoryBarrier b{};
        b.oldLayout = vk::ImageLayout::eGeneral;
        b.newLayout = vk::ImageLayout::eGeneral;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = img;
        b.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
        b.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
        b.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        return b;
    };

    vk::ImageMemoryBarrier rawColorWriteBarrier{};
    rawColorWriteBarrier.oldLayout = vk::ImageLayout::eUndefined;
    rawColorWriteBarrier.newLayout = vk::ImageLayout::eGeneral;
    rawColorWriteBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    rawColorWriteBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    rawColorWriteBarrier.image = rawColor.image;
    rawColorWriteBarrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
    rawColorWriteBarrier.srcAccessMask = {};
    rawColorWriteBarrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite;

    std::array<vk::ImageMemoryBarrier, 6> toCompositeBarriers = {
        makeGBufReadBarrier(gbuf.albedo.image),
        makeGBufReadBarrier(gbuf.normal.image),
        makeGBufReadBarrier(gbuf.material.image),
        makeGBufReadBarrier(gbuf.depth.image),
        makeGBufReadBarrier(gbuf.motion.image),
        rawColorWriteBarrier
    };

    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eComputeShader,
        {},
        0, nullptr,
        0, nullptr,
        static_cast<uint32_t>(toCompositeBarriers.size()), toCompositeBarriers.data()
    );

    if (isDebugActive) {
        // =========================================================================
        // 3a. Debug Composite Pass (SDFDebugComposite.glsl -> m_RawColorImage)
        // =========================================================================
        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_DebugCompositeProgram->GetPipeline());
        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute,
            m_DebugCompositeProgram->GetLayout(),
            0,
            1, &m_DebugCompositeDescriptorSet,
            0, nullptr
        );

        DebugCompositePushConstants debugPush{};
        debugPush.screenRes = glm::vec4(
            static_cast<float>(renderWidth),
            static_cast<float>(renderHeight),
            static_cast<float>(effectiveDebugMode),
            0.0f
        );

        cmd.pushConstants(
            m_DebugCompositeProgram->GetLayout(),
            vk::ShaderStageFlagBits::eCompute,
            0,
            sizeof(DebugCompositePushConstants),
            &debugPush
        );

        cmd.dispatch(groupX, groupY, 1);
    } else {
        // =========================================================================
        // 3b. Deferred PBR & IBL Pass (DeferredLighting.glsl -> m_RawColorImage Linear HDR)
        // =========================================================================
        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_DeferredLightingProgram->GetPipeline());
        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute,
            m_DeferredLightingProgram->GetLayout(),
            0,
            1, &m_DeferredLightingDescriptorSet,
            0, nullptr
        );

        DeferredLightingPushConstants defPush{};
        defPush.cameraRight = glm::vec4(camera.right, camera.projection[1][1] * 0.5f);
        defPush.cameraUp = glm::vec4(camera.up, settings.useGrid ? 1.0f : 0.0f);
        const glm::vec3 camPos = camera.position;
        const glm::vec3 camDir = camera.forward;
        defPush.camPos = glm::vec4(camPos, static_cast<float>(m_IBLManager->GetPrefilteredMipLevels()));
        defPush.camDir = glm::vec4(camDir, 1.0f); // xyz: dir, w: exposure = 1.0
        defPush.screenRes = glm::vec4(
            static_cast<float>(renderWidth),
            static_cast<float>(renderHeight),
            1.0f, // z: iblIntensity = 1.0
            static_cast<float>(m_SceneGpuData ? m_SceneGpuData->GetActiveEditCount() : 0) // w: editCount
        );
        defPush.rayParams = glm::vec4(jitter.x, jitter.y, qs.shadowMaxDistance, qs.surfaceBias); // Match GBuffer jitter, including TAA disabled.
        defPush.shadowAOParams = glm::vec4(
            static_cast<float>(qs.shadowMaxSteps),
            qs.shadowK,
            static_cast<float>(qs.aoSamples),
            qs.aoRadius
        ); // x: shadowMaxSteps, y: shadowK, z: aoSamples, w: aoRadius
        auto gridParams = m_SceneGpuData ? m_SceneGpuData->GetViews().gridParams : glm::vec4(0.0f);
        float shadowFlag = (settings.optimizedShadows && qs.enableShadows) ? 1.0f : 0.0f;
        defPush.qualityParams = glm::vec4(shadowFlag, gridParams.x, gridParams.y, gridParams.w);

        cmd.pushConstants(
            m_DeferredLightingProgram->GetLayout(),
            vk::ShaderStageFlagBits::eCompute,
            0,
            sizeof(DeferredLightingPushConstants),
            &defPush
        );

        cmd.dispatch(groupX, groupY, 1);
    }

    // =========================================================================
    // 4. TAA Resolve & ACES Tonemapping Pass (Dogrusal HDR -> sRGB)
    // =========================================================================
    uint32_t readIdx = m_HistoryPingPong;
    uint32_t writeIdx = 1 - m_HistoryPingPong;

    // Barrier: m_RawColorImage ShaderWrite -> ShaderRead
    vk::ImageMemoryBarrier toTaaBarrier{};
    toTaaBarrier.oldLayout = vk::ImageLayout::eGeneral;
    toTaaBarrier.newLayout = vk::ImageLayout::eGeneral;
    toTaaBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTaaBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTaaBarrier.image = rawColor.image;
    toTaaBarrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
    toTaaBarrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
    toTaaBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    // m_StorageImage yazmaya hazirla
    vk::ImageMemoryBarrier storageBarrier = toTaaBarrier;
    storageBarrier.image = outputImg.image;
    storageBarrier.oldLayout = vk::ImageLayout::eGeneral;
    storageBarrier.newLayout = vk::ImageLayout::eGeneral;
    storageBarrier.srcAccessMask = {};
    storageBarrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite;

    // Tarihce okuma (readIdx)
    vk::ImageMemoryBarrier histReadBarrier = toTaaBarrier;
    histReadBarrier.image = m_RenderTargets->GetHistoryColor(readIdx).image;
    histReadBarrier.oldLayout = vk::ImageLayout::eGeneral;
    histReadBarrier.newLayout = vk::ImageLayout::eGeneral;
    histReadBarrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
    histReadBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    // Tarihce yazma (writeIdx)
    bool isFirstHistory = !m_HistoryInitialized || (settings.frameId == 0);
    vk::ImageMemoryBarrier histWriteBarrier = toTaaBarrier;
    histWriteBarrier.image = m_RenderTargets->GetHistoryColor(writeIdx).image;
    histWriteBarrier.oldLayout = vk::ImageLayout::eGeneral;
    histWriteBarrier.newLayout = vk::ImageLayout::eGeneral;
    histWriteBarrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
    histWriteBarrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite;

    // Tarihce ekstra okuma (readIdx)
    vk::ImageMemoryBarrier histExtraReadBarrier = toTaaBarrier;
    histExtraReadBarrier.image = m_RenderTargets->GetHistoryExtra(readIdx).image;
    histExtraReadBarrier.oldLayout = vk::ImageLayout::eGeneral;
    histExtraReadBarrier.newLayout = vk::ImageLayout::eGeneral;
    histExtraReadBarrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
    histExtraReadBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    // Tarihce ekstra yazma (writeIdx)
    vk::ImageMemoryBarrier histExtraWriteBarrier = toTaaBarrier;
    histExtraWriteBarrier.image = m_RenderTargets->GetHistoryExtra(writeIdx).image;
    histExtraWriteBarrier.oldLayout = vk::ImageLayout::eGeneral;
    histExtraWriteBarrier.newLayout = vk::ImageLayout::eGeneral;
    histExtraWriteBarrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
    histExtraWriteBarrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite;
    m_HistoryInitialized = true;

    std::array<vk::ImageMemoryBarrier, 6> taaBarriers = {
        toTaaBarrier, storageBarrier, histReadBarrier, histWriteBarrier,
        histExtraReadBarrier, histExtraWriteBarrier
    };
    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eComputeShader,
        {},
        0, nullptr,
        0, nullptr,
        static_cast<uint32_t>(taaBarriers.size()), taaBarriers.data()
    );

    // TAA Pipeline bagla ve calistir
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_TaaProgram->GetPipeline());
    cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute,
        m_TaaProgram->GetLayout(),
        0,
        1, &m_TaaDescriptorSet[readIdx],
        0, nullptr
    );

    TAAPushConstants taaPush{};
    taaPush.colorParams.x = effectiveExposure;
    uint32_t numChangedRects = 0;
    std::array<glm::vec4, 4> packedChangedRects{};
    m_CurrentChangeSet.GetPackedRects(packedChangedRects, numChangedRects);
    taaPush.colorParams.y = static_cast<float>(numChangedRects);
    taaPush.colorParams.z = m_CurrentChangeSet.IsGlobalChange() ? 1.0f : 0.0f;
    taaPush.changedRect0 = packedChangedRects[0];
    taaPush.changedRect1 = packedChangedRects[1];
    taaPush.changedRect2 = packedChangedRects[2];
    taaPush.changedRect3 = packedChangedRects[3];

    float blendAlpha = qs.taaBlendAlpha;
    if (isDebugActive) {
        blendAlpha = -1.0f; // Debug bypass modu: tonemap ve gamma uygulamadan ham veri aktarimi
    } else if (!settings.taaEnabled) {
        blendAlpha = 1.0f;  // TAA kapali: tarihcesiz ACES tonemap ve sRGB gamma
    }

    taaPush.screenRes = glm::vec4(
        static_cast<float>(renderWidth),
        static_cast<float>(renderHeight),
        (isFirstHistory || !settings.taaEnabled || isDebugActive) ? 0.0f : static_cast<float>(settings.frameId),
        blendAlpha
    );

    cmd.pushConstants(
        m_TaaProgram->GetLayout(),
        vk::ShaderStageFlagBits::eCompute,
        0,
        sizeof(TAAPushConstants),
        &taaPush
    );

    cmd.dispatch(groupX, groupY, 1);

    // m_StorageImage'i Swapchain blit veya ImGui icin hazirla (eGeneral -> eTransferSrcOptimal)
    vk::ImageMemoryBarrier storageReadyBarrier{};
    storageReadyBarrier.oldLayout = vk::ImageLayout::eGeneral;
    storageReadyBarrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    storageReadyBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    storageReadyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    storageReadyBarrier.image = outputImg.image;
    storageReadyBarrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
    storageReadyBarrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
    storageReadyBarrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eTransfer,
        {},
        0, nullptr,
        0, nullptr,
        1, &storageReadyBarrier
    );

    // Tarihce ping-pong indeksini guncelle
    m_HistoryPingPong = writeIdx;

    // m_StorageImage'i ImGui Viewport sampling icin eGeneral duzenine gecir (Shader Read)
    vk::ImageMemoryBarrier toGeneral{};
    toGeneral.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
    toGeneral.newLayout = vk::ImageLayout::eGeneral;
    toGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toGeneral.image = outputImg.image;
    toGeneral.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
    toGeneral.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    toGeneral.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eFragmentShader,
        {},
        0, nullptr,
        0, nullptr,
        1, &toGeneral
    );

    if (m_TemporalHistory) {
        m_TemporalHistory->CommitRender();
    }
}

void SDFRenderer::SetPickingRequest(int mouseX, int mouseY) {
    m_PickingRequested = true;
    m_PickingMouseX = mouseX;
    m_PickingMouseY = mouseY;
}

SDFRenderer::SelectionResult SDFRenderer::GetSelectionResult() const {
    SelectionResult result{};
    if (m_SelectionBuffer && m_SelectionBuffer->GetMappedData()) {
        auto* data = static_cast<const SelectionDataGPU*>(m_SelectionBuffer->GetMappedData());
        result.hitIndex = data->hitIndex;
        result.hasHit = (data->hitIndex >= 0);
        result.hitPoint = glm::vec3(data->hitPoint.x, data->hitPoint.y, data->hitPoint.z);
        result.hitDistance = data->hitPoint.w;
    }
    return result;
}

SDFRenderer::SelectionResult SDFRenderer::ConsumeSelectionResult() {
    SelectionResult result = GetSelectionResult();
    ClearSelectionResult();
    return result;
}

void SDFRenderer::ClearSelectionResult() {
    m_PickPendingRead = false;
    if (m_SelectionBuffer && m_SelectionBuffer->GetMappedData()) {
        auto* data = static_cast<SelectionDataGPU*>(m_SelectionBuffer->GetMappedData());
        data->hitIndex = -1;
    }
}

void SDFRenderer::LoadEnvironment(const std::filesystem::path& path) {
    auto replacement = std::make_unique<IBLManager>(m_Context, path);
    m_Device.waitIdle();
    m_IBLManager.swap(replacement);
    UpdateDeferredLightingDescriptorSets();
    ResetTemporalHistory();
}

void SDFRenderer::SetExposure(float multiplier) {
    if (!std::isfinite(multiplier) || multiplier < 0.0f || multiplier > 64.0f)
        throw std::invalid_argument("Exposure must be finite and in [0, 64]");
    m_Exposure = multiplier;
}

void SDFRenderer::SetCamera(const std::optional<RenderCamera>& camera, const glm::vec2& jitter) {
    if (!camera || !m_RenderCamera || camera->entity != m_RenderCamera->entity ||
        camera->sceneInstance != m_RenderCamera->sceneInstance ||
        camera->projection != m_RenderCamera->projection) {
        m_CameraMatricesInitialized = false;
        m_HistoryInitialized = false;
        if (m_TemporalHistory) {
            m_TemporalHistory->SetCameraCut(true);
        }
    }
    m_PreviousCameraPosition = (m_CameraMatricesInitialized && m_RenderCamera)
        ? m_RenderCamera->position : (camera ? camera->position : glm::vec3(0.0f));
    m_RenderCamera = camera;
    if (camera) {
        SetCameraMatrices(camera->view, camera->projection, jitter);
        if (m_TemporalHistory) {
            m_TemporalHistory->BeginFrame(camera->view, camera->projection, camera->position, jitter, camera->sceneInstance);
        }
    }
}
void SDFRenderer::SetCameraMatrices(const glm::mat4& view, const glm::mat4& proj, const glm::vec2& jitter) {
    glm::mat4 vkProj = proj;
    // Vulkan NDC: Y eksenini ekran koordinatlariyla (0 = ust, 1 = alt) eslestir
    if (vkProj[1][1] > 0.0f) {
        vkProj[1][1] *= -1.0f;
    }
    glm::mat4 vp = vkProj * view;
    if (!m_CameraMatricesInitialized) {
        m_PrevViewProj = vp;
        m_CurrViewProj = vp;
        m_CameraMatricesInitialized = true;
    } else {
        m_PrevViewProj = m_CurrViewProj;
        m_CurrViewProj = vp;
    }
    m_PrevJitter = m_CurrJitter;
    m_CurrJitter = jitter;

    CameraUBOData uboData{};
    uboData.currViewProj = m_CurrViewProj;
    uboData.prevViewProj = m_PrevViewProj;
    uboData.prevCameraPosition = glm::vec4(m_PreviousCameraPosition, 0.0f);
    uboData.jitter = glm::vec4(m_CurrJitter, m_PrevJitter);
    if (m_SceneGpuData) {
        m_SceneGpuData->UploadCamera(uboData);
    }
}

} // namespace Astral
