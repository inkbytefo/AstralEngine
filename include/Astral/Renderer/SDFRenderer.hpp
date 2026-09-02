#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include <vulkan/vulkan.hpp>
#include <memory>
#include <string>
#include <vector>
#include "Astral/Renderer/SDFEdit.hpp"
#include "Astral/Renderer/BrickGrid.hpp"

namespace Astral {

class VulkanContext;
class ComputePipeline;
class Buffer;

class SDFRenderer {
public:
    static constexpr size_t MAX_EDITS = 256;

    SDFRenderer(VulkanContext& context, const std::string& spvPath, int width, int height, bool persistentMap = true);
    ~SDFRenderer();

    SDFRenderer(const SDFRenderer&) = delete;
    SDFRenderer& operator=(const SDFRenderer&) = delete;

    /// Dinamik primitifleri GPU SSBO'ya aktarir ve Two-Level BrickGrid'i gunceller
    void UpdateEdits(const std::vector<SDFEditGPU>& edits, bool useLegacyMapUnmap = false);

    void Render(vk::CommandBuffer cmd, float time, uint32_t normalMode, int width, int height, bool useGrid = true);
    void Resize(int width, int height);

    vk::Image GetStorageImage() const { return m_StorageImage.get(); }
    vk::ImageView GetStorageImageView() const { return m_StorageImageView.get(); }
    Buffer* GetEditBuffer() const { return m_EditBuffer.get(); }
    BrickGrid* GetBrickGrid() const { return m_BrickGrid.get(); }
    size_t GetActiveEditCount() const { return m_ActiveEditCount; }

private:
    VulkanContext& m_Context;
    vk::Device m_Device;
    vk::PhysicalDevice m_PhysicalDevice;

    int m_Width = 1280;
    int m_Height = 720;
    size_t m_ActiveEditCount = 0;

    std::unique_ptr<ComputePipeline> m_ComputePipeline;
    std::unique_ptr<Buffer> m_EditBuffer;
    std::unique_ptr<BrickGrid> m_BrickGrid;

    vk::UniqueImage m_StorageImage;
    vk::UniqueDeviceMemory m_StorageImageMemory;
    vk::UniqueImageView m_StorageImageView;

    vk::UniqueDescriptorPool m_DescriptorPool;
    vk::DescriptorSet m_DescriptorSet;

    void CreateStorageImage();
    void CreateEditBuffer(bool persistentMap);
    void CreateDescriptorPoolAndSet();
    void UpdateDescriptorSets();
    void CleanupStorageImage();
};

} // namespace Astral
