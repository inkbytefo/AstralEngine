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
#include "Astral/Renderer/ShaderInterop.hpp"
#include "Astral/Renderer/RenderCamera.hpp"
#include "Astral/Renderer/SDFTemporalHistory.hpp"
#include "Astral/Geometry/SDFChangeSet.hpp"
#include "Astral/Geometry/SDFSceneSnapshot.hpp"
#include "Astral/Renderer/QualitySettings.hpp"
#include "Astral/Renderer/RenderFrameSettings.hpp"
#include "Astral/Renderer/ComputeProgram.hpp"
#include "Astral/Renderer/RenderTargets.hpp"
#include "Astral/Renderer/SceneGpuData.hpp"
#include <span>
#include <optional>

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
    void UpdateEdits(const std::vector<LegacySDFEdit>& edits, bool useLegacyMapUnmap = false);
    void UpdateEdits(std::span<const SDFPrimitiveRecord> records, bool useLegacyMapUnmap = false);
    void UpdateEdits(const SDFSceneSnapshot& snapshot, bool useLegacyMapUnmap = false);

    /// Acik kare ayarlariyla compute render gecislerini calistirir.
    void Render(vk::CommandBuffer cmd, const RenderFrameSettings& settings);

    /// Eski parametre zincirli render cagrisi. Girdileri RenderFrameSettings yapisina paketleyip yeni overload'a delege eder.
    /// Not: 'time', 'width' ve 'height' parametreleri legacy olup kullanilmaz; render hedef boyutlari m_Width ve m_Height tarafindan belirlenir.
    void Render(vk::CommandBuffer cmd, float time, uint32_t normalMode, int width, int height,
                bool useGrid = true, bool optShadow = true, bool enableTAA = true, uint32_t frameIndex = 0,
                const QualitySettings& qualitySettings = QualitySettings{});
    void Resize(int width, int height);

    void SetQualitySettings(const QualitySettings& qs) noexcept { m_QualitySettings = qs; }
    [[nodiscard]] const QualitySettings& GetQualitySettings() const noexcept { return m_QualitySettings; }

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
    void SetCamera(const std::optional<RenderCamera>& camera, const glm::vec2& jitter = glm::vec2(0.0f));

    /// G-Buffer modunu aktif/pasif yapar
    void ResetTemporalHistory() noexcept {
        m_HistoryInitialized = false;
        m_CameraMatricesInitialized = false;
        m_HasPrevCameraViewProj = false;
        m_CurrentChangeSet = SDFChangeSet{};
        m_PreviousSnapshot = SDFSceneSnapshot{};
        if (m_SceneGpuData) m_SceneGpuData->ResetTransformHistory();
        if (m_TemporalHistory) m_TemporalHistory->Reset();
    }
    /// Main/render thread only. Import and filtering complete before replacing live resources.
    void LoadEnvironment(const std::filesystem::path& path);
    void SetExposure(float multiplier);
    /// Pure Deferred Architecture: Deferred path is the single canonical rendering path.
    void SetUseGBuffer([[maybe_unused]] bool enabled = true) noexcept {}
    [[nodiscard]] bool IsUsingGBuffer() const noexcept { return true; }

    /// G-Buffer onizleme modunu ayarlar (0: Shaded, 1: Albedo, 2: Normal, 3: Depth, 4: Motion, 5: Material)
    void SetDebugMode(int mode) noexcept { if (mode != m_DebugMode) ResetTemporalHistory(); m_DebugMode = mode; }
    [[nodiscard]] int GetDebugMode() const noexcept { return m_DebugMode; }

    [[nodiscard]] const RenderTargets* GetRenderTargets() const noexcept { return m_RenderTargets.get(); }

    // G-Buffer Render Hedefleri Getter'lari
    [[nodiscard]] vk::Image GetGBufferAlbedo() const noexcept { return m_RenderTargets ? m_RenderTargets->GetGBuffer().albedo.image : vk::Image{}; }
    [[nodiscard]] vk::ImageView GetGBufferAlbedoView() const noexcept { return m_RenderTargets ? m_RenderTargets->GetGBuffer().albedo.view : vk::ImageView{}; }
    [[nodiscard]] vk::Image GetGBufferNormal() const noexcept { return m_RenderTargets ? m_RenderTargets->GetGBuffer().normal.image : vk::Image{}; }
    [[nodiscard]] vk::ImageView GetGBufferNormalView() const noexcept { return m_RenderTargets ? m_RenderTargets->GetGBuffer().normal.view : vk::ImageView{}; }
    [[nodiscard]] vk::Image GetGBufferMaterial() const noexcept { return m_RenderTargets ? m_RenderTargets->GetGBuffer().material.image : vk::Image{}; }
    [[nodiscard]] vk::ImageView GetGBufferMaterialView() const noexcept { return m_RenderTargets ? m_RenderTargets->GetGBuffer().material.view : vk::ImageView{}; }
    [[nodiscard]] vk::Image GetGBufferDepth() const noexcept { return m_RenderTargets ? m_RenderTargets->GetGBuffer().depth.image : vk::Image{}; }
    [[nodiscard]] vk::ImageView GetGBufferDepthView() const noexcept { return m_RenderTargets ? m_RenderTargets->GetGBuffer().depth.view : vk::ImageView{}; }
    [[nodiscard]] vk::Image GetGBufferMotion() const noexcept { return m_RenderTargets ? m_RenderTargets->GetGBuffer().motion.image : vk::Image{}; }
    [[nodiscard]] vk::ImageView GetGBufferMotionView() const noexcept { return m_RenderTargets ? m_RenderTargets->GetGBuffer().motion.view : vk::ImageView{}; }

    [[nodiscard]] vk::Image GetStorageImage() const noexcept { return m_RenderTargets ? m_RenderTargets->GetOutput().image : vk::Image{}; }
    [[nodiscard]] vk::ImageView GetStorageImageView() const noexcept { return m_RenderTargets ? m_RenderTargets->GetOutput().view : vk::ImageView{}; }
    [[nodiscard]] const SceneGpuData* GetSceneGpuData() const noexcept { return m_SceneGpuData.get(); }
    [[nodiscard]] SceneGpuData* GetSceneGpuData() noexcept { return m_SceneGpuData.get(); }

    Buffer* GetEditBuffer() const { return m_SceneGpuData ? m_SceneGpuData->GetEditBuffer() : nullptr; }
    Buffer* GetSelectionBuffer() const { return m_SelectionBuffer.get(); }
    Buffer* GetCameraUBO() const { return m_SceneGpuData ? m_SceneGpuData->GetCameraUBO() : nullptr; }
    BrickGrid* GetBrickGrid() const { return m_SceneGpuData ? m_SceneGpuData->GetBrickGrid() : nullptr; }
    size_t GetActiveEditCount() const { return m_SceneGpuData ? m_SceneGpuData->GetActiveEditCount() : 0; }

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
    [[nodiscard]] std::vector<LightGPU>& GetLights() noexcept { return m_SceneGpuData->GetLights(); }
    [[nodiscard]] const std::vector<LightGPU>& GetLights() const noexcept { return m_SceneGpuData->GetLights(); }

    [[nodiscard]] SDFTemporalHistory* GetTemporalHistory() noexcept { return m_TemporalHistory.get(); }
    [[nodiscard]] const SDFTemporalHistory* GetTemporalHistory() const noexcept { return m_TemporalHistory.get(); }

    void SetChangeSet(const SDFChangeSet& changeSet) noexcept { m_CurrentChangeSet = changeSet; }
    [[nodiscard]] const SDFChangeSet& GetChangeSet() const noexcept { return m_CurrentChangeSet; }

