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
#include "Astral/Renderer/VulkanContext.hpp"
#include "Astral/Renderer/IBLManager.hpp"
#include "Astral/Renderer/ComputePipeline.hpp"

namespace Astral {

class ComputePipeline;
class Buffer;

class SDFRenderer {
public:
    static constexpr size_t MAX_EDITS = MAX_SDF_EDITS;

    SDFRenderer(VulkanContext& context, const std::string& spvPath, int width, int height,
                bool persistentMap = true, const std::string& taaSpvPath = "",
                const std::string& gbufferSpvPath = "", const std::string& debugCompositeSpvPath = "",
                const std::string& deferredLightingSpvPath = "");
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

    /// GPU compute sonrasi bekleyen secim sonucu olup olmadigini dondurur
    [[nodiscard]] bool HasPendingSelection() const noexcept { return m_PickPendingRead; }

    /// GPU compute sonrasi fence-senkronize secim sonucunu dondurur ve sonucu tuketir (tek seferlik okuma)
    SelectionResult ConsumeSelectionResult();

    /// GPU compute sonrasi fence-senkronize secim sonucunu dondurur (okuma sonrasi sifirlamaz)
    SelectionResult GetSelectionResult() const;

    /// Secim tamponunu sifirlar (-1 yazar)
    void ClearSelectionResult();

    /// Kamera View ve Projection matrislerini ve TAA jitter ofsetini gunceller (Motion Vectors icin)
    void SetCameraMatrices(const glm::mat4& view, const glm::mat4& proj, const glm::vec2& jitter = glm::vec2(0.0f));

    /// G-Buffer modunu aktif/pasif yapar
    void SetUseGBuffer(bool enabled) noexcept { m_UseGBuffer = enabled; }
    [[nodiscard]] bool IsUsingGBuffer() const noexcept { return m_UseGBuffer; }

    /// G-Buffer onizleme modunu ayarlar (0: Shaded, 1: Albedo, 2: Normal, 3: Depth, 4: Motion, 5: Material)
    void SetDebugMode(int mode) noexcept { m_DebugMode = mode; }
    [[nodiscard]] int GetDebugMode() const noexcept { return m_DebugMode; }

    // G-Buffer Render Hedefleri Getter'lari
    [[nodiscard]] vk::Image GetGBufferAlbedo() const noexcept { return m_GBufAlbedo.image; }
    [[nodiscard]] vk::ImageView GetGBufferAlbedoView() const noexcept { return m_GBufAlbedoView.get(); }
    [[nodiscard]] vk::Image GetGBufferNormal() const noexcept { return m_GBufNormal.image; }
    [[nodiscard]] vk::ImageView GetGBufferNormalView() const noexcept { return m_GBufNormalView.get(); }
    [[nodiscard]] vk::Image GetGBufferMaterial() const noexcept { return m_GBufMaterial.image; }
    [[nodiscard]] vk::ImageView GetGBufferMaterialView() const noexcept { return m_GBufMaterialView.get(); }
    [[nodiscard]] vk::Image GetGBufferDepth() const noexcept { return m_GBufDepth.image; }
    [[nodiscard]] vk::ImageView GetGBufferDepthView() const noexcept { return m_GBufDepthView.get(); }
    [[nodiscard]] vk::Image GetGBufferMotion() const noexcept { return m_GBufMotion.image; }
    [[nodiscard]] vk::ImageView GetGBufferMotionView() const noexcept { return m_GBufMotionView.get(); }

    vk::Image GetStorageImage() const { return m_StorageImage; }
    vk::ImageView GetStorageImageView() const { return m_StorageImageView.get(); }
    Buffer* GetEditBuffer() const { return m_EditBuffer.get(); }
    Buffer* GetSelectionBuffer() const { return m_SelectionBuffer.get(); }
    Buffer* GetCameraUBO() const { return m_CameraUBO.get(); }
    BrickGrid* GetBrickGrid() const { return m_BrickGrid.get(); }
    size_t GetActiveEditCount() const { return m_ActiveEditCount; }

    [[nodiscard]] int GetWidth() const noexcept { return m_Width; }
    [[nodiscard]] int GetHeight() const noexcept { return m_Height; }

    /// Secili nesne Fresnel Rim-Light vurgusu icin hit indeksini ayarlar (-1 = secim yok)
    void SetSelectedHitIndex(int hitIndex) noexcept { m_SelectedHitIndex = hitIndex; }
    [[nodiscard]] int GetSelectedHitIndex() const noexcept { return m_SelectedHitIndex; }

    /// Viewport ornekleyicisi getter'i (Editor ImGui doku kaydini kendisi yapar)
    [[nodiscard]] vk::Sampler GetViewportSampler() const noexcept { return m_ViewportSampler.get(); }

    // IBL & Lights API (Faz 2)
    [[nodiscard]] IBLManager* GetIBLManager() const noexcept { return m_IBLManager.get(); }
    void SetLights(const std::vector<LightGPU>& lights);
    [[nodiscard]] std::vector<LightGPU>& GetLights() noexcept { return m_Lights; }
    [[nodiscard]] const std::vector<LightGPU>& GetLights() const noexcept { return m_Lights; }

private:
    vk::UniqueSampler m_ViewportSampler;
    vk::UniqueSampler m_LinearClampSampler; // TAA alt-piksel Catmull-Rom ornekleyicisi
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
    bool m_PickPendingRead = false;
    int m_PickingMouseX = -1;
    int m_PickingMouseY = -1;
    int m_SelectedHitIndex = -1;
    bool m_HistoryInitialized = false;

