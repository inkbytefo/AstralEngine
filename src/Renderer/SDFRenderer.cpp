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

namespace Astral {

// 8-Fazli Low-Discrepancy Halton(2, 3) Sub-Pixel Jitter Dizisi
static constexpr std::array<glm::vec2, 8> HALTON_8 = {{
    { 0.0f,        -0.333333f},
    {-0.5f,         0.333333f},
    { 0.5f,        -0.777778f},
    {-0.75f,       -0.111111f},
    { 0.25f,        0.555556f},
    {-0.25f,       -0.555556f},
    { 0.75f,        0.111111f},
    {-0.875f,       0.777778f}
}};

static std::vector<char> ReadFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("[Astral::SDFRenderer] Dosya acilamadi: " + filename);
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

static uint32_t FindMemoryType(vk::PhysicalDevice physDev, uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
    auto memProps = physDev.getMemoryProperties();
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("[Astral::SDFRenderer] Uygun bellek tipi bulunamadi!");
}

SDFRenderer::SDFRenderer(VulkanContext& context, const std::string& spvPath, int width, int height, bool persistentMap, const std::string& taaSpvPath)
    : m_Context(context),
      m_Device(context.GetDevice()),
      m_PhysicalDevice(context.GetPhysicalDevice()),
      m_Width(width),
      m_Height(height),
      m_TaaSpvPath(taaSpvPath) {

    // TAA shader yolunu tespit et
    if (m_TaaSpvPath.empty()) {
        std::filesystem::path p(spvPath);
        std::filesystem::path cand = p.parent_path() / "TAAResolve.spv";
        if (std::filesystem::exists(cand)) {
            m_TaaSpvPath = cand.string();
        } else if (std::filesystem::exists("build/shaders/TAAResolve.spv")) {
            m_TaaSpvPath = "build/shaders/TAAResolve.spv";
        } else {
            m_TaaSpvPath = "shaders/TAAResolve.spv";
        }
    }

    m_ComputePipeline = std::make_unique<ComputePipeline>(m_Device, spvPath);
    CreateImages();
    CreateEditBuffer(persistentMap);

    // PR-9: Selection Buffer (32 bayt, persistent mapped, host-visible & coherent)
    m_SelectionBuffer = std::make_unique<Buffer>(
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

    m_BrickGrid = std::make_unique<BrickGrid>(m_Device, m_PhysicalDevice);
    CreateTAAPipeline();
    CreateDescriptorPoolAndSets();

    // 1. ImGui Viewport Sampler
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

    std::cout << "[Astral::SDFRenderer] SDF Renderer baslatildi (" << m_Width << "x" << m_Height 
              << ", EditBuffer: " << (MAX_EDITS * sizeof(SDFEditGPU)) / 1024 << " KB"
              << ", Two-Level BrickGrid & TAA Aktif, SelectionBuffer Hazir).\n";
}

SDFRenderer::~SDFRenderer() {
    m_Device.waitIdle();
    m_ViewportSampler.reset();

    m_DescriptorPool.reset();
    m_TAAPipeline.reset();
    m_TaaPipelineLayout.reset();
    m_TaaDescriptorSetLayout.reset();
    m_TaaShaderModule.reset();
    m_BrickGrid.reset();
    m_SelectionBuffer.reset();
    m_EditBuffer.reset();
    CleanupImages();
    m_ComputePipeline.reset();
}

void SDFRenderer::CreateTexture(vk::UniqueImage& img, vk::UniqueDeviceMemory& mem, vk::UniqueImageView& view, vk::ImageUsageFlags usage) {
    vk::ImageCreateInfo imageInfo{};
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.extent = vk::Extent3D(static_cast<uint32_t>(m_Width), static_cast<uint32_t>(m_Height), 1);
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = vk::Format::eR8G8B8A8Unorm;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageInfo.usage = usage;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;

    img = m_Device.createImageUnique(imageInfo);

    auto memReq = m_Device.getImageMemoryRequirements(img.get());
    uint32_t memTypeIndex = FindMemoryType(
        m_PhysicalDevice,
        memReq.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eDeviceLocal
    );

    vk::MemoryAllocateInfo allocInfo(memReq.size, memTypeIndex);
    mem = m_Device.allocateMemoryUnique(allocInfo);
    m_Device.bindImageMemory(img.get(), mem.get(), 0);

    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image = img.get();
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = vk::Format::eR8G8B8A8Unorm;
    viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    view = m_Device.createImageViewUnique(viewInfo);
}

void SDFRenderer::CreateImages() {
    CreateTexture(m_StorageImage, m_StorageImageMemory, m_StorageImageView,
                  vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc | 
                  vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled);

    CreateTexture(m_RawColorImage, m_RawColorImageMemory, m_RawColorImageView,
                  vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc);

    CreateTexture(m_HistoryImage, m_HistoryImageMemory, m_HistoryImageView,
                  vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst);
}

void SDFRenderer::CleanupImages() {
    m_HistoryImageView.reset();
    m_HistoryImage.reset();
    m_HistoryImageMemory.reset();

    m_RawColorImageView.reset();
    m_RawColorImage.reset();
    m_RawColorImageMemory.reset();

    m_StorageImageView.reset();
    m_StorageImage.reset();
    m_StorageImageMemory.reset();
}

void SDFRenderer::CreateEditBuffer(bool persistentMap) {
    vk::DeviceSize bufferSize = MAX_EDITS * sizeof(SDFEditGPU);
    m_EditBuffer = std::make_unique<Buffer>(
        m_Device,
        m_PhysicalDevice,
        bufferSize,
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        persistentMap
    );
}

void SDFRenderer::CreateTAAPipeline() {
    // 1. Descriptor Set Layout (3 storage image bindings)
    std::array<vk::DescriptorSetLayoutBinding, 3> bindings{};
    // Binding 0: currImage (Raw)
    bindings[0].binding = 0;
    bindings[0].descriptorType = vk::DescriptorType::eStorageImage;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = vk::ShaderStageFlagBits::eCompute;

    // Binding 1: historyImage
    bindings[1].binding = 1;
    bindings[1].descriptorType = vk::DescriptorType::eStorageImage;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = vk::ShaderStageFlagBits::eCompute;

    // Binding 2: outImage (Resolved final)
    bindings[2].binding = 2;
    bindings[2].descriptorType = vk::DescriptorType::eStorageImage;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = vk::ShaderStageFlagBits::eCompute;

    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    m_TaaDescriptorSetLayout = m_Device.createDescriptorSetLayoutUnique(layoutInfo);

    // 2. Pipeline Layout
    vk::PushConstantRange pushRange{};
    pushRange.stageFlags = vk::ShaderStageFlagBits::eCompute;
    pushRange.offset = 0;
    pushRange.size = sizeof(TAAPushConstants);

    vk::PipelineLayoutCreateInfo pipeLayoutInfo{};
    pipeLayoutInfo.setLayoutCount = 1;
    auto rawLayout = m_TaaDescriptorSetLayout.get();
    pipeLayoutInfo.pSetLayouts = &rawLayout;
    pipeLayoutInfo.pushConstantRangeCount = 1;
    pipeLayoutInfo.pPushConstantRanges = &pushRange;
    m_TaaPipelineLayout = m_Device.createPipelineLayoutUnique(pipeLayoutInfo);

    // 3. Shader Module & Pipeline
    auto code = ReadFile(m_TaaSpvPath);
    vk::ShaderModuleCreateInfo moduleInfo{};
    moduleInfo.codeSize = code.size();
    moduleInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    m_TaaShaderModule = m_Device.createShaderModuleUnique(moduleInfo);

    vk::ComputePipelineCreateInfo pipeInfo{};
    pipeInfo.layout = m_TaaPipelineLayout.get();
    pipeInfo.stage.stage = vk::ShaderStageFlagBits::eCompute;
    pipeInfo.stage.module = m_TaaShaderModule.get();
    pipeInfo.stage.pName = "main";

    auto result = m_Device.createComputePipelineUnique(nullptr, pipeInfo);
    if (result.result != vk::Result::eSuccess) {
        throw std::runtime_error("[Astral::SDFRenderer] TAA Compute pipeline olusturulamadi!");
    }
    m_TAAPipeline = std::move(result.value);
}

void SDFRenderer::CreateDescriptorPoolAndSets() {
    std::array<vk::DescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = vk::DescriptorType::eStorageImage;
    poolSizes[0].descriptorCount = 4; // Raymarch rawColor (1) + TAA (curr, hist, out) (3)
    poolSizes[1].type = vk::DescriptorType::eStorageBuffer;
    poolSizes[1].descriptorCount = 3; // EditBuffer (1) + GridBuffer (1) + SelectionBuffer (1)

    vk::DescriptorPoolCreateInfo poolInfo(
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        2, // 2 Set: Raymarch set + TAA set
        static_cast<uint32_t>(poolSizes.size()),
        poolSizes.data()
    );
    m_DescriptorPool = m_Device.createDescriptorPoolUnique(poolInfo);

    // 1. Raymarch Descriptor Set
    auto rawLayout = m_ComputePipeline->GetDescriptorSetLayout();
    vk::DescriptorSetAllocateInfo setAllocInfo(m_DescriptorPool.get(), 1, &rawLayout);
    auto sets = m_Device.allocateDescriptorSets(setAllocInfo);
    m_DescriptorSet = sets[0];

    // 2. TAA Descriptor Set
    auto taaLayout = m_TaaDescriptorSetLayout.get();
    vk::DescriptorSetAllocateInfo taaAllocInfo(m_DescriptorPool.get(), 1, &taaLayout);
    auto taaSets = m_Device.allocateDescriptorSets(taaAllocInfo);
    m_TaaDescriptorSet = taaSets[0];

    UpdateDescriptorSets();
}

void SDFRenderer::UpdateDescriptorSets() {
    // 1. Raymarch Set Guncelleme
    vk::DescriptorImageInfo rawImgInfo(nullptr, m_RawColorImageView.get(), vk::ImageLayout::eGeneral);
    auto editBufInfo = m_EditBuffer->GetDescriptorInfo();
    auto gridBufInfo = m_BrickGrid->GetBuffer()->GetDescriptorInfo();
    auto selBufInfo = m_SelectionBuffer->GetDescriptorInfo();

    std::array<vk::WriteDescriptorSet, 4> writeSets{};
    writeSets[0].dstSet = m_DescriptorSet;
    writeSets[0].dstBinding = 0;
    writeSets[0].dstArrayElement = 0;
    writeSets[0].descriptorCount = 1;
    writeSets[0].descriptorType = vk::DescriptorType::eStorageImage;
    writeSets[0].pImageInfo = &rawImgInfo;

    writeSets[1].dstSet = m_DescriptorSet;
    writeSets[1].dstBinding = 1;
    writeSets[1].dstArrayElement = 0;
    writeSets[1].descriptorCount = 1;
    writeSets[1].descriptorType = vk::DescriptorType::eStorageBuffer;
    writeSets[1].pBufferInfo = &editBufInfo;

    writeSets[2].dstSet = m_DescriptorSet;
    writeSets[2].dstBinding = 2;
    writeSets[2].dstArrayElement = 0;
    writeSets[2].descriptorCount = 1;
    writeSets[2].descriptorType = vk::DescriptorType::eStorageBuffer;
    writeSets[2].pBufferInfo = &gridBufInfo;

    writeSets[3].dstSet = m_DescriptorSet;
    writeSets[3].dstBinding = 3;
    writeSets[3].dstArrayElement = 0;
    writeSets[3].descriptorCount = 1;
    writeSets[3].descriptorType = vk::DescriptorType::eStorageBuffer;
    writeSets[3].pBufferInfo = &selBufInfo;

    m_Device.updateDescriptorSets(static_cast<uint32_t>(writeSets.size()), writeSets.data(), 0, nullptr);

    // 2. TAA Set Guncelleme
    vk::DescriptorImageInfo taaCurrInfo(nullptr, m_RawColorImageView.get(), vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo taaHistInfo(nullptr, m_HistoryImageView.get(), vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo taaOutInfo(nullptr, m_StorageImageView.get(), vk::ImageLayout::eGeneral);

    std::array<vk::WriteDescriptorSet, 3> taaWriteSets{};
    taaWriteSets[0].dstSet = m_TaaDescriptorSet;
    taaWriteSets[0].dstBinding = 0;
    taaWriteSets[0].dstArrayElement = 0;
    taaWriteSets[0].descriptorCount = 1;
    taaWriteSets[0].descriptorType = vk::DescriptorType::eStorageImage;
    taaWriteSets[0].pImageInfo = &taaCurrInfo;

    taaWriteSets[1].dstSet = m_TaaDescriptorSet;
    taaWriteSets[1].dstBinding = 1;
    taaWriteSets[1].dstArrayElement = 0;
    taaWriteSets[1].descriptorCount = 1;
    taaWriteSets[1].descriptorType = vk::DescriptorType::eStorageImage;
    taaWriteSets[1].pImageInfo = &taaHistInfo;

    taaWriteSets[2].dstSet = m_TaaDescriptorSet;
    taaWriteSets[2].dstBinding = 2;
    taaWriteSets[2].dstArrayElement = 0;
    taaWriteSets[2].descriptorCount = 1;
    taaWriteSets[2].descriptorType = vk::DescriptorType::eStorageImage;
    taaWriteSets[2].pImageInfo = &taaOutInfo;

    m_Device.updateDescriptorSets(static_cast<uint32_t>(taaWriteSets.size()), taaWriteSets.data(), 0, nullptr);
}

void SDFRenderer::UpdateEdits(const std::vector<SDFEditGPU>& edits, bool useLegacyMapUnmap) {
    m_ActiveEditCount = std::min(edits.size(), MAX_EDITS);
    if (m_ActiveEditCount > 0) {
        size_t uploadBytes = m_ActiveEditCount * sizeof(SDFEditGPU);
        if (useLegacyMapUnmap) {
            m_EditBuffer->UpdateDataLegacy(edits.data(), uploadBytes);
        } else {
            m_EditBuffer->UpdateData(edits.data(), uploadBytes);
        }
    }

    if (m_BrickGrid) {
        m_BrickGrid->Build(std::span<const SDFEditGPU>(edits.data(), m_ActiveEditCount));
    }
}

void SDFRenderer::Resize(int width, int height) {
    if (width <= 0 || height <= 0 || (width == m_Width && height == m_Height)) return;

    m_Device.waitIdle();

    m_Width = width;
    m_Height = height;

    CleanupImages();
    CreateImages();
    UpdateDescriptorSets();
    m_HistoryInitialized = false;
}

void SDFRenderer::Render(vk::CommandBuffer cmd, float time, uint32_t normalMode, int width, int height,
                         bool useGrid, bool optShadow, bool enableTAA, uint32_t frameIndex) {
    (void)width;
    (void)height;

    // 1. Raymarching ciktisini (m_RawColorImage) eGeneral durumuna gecir
    vk::ImageMemoryBarrier rawBarrier{};
    rawBarrier.oldLayout = vk::ImageLayout::eUndefined;
    rawBarrier.newLayout = vk::ImageLayout::eGeneral;
    rawBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    rawBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    rawBarrier.image = m_RawColorImage.get();
    rawBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    rawBarrier.subresourceRange.baseMipLevel = 0;
    rawBarrier.subresourceRange.levelCount = 1;
    rawBarrier.subresourceRange.baseArrayLayer = 0;
    rawBarrier.subresourceRange.layerCount = 1;
    rawBarrier.srcAccessMask = {};
    rawBarrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite;

    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eComputeShader,
        {},
        0, nullptr,
        0, nullptr,
        1, &rawBarrier
    );

    // 2. Raymarch Compute Pipeline ve Descriptors bagla
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_ComputePipeline->GetPipeline());
    cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute,
        m_ComputePipeline->GetPipelineLayout(),
        0,
        1, &m_DescriptorSet,
        0, nullptr
    );

    // 3. Raymarch Push Constants aktar
    SDFPushConstants pushConstants{};
    pushConstants.camPos = glm::vec4(0.0f, 1.5f, 4.0f, time);
    pushConstants.camDir = glm::vec4(0.0f, -0.25f, -1.0f, static_cast<float>(normalMode));
    pushConstants.screenRes = glm::vec4(
        static_cast<float>(m_Width),
        static_cast<float>(m_Height),
        static_cast<float>(m_ActiveEditCount),
        useGrid ? 1.0f : 0.0f
    );
    pushConstants.gridParams = m_BrickGrid->GetGridParams();
    pushConstants.gridParams.z = optShadow ? 1.0f : 0.0f;

    // TAA Sub-Pixel Jitter (Halton 8-faz)
    glm::vec2 jitter = enableTAA ? HALTON_8[frameIndex % 8] : glm::vec2(0.0f);
    pushConstants.taaParams = glm::vec4(jitter.x, jitter.y, enableTAA ? 1.0f : 0.0f, 0.12f);

    // PR-9: Mouse Picking Params & Secili Nesne Fresnel Vurgusu
    pushConstants.mouseParams = glm::vec4(
        static_cast<float>(m_PickingMouseX),
        static_cast<float>(m_PickingMouseY),
        m_PickingRequested ? 1.0f : 0.0f,
        static_cast<float>(m_SelectedHitIndex)
    );

    if (m_PickingRequested && m_SelectionBuffer && m_SelectionBuffer->GetMappedData()) {
        auto* data = static_cast<SelectionDataGPU*>(m_SelectionBuffer->GetMappedData());
        data->hitIndex = -1;
    }

    cmd.pushConstants(
        m_ComputePipeline->GetPipelineLayout(),
        vk::ShaderStageFlagBits::eCompute,
        0,
        sizeof(SDFPushConstants),
        &pushConstants
    );

    // 4. Raymarching Dispatch (8x8 workgroup)
    uint32_t groupX = static_cast<uint32_t>(std::ceil(static_cast<float>(m_Width) / 8.0f));
    uint32_t groupY = static_cast<uint32_t>(std::ceil(static_cast<float>(m_Height) / 8.0f));
    cmd.dispatch(groupX, groupY, 1);

    // PR-9: BufferMemoryBarrier (ShaderWrite -> HostRead)
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

    if (enableTAA) {
        // --- 5a. TAA Resolve Pass ---
        // Raymarch çıktısı yazıldı -> TAA için okunabilir yap (eGeneral / eShaderRead)
        vk::ImageMemoryBarrier toTaaBarrier{};
        toTaaBarrier.oldLayout = vk::ImageLayout::eGeneral;
        toTaaBarrier.newLayout = vk::ImageLayout::eGeneral;
        toTaaBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTaaBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTaaBarrier.image = m_RawColorImage.get();
        toTaaBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        toTaaBarrier.subresourceRange.baseMipLevel = 0;
        toTaaBarrier.subresourceRange.levelCount = 1;
        toTaaBarrier.subresourceRange.baseArrayLayer = 0;
        toTaaBarrier.subresourceRange.layerCount = 1;
        toTaaBarrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
        toTaaBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        // m_StorageImage yazmaya hazırla
        vk::ImageMemoryBarrier storageBarrier = toTaaBarrier;
        storageBarrier.image = m_StorageImage.get();
        storageBarrier.oldLayout = vk::ImageLayout::eUndefined;
        storageBarrier.newLayout = vk::ImageLayout::eGeneral;
        storageBarrier.srcAccessMask = {};
        storageBarrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite;

        // m_HistoryImage ilk karede veya resize sonrasi undefined'dan General'a geçir
        bool isFirstHistory = !m_HistoryInitialized || (frameIndex == 0);
        vk::ImageMemoryBarrier histBarrier = toTaaBarrier;
        histBarrier.image = m_HistoryImage.get();
        histBarrier.oldLayout = isFirstHistory ? vk::ImageLayout::eUndefined : vk::ImageLayout::eGeneral;
        histBarrier.newLayout = vk::ImageLayout::eGeneral;
        histBarrier.srcAccessMask = isFirstHistory ? vk::AccessFlags{} : vk::AccessFlagBits::eTransferWrite;
        histBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        m_HistoryInitialized = true;

        std::array<vk::ImageMemoryBarrier, 3> taaBarriers = { toTaaBarrier, storageBarrier, histBarrier };
        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eComputeShader,
            {},
            0, nullptr,
            0, nullptr,
            static_cast<uint32_t>(taaBarriers.size()), taaBarriers.data()
        );

        // TAA Pipeline bagla ve calistir
        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_TAAPipeline.get());
        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute,
            m_TaaPipelineLayout.get(),
            0,
            1, &m_TaaDescriptorSet,
            0, nullptr
        );

        TAAPushConstants taaPush{};
        taaPush.screenRes = glm::vec4(
            static_cast<float>(m_Width),
            static_cast<float>(m_Height),
            isFirstHistory ? 0.0f : static_cast<float>(frameIndex),
            0.12f // EMA Blend Alpha
        );

        cmd.pushConstants(
            m_TaaPipelineLayout.get(),
            vk::ShaderStageFlagBits::eCompute,
            0,
            sizeof(TAAPushConstants),
            &taaPush
        );

        cmd.dispatch(groupX, groupY, 1);

        // Tarihçe kopyası: m_StorageImage -> m_HistoryImage
        vk::ImageMemoryBarrier beforeCopySrc{};
        beforeCopySrc.oldLayout = vk::ImageLayout::eGeneral;
        beforeCopySrc.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        beforeCopySrc.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        beforeCopySrc.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        beforeCopySrc.image = m_StorageImage.get();
        beforeCopySrc.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
        beforeCopySrc.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
        beforeCopySrc.dstAccessMask = vk::AccessFlagBits::eTransferRead;

        vk::ImageMemoryBarrier beforeCopyDst{};
        beforeCopyDst.oldLayout = vk::ImageLayout::eGeneral;
        beforeCopyDst.newLayout = vk::ImageLayout::eTransferDstOptimal;
        beforeCopyDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        beforeCopyDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        beforeCopyDst.image = m_HistoryImage.get();
        beforeCopyDst.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
        beforeCopyDst.srcAccessMask = vk::AccessFlagBits::eShaderRead;
        beforeCopyDst.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        std::array<vk::ImageMemoryBarrier, 2> copyBarriers = { beforeCopySrc, beforeCopyDst };
        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eTransfer,
            {},
            0, nullptr,
            0, nullptr,
            2, copyBarriers.data()
        );

        vk::ImageCopy copyRegion{};
        copyRegion.srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
        copyRegion.dstSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
        copyRegion.extent = vk::Extent3D(static_cast<uint32_t>(m_Width), static_cast<uint32_t>(m_Height), 1);

        cmd.copyImage(
            m_StorageImage.get(), vk::ImageLayout::eTransferSrcOptimal,
            m_HistoryImage.get(), vk::ImageLayout::eTransferDstOptimal,
            1, &copyRegion
        );

        // m_HistoryImage'i sonraki kare icin eGeneral'a dondur
        vk::ImageMemoryBarrier afterCopyHist{};
        afterCopyHist.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        afterCopyHist.newLayout = vk::ImageLayout::eGeneral;
        afterCopyHist.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        afterCopyHist.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        afterCopyHist.image = m_HistoryImage.get();
        afterCopyHist.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
        afterCopyHist.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        afterCopyHist.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eComputeShader,
            {},
            0, nullptr,
            0, nullptr,
            1, &afterCopyHist
        );

        // m_StorageImage artik Swapchain blit icin hazirdir (eTransferSrcOptimal)
    } else {
        // --- 5b. TAA Kapali (Passthrough) ---
        // m_RawColorImage -> m_StorageImage kopyala
        vk::ImageMemoryBarrier beforeCopySrc{};
        beforeCopySrc.oldLayout = vk::ImageLayout::eGeneral;
        beforeCopySrc.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        beforeCopySrc.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        beforeCopySrc.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        beforeCopySrc.image = m_RawColorImage.get();
        beforeCopySrc.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
        beforeCopySrc.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
        beforeCopySrc.dstAccessMask = vk::AccessFlagBits::eTransferRead;

        vk::ImageMemoryBarrier beforeCopyDst{};
        beforeCopyDst.oldLayout = vk::ImageLayout::eUndefined;
        beforeCopyDst.newLayout = vk::ImageLayout::eTransferDstOptimal;
        beforeCopyDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        beforeCopyDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        beforeCopyDst.image = m_StorageImage.get();
        beforeCopyDst.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
        beforeCopyDst.srcAccessMask = {};
        beforeCopyDst.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        std::array<vk::ImageMemoryBarrier, 2> copyBarriers = { beforeCopySrc, beforeCopyDst };
        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eTransfer,
            {},
            0, nullptr,
            0, nullptr,
            2, copyBarriers.data()
        );

        vk::ImageCopy copyRegion{};
        copyRegion.srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
        copyRegion.dstSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
        copyRegion.extent = vk::Extent3D(static_cast<uint32_t>(m_Width), static_cast<uint32_t>(m_Height), 1);

        cmd.copyImage(
            m_RawColorImage.get(), vk::ImageLayout::eTransferSrcOptimal,
            m_StorageImage.get(), vk::ImageLayout::eTransferDstOptimal,
            1, &copyRegion
        );

        // m_StorageImage'i swapchain blit icin eTransferSrcOptimal yap
        vk::ImageMemoryBarrier afterCopyStorage{};
        afterCopyStorage.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        afterCopyStorage.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        afterCopyStorage.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        afterCopyStorage.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        afterCopyStorage.image = m_StorageImage.get();
        afterCopyStorage.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
        afterCopyStorage.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        afterCopyStorage.dstAccessMask = vk::AccessFlagBits::eTransferRead;

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eTransfer,
            {},
            0, nullptr,
            0, nullptr,
            1, &afterCopyStorage
        );
    }

    // m_StorageImage'i ImGui Viewport sampling icin eGeneral duzenine gecir (Shader Read)
    vk::ImageMemoryBarrier toGeneral{};
    toGeneral.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
    toGeneral.newLayout = vk::ImageLayout::eGeneral;
    toGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toGeneral.image = m_StorageImage.get();
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

} // namespace Astral