private:
    vk::UniqueSampler m_ViewportSampler;
    vk::UniqueSampler m_LinearClampSampler; // TAA alt-piksel Catmull-Rom ornekleyicisi
    VulkanContext& m_Context;
    vk::Device m_Device;
    vk::PhysicalDevice m_PhysicalDevice;

    int m_Width = 1280;
    int m_Height = 720;

    // Scene GPU Data (G06: Primitifler, dunya donusumleri, isiklar, BrickGrid ve Kamera UBO)
    std::unique_ptr<SceneGpuData> m_SceneGpuData;
    std::unique_ptr<Buffer> m_SelectionBuffer;

    bool m_PickingRequested = false;
    bool m_PickPendingRead = false;
    int m_PickingMouseX = -1;
    int m_PickingMouseY = -1;
    int m_SelectedHitIndex = -1;
    bool m_HistoryInitialized = false;
    bool m_PreviousTAAEnabled = false;
    float m_Exposure = 1.0f;
    glm::vec3 m_PreviousCameraPosition{0.0f};
    QualitySettings m_QualitySettings{};

    // G-Buffer Pipeline & Degiskenleri (Faz 1)
    int m_DebugMode = 0;

    // Camera Matrices (Motion Vectors)
    glm::mat4 m_CurrViewProj{1.0f};
    glm::mat4 m_PrevViewProj{1.0f};
    glm::vec2 m_CurrJitter{0.0f};
    glm::vec2 m_PrevJitter{0.0f};
    bool m_CameraMatricesInitialized = false;
    std::optional<RenderCamera> m_RenderCamera;
    void SetCameraMatrices(const glm::mat4& view, const glm::mat4& proj, const glm::vec2& jitter);

    // Compute Programs (G04)
    std::string m_GBufferSpvPath;
    std::unique_ptr<ComputeProgram> m_GBufferProgram;
    vk::DescriptorSet m_GBufferDescriptorSet;

    // Debug Composite Pipeline
    std::string m_DebugCompositeSpvPath;
    std::unique_ptr<ComputeProgram> m_DebugCompositeProgram;
    vk::DescriptorSet m_DebugCompositeDescriptorSet;

    // Deferred Lighting Pipeline (Faz 2)
    std::string m_DeferredLightingSpvPath;
    std::unique_ptr<ComputeProgram> m_DeferredLightingProgram;
    vk::DescriptorSet m_DeferredLightingDescriptorSet;

    std::unique_ptr<IBLManager> m_IBLManager;
    std::unique_ptr<SDFTemporalHistory> m_TemporalHistory;
    SDFChangeSet m_CurrentChangeSet;
    SDFSceneSnapshot m_PreviousSnapshot;
    glm::mat4 m_PrevCameraViewProj{1.0f};
    bool m_HasPrevCameraViewProj = false;

    // TAA Pipeline (PR-8)
    std::string m_TaaSpvPath;
    std::unique_ptr<ComputeProgram> m_TaaProgram;
    std::array<vk::DescriptorSet, 2> m_TaaDescriptorSet;

    // RenderTargets (G05: 11 goruntuyu tek atomik RAII paketi olarak yonetir)
    std::unique_ptr<RenderTargets> m_RenderTargets;
    uint32_t m_HistoryPingPong = 0;

    vk::UniqueDescriptorPool m_DescriptorPool;

    void CreateDescriptorPoolAndSets();
    void UpdateTAADescriptorSets();
    void UpdateGBufferDescriptorSets();
    void UpdateDebugCompositeDescriptorSets();
    void UpdateDeferredLightingDescriptorSets();
    void CreateTAAPipeline();
    void CreateGBufferPipeline();
    void CreateDebugCompositePipeline();
    void CreateDeferredLightingPipeline();
};

} // namespace Astral
