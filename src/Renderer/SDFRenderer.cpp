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

    // TAA shader yolunu tespit et
    if (m_TaaSpvPath.empty()) {
        std::filesystem::path cand = p.parent_path() / "TAAResolve.spv";
        if (std::filesystem::exists(cand)) {
            m_TaaSpvPath = cand.string();
        } else if (std::filesystem::exists("build/shaders/TAAResolve.spv")) {
            m_TaaSpvPath = "build/shaders/TAAResolve.spv";
        } else {
            m_TaaSpvPath = "shaders/TAAResolve.spv";
        }
    }

    // G-Buffer shader yolunu tespit et
    if (m_GBufferSpvPath.empty()) {
        std::filesystem::path cand = p.parent_path() / "SDFGBuffer.spv";
        if (std::filesystem::exists(cand)) {
            m_GBufferSpvPath = cand.string();
        } else if (std::filesystem::exists("build/shaders/SDFGBuffer.spv")) {
            m_GBufferSpvPath = "build/shaders/SDFGBuffer.spv";
        } else {
            m_GBufferSpvPath = "shaders/SDFGBuffer.spv";
        }
    }

    // Debug Composite shader yolunu tespit et
    if (m_DebugCompositeSpvPath.empty()) {
        std::filesystem::path cand = p.parent_path() / "SDFDebugComposite.spv";
        if (std::filesystem::exists(cand)) {
            m_DebugCompositeSpvPath = cand.string();
        } else if (std::filesystem::exists("build/shaders/SDFDebugComposite.spv")) {
            m_DebugCompositeSpvPath = "build/shaders/SDFDebugComposite.spv";
        } else {
            m_DebugCompositeSpvPath = "shaders/SDFDebugComposite.spv";
        }
    }

    // Deferred Lighting shader yolunu tespit et
    if (m_DeferredLightingSpvPath.empty()) {
        std::filesystem::path cand = p.parent_path() / "DeferredLighting.spv";
        if (std::filesystem::exists(cand)) {
            m_DeferredLightingSpvPath = cand.string();
        } else if (std::filesystem::exists("build/shaders/DeferredLighting.spv")) {
            m_DeferredLightingSpvPath = "build/shaders/DeferredLighting.spv";
        } else {
            m_DeferredLightingSpvPath = "shaders/DeferredLighting.spv";
        }
    }

    m_ComputePipeline = std::make_unique<ComputePipeline>(m_Device, spvPath);
    CreateImages();
    CreateEditBuffer(persistentMap);
    CreateCameraUBO();

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

    m_BrickGrid = std::make_unique<BrickGrid>(m_Context.GetAllocator(), m_Device, m_PhysicalDevice);

    // Faz 2: IBL ve Isik Buffer kurulumu
    m_IBLManager = std::make_unique<IBLManager>(m_Context);
    CreateLightBuffer();

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
              << ", EditBuffer: " << (MAX_EDITS * sizeof(SDFEditGPU)) / 1024 << " KB"
              << ", Two-Level BrickGrid, Deferred PBR & IBL Lighting Aktif).\n";
}

