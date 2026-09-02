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
#include <array>
#include "Astral/Renderer/SDFEdit.hpp"
#include "Astral/Renderer/BrickGrid.hpp"

namespace Astral {

class VulkanContext;
class ComputePipeline;
class Buffer;

class SDFRenderer {
public:
    static constexpr size_t MAX_EDITS = 256;

    SDFRenderer(VulkanContext& context, const std::string& spvPath, int width, int height, bool persistentMap = true, const std::string& taaSpvPath = "");
    ~SDFRenderer();

    SDFRenderer(const SDFRenderer&) = delete;
    SDFRenderer& operator=(const SDFRenderer&) = delete;

    /// Dinamik primitifleri GPU SSBO'ya aktarir ve Two-Level BrickGrid'i gunceller
    void UpdateEdits(const std::vector<SDFEditGPU>& edits, bool useLegacyMapUnmap = false);

    void Render(vk::CommandBuffer cmd, float time, uint32_t normalMode, int width, int height,
                bool useGrid = true, bool optShadow = true, bool enableTAA = true, uint32_t frameIndex = 0);
    void Resize(int width, int height);

    struct SelectionResult {
        int32_t hitIndex = -1;
        bool hasHit = false;
        glm::vec3 hitPoint{0.0f};
        float hitDistance = 0.0f;
    };

    /// Fare tiklamasiyla piksel secim istegi gonderir
    void SetPickingRequest(int mouseX, int mouseY);

    /// GPU compute sonrasi fence-senkronize secim sonucunu dondurur
    SelectionResult GetSelectionResult() const;

    vk::Image GetStorageImage() const { return m_StorageImage.get(); }
    vk::ImageView GetStorageImageView() const { return m_StorageImageView.get(); }
    Buffer* GetEditBuffer() const { return m_EditBuffer.get(); }
    Buffer* GetSelectionBuffer() const { return m_SelectionBuffer.get(); }
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
    std::unique_ptr<Buffer> m_SelectionBuffer;
    std::unique_ptr<BrickGrid> m_BrickGrid;

    bool m_PickingRequested = false;
    int m_PickingMouseX = -1;
    int m_PickingMouseY = -1;

    // TAA Pipeline (PR-8)
    std::string m_TaaSpvPath;
    vk::UniqueShaderModule m_TaaShaderModule;
    vk::UniqueDescriptorSetLayout m_TaaDescriptorSetLayout;
    vk::UniquePipelineLayout m_TaaPipelineLayout;
    vk::UniquePipeline m_TAAPipeline;
    vk::DescriptorSet m_TaaDescriptorSet;

    // Render Hedefleri
    vk::UniqueImage m_StorageImage;         // Son cikan goruntu (Swapchain'e blit edilen)
    vk::UniqueDeviceMemory m_StorageImageMemory;
    vk::UniqueImageView m_StorageImageView;

    vk::UniqueImage m_RawColorImage;        // Raymarching ham ciktisi
    vk::UniqueDeviceMemory m_RawColorImageMemory;
    vk::UniqueImageView m_RawColorImageView;

    vk::UniqueImage m_HistoryImage;         // Onceki kare birikim tamponu
    vk::UniqueDeviceMemory m_HistoryImageMemory;
    vk::UniqueImageView m_HistoryImageView;

    vk::UniqueDescriptorPool m_DescriptorPool;
    vk::DescriptorSet m_DescriptorSet;      // Raymarching (rawColor, edits, grid)

    void CreateImages();
    void CleanupImages();
    void CreateEditBuffer(bool persistentMap);
    void CreateDescriptorPoolAndSets();
    void UpdateDescriptorSets();
    void CreateTAAPipeline();
    void CreateTexture(vk::UniqueImage& img, vk::UniqueDeviceMemory& mem, vk::UniqueImageView& view, vk::ImageUsageFlags usage);
};

} // namespace Astral
