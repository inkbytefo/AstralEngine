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
#include "Astral/Renderer/TemporalState.hpp"
#include "Astral/Renderer/PickingReadback.hpp"
#include "Astral/Geometry/SDFChangeSet.hpp"
#include "Astral/Geometry/SDFSceneSnapshot.hpp"
#include "Astral/Renderer/QualitySettings.hpp"
#include "Astral/Renderer/RenderFrameSettings.hpp"
#include "Astral/Renderer/ComputeProgram.hpp"
#include "Astral/Renderer/RenderTargets.hpp"
#include "Astral/Renderer/SceneGpuData.hpp"
#include "Astral/Renderer/ImageTransitions.hpp"
#include "Astral/Renderer/Passes/GBufferPass.hpp"
#include "Astral/Renderer/Passes/DeferredLightingPass.hpp"
#include "Astral/Renderer/Passes/DebugCompositePass.hpp"
#include "Astral/Renderer/Passes/TemporalResolvePass.hpp"
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

    /// G12: Girdileri facade sinirinda cozumleyerek pass'ler icin nihai etkin ayarlari uretir.
    [[nodiscard]] RenderFrameSettings ResolveFrameSettings(const RenderFrameSettings& input) const;

    /// G12: Bagimsiz/statik cozumleyici yardimci (CPU testleri ve facade dogrulama icin).
    [[nodiscard]] static RenderFrameSettings ResolveFrameSettings(
        const RenderFrameSettings& input,
        const QualitySettings& defaultQuality,
        float defaultExposure = 1.0f,
        int defaultHitIndex = -1,
        int defaultDebugMode = 0,
        uint32_t allocatedWidth = 0,
        uint32_t allocatedHeight = 0
    );

    /// G11: Basarili kuyruk gonderiminden sonra aday kare durumunu (history, camera, transforms) committed yapar.
    /// Eger hazirlanmis aday yoksa std::logic_error firlatir.
    void CommitSubmittedFrame();

    /// G11: Komut kaydi istisna verirse veya kuyruk gonderimi basarisiz olursa aday durumu iptal eder.
    void AbortPreparedFrame() noexcept;

    [[nodiscard]] bool HasPreparedCandidate() const noexcept { return m_HasPreparedCandidate; }

    /// Eski parametre zincirli render cagrisi. Girdileri RenderFrameSettings yapisina paketleyip yeni overload'a delege eder.
    /// Not: 'time', 'width' ve 'height' parametreleri legacy olup kullanilmaz; render hedef boyutlari m_Width ve m_Height tarafindan belirlenir.
    void Render(vk::CommandBuffer cmd, float time, uint32_t normalMode, int width, int height,
                bool useGrid = true, bool optShadow = true, bool enableTAA = true, uint32_t frameIndex = 0,
                std::optional<QualitySettings> qualitySettings = std::nullopt);
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
    [[nodiscard]] bool HasPendingSelection() const noexcept;

    /// GPU compute sonrasi fence-senkronize secim sonucunu dondurur ve sonucu tuketir (tek seferlik okuma)
    SelectionResult ConsumeSelectionResult();

    /// G08: Tipli ve kare serili tamamlanmis secim sonucunu dondurur ve sonucu tuketir (tek seferlik okuma)
    std::optional<CompletedPick> ConsumeCompletedPick();

    /// GPU compute sonrasi fence-senkronize secim sonucunu dondurur (okuma sonrasi sifirlamaz)
    SelectionResult GetSelectionResult() const;

    /// Secim tamponunu sifirlar (-1 yazar)
    void ClearSelectionResult();

    /// Kare tamamlandiginda fence sonrasi host readback'i tetikler
    void OnFrameCompleted(uint64_t frameSerial);

    /// Kamera View ve Projection matrislerini ve TAA jitter ofsetini gunceller (Motion Vectors icin)
    void SetCamera(const std::optional<RenderCamera>& camera, const glm::vec2& jitter = glm::vec2(0.0f));

    /// G-Buffer modunu aktif/pasif yapar
    void ResetTemporalHistory(TemporalResetReason reason = TemporalResetReason::Explicit) noexcept {
        m_HistoryInitialized = false;
        m_CameraMatricesInitialized = false;
        m_HasPrevCameraViewProj = false;
        m_HasCandidateCameraViewProj = false;
        m_HasCandidateSnapshot = false;
        m_CandidateSnapshot = SDFSceneSnapshot{};
        m_CurrentChangeSet = SDFChangeSet{};
        m_PreviousSnapshot = SDFSceneSnapshot{};
        m_HasPreparedCandidate = false;
        if (m_SceneGpuData) m_SceneGpuData->ResetTransformHistory();
        if (m_TemporalState) m_TemporalState->Reset(reason);
        if (m_PickingReadback) m_PickingReadback->AbortPrepared();
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
    void NotifyOutputUsed(ImageUse use) noexcept { m_OutputLastUse = use; }
    [[nodiscard]] ImageUse GetOutputLastUse() const noexcept { return m_OutputLastUse; }
    [[nodiscard]] const SceneGpuData* GetSceneGpuData() const noexcept { return m_SceneGpuData.get(); }
    [[nodiscard]] SceneGpuData* GetSceneGpuData() noexcept { return m_SceneGpuData.get(); }

    Buffer* GetEditBuffer() const { return m_SceneGpuData ? m_SceneGpuData->GetEditBuffer() : nullptr; }
    Buffer* GetSelectionBuffer() const { return m_PickingReadback ? m_PickingReadback->GetSelectionBuffer() : nullptr; }
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

    [[nodiscard]] TemporalState* GetTemporalState() noexcept { return m_TemporalState.get(); }
    [[nodiscard]] const TemporalState* GetTemporalState() const noexcept { return m_TemporalState.get(); }

    [[nodiscard]] SDFTemporalHistory* GetTemporalHistory() noexcept {
        return m_TemporalState ? &m_TemporalState->GetHistoryEvaluator() : nullptr;
    }
    [[nodiscard]] const SDFTemporalHistory* GetTemporalHistory() const noexcept {
        return m_TemporalState ? &m_TemporalState->GetHistoryEvaluator() : nullptr;
    }

    [[nodiscard]] PickingReadback* GetPickingReadback() noexcept { return m_PickingReadback.get(); }
    [[nodiscard]] const PickingReadback* GetPickingReadback() const noexcept { return m_PickingReadback.get(); }

    void SetChangeSet(const SDFChangeSet& changeSet) noexcept { m_CurrentChangeSet = changeSet; }
    [[nodiscard]] const SDFChangeSet& GetChangeSet() const noexcept { return m_CurrentChangeSet; }

    [[nodiscard]] GBufferPass* GetGBufferPass() noexcept { return m_GBufferPass.get(); }
    [[nodiscard]] const GBufferPass* GetGBufferPass() const noexcept { return m_GBufferPass.get(); }
    [[nodiscard]] DeferredLightingPass* GetDeferredLightingPass() noexcept { return m_DeferredLightingPass.get(); }
    [[nodiscard]] const DeferredLightingPass* GetDeferredLightingPass() const noexcept { return m_DeferredLightingPass.get(); }
    [[nodiscard]] DebugCompositePass* GetDebugCompositePass() noexcept { return m_DebugCompositePass.get(); }
    [[nodiscard]] const DebugCompositePass* GetDebugCompositePass() const noexcept { return m_DebugCompositePass.get(); }
    [[nodiscard]] TemporalResolvePass* GetTemporalResolvePass() noexcept { return m_TemporalResolvePass.get(); }
    [[nodiscard]] const TemporalResolvePass* GetTemporalResolvePass() const noexcept { return m_TemporalResolvePass.get(); }

private:
    VulkanContext& m_Context;
    vk::Device m_Device;
    vk::PhysicalDevice m_PhysicalDevice;

    int m_Width = 1280;
    int m_Height = 720;

    std::string m_GBufferSpvPath;
    std::string m_DebugCompositeSpvPath;
    std::string m_DeferredLightingSpvPath;
    std::string m_TaaSpvPath;

    // RenderTargets (G05: 11 goruntuyu tek atomik RAII paketi olarak yonetir - pass'lerden once tanimlanir)
    std::unique_ptr<RenderTargets> m_RenderTargets;
    uint32_t m_HistoryPingPong = 0;
    ImageUse m_OutputLastUse = ImageUse::FragmentRead;

    // Scene GPU Data (G06: Primitifler, dunya donusumleri, isiklar, BrickGrid ve Kamera UBO)
    std::unique_ptr<SceneGpuData> m_SceneGpuData;
    std::unique_ptr<PickingReadback> m_PickingReadback;
    std::unique_ptr<IBLManager> m_IBLManager;
    std::unique_ptr<TemporalState> m_TemporalState;

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

    // G11: Aday Kare Durumu (Prepare -> Commit / Abort)
    bool m_HasPreparedCandidate = false;
    uint32_t m_CandidatePingPongWriteIndex = 0;
    uint64_t m_CandidateFrameId = 0;
    bool m_HasCandidateSnapshot = false;
    SDFSceneSnapshot m_CandidateSnapshot;
    bool m_HasCandidateCameraViewProj = false;
    glm::mat4 m_CandidatePrevCameraViewProj{1.0f};

    SDFChangeSet m_CurrentChangeSet;
    SDFSceneSnapshot m_PreviousSnapshot;
    glm::mat4 m_PrevCameraViewProj{1.0f};
    bool m_HasPrevCameraViewProj = false;

    // Render Passes (G09)
    std::unique_ptr<GBufferPass> m_GBufferPass;
    std::unique_ptr<DebugCompositePass> m_DebugCompositePass;
    std::unique_ptr<DeferredLightingPass> m_DeferredLightingPass;
    std::unique_ptr<TemporalResolvePass> m_TemporalResolvePass;

    // Samplers
    vk::UniqueSampler m_ViewportSampler;
    vk::UniqueSampler m_LinearClampSampler; // TAA alt-piksel Catmull-Rom ornekleyicisi

    void UpdatePassResources();
};

} // namespace Astral