SDFRenderer::~SDFRenderer() {
    m_Device.waitIdle();
    m_ViewportSampler.reset();
    m_LinearClampSampler.reset();

    m_DescriptorPool.reset();
    m_DeferredLightingPipeline.reset();
    m_DeferredLightingPipelineLayout.reset();
    m_DeferredLightingDescriptorSetLayout.reset();
    m_DeferredLightingShaderModule.reset();

    m_DebugCompositePipeline.reset();
    m_DebugCompositePipelineLayout.reset();
    m_DebugCompositeDescriptorSetLayout.reset();
    m_DebugCompositeShaderModule.reset();

    m_GBufferPipeline.reset();
    m_GBufferPipelineLayout.reset();
    m_GBufferDescriptorSetLayout.reset();
    m_GBufferShaderModule.reset();

    m_TAAPipeline.reset();
    m_TaaPipelineLayout.reset();
    m_TaaDescriptorSetLayout.reset();
    m_TaaShaderModule.reset();

    m_LightBuffer.reset();
    m_IBLManager.reset();
    m_BrickGrid.reset();
    m_SelectionBuffer.reset();
    m_CameraUBO.reset();
    m_EditBuffer.reset();
    CleanupImages();
    m_ComputePipeline.reset();
}
void SDFRenderer::CreateTexture(VmaImage& img, vk::UniqueImageView& view, vk::ImageUsageFlags usage) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = VkExtent3D{static_cast<uint32_t>(m_Width), static_cast<uint32_t>(m_Height), 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = static_cast<VkImageUsageFlags>(usage);
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VkImage rawImg = VK_NULL_HANDLE;
    VmaAllocation alloc = VK_NULL_HANDLE;
    VkResult res = vmaCreateImage(m_Context.GetAllocator(), &imageInfo, &allocCreateInfo, &rawImg, &alloc, nullptr);
    if (res != VK_SUCCESS) {
        throw std::runtime_error("[Astral::SDFRenderer] vmaCreateImage basarisiz! VkResult: " + std::to_string(res));
    }
    img.image = rawImg;
    img.allocation = alloc;

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

void SDFRenderer::CreateGBufferTexture(VmaImage& img, vk::UniqueImageView& view, vk::Format format, vk::ImageUsageFlags usage) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = VkExtent3D{static_cast<uint32_t>(m_Width), static_cast<uint32_t>(m_Height), 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = static_cast<VkFormat>(format);
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = static_cast<VkImageUsageFlags>(usage);
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VkImage rawImg = VK_NULL_HANDLE;
    VmaAllocation alloc = VK_NULL_HANDLE;
    VkResult res = vmaCreateImage(m_Context.GetAllocator(), &imageInfo, &allocCreateInfo, &rawImg, &alloc, nullptr);
    if (res != VK_SUCCESS) {
        throw std::runtime_error("[Astral::SDFRenderer] G-Buffer vmaCreateImage basarisiz! VkResult: " + std::to_string(res));
    }
    img.image = rawImg;
    img.allocation = alloc;

    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image = img.get();
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    view = m_Device.createImageViewUnique(viewInfo);
}

void SDFRenderer::CreateImages() {
    CreateTexture(m_StorageImage, m_StorageImageView,
                  vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc | 
                  vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled);

    // m_RawColorImage: Dogrusal HDR ciktisi (VK_FORMAT_R16G16B16A16_SFLOAT)
    CreateGBufferTexture(m_RawColorImage, m_RawColorImageView, vk::Format::eR16G16B16A16Sfloat,
                         vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst);

    // Ping-pong TAA Tarihce Tamponlari: Dogrusal HDR (VK_FORMAT_R16G16B16A16_SFLOAT)
    for (int i = 0; i < 2; ++i) {
        CreateGBufferTexture(m_HistoryImage[i], m_HistoryImageView[i], vk::Format::eR16G16B16A16Sfloat,
                             vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);
    }

    // G-Buffer Render Hedefleri (Faz 1)
    CreateGBufferTexture(m_GBufAlbedo, m_GBufAlbedoView, vk::Format::eR8G8B8A8Unorm,
                         vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled);
    CreateGBufferTexture(m_GBufNormal, m_GBufNormalView, vk::Format::eR16G16B16A16Sfloat,
                         vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled);
    CreateGBufferTexture(m_GBufMaterial, m_GBufMaterialView, vk::Format::eR8G8B8A8Unorm,
                         vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled);
    CreateGBufferTexture(m_GBufDepth, m_GBufDepthView, vk::Format::eR32Sfloat,
                         vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled);
    CreateGBufferTexture(m_GBufMotion, m_GBufMotionView, vk::Format::eR16G16Sfloat,
                         vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);

    // Tum goruntuleri baslangicta guvenli eGeneral duzenine gecir (Monolitik / G-Buffer modu tam uyumluluk)
    m_Context.ExecuteImmediate([&](vk::CommandBuffer cmd) {
        auto makeInitBarrier = [](VkImage img) {
            vk::ImageMemoryBarrier b{};
            b.oldLayout = vk::ImageLayout::eUndefined;
            b.newLayout = vk::ImageLayout::eGeneral;
            b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image = img;
            b.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
            b.srcAccessMask = {};
            b.dstAccessMask = vk::AccessFlagBits::eTransferWrite | vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
            return b;
        };

        std::array<vk::ImageMemoryBarrier, 9> initBarriers = {
            makeInitBarrier(m_StorageImage.get()),
            makeInitBarrier(m_RawColorImage.get()),
            makeInitBarrier(m_HistoryImage[0].get()),
            makeInitBarrier(m_HistoryImage[1].get()),
            makeInitBarrier(m_GBufAlbedo.get()),
            makeInitBarrier(m_GBufNormal.get()),
            makeInitBarrier(m_GBufMaterial.get()),
            makeInitBarrier(m_GBufDepth.get()),
            makeInitBarrier(m_GBufMotion.get())
        };

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader,
            {},
            0, nullptr, 0, nullptr,
            static_cast<uint32_t>(initBarriers.size()), initBarriers.data()
        );

        // Tarihce, motion vector ve raw color hedeflerini 0.0 ile temizle (NaN/çöp veri önleme)
        vk::ClearColorValue zeroColor(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f});
        vk::ImageSubresourceRange clearRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
        cmd.clearColorImage(m_GBufMotion.get(), vk::ImageLayout::eGeneral, zeroColor, clearRange);
        cmd.clearColorImage(m_HistoryImage[0].get(), vk::ImageLayout::eGeneral, zeroColor, clearRange);
        cmd.clearColorImage(m_HistoryImage[1].get(), vk::ImageLayout::eGeneral, zeroColor, clearRange);
        cmd.clearColorImage(m_RawColorImage.get(), vk::ImageLayout::eGeneral, zeroColor, clearRange);

        vk::MemoryBarrier clearDoneBarrier{};
        clearDoneBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        clearDoneBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eComputeShader,
            {},
            1, &clearDoneBarrier,
            0, nullptr,
            0, nullptr
        );
    });
}

