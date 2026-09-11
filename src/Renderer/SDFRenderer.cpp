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

    // PR-9 / G08: PickingReadback (Selection Buffer ve İstek/Sonuç Yaşam Döngüsü)
    m_PickingReadback = std::make_unique<PickingReadback>(m_Context);

    // Faz 2: IBL kurulumu (Isik tamponu SceneGpuData tarafindan yonetilir)
    m_IBLManager = std::make_unique<IBLManager>(m_Context);

    m_TemporalState = std::make_unique<TemporalState>();

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

    m_GBufferPass = std::make_unique<GBufferPass>(m_Device, m_GBufferSpvPath);
    m_DeferredLightingPass = std::make_unique<DeferredLightingPass>(m_Device, m_DeferredLightingSpvPath);
    m_DebugCompositePass = std::make_unique<DebugCompositePass>(m_Device, m_DebugCompositeSpvPath);
    m_TemporalResolvePass = std::make_unique<TemporalResolvePass>(m_Device, m_TaaSpvPath);
    UpdatePassResources();

    std::cout << "[Astral::SDFRenderer] SDF Renderer baslatildi (" << m_Width << "x" << m_Height 
              << ", EditBuffer: " << (MAX_EDITS * sizeof(SDFPrimitiveRecord)) / 1024 << " KB"
              << ", Two-Level BrickGrid, Deferred PBR & IBL Lighting Aktif).\n";
}

SDFRenderer::~SDFRenderer() {
    m_Device.waitIdle();
    m_ViewportSampler.reset();
    m_LinearClampSampler.reset();

    m_TemporalResolvePass.reset();
    m_DeferredLightingPass.reset();
    m_DebugCompositePass.reset();
    m_GBufferPass.reset();

    m_TemporalState.reset();
    m_IBLManager.reset();
    m_PickingReadback.reset();
    m_SceneGpuData.reset();
    m_RenderTargets.reset();
}

void SDFRenderer::SetLights(const std::vector<LightGPU>& lights) {
    if (m_SceneGpuData) {
        m_SceneGpuData->SetLights(lights);
    }
}