    // G-Buffer Pipeline & Degiskenleri (Faz 1)
    bool m_UseGBuffer = false;
    int m_DebugMode = 0;

    // G-Buffer Render Hedefleri (VMA)
    VmaImage m_GBufAlbedo;       // VK_FORMAT_R8G8B8A8_UNORM
    vk::UniqueImageView m_GBufAlbedoView;

    VmaImage m_GBufNormal;       // VK_FORMAT_R16G16B16A16_SFLOAT
    vk::UniqueImageView m_GBufNormalView;

    VmaImage m_GBufMaterial;     // VK_FORMAT_R8G8B8A8_UNORM
    vk::UniqueImageView m_GBufMaterialView;

    VmaImage m_GBufDepth;        // VK_FORMAT_R32_SFLOAT
    vk::UniqueImageView m_GBufDepthView;

    VmaImage m_GBufMotion;       // VK_FORMAT_R16G16_SFLOAT
    vk::UniqueImageView m_GBufMotionView;

    // Camera Matrices & Camera UBO (Motion Vectors)
    std::unique_ptr<Buffer> m_CameraUBO;
    glm::mat4 m_CurrViewProj{1.0f};
    glm::mat4 m_PrevViewProj{1.0f};
    glm::vec2 m_CurrJitter{0.0f};
    glm::vec2 m_PrevJitter{0.0f};
    bool m_CameraMatricesInitialized = false;

    // G-Buffer Compute Pipeline
    std::string m_GBufferSpvPath;
    vk::UniqueShaderModule m_GBufferShaderModule;
    vk::UniqueDescriptorSetLayout m_GBufferDescriptorSetLayout;
    vk::UniquePipelineLayout m_GBufferPipelineLayout;
    vk::UniquePipeline m_GBufferPipeline;
    vk::DescriptorSet m_GBufferDescriptorSet;

    // Debug Composite Pipeline
    std::string m_DebugCompositeSpvPath;
    vk::UniqueShaderModule m_DebugCompositeShaderModule;
    vk::UniqueDescriptorSetLayout m_DebugCompositeDescriptorSetLayout;
    vk::UniquePipelineLayout m_DebugCompositePipelineLayout;
    vk::UniquePipeline m_DebugCompositePipeline;
    vk::DescriptorSet m_DebugCompositeDescriptorSet;

    // Deferred Lighting Pipeline (Faz 2)
    std::string m_DeferredLightingSpvPath;
    vk::UniqueShaderModule m_DeferredLightingShaderModule;
    vk::UniqueDescriptorSetLayout m_DeferredLightingDescriptorSetLayout;
    vk::UniquePipelineLayout m_DeferredLightingPipelineLayout;
    vk::UniquePipeline m_DeferredLightingPipeline;
    vk::DescriptorSet m_DeferredLightingDescriptorSet;

    std::unique_ptr<IBLManager> m_IBLManager;
    std::unique_ptr<Buffer> m_LightBuffer;
    std::vector<LightGPU> m_Lights;

    // TAA Pipeline (PR-8)
    std::string m_TaaSpvPath;
    vk::UniqueShaderModule m_TaaShaderModule;
    vk::UniqueDescriptorSetLayout m_TaaDescriptorSetLayout;
    vk::UniquePipelineLayout m_TaaPipelineLayout;
    vk::UniquePipeline m_TAAPipeline;
    std::array<vk::DescriptorSet, 2> m_TaaDescriptorSet;

    // Render Hedefleri (VMA ile yonetilir)
    VmaImage m_StorageImage;         // Son cikan goruntu (Swapchain'e blit edilen, RGBA8)
    vk::UniqueImageView m_StorageImageView;

    VmaImage m_RawColorImage;        // Raymarching / Deferred Lighting ciktisi (Linear HDR, RGBA16F)
    vk::UniqueImageView m_RawColorImageView;

    std::array<VmaImage, 2> m_HistoryImage;         // Ping-pong tarihce tamponlari (Linear HDR, RGBA16F)
    std::array<vk::UniqueImageView, 2> m_HistoryImageView;
    uint32_t m_HistoryPingPong = 0;

    vk::UniqueDescriptorPool m_DescriptorPool;
    vk::DescriptorSet m_DescriptorSet;      // Raymarching (rawColor, edits, grid, selection)

    void CreateImages();
    void CleanupImages();
    void CreateEditBuffer(bool persistentMap);
    void CreateCameraUBO();
    void CreateLightBuffer();
    void UpdateLights();
    void CreateDescriptorPoolAndSets();
    void UpdateDescriptorSets();
    void UpdateGBufferDescriptorSets();
    void UpdateDebugCompositeDescriptorSets();
    void UpdateDeferredLightingDescriptorSets();
    void CreateTAAPipeline();
    void CreateGBufferPipeline();
    void CreateDebugCompositePipeline();
    void CreateDeferredLightingPipeline();
    void CreateTexture(VmaImage& img, vk::UniqueImageView& view, vk::ImageUsageFlags usage);
    void CreateGBufferTexture(VmaImage& img, vk::UniqueImageView& view, vk::Format format, vk::ImageUsageFlags usage);
};

} // namespace Astral