void SDFRenderer::CleanupImages() {
    VmaAllocator allocator = m_Context.GetAllocator();

    auto destroyImg = [allocator](VmaImage& img, vk::UniqueImageView& view) {
        view.reset();
        if (img.image && allocator != VK_NULL_HANDLE) {
            vmaDestroyImage(allocator, img.image, img.allocation);
            img.reset();
        }
    };

    destroyImg(m_GBufMotion, m_GBufMotionView);
    destroyImg(m_GBufDepth, m_GBufDepthView);
    destroyImg(m_GBufMaterial, m_GBufMaterialView);
    destroyImg(m_GBufNormal, m_GBufNormalView);
    destroyImg(m_GBufAlbedo, m_GBufAlbedoView);

    for (int i = 0; i < 2; ++i) {
        destroyImg(m_HistoryImage[i], m_HistoryImageView[i]);
    }
    destroyImg(m_RawColorImage, m_RawColorImageView);
    destroyImg(m_StorageImage, m_StorageImageView);
}

void SDFRenderer::CreateLightBuffer() {
    constexpr size_t MAX_LIGHTS = 64;
    VkDeviceSize bufSize = sizeof(LightBufferHeader) + MAX_LIGHTS * sizeof(LightGPU);

    m_LightBuffer = std::make_unique<Buffer>(
        m_Context.GetAllocator(),
        m_Device,
        m_PhysicalDevice,
        bufSize,
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        true
    );

    // Varsayilan Isiklar: 1 Yonlu (Gunes) + 1 Noktasal (Dolgu)
    m_Lights.clear();

    // 1. Yonlu Gunes Isigi (Warm sunlight)
    LightGPU sunLight{};
    sunLight.position = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); // w=0: Directional
    sunLight.direction = glm::vec4(glm::normalize(glm::vec3(0.5f, -0.8f, 0.4f)), 3.0f); // xyz: dir, w: intensity
    sunLight.color = glm::vec4(1.0f, 0.95f, 0.88f, 0.0f); // rgb: color, w: range
    m_Lights.push_back(sunLight);

    // 2. Noktasal Dolgu Isigi (Cool blue fill)
    LightGPU fillLight{};
    fillLight.position = glm::vec4(-2.0f, 2.5f, 2.0f, 1.0f); // w=1: Point
    fillLight.direction = glm::vec4(0.0f, 0.0f, 0.0f, 2.0f); // w: intensity
    fillLight.color = glm::vec4(0.4f, 0.65f, 1.0f, 20.0f); // rgb: soft blue, w: range = 20.0
    m_Lights.push_back(fillLight);

    UpdateLights();
}

void SDFRenderer::UpdateLights() {
    if (!m_LightBuffer) return;

    LightBufferHeader header{};
    header.lightCount = static_cast<uint32_t>(m_Lights.size());
    m_LightBuffer->UpdateData(&header, sizeof(LightBufferHeader), 0);

    if (!m_Lights.empty()) {
        m_LightBuffer->UpdateData(m_Lights.data(), m_Lights.size() * sizeof(LightGPU), sizeof(LightBufferHeader));
    }
}

void SDFRenderer::SetLights(const std::vector<LightGPU>& lights) {
    m_Lights = lights;
    UpdateLights();
}

void SDFRenderer::CreateCameraUBO() {
    m_CameraUBO = std::make_unique<Buffer>(
        m_Context.GetAllocator(),
        m_Device,
        m_PhysicalDevice,
        sizeof(CameraUBOData),
        vk::BufferUsageFlagBits::eUniformBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        true
    );
    if (m_CameraUBO->GetMappedData()) {
        CameraUBOData initData{};
        std::memcpy(m_CameraUBO->GetMappedData(), &initData, sizeof(CameraUBOData));
    }
}

void SDFRenderer::CreateEditBuffer(bool persistentMap) {
    vk::DeviceSize bufferSize = MAX_EDITS * sizeof(SDFEditGPU);
    m_EditBuffer = std::make_unique<Buffer>(
        m_Context.GetAllocator(),
        m_Device,
        m_PhysicalDevice,
        bufferSize,
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        persistentMap
    );
}

