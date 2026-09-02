#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include "Astral/Renderer/SDFRenderer.hpp"
#include "Astral/Renderer/VulkanContext.hpp"
#include "Astral/Renderer/ComputePipeline.hpp"
#include "Astral/Renderer/Buffer.hpp"
#include "Astral/Renderer/BrickGrid.hpp"

#include <iostream>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <array>

namespace Astral {

static uint32_t FindMemoryType(vk::PhysicalDevice physDev, uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
    auto memProps = physDev.getMemoryProperties();
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("[Astral::SDFRenderer] Uygun bellek tipi bulunamadi!");
}

SDFRenderer::SDFRenderer(VulkanContext& context, const std::string& spvPath, int width, int height, bool persistentMap)
    : m_Context(context),
      m_Device(context.GetDevice()),
      m_PhysicalDevice(context.GetPhysicalDevice()),
      m_Width(width),
      m_Height(height) {

    m_ComputePipeline = std::make_unique<ComputePipeline>(m_Device, spvPath);
    CreateStorageImage();
    CreateEditBuffer(persistentMap);
    m_BrickGrid = std::make_unique<BrickGrid>(m_Device, m_PhysicalDevice);
    CreateDescriptorPoolAndSet();

    std::cout << "[Astral::SDFRenderer] SDF Renderer basariyla baslatildi (" << m_Width << "x" << m_Height 
              << ", EditBuffer: " << (MAX_EDITS * sizeof(SDFEditGPU)) / 1024 << " KB"
              << ", Two-Level BrickGrid aktif).\n";
}

SDFRenderer::~SDFRenderer() {
    m_Device.waitIdle();
    m_DescriptorPool.reset();
    m_BrickGrid.reset();
    m_EditBuffer.reset();
    CleanupStorageImage();
    m_ComputePipeline.reset();
}

void SDFRenderer::CleanupStorageImage() {
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

void SDFRenderer::UpdateEdits(const std::vector<SDFEditGPU>& edits, bool useLegacyMapUnmap) {
    m_ActiveEditCount = std::min(edits.size(), MAX_EDITS);
    if (m_ActiveEditCount == 0) return;

    size_t uploadBytes = m_ActiveEditCount * sizeof(SDFEditGPU);
    if (useLegacyMapUnmap) {
        m_EditBuffer->UpdateDataLegacy(edits.data(), uploadBytes);
    } else {
        m_EditBuffer->UpdateData(edits.data(), uploadBytes);
    }

    // Two-Level Spatial Grid'i guncelle (Empty Space Skipping icin)
    if (m_BrickGrid) {
        m_BrickGrid->Build(edits);
    }
}

void SDFRenderer::CreateStorageImage() {
    vk::ImageCreateInfo imageInfo{};
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.extent = vk::Extent3D(static_cast<uint32_t>(m_Width), static_cast<uint32_t>(m_Height), 1);
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = vk::Format::eR8G8B8A8Unorm;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageInfo.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;

    m_StorageImage = m_Device.createImageUnique(imageInfo);

    auto memReq = m_Device.getImageMemoryRequirements(m_StorageImage.get());
    uint32_t memTypeIndex = FindMemoryType(
        m_PhysicalDevice,
        memReq.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eDeviceLocal
    );

    vk::MemoryAllocateInfo allocInfo(memReq.size, memTypeIndex);
    m_StorageImageMemory = m_Device.allocateMemoryUnique(allocInfo);
    m_Device.bindImageMemory(m_StorageImage.get(), m_StorageImageMemory.get(), 0);

    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image = m_StorageImage.get();
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = vk::Format::eR8G8B8A8Unorm;
    viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    m_StorageImageView = m_Device.createImageViewUnique(viewInfo);
}

void SDFRenderer::CreateDescriptorPoolAndSet() {
    std::array<vk::DescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = vk::DescriptorType::eStorageImage;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = vk::DescriptorType::eStorageBuffer;
    poolSizes[1].descriptorCount = 2; // EditBuffer (Binding 1) + GridBuffer (Binding 2)

    vk::DescriptorPoolCreateInfo poolInfo(
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        1,
        static_cast<uint32_t>(poolSizes.size()),
        poolSizes.data()
    );
    m_DescriptorPool = m_Device.createDescriptorPoolUnique(poolInfo);

    auto rawLayout = m_ComputePipeline->GetDescriptorSetLayout();
    vk::DescriptorSetAllocateInfo setAllocInfo(m_DescriptorPool.get(), 1, &rawLayout);
    auto sets = m_Device.allocateDescriptorSets(setAllocInfo);
    m_DescriptorSet = sets[0];

    UpdateDescriptorSets();
}

void SDFRenderer::UpdateDescriptorSets() {
    vk::DescriptorImageInfo imgInfo(nullptr, m_StorageImageView.get(), vk::ImageLayout::eGeneral);
    auto editBufInfo = m_EditBuffer->GetDescriptorInfo();
    auto gridBufInfo = m_BrickGrid->GetBuffer()->GetDescriptorInfo();

    std::array<vk::WriteDescriptorSet, 3> writeSets{};
    // Binding 0: Storage Image (outImage)
    writeSets[0].dstSet = m_DescriptorSet;
    writeSets[0].dstBinding = 0;
    writeSets[0].dstArrayElement = 0;
    writeSets[0].descriptorCount = 1;
    writeSets[0].descriptorType = vk::DescriptorType::eStorageImage;
    writeSets[0].pImageInfo = &imgInfo;

    // Binding 1: Storage Buffer (EditBuffer SSBO)
    writeSets[1].dstSet = m_DescriptorSet;
    writeSets[1].dstBinding = 1;
    writeSets[1].dstArrayElement = 0;
    writeSets[1].descriptorCount = 1;
    writeSets[1].descriptorType = vk::DescriptorType::eStorageBuffer;
    writeSets[1].pBufferInfo = &editBufInfo;

    // Binding 2: Storage Buffer (GridBuffer SSBO - PR-6)
    writeSets[2].dstSet = m_DescriptorSet;
    writeSets[2].dstBinding = 2;
    writeSets[2].dstArrayElement = 0;
    writeSets[2].descriptorCount = 1;
    writeSets[2].descriptorType = vk::DescriptorType::eStorageBuffer;
    writeSets[2].pBufferInfo = &gridBufInfo;

    m_Device.updateDescriptorSets(
        static_cast<uint32_t>(writeSets.size()),
        writeSets.data(),
        0, nullptr
    );
}

void SDFRenderer::Resize(int width, int height) {
    if (width <= 0 || height <= 0 || (width == m_Width && height == m_Height)) return;

    m_Device.waitIdle();
    m_Width = width;
    m_Height = height;

    CleanupStorageImage();
    CreateStorageImage();
    UpdateDescriptorSets();
}

void SDFRenderer::Render(vk::CommandBuffer cmd, float time, uint32_t normalMode, int width, int height, bool useGrid) {
    if (width != m_Width || height != m_Height) {
        Resize(width, height);
    }

    // 1. Pipeline Barrier: Storage image'i eGeneral durumuna gecir
    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout = vk::ImageLayout::eUndefined;
    barrier.newLayout = vk::ImageLayout::eGeneral;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_StorageImage.get();
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = {};
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite;

    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eComputeShader,
        {},
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    // 2. Compute Pipeline ve Descriptors bagla
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_ComputePipeline->GetPipeline());
    cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute,
        m_ComputePipeline->GetPipelineLayout(),
        0,
        1, &m_DescriptorSet,
        0, nullptr
    );