void SDFRenderer::UpdatePassResources() {
    if (!m_RenderTargets || !m_SceneGpuData) return;

    auto gbuf = m_RenderTargets->GetGBuffer();
    auto rawColor = m_RenderTargets->GetRawColor();
    auto output = m_RenderTargets->GetOutput();
    auto sceneViews = m_SceneGpuData->GetViews();
    auto camDesc = m_SceneGpuData->GetCameraDescriptor();
    auto selDesc = m_PickingReadback ? m_PickingReadback->GetDescriptorInfo() : vk::DescriptorBufferInfo{};

    if (m_GBufferPass) {
        GBufferInputs gbufInputs{
            .targets = gbuf,
            .scene = sceneViews,
            .camera = camDesc,
            .selection = selDesc
        };
        m_GBufferPass->BindResources(gbufInputs);
    }

    if (m_DeferredLightingPass && m_IBLManager) {
        LightingInputs lightInputs{
            .gbuffer = gbuf,
            .output = rawColor,
            .scene = sceneViews,
            .irradiance = m_IBLManager->GetIrradianceDescriptor(),
            .prefiltered = m_IBLManager->GetPrefilteredDescriptor(),
            .brdf = m_IBLManager->GetBRDFLUTDescriptor(),
            .prefilteredMipLevels = m_IBLManager->GetPrefilteredMipLevels()
        };
        m_DeferredLightingPass->BindResources(lightInputs);
    }

    if (m_DebugCompositePass) {
        DebugInputs debugInputs{
            .gbuffer = gbuf,
            .output = rawColor
        };
        m_DebugCompositePass->BindResources(debugInputs);
    }

    if (m_TemporalResolvePass) {
        std::array<ImageViewRef, 2> histColors = {
            m_RenderTargets->GetHistoryColor(0),
            m_RenderTargets->GetHistoryColor(1)
        };
        std::array<ImageViewRef, 2> histExtras = {
            m_RenderTargets->GetHistoryExtra(0),
            m_RenderTargets->GetHistoryExtra(1)
        };
        ResolveInputs resolveInputs{
            .gbuffer = gbuf,
            .rawColor = rawColor,
            .output = output,
            .historyColor = histColors,
            .historyExtra = histExtras,
            .historySampler = m_LinearClampSampler.get()
        };
        m_TemporalResolvePass->BindResources(resolveInputs);
    }
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
    m_CandidateSnapshot = snapshot;
    m_HasCandidateSnapshot = true;
    if (m_RenderCamera) {
        m_CandidatePrevCameraViewProj = m_CameraMatricesInitialized ? m_CurrViewProj : (m_RenderCamera->projection * m_RenderCamera->view);
        m_HasCandidateCameraViewProj = true;
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

    UpdatePassResources();
    m_HistoryInitialized = false;
    m_HasPrevCameraViewProj = false;
    m_HasCandidateCameraViewProj = false;
    m_HasCandidateSnapshot = false;
    m_CandidateSnapshot = SDFSceneSnapshot{};
    m_PreviousSnapshot = SDFSceneSnapshot{};
    m_HasPreparedCandidate = false;
    if (m_TemporalState) m_TemporalState->Reset(TemporalResetReason::Resize);
    if (m_SceneGpuData) m_SceneGpuData->ResetTransformHistory();
    if (m_PickingReadback) m_PickingReadback->AbortPrepared();
    m_CurrentChangeSet = SDFChangeSet{};
}

RenderFrameSettings SDFRenderer::ResolveFrameSettings(const RenderFrameSettings& input) const {
    return ResolveFrameSettings(
        input,
        m_QualitySettings,
        m_Exposure,
        m_SelectedHitIndex,
        m_DebugMode,
        static_cast<uint32_t>(m_Width),
        static_cast<uint32_t>(m_Height)
    );
}

RenderFrameSettings SDFRenderer::ResolveFrameSettings(
    const RenderFrameSettings& input,
    const QualitySettings& defaultQuality,
    float defaultExposure,
    int defaultHitIndex,
    int defaultDebugMode,
    uint32_t allocatedWidth,
    uint32_t allocatedHeight
) {
    RenderFrameSettings resolved = input;

    // 1. Target dimensions resolution & safe fallback
    if (input.width > 0 && input.height > 0) {
        if (allocatedWidth > 0 && allocatedHeight > 0 &&
            (input.width != allocatedWidth || input.height != allocatedHeight)) {
            // Mismatch with allocated render targets: fallback/clamp to allocated dimensions safely
            resolved.width = allocatedWidth;
            resolved.height = allocatedHeight;
        } else {
            resolved.width = input.width;
            resolved.height = input.height;
        }
    } else {
        resolved.width = (allocatedWidth > 0) ? allocatedWidth : input.width;
        resolved.height = (allocatedHeight > 0) ? allocatedHeight : input.height;
    }

    // 2. Quality resolution: explicit qualityOverride > defaultQuality (no sentinels)
    if (input.qualityOverride.has_value()) {
        resolved.quality = *input.qualityOverride;
        resolved.qualityOverride = input.qualityOverride;
    } else {
        resolved.quality = defaultQuality;
        resolved.qualityOverride = std::nullopt;
    }

    // 3. Exposure resolution: positive finite requirement; fallback safely on NaN/Inf/<=0
    if (std::isfinite(input.exposure) && input.exposure > 0.0f) {
        resolved.exposure = input.exposure;
    } else {
        resolved.exposure = (std::isfinite(defaultExposure) && defaultExposure >= 0.0f) ? defaultExposure : 1.0f;
    }

    // 4. Debug mode precedence
    resolved.debugMode = (input.debugMode != 0) ? input.debugMode : defaultDebugMode;

    // 5. Selected hit index precedence
    resolved.selectedHitIndex = (input.selectedHitIndex != -1) ? input.selectedHitIndex : defaultHitIndex;

    // 6. Jitter synchronization: when TAA is disabled, jitter MUST be (0, 0)
    if (!input.taaEnabled) {
        resolved.jitter = glm::vec2(0.0f);
    }

    return resolved;
}

void SDFRenderer::Render(vk::CommandBuffer cmd, float time, uint32_t normalMode, int width, int height,
                         bool useGrid, bool optShadow, bool enableTAA, uint32_t frameIndex,
                         std::optional<QualitySettings> qualitySettings) {
    (void)time;
    (void)width;
    (void)height;

    RenderFrameSettings settings;
    settings.camera = m_RenderCamera;
    settings.jitter = m_CurrJitter;
    settings.qualityOverride = qualitySettings;
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
    // G12: Resolve inputs at facade boundary; passes do not perform fallback.
    RenderFrameSettings effectiveSettings = ResolveFrameSettings(settings);

    if (settings.width > 0 && settings.height > 0 &&
        (settings.width != static_cast<uint32_t>(m_Width) || settings.height != static_cast<uint32_t>(m_Height))) {
        std::cerr << "[Astral::SDFRenderer] WARNING: RenderFrameSettings dimensions ("
                  << settings.width << "x" << settings.height
                  << ") do not match allocated render target dimensions ("
                  << m_Width << "x" << m_Height << "). Using allocated dimensions. Call Resize() before Render() to change resolution.\n";
    }

    // Determine effective camera: settings.camera takes precedence over m_RenderCamera
    std::optional<RenderCamera> effectiveCamera = effectiveSettings.camera ? effectiveSettings.camera : m_RenderCamera;

    if (effectiveSettings.camera.has_value() && (!m_RenderCamera || effectiveSettings.camera->entity != m_RenderCamera->entity ||
        effectiveSettings.camera->sceneInstance != m_RenderCamera->sceneInstance ||
        effectiveSettings.camera->projection != m_RenderCamera->projection ||
        effectiveSettings.camera->view != m_RenderCamera->view ||
        effectiveSettings.jitter != m_CurrJitter)) {
        SetCamera(effectiveSettings.camera, effectiveSettings.jitter);
        effectiveCamera = m_RenderCamera;
    }
    effectiveSettings.camera = effectiveCamera;

    if (!effectiveCamera) {
        // No implicit camera: discard the previous frame and produce opaque black.
        DiscardAndTransitionImage(cmd, m_RenderTargets->GetOutput().image,
            m_OutputLastUse, ImageUse::TransferWrite);

        cmd.clearColorImage(m_RenderTargets->GetOutput().image, vk::ImageLayout::eTransferDstOptimal,
            vk::ClearColorValue(std::array<float, 4>{0, 0, 0, 1}),
            vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

        TransitionImage(cmd, m_RenderTargets->GetOutput().image,
            ImageUse::TransferWrite, ImageUse::FragmentRead);
        m_OutputLastUse = ImageUse::FragmentRead;

        m_HistoryInitialized = false;
        RenderFrameSettings noCamSettings = effectiveSettings;
        noCamSettings.camera = std::nullopt;
        if (m_TemporalState) {
            m_TemporalState->Prepare(noCamSettings);
        }
        ClearSelectionResult();
        m_CandidatePingPongWriteIndex = m_HistoryPingPong;
        m_CandidateFrameId = effectiveSettings.frameId;
        m_HasPreparedCandidate = true;
        return;
    }

    if (m_PreviousTAAEnabled != effectiveSettings.taaEnabled) {
        m_HistoryInitialized = false;
        if (m_TemporalState) m_TemporalState->Reset(TemporalResetReason::TaaChanged);
    }
    m_PreviousTAAEnabled = effectiveSettings.taaEnabled;

    if (effectiveSettings.debugMode != m_DebugMode) {
        ResetTemporalHistory(TemporalResetReason::DebugChanged);
        m_DebugMode = effectiveSettings.debugMode;
    }
    bool isDebugActive = (effectiveSettings.debugMode != 0);

    // Jitter: if input jitter was default (0,0) and TAA is enabled, use current sequence jitter
    if (effectiveSettings.taaEnabled && effectiveSettings.jitter == glm::vec2(0.0f)) {
        effectiveSettings.jitter = m_CurrJitter;
    }

    // Prepare effective frame settings for TemporalState and Passes
    int pickMouseX = -1;
    int pickMouseY = -1;
    float pickFlag = 0.0f;
    if (m_PickingReadback) {
        m_PickingReadback->PrepareDispatch(effectiveSettings.width, effectiveSettings.height, effectiveCamera.has_value(),
                                           pickMouseX, pickMouseY, pickFlag);
    }

    auto sceneViews = m_SceneGpuData ? m_SceneGpuData->GetViews() : SceneBufferViews{};
    uint32_t activeCount = (effectiveSettings.activePrimitiveCount > 0)
        ? effectiveSettings.activePrimitiveCount
        : (m_SceneGpuData ? m_SceneGpuData->GetActiveEditCount() : 0);
    glm::vec4 gridParams = (effectiveSettings.gridParams != glm::vec4(0.0f))
        ? effectiveSettings.gridParams
        : sceneViews.gridParams;

    effectiveSettings.activePrimitiveCount = activeCount;
    effectiveSettings.gridParams = gridParams;
    effectiveSettings.pickMouseX = pickMouseX;
    effectiveSettings.pickMouseY = pickMouseY;
    effectiveSettings.pickFlag = pickFlag;

    TemporalPlan temporalPlan{};
    if (m_TemporalState) {
        temporalPlan = m_TemporalState->Prepare(effectiveSettings);
        if (m_SceneGpuData) {
            m_SceneGpuData->UploadCamera(m_TemporalState->GetCameraUBOData());
        }
    }
    
    // =========================================================================
    // 1. G-Buffer Compute Pass (SDFGBuffer.glsl)
    // =========================================================================
    auto gbuf = m_RenderTargets->GetGBuffer();
    auto rawColor = m_RenderTargets->GetRawColor();
    auto outputImg = m_RenderTargets->GetOutput();

    // =========================================================================
    // 1. G-Buffer Targets -> ComputeWrite Barrier
    // =========================================================================
    const std::array<ImageTransitionItem, 5> gbufBarriers = {
        ImageTransitionItem{ gbuf.albedo.image, ImageUse::ComputeRead, ImageUse::ComputeWrite },
        ImageTransitionItem{ gbuf.normal.image, ImageUse::ComputeRead, ImageUse::ComputeWrite },
        ImageTransitionItem{ gbuf.material.image, ImageUse::ComputeRead, ImageUse::ComputeWrite },
        ImageTransitionItem{ gbuf.depth.image, ImageUse::ComputeRead, ImageUse::ComputeWrite },
        ImageTransitionItem{ gbuf.motion.image, ImageUse::ComputeRead, ImageUse::ComputeWrite }
    };
    TransitionImages(cmd, gbufBarriers);

    if (m_GBufferPass) {
        m_GBufferPass->Record(cmd, effectiveSettings);
    }

    if (m_PickingReadback && pickFlag > 0.0f) {
        m_PickingReadback->RecordBarrier(cmd);
    }

    // =========================================================================
    // 2. G-Buffer -> Composite/Lighting Barriers
    // =========================================================================
    const std::array<ImageTransitionItem, 6> toCompositeBarriers = {
        ImageTransitionItem{ gbuf.albedo.image, ImageUse::ComputeWrite, ImageUse::ComputeRead },
        ImageTransitionItem{ gbuf.normal.image, ImageUse::ComputeWrite, ImageUse::ComputeRead },
        ImageTransitionItem{ gbuf.material.image, ImageUse::ComputeWrite, ImageUse::ComputeRead },
        ImageTransitionItem{ gbuf.depth.image, ImageUse::ComputeWrite, ImageUse::ComputeRead },
        ImageTransitionItem{ gbuf.motion.image, ImageUse::ComputeWrite, ImageUse::ComputeRead },
        ImageTransitionItem{ rawColor.image, ImageUse::ComputeRead, ImageUse::ComputeWrite }
    };
    TransitionImages(cmd, toCompositeBarriers);

    if (isDebugActive) {
        // =========================================================================
        // 3a. Debug Composite Pass (SDFDebugComposite.glsl -> m_RawColorImage)
        // =========================================================================
        if (m_DebugCompositePass) {
            m_DebugCompositePass->Record(cmd, effectiveSettings);
        }
    } else {
        // =========================================================================
        // 3b. Deferred PBR & IBL Pass (DeferredLighting.glsl -> m_RawColorImage Linear HDR)
        // =========================================================================
        if (m_DeferredLightingPass) {
            m_DeferredLightingPass->Record(cmd, effectiveSettings);
        }
    }

    // =========================================================================
    // 4. TAA Resolve & ACES Tonemapping Pass (Dogrusal HDR -> sRGB)
    // =========================================================================
    uint32_t readIdx = m_TemporalState ? temporalPlan.readIndex : m_HistoryPingPong;
    uint32_t writeIdx = m_TemporalState ? temporalPlan.writeIndex : (1 - m_HistoryPingPong);

    // Barrier: Composite/Lighting -> TemporalResolve
    const std::array<ImageTransitionItem, 6> taaBarriers = {
        ImageTransitionItem{ rawColor.image, ImageUse::ComputeWrite, ImageUse::ComputeRead },
        ImageTransitionItem{ outputImg.image, m_OutputLastUse, ImageUse::ComputeWrite },
        ImageTransitionItem{ m_RenderTargets->GetHistoryColor(readIdx).image, ImageUse::ComputeWrite, ImageUse::ComputeRead },
        ImageTransitionItem{ m_RenderTargets->GetHistoryColor(writeIdx).image, ImageUse::ComputeRead, ImageUse::ComputeWrite },
        ImageTransitionItem{ m_RenderTargets->GetHistoryExtra(readIdx).image, ImageUse::ComputeWrite, ImageUse::ComputeRead },
        ImageTransitionItem{ m_RenderTargets->GetHistoryExtra(writeIdx).image, ImageUse::ComputeRead, ImageUse::ComputeWrite }
    };
    TransitionImages(cmd, taaBarriers);
    m_HistoryInitialized = true;

    if (m_TemporalResolvePass) {
        m_TemporalResolvePass->Record(cmd, effectiveSettings, temporalPlan, m_CurrentChangeSet);
    }

    // Output: ComputeWrite -> FragmentRead (for ImGui viewport sampling)
    // Layout remains eGeneral without redundant transfer-src roundtrips.
    TransitionImage(cmd, outputImg.image, ImageUse::ComputeWrite, ImageUse::FragmentRead);
    m_OutputLastUse = ImageUse::FragmentRead;

    // G11: Aday durumunu kaydet; basarili submit sonrasi CommitSubmittedFrame ile committed yapilacak
    m_CandidatePingPongWriteIndex = writeIdx;
    m_CandidateFrameId = settings.frameId;
    m_HasPreparedCandidate = true;
}

void SDFRenderer::CommitSubmittedFrame() {
    if (!m_HasPreparedCandidate) {
        throw std::logic_error("[Astral::SDFRenderer] CommitSubmittedFrame called with no prepared candidate!");
    }

    if (m_TemporalState) {
        m_TemporalState->CommitSubmitted();
    }
    if (m_SceneGpuData) {
        m_SceneGpuData->CommitSubmitted();
    }
    if (m_PickingReadback) {
        m_PickingReadback->OnFrameSubmitted(m_CandidateFrameId);
    }

    if (m_HasCandidateSnapshot) {
        m_PreviousSnapshot = std::move(m_CandidateSnapshot);
        m_CandidateSnapshot = SDFSceneSnapshot{};
        m_HasCandidateSnapshot = false;
    }
    if (m_HasCandidateCameraViewProj) {
        m_PrevCameraViewProj = m_CandidatePrevCameraViewProj;
        m_HasPrevCameraViewProj = true;
        m_HasCandidateCameraViewProj = false;
    }

    m_HistoryPingPong = m_CandidatePingPongWriteIndex;
    m_HasPreparedCandidate = false;
}

void SDFRenderer::AbortPreparedFrame() noexcept {
    if (!m_HasPreparedCandidate) {
        return;
    }

    if (m_TemporalState) {
        m_TemporalState->AbortPrepared();
    }
    if (m_SceneGpuData) {
        m_SceneGpuData->AbortPrepared();
    }
    if (m_PickingReadback) {
        m_PickingReadback->AbortPrepared();
    }

    m_CandidateSnapshot = SDFSceneSnapshot{};
    m_HasCandidateSnapshot = false;
    m_HasCandidateCameraViewProj = false;
    m_HasPreparedCandidate = false;
}

void SDFRenderer::SetPickingRequest(int mouseX, int mouseY) {
    if (m_PickingReadback) {
        uint64_t sceneInstance = m_RenderCamera ? m_RenderCamera->sceneInstance : 0;
        m_PickingReadback->RequestPick(mouseX, mouseY, sceneInstance);
    }
}

bool SDFRenderer::HasPendingSelection() const noexcept {
    return m_PickingReadback ? m_PickingReadback->HasPendingRead() : false;
}

SDFRenderer::SelectionResult SDFRenderer::GetSelectionResult() const {
    SelectionResult result{};
    if (m_PickingReadback) {
        auto completed = m_PickingReadback->PeekCompleted();
        if (completed.has_value()) {
            result.hitIndex = completed->data.hitIndex;
            result.hasHit = (completed->data.hitIndex >= 0);
            result.hitPoint = glm::vec3(completed->data.hitPoint.x, completed->data.hitPoint.y, completed->data.hitPoint.z);
            result.hitDistance = completed->data.hitPoint.w;
        }
    }
    return result;
}

SDFRenderer::SelectionResult SDFRenderer::ConsumeSelectionResult() {
    SelectionResult result{};
    if (m_PickingReadback) {
        auto completed = m_PickingReadback->ConsumeCompleted();
        if (completed.has_value()) {
            result.hitIndex = completed->data.hitIndex;
            result.hasHit = (completed->data.hitIndex >= 0);
            result.hitPoint = glm::vec3(completed->data.hitPoint.x, completed->data.hitPoint.y, completed->data.hitPoint.z);
            result.hitDistance = completed->data.hitPoint.w;
        }
    }
    return result;
}

std::optional<CompletedPick> SDFRenderer::ConsumeCompletedPick() {
    return m_PickingReadback ? m_PickingReadback->ConsumeCompleted() : std::nullopt;
}

void SDFRenderer::ClearSelectionResult() {
    if (m_PickingReadback) {
        m_PickingReadback->Clear();
    }
}

void SDFRenderer::OnFrameCompleted(uint64_t frameSerial) {
    if (m_PickingReadback) {
        m_PickingReadback->OnFrameCompleted(frameSerial);
    }
}

void SDFRenderer::LoadEnvironment(const std::filesystem::path& path) {
    auto replacement = std::make_unique<IBLManager>(m_Context, path);
    m_Device.waitIdle();
    m_IBLManager.swap(replacement);
    UpdatePassResources();
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
        if (m_TemporalState) {
            m_TemporalState->Reset(camera ? TemporalResetReason::CameraCut : TemporalResetReason::MissingCamera);
        }
    }
    m_PreviousCameraPosition = (m_CameraMatricesInitialized && m_RenderCamera)
        ? m_RenderCamera->position : (camera ? camera->position : glm::vec3(0.0f));
    m_RenderCamera = camera;
    if (camera) {
        SetCameraMatrices(camera->view, camera->projection, jitter);
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