void SDFRenderer::CreateTAAPipeline() {
    // 1. Descriptor Set Layout (5 bindings: 4 storage image + 1 combined image sampler)
    std::array<vk::DescriptorSetLayoutBinding, 5> bindings{};
    // Binding 0: currImage (Raw HDR, rgba16f)
    bindings[0].binding = 0;
    bindings[0].descriptorType = vk::DescriptorType::eStorageImage;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = vk::ShaderStageFlagBits::eCompute;

    // Binding 1: historyTexture (Combined Image Sampler, Linear Clamp, sampler2D)
    bindings[1].binding = 1;
    bindings[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = vk::ShaderStageFlagBits::eCompute;

    // Binding 2: outImage (Resolved final sRGB, rgba8)
    bindings[2].binding = 2;
    bindings[2].descriptorType = vk::DescriptorType::eStorageImage;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = vk::ShaderStageFlagBits::eCompute;

    // Binding 3: outHistory (write HDR, rgba16f)
    bindings[3].binding = 3;
    bindings[3].descriptorType = vk::DescriptorType::eStorageImage;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = vk::ShaderStageFlagBits::eCompute;

    // Binding 4: motionVectors (read velocity, rg16f)
    bindings[4].binding = 4;
    bindings[4].descriptorType = vk::DescriptorType::eStorageImage;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = vk::ShaderStageFlagBits::eCompute;

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

void SDFRenderer::CreateGBufferPipeline() {
    std::array<vk::DescriptorSetLayoutBinding, 9> bindings{};
    for (uint32_t i = 0; i < 5; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = vk::DescriptorType::eStorageImage;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = vk::ShaderStageFlagBits::eCompute;
    }
    for (uint32_t i = 5; i < 8; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = vk::DescriptorType::eStorageBuffer;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = vk::ShaderStageFlagBits::eCompute;
    }
    bindings[8].binding = 8;
    bindings[8].descriptorType = vk::DescriptorType::eUniformBuffer;
    bindings[8].descriptorCount = 1;
    bindings[8].stageFlags = vk::ShaderStageFlagBits::eCompute;

    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    m_GBufferDescriptorSetLayout = m_Device.createDescriptorSetLayoutUnique(layoutInfo);

    vk::PushConstantRange pushRange{};
    pushRange.stageFlags = vk::ShaderStageFlagBits::eCompute;
    pushRange.offset = 0;
    pushRange.size = sizeof(SDFPushConstants);

    vk::PipelineLayoutCreateInfo pipeLayoutInfo{};
    pipeLayoutInfo.setLayoutCount = 1;
    auto rawLayout = m_GBufferDescriptorSetLayout.get();
    pipeLayoutInfo.pSetLayouts = &rawLayout;
    pipeLayoutInfo.pushConstantRangeCount = 1;
    pipeLayoutInfo.pPushConstantRanges = &pushRange;
    m_GBufferPipelineLayout = m_Device.createPipelineLayoutUnique(pipeLayoutInfo);

    auto code = ReadFile(m_GBufferSpvPath);
    vk::ShaderModuleCreateInfo moduleInfo{};
    moduleInfo.codeSize = code.size();
    moduleInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    m_GBufferShaderModule = m_Device.createShaderModuleUnique(moduleInfo);

    vk::ComputePipelineCreateInfo pipeInfo{};
    pipeInfo.layout = m_GBufferPipelineLayout.get();
    pipeInfo.stage.stage = vk::ShaderStageFlagBits::eCompute;
    pipeInfo.stage.module = m_GBufferShaderModule.get();
    pipeInfo.stage.pName = "main";

    auto result = m_Device.createComputePipelineUnique(nullptr, pipeInfo);
    if (result.result != vk::Result::eSuccess) {
        throw std::runtime_error("[Astral::SDFRenderer] G-Buffer Compute pipeline olusturulamadi!");
    }
    m_GBufferPipeline = std::move(result.value);
}

void SDFRenderer::CreateDebugCompositePipeline() {
    std::array<vk::DescriptorSetLayoutBinding, 6> bindings{};
    for (uint32_t i = 0; i < 6; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = vk::DescriptorType::eStorageImage;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = vk::ShaderStageFlagBits::eCompute;
    }

    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    m_DebugCompositeDescriptorSetLayout = m_Device.createDescriptorSetLayoutUnique(layoutInfo);

    vk::PushConstantRange pushRange{};
    pushRange.stageFlags = vk::ShaderStageFlagBits::eCompute;
    pushRange.offset = 0;
    pushRange.size = sizeof(DebugCompositePushConstants);

    vk::PipelineLayoutCreateInfo pipeLayoutInfo{};
    pipeLayoutInfo.setLayoutCount = 1;
    auto rawLayout = m_DebugCompositeDescriptorSetLayout.get();
    pipeLayoutInfo.pSetLayouts = &rawLayout;
    pipeLayoutInfo.pushConstantRangeCount = 1;
    pipeLayoutInfo.pPushConstantRanges = &pushRange;
    m_DebugCompositePipelineLayout = m_Device.createPipelineLayoutUnique(pipeLayoutInfo);

    auto code = ReadFile(m_DebugCompositeSpvPath);
    vk::ShaderModuleCreateInfo moduleInfo{};
    moduleInfo.codeSize = code.size();
    moduleInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    m_DebugCompositeShaderModule = m_Device.createShaderModuleUnique(moduleInfo);

    vk::ComputePipelineCreateInfo pipeInfo{};
    pipeInfo.layout = m_DebugCompositePipelineLayout.get();
    pipeInfo.stage.stage = vk::ShaderStageFlagBits::eCompute;
    pipeInfo.stage.module = m_DebugCompositeShaderModule.get();
    pipeInfo.stage.pName = "main";

    auto result = m_Device.createComputePipelineUnique(nullptr, pipeInfo);
    if (result.result != vk::Result::eSuccess) {
        throw std::runtime_error("[Astral::SDFRenderer] DebugComposite Compute pipeline olusturulamadi!");
    }
    m_DebugCompositePipeline = std::move(result.value);
}

void SDFRenderer::CreateDeferredLightingPipeline() {
    std::array<vk::DescriptorSetLayoutBinding, 9> bindings{};

    // 0: g_Albedo (rgba8)
    bindings[0].binding = 0;
    bindings[0].descriptorType = vk::DescriptorType::eStorageImage;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = vk::ShaderStageFlagBits::eCompute;

    // 1: g_Normal (rgba16f)
    bindings[1].binding = 1;
    bindings[1].descriptorType = vk::DescriptorType::eStorageImage;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = vk::ShaderStageFlagBits::eCompute;

    // 2: g_Material (rgba8)
    bindings[2].binding = 2;
    bindings[2].descriptorType = vk::DescriptorType::eStorageImage;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = vk::ShaderStageFlagBits::eCompute;

    // 3: g_Depth (r32f)
    bindings[3].binding = 3;
    bindings[3].descriptorType = vk::DescriptorType::eStorageImage;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = vk::ShaderStageFlagBits::eCompute;

    // 4: outColor (rgba16f, m_RawColorImage Linear HDR)
    bindings[4].binding = 4;
    bindings[4].descriptorType = vk::DescriptorType::eStorageImage;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = vk::ShaderStageFlagBits::eCompute;

    // 5: u_IrradianceMap (samplerCube)
    bindings[5].binding = 5;
    bindings[5].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    bindings[5].descriptorCount = 1;
    bindings[5].stageFlags = vk::ShaderStageFlagBits::eCompute;

    // 6: u_PrefilteredMap (samplerCube)
    bindings[6].binding = 6;
    bindings[6].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    bindings[6].descriptorCount = 1;
    bindings[6].stageFlags = vk::ShaderStageFlagBits::eCompute;

    // 7: u_BRDFLut (sampler2D)
    bindings[7].binding = 7;
    bindings[7].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    bindings[7].descriptorCount = 1;
    bindings[7].stageFlags = vk::ShaderStageFlagBits::eCompute;

    // 8: LightBuffer (SSBO)
    bindings[8].binding = 8;
    bindings[8].descriptorType = vk::DescriptorType::eStorageBuffer;
    bindings[8].descriptorCount = 1;
    bindings[8].stageFlags = vk::ShaderStageFlagBits::eCompute;

    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    m_DeferredLightingDescriptorSetLayout = m_Device.createDescriptorSetLayoutUnique(layoutInfo);

    vk::PushConstantRange pushRange{};
    pushRange.stageFlags = vk::ShaderStageFlagBits::eCompute;
    pushRange.offset = 0;
    pushRange.size = sizeof(DeferredLightingPushConstants);

    vk::PipelineLayoutCreateInfo pipeLayoutInfo{};
    pipeLayoutInfo.setLayoutCount = 1;
    auto rawLayout = m_DeferredLightingDescriptorSetLayout.get();
    pipeLayoutInfo.pSetLayouts = &rawLayout;
    pipeLayoutInfo.pushConstantRangeCount = 1;
    pipeLayoutInfo.pPushConstantRanges = &pushRange;
    m_DeferredLightingPipelineLayout = m_Device.createPipelineLayoutUnique(pipeLayoutInfo);

    auto code = ReadFile(m_DeferredLightingSpvPath);
    vk::ShaderModuleCreateInfo moduleInfo{};
    moduleInfo.codeSize = code.size();
    moduleInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    m_DeferredLightingShaderModule = m_Device.createShaderModuleUnique(moduleInfo);

    vk::ComputePipelineCreateInfo pipeInfo{};
    pipeInfo.layout = m_DeferredLightingPipelineLayout.get();
    pipeInfo.stage.stage = vk::ShaderStageFlagBits::eCompute;
    pipeInfo.stage.module = m_DeferredLightingShaderModule.get();
    pipeInfo.stage.pName = "main";

    auto result = m_Device.createComputePipelineUnique(nullptr, pipeInfo);
    if (result.result != vk::Result::eSuccess) {
        throw std::runtime_error("[Astral::SDFRenderer] DeferredLighting Compute pipeline olusturulamadi!");
    }
    m_DeferredLightingPipeline = std::move(result.value);
}

void SDFRenderer::CreateDescriptorPoolAndSets() {
    std::array<vk::DescriptorPoolSize, 4> poolSizes{};
    poolSizes[0].type = vk::DescriptorType::eStorageImage;
    poolSizes[0].descriptorCount = 48;
    poolSizes[1].type = vk::DescriptorType::eStorageBuffer;
    poolSizes[1].descriptorCount = 16;
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

    // 1. Raymarch Descriptor Set
    auto rawLayout = m_ComputePipeline->GetDescriptorSetLayout();
    vk::DescriptorSetAllocateInfo setAllocInfo(m_DescriptorPool.get(), 1, &rawLayout);
    auto sets = m_Device.allocateDescriptorSets(setAllocInfo);
    m_DescriptorSet = sets[0];

    // 2. TAA Descriptor Sets (Ping-Pong 0 & 1)
    auto taaLayout = m_TaaDescriptorSetLayout.get();
    for (uint32_t i = 0; i < 2; ++i) {
        vk::DescriptorSetAllocateInfo taaAllocInfo(m_DescriptorPool.get(), 1, &taaLayout);
        auto taaSets = m_Device.allocateDescriptorSets(taaAllocInfo);
        m_TaaDescriptorSet[i] = taaSets[0];
    }

    // 3. G-Buffer Descriptor Set
    auto gbufLayout = m_GBufferDescriptorSetLayout.get();
    vk::DescriptorSetAllocateInfo gbufAllocInfo(m_DescriptorPool.get(), 1, &gbufLayout);
    auto gbufSets = m_Device.allocateDescriptorSets(gbufAllocInfo);
    m_GBufferDescriptorSet = gbufSets[0];

    // 4. DebugComposite Descriptor Set
    auto debugLayout = m_DebugCompositeDescriptorSetLayout.get();
    vk::DescriptorSetAllocateInfo debugAllocInfo(m_DescriptorPool.get(), 1, &debugLayout);
    auto debugSets = m_Device.allocateDescriptorSets(debugAllocInfo);
    m_DebugCompositeDescriptorSet = debugSets[0];

    // 5. DeferredLighting Descriptor Set
    auto defLayout = m_DeferredLightingDescriptorSetLayout.get();
    vk::DescriptorSetAllocateInfo defAllocInfo(m_DescriptorPool.get(), 1, &defLayout);
    auto defSets = m_Device.allocateDescriptorSets(defAllocInfo);
    m_DeferredLightingDescriptorSet = defSets[0];

    UpdateDescriptorSets();
    UpdateGBufferDescriptorSets();
    UpdateDebugCompositeDescriptorSets();
    UpdateDeferredLightingDescriptorSets();
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

    // 2. TAA Setleri Guncelleme (Ping-Pong 0 ve 1)
    // TAA Set 0: curr=RawColor, hist=History[0] (Sampler), out=Storage, outHist=History[1], motion=GBufMotion
    // TAA Set 1: curr=RawColor, hist=History[1] (Sampler), out=Storage, outHist=History[0], motion=GBufMotion
    for (uint32_t i = 0; i < 2; ++i) {
        uint32_t readIdx = i;
        uint32_t writeIdx = 1 - i;

        vk::DescriptorImageInfo taaCurrInfo(nullptr, m_RawColorImageView.get(), vk::ImageLayout::eGeneral);
        vk::DescriptorImageInfo taaHistInfo(m_LinearClampSampler.get(), m_HistoryImageView[readIdx].get(), vk::ImageLayout::eGeneral);
        vk::DescriptorImageInfo taaOutInfo(nullptr, m_StorageImageView.get(), vk::ImageLayout::eGeneral);
        vk::DescriptorImageInfo taaOutHistInfo(nullptr, m_HistoryImageView[writeIdx].get(), vk::ImageLayout::eGeneral);
        vk::DescriptorImageInfo taaMotionInfo(nullptr, m_GBufMotionView.get(), vk::ImageLayout::eGeneral);

        std::array<vk::WriteDescriptorSet, 5> taaWriteSets{};
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

        m_Device.updateDescriptorSets(static_cast<uint32_t>(taaWriteSets.size()), taaWriteSets.data(), 0, nullptr);
    }
}

void SDFRenderer::UpdateGBufferDescriptorSets() {
    vk::DescriptorImageInfo albedoInfo(nullptr, m_GBufAlbedoView.get(), vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo normalInfo(nullptr, m_GBufNormalView.get(), vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo materialInfo(nullptr, m_GBufMaterialView.get(), vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo depthInfo(nullptr, m_GBufDepthView.get(), vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo motionInfo(nullptr, m_GBufMotionView.get(), vk::ImageLayout::eGeneral);

    auto editBufInfo = m_EditBuffer->GetDescriptorInfo();
    auto gridBufInfo = m_BrickGrid->GetBuffer()->GetDescriptorInfo();
    auto selBufInfo = m_SelectionBuffer->GetDescriptorInfo();
    auto camBufInfo = m_CameraUBO->GetDescriptorInfo();

    std::array<vk::WriteDescriptorSet, 9> writeSets{};
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

    m_Device.updateDescriptorSets(static_cast<uint32_t>(writeSets.size()), writeSets.data(), 0, nullptr);
}

void SDFRenderer::UpdateDebugCompositeDescriptorSets() {
    vk::DescriptorImageInfo albedoInfo(nullptr, m_GBufAlbedoView.get(), vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo normalInfo(nullptr, m_GBufNormalView.get(), vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo materialInfo(nullptr, m_GBufMaterialView.get(), vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo depthInfo(nullptr, m_GBufDepthView.get(), vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo motionInfo(nullptr, m_GBufMotionView.get(), vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo outInfo(nullptr, m_RawColorImageView.get(), vk::ImageLayout::eGeneral);

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
    if (!m_DeferredLightingDescriptorSet) return;

    vk::DescriptorImageInfo albedoInfo(nullptr, m_GBufAlbedoView.get(), vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo normalInfo(nullptr, m_GBufNormalView.get(), vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo materialInfo(nullptr, m_GBufMaterialView.get(), vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo depthInfo(nullptr, m_GBufDepthView.get(), vk::ImageLayout::eGeneral);
    vk::DescriptorImageInfo outColorInfo(nullptr, m_RawColorImageView.get(), vk::ImageLayout::eGeneral);

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

    auto lightBufInfo = m_LightBuffer->GetDescriptorInfo();

    std::array<vk::WriteDescriptorSet, 9> writeSets{};

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

    m_Device.updateDescriptorSets(static_cast<uint32_t>(writeSets.size()), writeSets.data(), 0, nullptr);
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
    UpdateGBufferDescriptorSets();
    UpdateDebugCompositeDescriptorSets();
    UpdateDeferredLightingDescriptorSets();
    m_HistoryInitialized = false;
}

void SDFRenderer::Render(vk::CommandBuffer cmd, float time, uint32_t normalMode, int width, int height,
                         bool useGrid, bool optShadow, bool enableTAA, uint32_t frameIndex) {
    (void)width;
    (void)height;

    uint32_t groupX = static_cast<uint32_t>(std::ceil(static_cast<float>(m_Width) / 8.0f));
    uint32_t groupY = static_cast<uint32_t>(std::ceil(static_cast<float>(m_Height) / 8.0f));

    // Kamera matrisleri dışarıdan beslenmediyse varsayılan değerlerle başlat
    if (!m_CameraMatricesInitialized) {
        glm::vec3 camPos = glm::vec3(0.0f, 1.5f, 4.0f);
        glm::vec3 camDir = glm::normalize(glm::vec3(0.0f, -0.25f, -1.0f));
        glm::mat4 view = glm::lookAt(camPos, camPos + camDir, glm::vec3(0.0f, 1.0f, 0.0f));
        float fovY = 2.0f * std::atan(0.5f / 1.5f);
        float aspect = (m_Height > 0) ? (static_cast<float>(m_Width) / static_cast<float>(m_Height)) : (16.0f / 9.0f);
        glm::mat4 proj = glm::perspective(fovY, aspect, 0.1f, 100.0f);
        SetCameraMatrices(view, proj, glm::vec2(0.0f));
    }

    if (m_UseGBuffer) {
        // =========================================================================
        // 1. G-Buffer Compute Pass (SDFGBuffer.glsl)
        // =========================================================================
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
            makeGBufBarrier(m_GBufAlbedo.get()),
            makeGBufBarrier(m_GBufNormal.get()),
            makeGBufBarrier(m_GBufMaterial.get()),
            makeGBufBarrier(m_GBufDepth.get()),
            makeGBufBarrier(m_GBufMotion.get())
        };

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eComputeShader,
            {},
            0, nullptr,
            0, nullptr,
            static_cast<uint32_t>(gbufBarriers.size()), gbufBarriers.data()
        );

        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_GBufferPipeline.get());
        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute,
            m_GBufferPipelineLayout.get(),
            0,
            1, &m_GBufferDescriptorSet,
            0, nullptr
        );

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

        glm::vec2 jitter = enableTAA ? HALTON_8[frameIndex % 8] : glm::vec2(0.0f);
        pushConstants.taaParams = glm::vec4(jitter.x, jitter.y, enableTAA ? 1.0f : 0.0f, 0.12f);

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
            m_GBufferPipelineLayout.get(),
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
        // 2. Barrier: G-Buffer ShaderWrite -> ShaderRead & m_RawColorImage ShaderWrite
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
        rawColorWriteBarrier.image = m_RawColorImage.get();
        rawColorWriteBarrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
        rawColorWriteBarrier.srcAccessMask = {};
        rawColorWriteBarrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite;

        std::array<vk::ImageMemoryBarrier, 6> toCompositeBarriers = {
            makeGBufReadBarrier(m_GBufAlbedo.get()),
            makeGBufReadBarrier(m_GBufNormal.get()),
            makeGBufReadBarrier(m_GBufMaterial.get()),
            makeGBufReadBarrier(m_GBufDepth.get()),
            makeGBufReadBarrier(m_GBufMotion.get()),
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

        bool isDebugActive = (m_DebugMode != 0);

        if (isDebugActive) {
            // =========================================================================
            // 3a. Debug Composite Pass (SDFDebugComposite.glsl -> m_RawColorImage)
            // =========================================================================
            cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_DebugCompositePipeline.get());
            cmd.bindDescriptorSets(
                vk::PipelineBindPoint::eCompute,
                m_DebugCompositePipelineLayout.get(),
                0,
                1, &m_DebugCompositeDescriptorSet,
                0, nullptr
            );

            DebugCompositePushConstants debugPush{};
            debugPush.screenRes = glm::vec4(
                static_cast<float>(m_Width),
                static_cast<float>(m_Height),
                static_cast<float>(m_DebugMode),
                0.0f
            );

            cmd.pushConstants(
                m_DebugCompositePipelineLayout.get(),
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
            cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_DeferredLightingPipeline.get());
            cmd.bindDescriptorSets(
                vk::PipelineBindPoint::eCompute,
                m_DeferredLightingPipelineLayout.get(),
                0,
                1, &m_DeferredLightingDescriptorSet,
                0, nullptr
            );

            DeferredLightingPushConstants defPush{};
            glm::vec3 camPos = glm::vec3(0.0f, 1.5f, 4.0f);
            glm::vec3 camDir = glm::normalize(glm::vec3(0.0f, -0.25f, -1.0f));
            defPush.camPos = glm::vec4(camPos, static_cast<float>(m_IBLManager->GetPrefilteredMipLevels()));
            defPush.camDir = glm::vec4(camDir, 1.0f); // xyz: dir, w: exposure = 1.0
            defPush.screenRes = glm::vec4(static_cast<float>(m_Width), static_cast<float>(m_Height), 1.0f, 0.0f); // z: iblIntensity = 1.0

            cmd.pushConstants(
                m_DeferredLightingPipelineLayout.get(),
                vk::ShaderStageFlagBits::eCompute,
                0,
                sizeof(DeferredLightingPushConstants),
                &defPush
            );

            cmd.dispatch(groupX, groupY, 1);
        }

    } else {
        // =========================================================================
        // Legacy Monolithic Mode (SDFCompute.glsl -> m_RawColorImage)
        // =========================================================================
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

        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_ComputePipeline->GetPipeline());
        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute,
            m_ComputePipeline->GetPipelineLayout(),
            0,
            1, &m_DescriptorSet,
            0, nullptr
        );

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

        glm::vec2 jitter = enableTAA ? HALTON_8[frameIndex % 8] : glm::vec2(0.0f);
        pushConstants.taaParams = glm::vec4(jitter.x, jitter.y, enableTAA ? 1.0f : 0.0f, 0.12f);

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
    }

    // =========================================================================
    // 5. TAA Resolve & ACES Tonemapping Pass (Dogrusal HDR -> sRGB)
    // =========================================================================
    bool isDebugActive = (m_UseGBuffer && m_DebugMode != 0);
    uint32_t readIdx = m_HistoryPingPong;
    uint32_t writeIdx = 1 - m_HistoryPingPong;

    // Barrier: m_RawColorImage ShaderWrite -> ShaderRead
    vk::ImageMemoryBarrier toTaaBarrier{};
    toTaaBarrier.oldLayout = vk::ImageLayout::eGeneral;
    toTaaBarrier.newLayout = vk::ImageLayout::eGeneral;
    toTaaBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTaaBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTaaBarrier.image = m_RawColorImage.get();
    toTaaBarrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
    toTaaBarrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
    toTaaBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    // m_StorageImage yazmaya hazirla
    vk::ImageMemoryBarrier storageBarrier = toTaaBarrier;
    storageBarrier.image = m_StorageImage.get();
    storageBarrier.oldLayout = vk::ImageLayout::eGeneral;
    storageBarrier.newLayout = vk::ImageLayout::eGeneral;
    storageBarrier.srcAccessMask = {};
    storageBarrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite;

    // Tarihce okuma (readIdx)
    vk::ImageMemoryBarrier histReadBarrier = toTaaBarrier;
    histReadBarrier.image = m_HistoryImage[readIdx].get();
    histReadBarrier.oldLayout = vk::ImageLayout::eGeneral;
    histReadBarrier.newLayout = vk::ImageLayout::eGeneral;
    histReadBarrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
    histReadBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    // Tarihce yazma (writeIdx)
    bool isFirstHistory = !m_HistoryInitialized || (frameIndex == 0);
    vk::ImageMemoryBarrier histWriteBarrier = toTaaBarrier;
    histWriteBarrier.image = m_HistoryImage[writeIdx].get();
    histWriteBarrier.oldLayout = vk::ImageLayout::eGeneral;
    histWriteBarrier.newLayout = vk::ImageLayout::eGeneral;
    histWriteBarrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
    histWriteBarrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite;
    m_HistoryInitialized = true;

    std::array<vk::ImageMemoryBarrier, 4> taaBarriers = { toTaaBarrier, storageBarrier, histReadBarrier, histWriteBarrier };
    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eComputeShader,
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
        1, &m_TaaDescriptorSet[readIdx],
        0, nullptr
    );

    TAAPushConstants taaPush{};
    float blendAlpha = 0.12f;
    if (isDebugActive) {
        blendAlpha = -1.0f; // Debug bypass modu: tonemap ve gamma uygulamadan ham veri aktarimi
    } else if (!enableTAA) {
        blendAlpha = 1.0f;  // TAA kapali: tarihcesiz ACES tonemap ve sRGB gamma
    }

    taaPush.screenRes = glm::vec4(
        static_cast<float>(m_Width),
        static_cast<float>(m_Height),
        (isFirstHistory || !enableTAA || isDebugActive) ? 0.0f : static_cast<float>(frameIndex),
        blendAlpha
    );

    cmd.pushConstants(
        m_TaaPipelineLayout.get(),
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
    storageReadyBarrier.image = m_StorageImage.get();
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

    if (m_CameraUBO && m_CameraUBO->GetMappedData()) {
        CameraUBOData uboData{};
        uboData.currViewProj = m_CurrViewProj;
        uboData.prevViewProj = m_PrevViewProj;
        std::memcpy(m_CameraUBO->GetMappedData(), &uboData, sizeof(CameraUBOData));
    }
}

} // namespace Astral