    // 3. Push Constants aktar
    SDFPushConstants pushConstants{};
    pushConstants.camPos = glm::vec4(0.0f, 1.5f, 4.0f, time);
    pushConstants.camDir = glm::vec4(0.0f, -0.25f, -1.0f, static_cast<float>(normalMode));
    // screenRes: x=width, y=height, z=editCount, w=useGrid (1.0 = aktif, 0.0 = kapali)
    pushConstants.screenRes = glm::vec4(
        static_cast<float>(m_Width),
        static_cast<float>(m_Height),
        static_cast<float>(m_ActiveEditCount),
        useGrid ? 1.0f : 0.0f
    );
    // gridParams: x=dimX (32), y=dimY (16), z=dimZ (32), w=cellSize (0.75)
    pushConstants.gridParams = m_BrickGrid->GetGridParams();

    cmd.pushConstants(
        m_ComputePipeline->GetPipelineLayout(),
        vk::ShaderStageFlagBits::eCompute,
        0,
        sizeof(SDFPushConstants),
        &pushConstants
    );

    // 4. Dispatch (8x8 workgroup)
    uint32_t groupX = static_cast<uint32_t>(std::ceil(static_cast<float>(m_Width) / 8.0f));
    uint32_t groupY = static_cast<uint32_t>(std::ceil(static_cast<float>(m_Height) / 8.0f));
    cmd.dispatch(groupX, groupY, 1);

    // 5. Compute bitisi senkronizasyonu
    vk::ImageMemoryBarrier endBarrier = barrier;
    endBarrier.oldLayout = vk::ImageLayout::eGeneral;
    endBarrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    endBarrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
    endBarrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eTransfer,
        {},
        0, nullptr,
        0, nullptr,
        1, &endBarrier
    );
}

} // namespace Astral
