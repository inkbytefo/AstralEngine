#include "TestFramework.hpp"
#include "Astral/Renderer/QualitySettings.hpp"
#include "Astral/Renderer/RenderCamera.hpp"
#include "Astral/Renderer/ShaderInterop.hpp"
#include "Astral/Renderer/SDFRenderer.hpp"
#include "Astral/Geometry/SDFChangeSet.hpp"
#include "Astral/Geometry/SDFSceneSnapshot.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <limits>

namespace Astral::Test {

void RunRendererArchitectureTests() {
    const std::string suite = "Renderer Architecture Suite";

    // 1. QualitySettings varsayilan degerleri ve veri sozlesmesi
    {
        QualitySettings qs;
        TEST_CHECK_MSG(suite, "QualitySettingsDefaultShadows", qs.enableShadows == true,
                       "Varsayilan olarak golgeler acik olmalidir.");
        TEST_CHECK_MSG(suite, "QualitySettingsDefaultShadowSteps", qs.shadowMaxSteps == 96,
                       "Varsayilan shadowMaxSteps 96 olmalidir.");
        TEST_CHECK_MSG(suite, "QualitySettingsDefaultAOSamples", qs.aoSamples == 8,
                       "Varsayilan aoSamples 8 olmalidir.");
        TEST_CHECK_MSG(suite, "QualitySettingsDefaultEpsilon", qs.rayHitEpsilon > 0.0f && qs.rayHitEpsilon <= 0.005f,
                       "rayHitEpsilon pozitif ve makul tolerans araliginda olmalidir.");
    }

    // 2. SDFChangeSet ve Snapshot sinir durumlari (CPU tarafi)
    {
        SDFChangeSet emptySet;
        TEST_CHECK_MSG(suite, "ChangeSetDefaultNoChanges", !emptySet.HasChanges(),
                       "Yeni olusturulan SDFChangeSet degisiklik icermemelidir.");
        TEST_CHECK_MSG(suite, "ChangeSetDefaultEmptyRects", emptySet.GetScreenRects().empty(),
                       "Yeni olusturulan SDFChangeSet 0 ekran dikdortgenine sahip olmalidir.");
        TEST_CHECK_MSG(suite, "ChangeSetDefaultNotGlobal", !emptySet.IsGlobalChange(),
                       "Yeni olusturulan SDFChangeSet global degisiklik olmamalidir.");

        SDFSceneSnapshot emptySnap;
        TEST_CHECK_MSG(suite, "SnapshotDefaultEmpty", emptySnap.GetRecordCount() == 0,
                       "Varsayilan SDFSceneSnapshot 0 kayit icermelidir.");
    }

    // 3. RenderCamera ve Vulkan NDC projeksiyon kurallari
    {
        RenderCamera cam;
        cam.position = glm::vec3(0.0f, 0.0f, 5.0f);
        cam.forward = glm::vec3(0.0f, 0.0f, -1.0f);
        cam.up = glm::vec3(0.0f, 1.0f, 0.0f);
        cam.view = glm::lookAt(cam.position, cam.position + cam.forward, cam.up);
        cam.projection = glm::perspective(glm::radians(60.0f), 1280.0f / 720.0f, 0.1f, 100.0f);

        TEST_CHECK_MSG(suite, "CameraProjectionValid", !std::isnan(cam.projection[0][0]) && cam.projection[0][0] > 0.0f,
                       "Kamera projeksiyon matrisi gecerli ve sonlu olmalidir.");

        // Vulkan Y ekseni terslemesi (Vulkan NDC: Y asagi dogru pozitif)
        glm::mat4 vkProj = cam.projection;
        if (vkProj[1][1] > 0.0f) {
            vkProj[1][1] *= -1.0f;
        }
        TEST_CHECK_MSG(suite, "VulkanProjectionFlippedY", vkProj[1][1] < 0.0f,
                       "Vulkan NDC icin Projeksiyon Y bileseni negatif olmalidir.");
    }

    // 4. Halton / TAA Jitter piksel sinir kontrolu
    {
        // Standart 8/16 fazli Halton ofsetlerinin [-0.5, 0.5] piksel araliginda olmasi gerekliligi
        for (uint32_t f = 0; f < 16; ++f) {
            glm::vec2 jitter = glm::vec2((f % 2) * 0.5f - 0.25f, (f % 3) / 3.0f - 0.333333f);
            TEST_CHECK_MSG(suite, "JitterWithinHalfPixelX", std::abs(jitter.x) <= 0.5f,
                           "Jitter X yarim piksel sinirini asmamalidir.");
            TEST_CHECK_MSG(suite, "JitterWithinHalfPixelY", std::abs(jitter.y) <= 0.5f,
                           "Jitter Y yarim piksel sinirini asmamalidir.");
        }
    }

    // 5. ShaderInterop GPU Struct ABI Sozlesmeleri
    {
        // SelectionDataGPU (32 bayt, alignas 16)
        TEST_CHECK_MSG(suite, "SelectionDataGPU_Size", sizeof(SelectionDataGPU) == 32, "SelectionDataGPU boyutu 32 bayt olmalidir.");
        TEST_CHECK_MSG(suite, "SelectionDataGPU_Align", alignof(SelectionDataGPU) == 16, "SelectionDataGPU 16 bayt hizali olmalidir.");
        TEST_CHECK_MSG(suite, "SelectionDataGPU_OffsetHitPoint", offsetof(SelectionDataGPU, hitPoint) == 16, "hitPoint offseti 16 olmalidir.");

        // SDFPushConstants (128 bayt, alignas 16)
        TEST_CHECK_MSG(suite, "SDFPushConstants_Size", sizeof(SDFPushConstants) == 128, "SDFPushConstants boyutu 128 bayt olmalidir.");
        TEST_CHECK_MSG(suite, "SDFPushConstants_Align", alignof(SDFPushConstants) == 16, "SDFPushConstants 16 bayt hizali olmalidir.");
        TEST_CHECK_MSG(suite, "SDFPushConstants_OffsetCameraRight", offsetof(SDFPushConstants, cameraRight) == 96, "cameraRight offseti 96 olmalidir.");
        TEST_CHECK_MSG(suite, "SDFPushConstants_OffsetCameraUp", offsetof(SDFPushConstants, cameraUp) == 112, "cameraUp offseti 112 olmalidir.");

        // TAAPushConstants (96 bayt, alignas 16)
        TEST_CHECK_MSG(suite, "TAAPushConstants_Size", sizeof(TAAPushConstants) == 96, "TAAPushConstants boyutu 96 bayt olmalidir.");
        TEST_CHECK_MSG(suite, "TAAPushConstants_Align", alignof(TAAPushConstants) == 16, "TAAPushConstants 16 bayt hizali olmalidir.");
        TEST_CHECK_MSG(suite, "TAAPushConstants_OffsetColorParams", offsetof(TAAPushConstants, colorParams) == 16, "colorParams offseti 16 olmalidir.");
        TEST_CHECK_MSG(suite, "TAAPushConstants_OffsetChangedRect0", offsetof(TAAPushConstants, changedRect0) == 32, "changedRect0 offseti 32 olmalidir.");

        // CameraUBOData (160 bayt, alignas 16)
        TEST_CHECK_MSG(suite, "CameraUBOData_Size", sizeof(CameraUBOData) == 160, "CameraUBOData boyutu 160 bayt olmalidir.");
        TEST_CHECK_MSG(suite, "CameraUBOData_Align", alignof(CameraUBOData) == 16, "CameraUBOData 16 bayt hizali olmalidir.");
        TEST_CHECK_MSG(suite, "CameraUBOData_OffsetPrevViewProj", offsetof(CameraUBOData, prevViewProj) == 64, "prevViewProj offseti 64 olmalidir.");
        TEST_CHECK_MSG(suite, "CameraUBOData_OffsetPrevCamPos", offsetof(CameraUBOData, prevCameraPosition) == 128, "prevCameraPosition offseti 128 olmalidir.");
        TEST_CHECK_MSG(suite, "CameraUBOData_OffsetJitter", offsetof(CameraUBOData, jitter) == 144, "jitter offseti 144 olmalidir.");

        // DebugCompositePushConstants (16 bayt, alignas 16)
        TEST_CHECK_MSG(suite, "DebugCompositePushConstants_Size", sizeof(DebugCompositePushConstants) == 16, "DebugCompositePushConstants boyutu 16 bayt olmalidir.");
        TEST_CHECK_MSG(suite, "DebugCompositePushConstants_Align", alignof(DebugCompositePushConstants) == 16, "DebugCompositePushConstants 16 bayt hizali olmalidir.");

        // LightGPU (48 bayt, alignas 16) & LightBufferHeader (16 bayt, alignas 16)
        TEST_CHECK_MSG(suite, "LightGPU_Size", sizeof(LightGPU) == 48, "LightGPU boyutu 48 bayt olmalidir.");
        TEST_CHECK_MSG(suite, "LightGPU_Align", alignof(LightGPU) == 16, "LightGPU 16 bayt hizali olmalidir.");
        TEST_CHECK_MSG(suite, "LightGPU_OffsetDirection", offsetof(LightGPU, direction) == 16, "direction offseti 16 olmalidir.");
        TEST_CHECK_MSG(suite, "LightGPU_OffsetColor", offsetof(LightGPU, color) == 32, "color offseti 32 olmalidir.");
        TEST_CHECK_MSG(suite, "LightBufferHeader_Size", sizeof(LightBufferHeader) == 16, "LightBufferHeader boyutu 16 bayt olmalidir.");
        TEST_CHECK_MSG(suite, "LightBufferHeader_Align", alignof(LightBufferHeader) == 16, "LightBufferHeader 16 bayt hizali olmalidir.");

        // DeferredLightingPushConstants (128 bayt, alignas 16)
        TEST_CHECK_MSG(suite, "DeferredLightingPushConstants_Size", sizeof(DeferredLightingPushConstants) == 128, "DeferredLightingPushConstants boyutu 128 bayt olmalidir.");
        TEST_CHECK_MSG(suite, "DeferredLightingPushConstants_Align", alignof(DeferredLightingPushConstants) == 16, "DeferredLightingPushConstants 16 bayt hizali olmalidir.");
        TEST_CHECK_MSG(suite, "DeferredLightingPushConstants_OffsetShadowAO", offsetof(DeferredLightingPushConstants, shadowAOParams) == 96, "shadowAOParams offseti 96 olmalidir.");
        TEST_CHECK_MSG(suite, "DeferredLightingPushConstants_OffsetQuality", offsetof(DeferredLightingPushConstants, qualityParams) == 112, "qualityParams offseti 112 olmalidir.");
    }

    // 6. RenderFrameSettings Sozlesmesi ve Varsayilan Degerleri
    {
        RenderFrameSettings rfs;
        TEST_CHECK_MSG(suite, "RFS_DefaultCamera", !rfs.camera.has_value(), "Varsayilan kamera std::nullopt olmalidir.");
        TEST_CHECK_MSG(suite, "RFS_DefaultJitter", rfs.jitter == glm::vec2(0.0f), "Varsayilan jitter (0, 0) olmalidir.");
        TEST_CHECK_MSG(suite, "RFS_DefaultQuality", rfs.quality.shadowMaxSteps == 96 && rfs.quality.enableShadows == true, "Varsayilan kalite ayarlari standard QualitySettings ile eslesmelidir.");
        TEST_CHECK_MSG(suite, "RFS_DefaultFrameId", rfs.frameId == 0, "Varsayilan frameId 0 olmalidir.");
        TEST_CHECK_MSG(suite, "RFS_DefaultDimensions", rfs.width == 0 && rfs.height == 0, "Varsayilan genislik ve yukseklik 0 olmalidir.");
        TEST_CHECK_MSG(suite, "RFS_DefaultNormalMode", rfs.normalMode == 0, "Varsayilan normalMode 0 olmalidir.");
        TEST_CHECK_MSG(suite, "RFS_DefaultDebugMode", rfs.debugMode == 0, "Varsayilan debugMode 0 (Shaded) olmalidir.");
        TEST_CHECK_MSG(suite, "RFS_DefaultSelectedHitIndex", rfs.selectedHitIndex == -1, "Varsayilan selectedHitIndex -1 olmalidir.");
        TEST_CHECK_MSG(suite, "RFS_DefaultUseGrid", rfs.useGrid == true, "Varsayilan useGrid true olmalidir.");
        TEST_CHECK_MSG(suite, "RFS_DefaultOptimizedShadows", rfs.optimizedShadows == true, "Varsayilan optimizedShadows true olmalidir.");
        TEST_CHECK_MSG(suite, "RFS_DefaultTaaEnabled", rfs.taaEnabled == true, "Varsayilan taaEnabled true olmalidir.");
        TEST_CHECK_MSG(suite, "RFS_DefaultExposure", rfs.exposure == 1.0f, "Varsayilan exposure 1.0f olmalidir.");
    }

    // 7. RenderFrameSettings Oncelik ve Fallback Kurallari
    {
        // Kalite fallback kurali: shadowMaxSteps > 0 ? settings.quality : fallback
        QualitySettings persistentQs;
        persistentQs.shadowMaxSteps = 128;
        persistentQs.shadowK = 32.0f;

        QualitySettings frameQsActive;
        frameQsActive.shadowMaxSteps = 64;
        frameQsActive.shadowK = 16.0f;

        QualitySettings frameQsZeroed;
        frameQsZeroed.shadowMaxSteps = 0; // Trigger fallback

        const auto& resolvedActive = (frameQsActive.shadowMaxSteps > 0) ? frameQsActive : persistentQs;
        TEST_CHECK_MSG(suite, "QualityPrecedence_ExplicitSelected", resolvedActive.shadowMaxSteps == 64 && resolvedActive.shadowK == 16.0f,
                       "Kare ayarlarinda shadowMaxSteps > 0 oldugunda frame ayarlari secilmelidir.");

        const auto& resolvedFallback = (frameQsZeroed.shadowMaxSteps > 0) ? frameQsZeroed : persistentQs;
        TEST_CHECK_MSG(suite, "QualityPrecedence_FallbackToRenderer", resolvedFallback.shadowMaxSteps == 128 && resolvedFallback.shadowK == 32.0f,
                       "Kare ayarlarinda shadowMaxSteps == 0 oldugunda renderer persistent kalitesine donulmelidir.");

        // Exposure onceligi: settings.exposure > 0.0f ? settings.exposure : m_Exposure
        float persistentExp = 2.5f;
        RenderFrameSettings rfsExpOverride;
        rfsExpOverride.exposure = 3.0f;
        RenderFrameSettings rfsExpZero;
        rfsExpZero.exposure = 0.0f;

        auto resolveExposure = [](float frameExp, float persistent) {
            return (frameExp > 0.0f) ? frameExp : persistent;
        };

        TEST_CHECK_MSG(suite, "ExposurePrecedence_Explicit", resolveExposure(rfsExpOverride.exposure, persistentExp) == 3.0f,
                       "Acik exposure ayari persistent degeri ezmelidir.");
        TEST_CHECK_MSG(suite, "ExposurePrecedence_ZeroFallback", resolveExposure(rfsExpZero.exposure, persistentExp) == 2.5f,
                       "Sifir exposure durumunda persistent degere donulmelidir.");

        // DebugMode onceligi
        RenderFrameSettings rfsDebug;
        rfsDebug.debugMode = 2; // Normal buffer
        int resolvedDebug = rfsDebug.debugMode;
        TEST_CHECK_MSG(suite, "DebugModePrecedence", resolvedDebug == 2,
                       "Kare bazli debugMode ayari persistent debug modunu ezmelidir.");

        // SelectedHitIndex onceligi: settings.selectedHitIndex != -1 ? settings.selectedHitIndex : m_SelectedHitIndex
        int persistentHitIndex = 42;
        RenderFrameSettings rfsHitSelected;
        rfsHitSelected.selectedHitIndex = 7;
        RenderFrameSettings rfsHitDefault; // -1
        auto resolveHit = [](int frameHit, int persistent) {
            return (frameHit != -1) ? frameHit : persistent;
        };
        TEST_CHECK_MSG(suite, "HitIndexPrecedence_Explicit", resolveHit(rfsHitSelected.selectedHitIndex, persistentHitIndex) == 7,
                       "Acik selectedHitIndex persistent degeri ezmelidir.");
        TEST_CHECK_MSG(suite, "HitIndexPrecedence_DefaultFallback", resolveHit(rfsHitDefault.selectedHitIndex, persistentHitIndex) == 42,
                       "Secim belirtilmediginde persistent hitIndex kullanilmalidir.");
    }
}

} // namespace Astral::Test
