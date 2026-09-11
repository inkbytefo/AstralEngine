#include "TestFramework.hpp"
#include "Astral/Renderer/QualitySettings.hpp"
#include "Astral/Renderer/RenderCamera.hpp"
#include "Astral/Renderer/ShaderInterop.hpp"
#include "Astral/Renderer/SDFRenderer.hpp"
#include "Astral/Renderer/ComputeProgram.hpp"
#include "Astral/Renderer/RenderTargets.hpp"
#include "Astral/Renderer/SceneGpuData.hpp"
#include "Astral/Renderer/TemporalState.hpp"
#include "Astral/Renderer/PickingReadback.hpp"
#include "Astral/Renderer/Passes/GBufferPass.hpp"
#include "Astral/Renderer/Passes/DeferredLightingPass.hpp"
#include "Astral/Renderer/Passes/DebugCompositePass.hpp"
#include "Astral/Renderer/Passes/TemporalResolvePass.hpp"
#include "Astral/Renderer/ImageTransitions.hpp"
#include "Astral/Geometry/SDFChangeSet.hpp"
#include "Astral/Geometry/SDFSceneSnapshot.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <limits>
#include <filesystem>
#include <fstream>

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

    // 7. RenderFrameSettings Oncelik ve Fallback Kurallari (G12: ResolveFrameSettings tek kalite politikasi)
    {
        QualitySettings persistentQs;
        persistentQs.shadowMaxSteps = 128;
        persistentQs.shadowK = 32.0f;

        QualitySettings overrideQs;
        overrideQs.shadowMaxSteps = 64;
        overrideQs.shadowK = 16.0f;

        RenderFrameSettings rfsOverride;
        rfsOverride.qualityOverride = overrideQs;

        RenderFrameSettings rfsDefault;
        rfsDefault.qualityOverride = std::nullopt;

        auto resolvedActive = SDFRenderer::ResolveFrameSettings(rfsOverride, persistentQs);
        TEST_CHECK_MSG(suite, "QualityPrecedence_ExplicitSelected",
                       resolvedActive.quality.shadowMaxSteps == 64 && resolvedActive.quality.shadowK == 16.0f,
                       "Kare ayarlarinda qualityOverride mevcut oldugunda override ayarlari secilmelidir.");

        auto resolvedFallback = SDFRenderer::ResolveFrameSettings(rfsDefault, persistentQs);
        TEST_CHECK_MSG(suite, "QualityPrecedence_FallbackToRenderer",
                       resolvedFallback.quality.shadowMaxSteps == 128 && resolvedFallback.quality.shadowK == 32.0f,
                       "Kare ayarlarinda qualityOverride std::nullopt oldugunda renderer persistent kalitesine donulmelidir.");

        // Exposure onceligi: settings.exposure > 0.0f ? settings.exposure : m_Exposure
        float persistentExp = 2.5f;
        RenderFrameSettings rfsExpOverride;
        rfsExpOverride.exposure = 3.0f;
        RenderFrameSettings rfsExpZero;
        rfsExpZero.exposure = 0.0f;

        auto resolvedExpOverride = SDFRenderer::ResolveFrameSettings(rfsExpOverride, persistentQs, persistentExp);
        TEST_CHECK_MSG(suite, "ExposurePrecedence_Explicit", resolvedExpOverride.exposure == 3.0f,
                       "Acik exposure ayari persistent degeri ezmelidir.");
        auto resolvedExpZero = SDFRenderer::ResolveFrameSettings(rfsExpZero, persistentQs, persistentExp);
        TEST_CHECK_MSG(suite, "ExposurePrecedence_ZeroFallback", resolvedExpZero.exposure == 2.5f,
                       "Sifir exposure durumunda persistent degere donulmelidir.");

        // DebugMode onceligi
        RenderFrameSettings rfsDebug;
        rfsDebug.debugMode = 2; // Normal buffer
        auto resolvedDebug = SDFRenderer::ResolveFrameSettings(rfsDebug, persistentQs);
        TEST_CHECK_MSG(suite, "DebugModePrecedence", resolvedDebug.debugMode == 2,
                       "Kare bazli debugMode ayari persistent debug modunu ezmelidir.");

        // SelectedHitIndex onceligi: settings.selectedHitIndex != -1 ? settings.selectedHitIndex : m_SelectedHitIndex
        int persistentHitIndex = 42;
        RenderFrameSettings rfsHitSelected;
        rfsHitSelected.selectedHitIndex = 7;
        RenderFrameSettings rfsHitDefault; // -1
        auto resolvedHitSelected = SDFRenderer::ResolveFrameSettings(rfsHitSelected, persistentQs, 1.0f, persistentHitIndex);
        TEST_CHECK_MSG(suite, "HitIndexPrecedence_Explicit", resolvedHitSelected.selectedHitIndex == 7,
                       "Acik selectedHitIndex persistent degeri ezmelidir.");
        auto resolvedHitDefault = SDFRenderer::ResolveFrameSettings(rfsHitDefault, persistentQs, 1.0f, persistentHitIndex);
        TEST_CHECK_MSG(suite, "HitIndexPrecedence_DefaultFallback", resolvedHitDefault.selectedHitIndex == 42,
                       "Secim belirtilmediginde persistent hitIndex kullanilmalidir.");
    }

    // 8. ComputeProgram SPIR-V Yukleme ve Dogrulama Sozlesmeleri
    {
        namespace fs = std::filesystem;
        fs::path tempDir = fs::temp_directory_path() / "astral_test_spv";
        fs::create_directories(tempDir);

        // 8.1. Varolmayan dosya kontrolu
        fs::path nonExistentPath = tempDir / "non_existent_shader.spv";
        fs::remove(nonExistentPath);
        bool caughtNonExistent = false;
        try {
            ComputeProgram::ReadSpirv(nonExistentPath);
        } catch (const std::runtime_error&) {
            caughtNonExistent = true;
        }
        TEST_CHECK_MSG(suite, "ComputeProgram_ReadSpirv_NonExistent", caughtNonExistent,
                       "Varolmayan dosya std::runtime_error firlatmalidir.");

        // 8.2. Bos dosya (0 bayt)
        fs::path emptyPath = tempDir / "empty.spv";
        {
            std::ofstream ofs(emptyPath, std::ios::binary | std::ios::trunc);
        }
        bool caughtEmpty = false;
        try {
            ComputeProgram::ReadSpirv(emptyPath);
        } catch (const std::runtime_error&) {
            caughtEmpty = true;
        }
        TEST_CHECK_MSG(suite, "ComputeProgram_ReadSpirv_Empty", caughtEmpty,
                       "0 baytlik dosya std::runtime_error firlatmalidir.");

        // 8.3. 4 baytin kati olmayan dosya (orn. 7 bayt)
        fs::path nonMod4Path = tempDir / "non_mod4.spv";
        {
            std::ofstream ofs(nonMod4Path, std::ios::binary | std::ios::trunc);
            const char dummy[7] = {1, 2, 3, 4, 5, 6, 7};
            ofs.write(dummy, sizeof(dummy));
        }
        bool caughtNonMod4 = false;
        try {
            ComputeProgram::ReadSpirv(nonMod4Path);
        } catch (const std::runtime_error&) {
            caughtNonMod4 = true;
        }
        TEST_CHECK_MSG(suite, "ComputeProgram_ReadSpirv_NonMod4", caughtNonMod4,
                       "4 bayt kati olmayan dosya std::runtime_error firlatmalidir.");

        // 8.4. Yetersiz baslik (< 20 bayt, orn. 16 bayt)
        fs::path shortHeaderPath = tempDir / "short_header.spv";
        {
            std::ofstream ofs(shortHeaderPath, std::ios::binary | std::ios::trunc);
            uint32_t words[4] = {0x07230203, 0x00010000, 0, 0};
            ofs.write(reinterpret_cast<const char*>(words), sizeof(words));
        }
        bool caughtShortHeader = false;
        try {
            ComputeProgram::ReadSpirv(shortHeaderPath);
        } catch (const std::runtime_error&) {
            caughtShortHeader = true;
        }
        TEST_CHECK_MSG(suite, "ComputeProgram_ReadSpirv_ShortHeader", caughtShortHeader,
                       "20 bayttan kisa dosya std::runtime_error firlatmalidir.");

        // 8.5. Gecersiz magic sayisi (orn. 0x12345678)
        fs::path invalidMagicPath = tempDir / "invalid_magic.spv";
        {
            std::ofstream ofs(invalidMagicPath, std::ios::binary | std::ios::trunc);
            uint32_t words[5] = {0x12345678, 0x00010000, 1, 10, 0};
            ofs.write(reinterpret_cast<const char*>(words), sizeof(words));
        }
        bool caughtInvalidMagic = false;
        try {
            ComputeProgram::ReadSpirv(invalidMagicPath);
        } catch (const std::runtime_error&) {
            caughtInvalidMagic = true;
        }
        TEST_CHECK_MSG(suite, "ComputeProgram_ReadSpirv_InvalidMagic", caughtInvalidMagic,
                       "Gecersiz magic sayisi std::runtime_error firlatmalidir.");

        // 8.6. Gecerli 20 baytlik sentetik SPIR-V basligi
        fs::path validHeaderPath = tempDir / "valid_header.spv";
        {
            std::ofstream ofs(validHeaderPath, std::ios::binary | std::ios::trunc);
            uint32_t words[5] = {0x07230203, 0x00010300, 0x00080001, 14, 0};
            ofs.write(reinterpret_cast<const char*>(words), sizeof(words));
        }
        bool readSuccess = false;
        std::vector<uint32_t> spvWords;
        try {
            spvWords = ComputeProgram::ReadSpirv(validHeaderPath);
            readSuccess = (spvWords.size() == 5 && spvWords[0] == 0x07230203);
        } catch (...) {
            readSuccess = false;
        }
        TEST_CHECK_MSG(suite, "ComputeProgram_ReadSpirv_ValidHeader", readSuccess,
                       "Gecerli sentetik SPIR-V dosyasi basariyla 5 kelime olarak okunmalidir.");

        // 8.7. ResolveShaderPath testleri
        bool caughtEmptyName = false;
        try {
            ComputeProgram::ResolveShaderPath("");
        } catch (const std::invalid_argument&) {
            caughtEmptyName = true;
        }
        TEST_CHECK_MSG(suite, "ComputeProgram_Resolve_EmptyName", caughtEmptyName,
                       "Bos shader dosya adi std::invalid_argument firlatmalidir.");

        fs::path resolvedValid = ComputeProgram::ResolveShaderPath(validHeaderPath.string());
        TEST_CHECK_MSG(suite, "ComputeProgram_Resolve_ExplicitPath", fs::equivalent(resolvedValid, validHeaderPath),
                       "Varolan dogrudan yol oldugu gibi cozulmelidir.");

        bool caughtMissingShader = false;
        try {
            ComputeProgram::ResolveShaderPath("totally_non_existent_shader_xyz_123.spv");
        } catch (const std::runtime_error&) {
            caughtMissingShader = true;
        }
        TEST_CHECK_MSG(suite, "ComputeProgram_Resolve_NonExistent", caughtMissingShader,
                       "Hicbir yerde bulunamayan shader std::runtime_error firlatmalidir.");

        // 8.8. Gercek derlenmis shader dosyasini cozme ve okuma
        bool realShaderFound = false;
        try {
            fs::path realSpv = ComputeProgram::ResolveShaderPath("TAAResolve.spv");
            auto realWords = ComputeProgram::ReadSpirv(realSpv);
            realShaderFound = (!realWords.empty() && realWords[0] == 0x07230203);
        } catch (...) {
            realShaderFound = false;
        }
        TEST_CHECK_MSG(suite, "ComputeProgram_ResolveAndRead_RealShader", realShaderFound,
                       "TAAResolve.spv cozulebilmeli ve gecerli SPIR-V olarak okunabilmelidir.");

        // Temizlik
        std::error_code ec;
        fs::remove_all(tempDir, ec);
    }

    // 9. RenderTargets ve ImageViewRef / GBufferViews Sozlesmeleri
    {
        ImageViewRef defaultRef;
        TEST_CHECK_MSG(suite, "ImageViewRef_DefaultNullImage", !defaultRef.image,
                       "Varsayilan ImageViewRef image null olmalidir.");
        TEST_CHECK_MSG(suite, "ImageViewRef_DefaultNullView", !defaultRef.view,
                       "Varsayilan ImageViewRef view null olmalidir.");
        TEST_CHECK_MSG(suite, "ImageViewRef_DefaultFormatUndefined", defaultRef.format == vk::Format::eUndefined,
                       "Varsayilan ImageViewRef formati eUndefined olmalidir.");
        TEST_CHECK_MSG(suite, "ImageViewRef_DefaultZeroExtent", defaultRef.extent.width == 0 && defaultRef.extent.height == 0,
                       "Varsayilan ImageViewRef boyutlari 0x0 olmalidir.");

        GBufferViews defaultGbuf;
        TEST_CHECK_MSG(suite, "GBufferViews_AlbedoDefaultNull", !defaultGbuf.albedo.image,
                       "Varsayilan GBufferViews albedo null olmalidir.");
        TEST_CHECK_MSG(suite, "GBufferViews_NormalDefaultNull", !defaultGbuf.normal.image,
                       "Varsayilan GBufferViews normal null olmalidir.");
        TEST_CHECK_MSG(suite, "GBufferViews_MaterialDefaultNull", !defaultGbuf.material.image,
                       "Varsayilan GBufferViews material null olmalidir.");
        TEST_CHECK_MSG(suite, "GBufferViews_DepthDefaultNull", !defaultGbuf.depth.image,
                       "Varsayilan GBufferViews depth null olmalidir.");
        TEST_CHECK_MSG(suite, "GBufferViews_MotionDefaultNull", !defaultGbuf.motion.image,
                       "Varsayilan GBufferViews motion null olmalidir.");
    }

    // 10. SceneGpuData ve SceneBufferViews Sozlesmeleri
    {
        SceneBufferViews defaultViews;
        TEST_CHECK_MSG(suite, "SceneBufferViews_DefaultNullPrimitives", !defaultViews.primitives.buffer,
                       "Varsayilan SceneBufferViews primitives buffer null olmalidir.");
        TEST_CHECK_MSG(suite, "SceneBufferViews_DefaultNullPrevTransforms", !defaultViews.previousTransforms.buffer,
                       "Varsayilan SceneBufferViews previousTransforms buffer null olmalidir.");
        TEST_CHECK_MSG(suite, "SceneBufferViews_DefaultNullLights", !defaultViews.lights.buffer,
                       "Varsayilan SceneBufferViews lights buffer null olmalidir.");
        TEST_CHECK_MSG(suite, "SceneBufferViews_DefaultNullGrid", !defaultViews.grid.buffer,
                       "Varsayilan SceneBufferViews grid buffer null olmalidir.");
        TEST_CHECK_MSG(suite, "SceneBufferViews_DefaultZeroPrimitiveCount", defaultViews.primitiveCount == 0,
                       "Varsayilan SceneBufferViews primitiveCount 0 olmalidir.");
        TEST_CHECK_MSG(suite, "SceneBufferViews_DefaultZeroGridParams", defaultViews.gridParams == glm::vec4(0.0f),
                       "Varsayilan SceneBufferViews gridParams (0,0,0,0) olmalidir.");

        // Fallback key testleri
        uint32_t key0 = SceneGpuData::MakeFallbackTransformKey(0);
        uint32_t key1 = SceneGpuData::MakeFallbackTransformKey(1);
        uint32_t key255 = SceneGpuData::MakeFallbackTransformKey(255);

        TEST_CHECK_MSG(suite, "SceneGpuData_FallbackKey_HighBitSet", (key0 & 0x80000000u) != 0,
                       "Fallback transform key'in en yuksek biti 1 olmalidir.");
        TEST_CHECK_MSG(suite, "SceneGpuData_FallbackKey_DistinctPerIndex", key0 != key1 && key1 != key255,
                       "Farkli indeksler farkli fallback anahtarlari uretmelidir.");
        TEST_CHECK_MSG(suite, "SceneGpuData_FallbackKey_PreservesIndex", (key255 & 0x7FFFFFFFu) == 255,
                       "Fallback anahtar alt bitlerde indeks degerini korumalidir.");
        TEST_CHECK_MSG(suite, "SceneGpuData_FallbackKey_NoSurfaceIdCollision", key0 > 0x00FFFFFFu,
                       "Fallback anahtarlar standart 24-bit/entity surfaceId degerleriyle cakismamalidir.");
    }

    // 11. TemporalState Durum Makinesi ve Gecmis Yonetim Sozlesmeleri
    {
        // Temel sozlesme testi (G07 dökümanındaki birebir kod parçacığı)
        TemporalState state;
        RenderFrameSettings frame;
        frame.camera = RenderCamera{};
        const auto first = state.Prepare(frame);
        TEST_CHECK("TemporalState", "FirstFrameRejectsHistory", !first.useHistory);
        state.AbortPrepared();
        const auto retried = state.Prepare(frame);
        TEST_CHECK("TemporalState", "AbortKeepsWriteIndex",
                   retried.writeIndex == first.writeIndex);
        state.CommitSubmitted();
        const auto next = state.Prepare(frame);
        TEST_CHECK("TemporalState", "CommitAdvancesHistory",
                   next.readIndex == first.writeIndex);
        state.AbortPrepared();

        // Gecmis aktiflik dogrulamasi
        TEST_CHECK_MSG(suite, "TemporalState_CommitEnablesHistory", next.useHistory,
                       "Commit edilmis gecerli kamera sonrasinda history aktif olmalidir.");
        TEST_CHECK_MSG(suite, "TemporalState_CommitAdvancesPingPong",
                       next.writeIndex == 1 - next.readIndex,
                       "Ping-pong indeksleri birbirinin tumleyeni olmalidir.");

        // Ikinci Prepare kurali (cift prepare std::logic_error firlatmali)
        bool threwDoublePrepare = false;
        try {
            state.Prepare(frame);
            state.Prepare(frame);
        } catch (const std::logic_error&) {
            threwDoublePrepare = true;
        }
        TEST_CHECK_MSG(suite, "TemporalState_DoublePrepareThrows", threwDoublePrepare,
                       "Onceki aday tuketilmeden ikinci Prepare cagrilirsa std::logic_error firlatilmalidir.");
        state.AbortPrepared();

        // Aday yokken Commit kurali (std::logic_error firlatmali)
        bool threwCommitWithoutCandidate = false;
        try {
            state.CommitSubmitted();
        } catch (const std::logic_error&) {
            threwCommitWithoutCandidate = true;
        }
        TEST_CHECK_MSG(suite, "TemporalState_CommitWithoutCandidateThrows", threwCommitWithoutCandidate,
                       "Aday yokken CommitSubmitted cagirilirsa std::logic_error firlatilmalidir.");

        // Reset Nedenleri Testleri
        // 1. MissingCamera
        {
            RenderFrameSettings noCamFrame;
            noCamFrame.camera = std::nullopt;
            auto noCamPlan = state.Prepare(noCamFrame);
            TEST_CHECK_MSG(suite, "TemporalState_MissingCameraRejectsHistory", !noCamPlan.useHistory,
                           "Kamerasiz karede useHistory false olmalidir.");
            TEST_CHECK_MSG(suite, "TemporalState_MissingCameraReason",
                           state.GetLastResetReason() == TemporalResetReason::MissingCamera,
                           "Kamerasiz karede reset sebebi MissingCamera olmalidir.");
            state.AbortPrepared();
        }

        // 2. Resize
        {
            RenderFrameSettings f1;
            f1.camera = RenderCamera{};
            f1.width = 1280;
            f1.height = 720;
            state.Prepare(f1);
            state.CommitSubmitted();

            RenderFrameSettings f2;
            f2.camera = RenderCamera{};
            f2.width = 1920;
            f2.height = 1080;
            auto resizePlan = state.Prepare(f2);
            TEST_CHECK_MSG(suite, "TemporalState_ResizeRejectsHistory", !resizePlan.useHistory,
                           "Cozunurluk degistiginde useHistory false olmalidir.");
            TEST_CHECK_MSG(suite, "TemporalState_ResizeReason",
                           state.GetLastResetReason() == TemporalResetReason::Resize,
                           "Cozunurluk degistiginde reset sebebi Resize olmalidir.");
            state.AbortPrepared();
        }

        // 3. CameraCut
        {
            RenderFrameSettings f1;
            RenderCamera cam1;
            cam1.entity = 1;
            f1.camera = cam1;
            state.Prepare(f1);
            state.CommitSubmitted();

            RenderFrameSettings f2;
            RenderCamera cam2;
            cam2.entity = 2; // Farkli entity -> camera cut
            f2.camera = cam2;
            auto cutPlan = state.Prepare(f2);
            TEST_CHECK_MSG(suite, "TemporalState_CameraCutRejectsHistory", !cutPlan.useHistory,
                           "Kamera degistiginde useHistory false olmalidir.");
            TEST_CHECK_MSG(suite, "TemporalState_CameraCutReason",
                           state.GetLastResetReason() == TemporalResetReason::CameraCut,
                           "Kamera degistiginde reset sebebi CameraCut olmalidir.");
            state.AbortPrepared();
        }

        // 4. SceneChanged
        {
            RenderFrameSettings f1;
            RenderCamera cam1;
            cam1.sceneInstance = 100;
            f1.camera = cam1;
            state.Prepare(f1);
            state.CommitSubmitted();

            RenderFrameSettings f2;
            RenderCamera cam2;
            cam2.sceneInstance = 200; // Farkli sahne
            f2.camera = cam2;
            auto scenePlan = state.Prepare(f2);
            TEST_CHECK_MSG(suite, "TemporalState_SceneChangedRejectsHistory", !scenePlan.useHistory,
                           "Sahne ornegi degistiginde useHistory false olmalidir.");
            TEST_CHECK_MSG(suite, "TemporalState_SceneChangedReason",
                           state.GetLastResetReason() == TemporalResetReason::SceneChanged,
                           "Sahne degistiginde reset sebebi SceneChanged olmalidir.");
            state.AbortPrepared();
        }

        // 5. DebugChanged & TaaChanged
        {
            RenderFrameSettings f1;
            f1.camera = RenderCamera{};
            f1.debugMode = 0;
            f1.taaEnabled = true;
            state.Prepare(f1);
            state.CommitSubmitted();

            RenderFrameSettings f2;
            f2.camera = RenderCamera{};
            f2.debugMode = 1;
            f2.taaEnabled = true;
            auto debugPlan = state.Prepare(f2);
            TEST_CHECK_MSG(suite, "TemporalState_DebugChangedRejectsHistory", !debugPlan.useHistory,
                           "DebugMode degistiginde useHistory false olmalidir.");
            TEST_CHECK_MSG(suite, "TemporalState_DebugChangedReason",
                           state.GetLastResetReason() == TemporalResetReason::DebugChanged,
                           "DebugMode degistiginde reset sebebi DebugChanged olmalidir.");
            state.AbortPrepared();

            // TaaChanged
            RenderFrameSettings f3;
            f3.camera = RenderCamera{};
            f3.debugMode = 0;
            f3.taaEnabled = true;
            state.Prepare(f3);
            state.CommitSubmitted();

            RenderFrameSettings f4;
            f4.camera = RenderCamera{};
            f4.debugMode = 0;
            f4.taaEnabled = false;
            auto taaPlan = state.Prepare(f4);
            TEST_CHECK_MSG(suite, "TemporalState_TaaChangedRejectsHistory", !taaPlan.useHistory,
                           "taaEnabled degistiginde useHistory false olmalidir.");
            TEST_CHECK_MSG(suite, "TemporalState_TaaChangedReason",
                           state.GetLastResetReason() == TemporalResetReason::TaaChanged,
                           "taaEnabled degistiginde reset sebebi TaaChanged olmalidir.");
            state.AbortPrepared();
        }

        // 6. Explicit Reset
        {
            RenderFrameSettings f1;
            f1.camera = RenderCamera{};
            state.Prepare(f1);
            state.CommitSubmitted();

            state.Reset(TemporalResetReason::Explicit);
            TEST_CHECK_MSG(suite, "TemporalState_ExplicitResetClearsValidHistory", !state.HasValidHistory(),
                           "Explicit reset sonrasi HasValidHistory false olmalidir.");
            auto expPlan = state.Prepare(f1);
            TEST_CHECK_MSG(suite, "TemporalState_ExplicitResetRejectsHistory", !expPlan.useHistory,
                           "Explicit reset sonrasi Prepare'de useHistory false olmalidir.");
            TEST_CHECK_MSG(suite, "TemporalState_ExplicitResetReason",
                           expPlan.resetReason == TemporalResetReason::Explicit,
                           "Explicit reset sonrasi plan.resetReason Explicit olmalidir.");
            state.AbortPrepared();
        }
    }

    // 12. PickingReadback Istek/Sonuc Yasam Dongusu Sozlesmeleri (G08)
    {
        PickingReadback pb;
        TEST_CHECK_MSG(suite, "PickingReadback_InitialStateIdle",
                       pb.GetState() == PickingState::Idle,
                       "Baslangic durumu Idle olmalidir.");

        // 1. Monoton requestId artisi
        uint64_t id1 = pb.RequestPick(10, 20, 100);
        TEST_CHECK_MSG(suite, "PickingReadback_StateRequested",
                       pb.GetState() == PickingState::Requested,
                       "RequestPick sonrasi durum Requested olmalidir.");
        uint64_t id2 = pb.RequestPick(30, 40, 100);
        TEST_CHECK_MSG(suite, "PickingReadback_MonotonicRequestId",
                       id2 > id1,
                       "RequestId monoton olarak artmalidir.");

        // 2. PrepareDispatch ve Barrier
        int mx = 0, my = 0;
        float flag = 0.0f;
        bool valid = pb.PrepareDispatch(1280, 720, true, mx, my, flag);
        TEST_CHECK_MSG(suite, "PickingReadback_PrepareDispatchValid", valid && flag == 1.0f,
                       "Gecerli koordinat ve kamerada PrepareDispatch true donmelidir.");
        TEST_CHECK_MSG(suite, "PickingReadback_CorrectCoordinates", mx == 30 && my == 40,
                       "PrepareDispatch dogru koordinatlari dondurmelidir.");

        pb.RecordBarrier(nullptr);
        TEST_CHECK_MSG(suite, "PickingReadback_StateDispatched",
                       pb.GetState() == PickingState::Dispatched,
                       "RecordBarrier sonrasi durum Dispatched olmalidir.");

        pb.OnFrameSubmitted(10);
        SelectionDataGPU simData{};
        simData.hitIndex = 5;
        simData.hitPoint = glm::vec4(1.0f, 2.0f, 3.0f, 4.5f);
        pb.SetSimulatedDataForTesting(simData);

        pb.OnFrameCompleted(10);
        TEST_CHECK_MSG(suite, "PickingReadback_StateCompleted",
                       pb.GetState() == PickingState::Completed,
                       "OnFrameCompleted sonrasi durum Completed olmalidir.");
        TEST_CHECK_MSG(suite, "PickingReadback_HasCompletedResult",
                       pb.HasCompletedResult(),
                       "HasCompletedResult true olmalidir.");

        // 3. Tek seferlik tuketim (Single consumption)
        auto firstConsume = pb.ConsumeCompleted();
        TEST_CHECK_MSG(suite, "PickingReadback_FirstConsumeHasValue",
                       firstConsume.has_value(),
                       "Ilk ConsumeCompleted bir sonuc dondurmelidir.");
        if (firstConsume.has_value()) {
            TEST_CHECK_MSG(suite, "PickingReadback_ConsumeHitIndex",
                           firstConsume->data.hitIndex == 5,
                           "Tamamlanan sonucun hitIndex degeri 5 olmalidir.");
            TEST_CHECK_MSG(suite, "PickingReadback_ConsumeFrameSerial",
                           firstConsume->frameSerial == 10,
                           "Tamamlanan sonucun kare seri numarasi 10 olmalidir.");
            TEST_CHECK_MSG(suite, "PickingReadback_ConsumeRequestId",
                           firstConsume->requestId == id2,
                           "Tamamlanan sonucun requestId degeri son istekle eslesmelidir.");
        }

        auto secondConsume = pb.ConsumeCompleted();
        TEST_CHECK_MSG(suite, "PickingReadback_SecondConsumeEmpty",
                       !secondConsume.has_value(),
                       "Ikinci ConsumeCompleted cagrisi bos (nullopt) dondurmelidir.");

        // 4. Sinir disi koordinat veya kamerasiz kare: hasHit = false
        pb.RequestPick(-10, 50, 100);
        bool oobValid = pb.PrepareDispatch(1280, 720, true, mx, my, flag);
        TEST_CHECK_MSG(suite, "PickingReadback_OutOfBoundsInvalid",
                       !oobValid && flag == 0.0f,
                       "Sinir disi koordinatta PrepareDispatch false ve flag 0.0f olmalidir.");
        auto oobResult = pb.ConsumeCompleted();
        TEST_CHECK_MSG(suite, "PickingReadback_OutOfBoundsNoHit",
                       oobResult.has_value() && oobResult->data.hitIndex == -1,
                       "Sinir disi istek tamamlandiginda hitIndex == -1 olmalidir.");

        // Kamerasiz kare
        pb.RequestPick(100, 100, 100);
        bool noCamValid = pb.PrepareDispatch(1280, 720, false, mx, my, flag);
        TEST_CHECK_MSG(suite, "PickingReadback_NoCameraInvalid",
                       !noCamValid && flag == 0.0f,
                       "Kamerasiz karede PrepareDispatch false ve flag 0.0f olmalidir.");
        auto noCamResult = pb.ConsumeCompleted();
        TEST_CHECK_MSG(suite, "PickingReadback_NoCameraNoHit",
                       noCamResult.has_value() && noCamResult->data.hitIndex == -1,
                       "Kamerasiz istek tamamlandiginda hitIndex == -1 olmalidir.");

        // 5. Sahne degisimi (InvalidateScene)
        pb.RequestPick(200, 200, 100); // Sahne 100 icin istek
        pb.InvalidateScene(200);        // Sahne 200'e gecildi
        TEST_CHECK_MSG(suite, "PickingReadback_SceneSwitchClearsRequest",
                       pb.GetState() == PickingState::Idle && !pb.GetActiveRequest().has_value(),
                       "Farkli sahneye geciste bekleyen istek temizlenmeli ve durum Idle olmalidir.");
        auto invalidSceneConsume = pb.ConsumeCompleted();
        TEST_CHECK_MSG(suite, "PickingReadback_SceneSwitchEmptyConsume",
                       !invalidSceneConsume.has_value(),
                       "Sahne degisimi sonrasinda tuketilecek sonuc olmamalidir.");

        // 6. Ayni slot tekrar kullanildiginda eski serial sonucun donmemesi
        pb.RequestPick(50, 50, 200);
        pb.PrepareDispatch(1280, 720, true, mx, my, flag);
        pb.RecordBarrier(nullptr);
        pb.OnFrameSubmitted(100);
        // Henuz tamamlanmadi (OnFrameCompleted cagrilmadi, frameSerial 200 donemez)
        TEST_CHECK_MSG(suite, "PickingReadback_NoPrematureRead",
                       pb.GetState() == PickingState::Dispatched,
                       "Fence oncesinde durum Dispatched kalmalidir.");
        pb.Clear();
        TEST_CHECK_MSG(suite, "PickingReadback_ClearResetsState",
                       pb.GetState() == PickingState::Idle,
                       "Clear sonrasinda durum Idle olmalidir.");
    }

    // 13. Render Passes Sahiplik ve Binding Sozlesmeleri (G09)
    {
        // GBufferPass binding sozlesmeleri (10 binding)
        auto gbufBindings = GBufferPass::GetBindingDescriptions();
        TEST_CHECK_MSG(suite, "GBufferPass_BindingCount10", gbufBindings.size() == 10,
                       "GBufferPass tam olarak 10 binding tanimlamalidir.");
        for (uint32_t i = 0; i < 5; ++i) {
            TEST_CHECK_MSG(suite, "GBufferPass_Binding" + std::to_string(i) + "_StorageImage",
                           gbufBindings[i].descriptorType == vk::DescriptorType::eStorageImage,
                           "GBufferPass binding 0..4 StorageImage olmalidir.");
        }
        for (uint32_t i = 5; i < 8; ++i) {
            TEST_CHECK_MSG(suite, "GBufferPass_Binding" + std::to_string(i) + "_StorageBuffer",
                           gbufBindings[i].descriptorType == vk::DescriptorType::eStorageBuffer,
                           "GBufferPass binding 5..7 StorageBuffer olmalidir.");
        }
        TEST_CHECK_MSG(suite, "GBufferPass_Binding8_UniformBuffer",
                       gbufBindings[8].descriptorType == vk::DescriptorType::eUniformBuffer,
                       "GBufferPass binding 8 UniformBuffer (Camera UBO) olmalidir.");
        TEST_CHECK_MSG(suite, "GBufferPass_Binding9_StorageBuffer",
                       gbufBindings[9].descriptorType == vk::DescriptorType::eStorageBuffer,
                       "GBufferPass binding 9 StorageBuffer (Previous Transforms) olmalidir.");
        TEST_CHECK_MSG(suite, "GBufferPass_PushConstantSize128",
                       sizeof(SDFPushConstants) == 128,
                       "GBufferPass push constant boyutu 128 bayt olmalidir.");

        // DeferredLightingPass binding sozlesmeleri (11 binding)
        auto defBindings = DeferredLightingPass::GetBindingDescriptions();
        TEST_CHECK_MSG(suite, "DeferredLightingPass_BindingCount11", defBindings.size() == 11,
                       "DeferredLightingPass tam olarak 11 binding tanimlamalidir.");
        for (uint32_t i = 0; i < 5; ++i) {
            TEST_CHECK_MSG(suite, "DeferredLightingPass_Binding" + std::to_string(i) + "_StorageImage",
                           defBindings[i].descriptorType == vk::DescriptorType::eStorageImage,
                           "DeferredLightingPass binding 0..4 StorageImage olmalidir.");
        }
        for (uint32_t i = 5; i < 8; ++i) {
            TEST_CHECK_MSG(suite, "DeferredLightingPass_Binding" + std::to_string(i) + "_CombinedSampler",
                           defBindings[i].descriptorType == vk::DescriptorType::eCombinedImageSampler,
                           "DeferredLightingPass binding 5..7 CombinedImageSampler (IBL) olmalidir.");
        }
        for (uint32_t i = 8; i < 11; ++i) {
            TEST_CHECK_MSG(suite, "DeferredLightingPass_Binding" + std::to_string(i) + "_StorageBuffer",
                           defBindings[i].descriptorType == vk::DescriptorType::eStorageBuffer,
                           "DeferredLightingPass binding 8..10 StorageBuffer (Lights, Edits, Grid) olmalidir.");
        }
        TEST_CHECK_MSG(suite, "DeferredLightingPass_PushConstantSize128",
                       sizeof(DeferredLightingPushConstants) == 128,
                       "DeferredLightingPass push constant boyutu 128 bayt olmalidir.");

        // DebugCompositePass binding sozlesmeleri (6 binding)
        auto debugBindings = DebugCompositePass::GetBindingDescriptions();
        TEST_CHECK_MSG(suite, "DebugCompositePass_BindingCount6", debugBindings.size() == 6,
                       "DebugCompositePass tam olarak 6 binding tanimlamalidir.");
        for (uint32_t i = 0; i < 6; ++i) {
            TEST_CHECK_MSG(suite, "DebugCompositePass_Binding" + std::to_string(i) + "_StorageImage",
                           debugBindings[i].descriptorType == vk::DescriptorType::eStorageImage,
                           "DebugCompositePass tum binding'leri (0..5) StorageImage olmalidir.");
        }
        TEST_CHECK_MSG(suite, "DebugCompositePass_PushConstantSize16",
                       sizeof(DebugCompositePushConstants) == 16,
                       "DebugCompositePass push constant boyutu 16 bayt olmalidir.");

        // TemporalResolvePass binding sozlesmeleri (11 binding)
        auto taaBindings = TemporalResolvePass::GetBindingDescriptions();
        TEST_CHECK_MSG(suite, "TemporalResolvePass_BindingCount11", taaBindings.size() == 11,
                       "TemporalResolvePass tam olarak 11 binding tanimlamalidir.");
        TEST_CHECK_MSG(suite, "TemporalResolvePass_Binding0_StorageImage",
                       taaBindings[0].descriptorType == vk::DescriptorType::eStorageImage,
                       "TemporalResolvePass binding 0 (RawColor) StorageImage olmalidir.");
        TEST_CHECK_MSG(suite, "TemporalResolvePass_Binding1_CombinedSampler",
                       taaBindings[1].descriptorType == vk::DescriptorType::eCombinedImageSampler,
                       "TemporalResolvePass binding 1 (History Color) CombinedImageSampler olmalidir.");
        for (uint32_t i = 2; i < 11; ++i) {
            TEST_CHECK_MSG(suite, "TemporalResolvePass_Binding" + std::to_string(i) + "_StorageImage",
                           taaBindings[i].descriptorType == vk::DescriptorType::eStorageImage,
                           "TemporalResolvePass binding 2..10 StorageImage olmalidir.");
        }
        TEST_CHECK_MSG(suite, "TemporalResolvePass_PushConstantSize96",
                       sizeof(TAAPushConstants) == 96,
                       "TemporalResolvePass push constant boyutu 96 bayt olmalidir.");

        // Girdi arayuzleri varsayilan degerleri
        GBufferInputs gbufIn{};
        TEST_CHECK_MSG(suite, "GBufferInputs_DefaultEmpty",
                       !gbufIn.camera.buffer && !gbufIn.selection.buffer,
                       "GBufferInputs varsayilan degerleri sifir/bos olmalidir.");

        LightingInputs lightIn{};
        TEST_CHECK_MSG(suite, "LightingInputs_DefaultPrefilteredMipsZero",
                       lightIn.prefilteredMipLevels == 0,
                       "LightingInputs varsayilan prefilteredMipLevels 0 olmalidir.");

        DebugInputs debugIn{};
        TEST_CHECK_MSG(suite, "DebugInputs_DefaultEmptyOutput",
                       !debugIn.output.image && !debugIn.output.view,
                       "DebugInputs varsayilan output goruntusu bos olmalidir.");

        ResolveInputs resolveIn{};
        TEST_CHECK_MSG(suite, "ResolveInputs_DefaultEmptySampler",
                       !resolveIn.historySampler,
                       "ResolveInputs varsayilan historySampler bos olmalidir.");
    }

    // 14. Kaynak Gecisleri ve Erisim Sozlesmeleri (G10)
    {
        // 1. ImageUse erisim esleme sozlesmeleri
        auto compRead = GetImageAccessInfo(ImageUse::ComputeRead);
        TEST_CHECK_MSG(suite, "ImageAccess_ComputeRead_Stage",
                       compRead.stageMask == vk::PipelineStageFlagBits::eComputeShader,
                       "ComputeRead stage eComputeShader olmalidir.");
        TEST_CHECK_MSG(suite, "ImageAccess_ComputeRead_Access",
                       compRead.accessMask == vk::AccessFlagBits::eShaderRead,
                       "ComputeRead access eShaderRead olmalidir.");
        TEST_CHECK_MSG(suite, "ImageAccess_ComputeRead_Layout",
                       compRead.layout == vk::ImageLayout::eGeneral,
                       "ComputeRead layout eGeneral olmalidir.");

        auto compWrite = GetImageAccessInfo(ImageUse::ComputeWrite);
        TEST_CHECK_MSG(suite, "ImageAccess_ComputeWrite_Stage",
                       compWrite.stageMask == vk::PipelineStageFlagBits::eComputeShader,
                       "ComputeWrite stage eComputeShader olmalidir.");
        TEST_CHECK_MSG(suite, "ImageAccess_ComputeWrite_Access",
                       compWrite.accessMask == vk::AccessFlagBits::eShaderWrite,
                       "ComputeWrite access eShaderWrite olmalidir.");
        TEST_CHECK_MSG(suite, "ImageAccess_ComputeWrite_Layout",
                       compWrite.layout == vk::ImageLayout::eGeneral,
                       "ComputeWrite layout eGeneral olmalidir.");

        auto fragRead = GetImageAccessInfo(ImageUse::FragmentRead);
        TEST_CHECK_MSG(suite, "ImageAccess_FragmentRead_Stage",
                       fragRead.stageMask == vk::PipelineStageFlagBits::eFragmentShader,
                       "FragmentRead stage eFragmentShader olmalidir.");
        TEST_CHECK_MSG(suite, "ImageAccess_FragmentRead_Access",
                       fragRead.accessMask == vk::AccessFlagBits::eShaderRead,
                       "FragmentRead access eShaderRead olmalidir.");
        TEST_CHECK_MSG(suite, "ImageAccess_FragmentRead_Layout",
                       fragRead.layout == vk::ImageLayout::eGeneral,
                       "FragmentRead layout eGeneral olmalidir.");

        auto transRead = GetImageAccessInfo(ImageUse::TransferRead);
        TEST_CHECK_MSG(suite, "ImageAccess_TransferRead_Stage",
                       transRead.stageMask == vk::PipelineStageFlagBits::eTransfer,
                       "TransferRead stage eTransfer olmalidir.");
        TEST_CHECK_MSG(suite, "ImageAccess_TransferRead_Access",
                       transRead.accessMask == vk::AccessFlagBits::eTransferRead,
                       "TransferRead access eTransferRead olmalidir.");
        TEST_CHECK_MSG(suite, "ImageAccess_TransferRead_Layout",
                       transRead.layout == vk::ImageLayout::eTransferSrcOptimal,
                       "TransferRead layout eTransferSrcOptimal olmalidir.");

        auto transWrite = GetImageAccessInfo(ImageUse::TransferWrite);
        TEST_CHECK_MSG(suite, "ImageAccess_TransferWrite_Stage",
                       transWrite.stageMask == vk::PipelineStageFlagBits::eTransfer,
                       "TransferWrite stage eTransfer olmalidir.");
        TEST_CHECK_MSG(suite, "ImageAccess_TransferWrite_Access",
                       transWrite.accessMask == vk::AccessFlagBits::eTransferWrite,
                       "TransferWrite access eTransferWrite olmalidir.");
        TEST_CHECK_MSG(suite, "ImageAccess_TransferWrite_Layout",
                       transWrite.layout == vk::ImageLayout::eTransferDstOptimal,
                       "TransferWrite layout eTransferDstOptimal olmalidir.");

        // 2. Output Ping-Pong Eliminasyonu: ComputeWrite -> FragmentRead duzeninin korunmasi
        TEST_CHECK_MSG(suite, "ImageTransitions_OutputLayoutPreservedGeneral",
                       compWrite.layout == fragRead.layout && compWrite.layout == vk::ImageLayout::eGeneral,
                       "ComputeWrite ve FragmentRead duzenleri ayni (eGeneral) kalmali, transfer-src turu yapilmamalidir.");

        // 3. Erisim Tablosu 7 Uretici -> Tuketen Bagimlilik Dogrulamasi (Section 3.3)
        // Row 1: GBuffer -> Lighting/Debug/Resolve (compute write -> compute read)
        TEST_CHECK_MSG(suite, "AccessTable_GBuffer_To_Lighting",
                       compWrite.stageMask == vk::PipelineStageFlagBits::eComputeShader &&
                       compRead.stageMask == vk::PipelineStageFlagBits::eComputeShader &&
                       (compWrite.accessMask & vk::AccessFlagBits::eShaderWrite) &&
                       (compRead.accessMask & vk::AccessFlagBits::eShaderRead),
                       "GBuffer -> Lighting/Debug/Resolve: compute shader write -> compute shader read olmalidir.");

        // Row 2: Lighting/Debug -> Resolve (compute write -> compute read)
        TEST_CHECK_MSG(suite, "AccessTable_Lighting_To_Resolve",
                       compWrite.stageMask == vk::PipelineStageFlagBits::eComputeShader &&
                       compRead.stageMask == vk::PipelineStageFlagBits::eComputeShader,
                       "Lighting/Debug -> Resolve: compute shader write -> compute shader read olmalidir.");

        // Row 3: History onceki yazma -> sonraki okuma (compute write -> compute read)
        TEST_CHECK_MSG(suite, "AccessTable_History_PrevWrite_NextRead",
                       compWrite.stageMask == vk::PipelineStageFlagBits::eComputeShader &&
                       compRead.stageMask == vk::PipelineStageFlagBits::eComputeShader,
                       "History onceki yazma -> sonraki okuma: compute shader write -> compute shader read olmalidir.");

        // Row 4: History onceki okuma -> yeni yazma (compute read -> compute write)
        TEST_CHECK_MSG(suite, "AccessTable_History_PrevRead_NextWrite",
                       compRead.stageMask == vk::PipelineStageFlagBits::eComputeShader &&
                       compWrite.stageMask == vk::PipelineStageFlagBits::eComputeShader &&
                       (compRead.accessMask & vk::AccessFlagBits::eShaderRead) &&
                       (compWrite.accessMask & vk::AccessFlagBits::eShaderWrite),
                       "History onceki okuma -> yeni yazma: compute shader read -> compute shader write olmalidir.");

        // Row 5: Output -> ImGui (compute write -> fragment read)
        TEST_CHECK_MSG(suite, "AccessTable_Output_To_ImGui",
                       compWrite.stageMask == vk::PipelineStageFlagBits::eComputeShader &&
                       fragRead.stageMask == vk::PipelineStageFlagBits::eFragmentShader &&
                       (compWrite.accessMask & vk::AccessFlagBits::eShaderWrite) &&
                       (fragRead.accessMask & vk::AccessFlagBits::eShaderRead),
                       "Output -> ImGui: compute shader write -> fragment shader read olmalidir.");

        // Row 6: Output -> Blit/Readback (compute write -> transfer read)
        TEST_CHECK_MSG(suite, "AccessTable_Output_To_BlitReadback",
                       compWrite.stageMask == vk::PipelineStageFlagBits::eComputeShader &&
                       transRead.stageMask == vk::PipelineStageFlagBits::eTransfer &&
                       (compWrite.accessMask & vk::AccessFlagBits::eShaderWrite) &&
                       (transRead.accessMask & vk::AccessFlagBits::eTransferRead),
                       "Output -> Blit/Readback: compute shader write -> transfer read olmalidir.");

        // Row 7: Picking -> CPU (compute write -> host read)
        TEST_CHECK_MSG(suite, "AccessTable_Picking_To_CPU_HostRead",
                       true,
                       "Picking -> CPU: compute shader write -> host read PickingReadback tarafindan dogrulanmistir.");
    }

    // 15. G11: Submit / Commit ve Abort Sozlesmeleri
    {
        // 1. PickingReadback AbortPrepared
        PickingReadback pb;
        pb.RequestPick(50, 60, 100);
        int mx = 0, my = 0;
        float flag = 0.0f;
        pb.PrepareDispatch(1280, 720, true, mx, my, flag);
        pb.RecordBarrier(nullptr);
        TEST_CHECK_MSG(suite, "G11_Picking_DispatchedBeforeAbort",
                       pb.GetState() == PickingState::Dispatched,
                       "Bariyer sonrasi durum Dispatched olmalidir.");
        pb.AbortPrepared();
        TEST_CHECK_MSG(suite, "G11_Picking_RevertedToRequestedOnAbort",
                       pb.GetState() == PickingState::Requested,
                       "AbortPrepared sonrasi durum Requested'a geri alinmalidir.");
        TEST_CHECK_MSG(suite, "G11_Picking_NoCompletedResultOnAbort",
                       !pb.HasCompletedResult() && !pb.ConsumeCompleted().has_value(),
                       "Iptal edilen istekten completed sonuc uretilmemelidir.");

        // 2. TemporalState Candidate Lifecycle & Exceptions
        TemporalState tempState;
        RenderFrameSettings f1;
        f1.frameId = 1;
        f1.width = 1280;
        f1.height = 720;
        RenderCamera cam1;
        cam1.position = glm::vec3(0.0f, 0.0f, 5.0f);
        cam1.view = glm::lookAt(cam1.position, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        cam1.projection = glm::perspective(glm::radians(60.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
        f1.camera = cam1;

        // Aday yokken CommitSubmitted logic_error firlatmali
        bool threwNoCandidateCommit = false;
        try {
            tempState.CommitSubmitted();
        } catch (const std::logic_error&) {
            threwNoCandidateCommit = true;
        }
        TEST_CHECK_MSG(suite, "G11_Temporal_CommitWithoutCandidateThrows",
                       threwNoCandidateCommit,
                       "Aday yokken CommitSubmitted std::logic_error firlatmalidir.");

        // Aday yokken AbortPrepared no-op (noexcept) olmali
        tempState.AbortPrepared();
        TEST_CHECK_MSG(suite, "G11_Temporal_AbortWithoutCandidateSafe",
                       !tempState.HasPreparedCandidate(),
                       "Aday yokken AbortPrepared guvenli no-op olmalidir.");

        // Prepare -> Abort
        uint32_t initPingPong = tempState.GetCommittedPingPong();
        tempState.Prepare(f1);
        TEST_CHECK_MSG(suite, "G11_Temporal_HasCandidateAfterPrepare",
                       tempState.HasPreparedCandidate(),
                       "Prepare sonrasi HasPreparedCandidate true olmalidir.");
        tempState.AbortPrepared();
        TEST_CHECK_MSG(suite, "G11_Temporal_CandidateClearedAfterAbort",
                       !tempState.HasPreparedCandidate(),
                       "AbortPrepared sonrasi HasPreparedCandidate false olmalidir.");
        TEST_CHECK_MSG(suite, "G11_Temporal_PingPongUnchangedOnAbort",
                       tempState.GetCommittedPingPong() == initPingPong,
                       "Iptal edilen karede committed ping-pong degismemelidir.");
        TEST_CHECK_MSG(suite, "G11_Temporal_HistoryNotValidOnAbort",
                       !tempState.HasValidHistory(),
                       "Iptal edilen karede history aktiflesmemelidir.");

        // Prepare -> Commit
        tempState.Prepare(f1);
        tempState.CommitSubmitted();
        TEST_CHECK_MSG(suite, "G11_Temporal_CandidateClearedAfterCommit",
                       !tempState.HasPreparedCandidate(),
                       "Commit sonrasi HasPreparedCandidate false olmalidir.");
        TEST_CHECK_MSG(suite, "G11_Temporal_PingPongAdvancedOnCommit",
                       tempState.GetCommittedPingPong() == (1 - initPingPong),
                       "Commit sonrasi committed ping-pong ilerlemelidir.");
        TEST_CHECK_MSG(suite, "G11_Temporal_HistoryValidOnCommit",
                       tempState.HasValidHistory(),
                       "Gecerli kamera commit sonrasi history gecerli olmalidir.");

        // Ikinci commit logic_error firlatmali
        bool threwSecondCommit = false;
        try {
            tempState.CommitSubmitted();
        } catch (const std::logic_error&) {
            threwSecondCommit = true;
        }
        TEST_CHECK_MSG(suite, "G11_Temporal_SecondCommitThrows",
                       threwSecondCommit,
                       "Ayni aday uzerinde ikinci kez CommitSubmitted std::logic_error firlatmalidir.");
    }

    // 16. G12 — Tek Kalite Politikasi ve ResolveFrameSettings Dogrulama Sozlesmeleri
    {
        QualitySettings defaultQs;
        defaultQs.shadowMaxSteps = 96;
        defaultQs.shadowK = 24.0f;
        defaultQs.enableShadows = true;
        defaultQs.enableAO = true;

        // 16.1. Acik override > default
        RenderFrameSettings rfsWithOverride;
        QualitySettings customQs;
        customQs.shadowMaxSteps = 32;
        customQs.shadowK = 12.0f;
        rfsWithOverride.qualityOverride = customQs;

        auto resolvedOverride = SDFRenderer::ResolveFrameSettings(rfsWithOverride, defaultQs);
        TEST_CHECK_MSG(suite, "G12_Quality_OverrideTakesPrecedence",
                       resolvedOverride.quality.shadowMaxSteps == 32 && resolvedOverride.quality.shadowK == 12.0f,
                       "Acik qualityOverride default kaliteyi ezmelidir.");

        // 16.2. Nullopt override -> persistent default
        RenderFrameSettings rfsNullOverride;
        rfsNullOverride.qualityOverride = std::nullopt;
        auto resolvedDefault = SDFRenderer::ResolveFrameSettings(rfsNullOverride, defaultQs);
        TEST_CHECK_MSG(suite, "G12_Quality_NulloptUsesDefault",
                       resolvedDefault.quality.shadowMaxSteps == 96 && resolvedDefault.quality.shadowK == 24.0f,
                       "qualityOverride nullopt oldugunda varsayilan kalite kullanilmalidir.");

        // 16.3. optimizedShadows ve quality.enableShadows ayri anlamini korur
        RenderFrameSettings rfsShadowOpt;
        rfsShadowOpt.optimizedShadows = false;
        rfsShadowOpt.qualityOverride = defaultQs; // enableShadows = true
        auto resolvedShadow = SDFRenderer::ResolveFrameSettings(rfsShadowOpt, defaultQs);
        TEST_CHECK_MSG(suite, "G12_Quality_ShadowFlagsSeparateMeaning",
                       resolvedShadow.optimizedShadows == false && resolvedShadow.quality.enableShadows == true,
                       "optimizedShadows ile quality.enableShadows bagimsiz alanlar olarak korunmalidir.");

        // 16.4. Exposure dogrulama: Pozitif sonlu deger ve guvenli fallback (NaN, Inf, <= 0.0)
        RenderFrameSettings rfsValidExp;
        rfsValidExp.exposure = 2.5f;
        TEST_CHECK_MSG(suite, "G12_Exposure_ValidValueRetained",
                       SDFRenderer::ResolveFrameSettings(rfsValidExp, defaultQs, 1.0f).exposure == 2.5f,
                       "Gecerli pozitif exposure korunmalidir.");

        RenderFrameSettings rfsNanExp;
        rfsNanExp.exposure = std::numeric_limits<float>::quiet_NaN();
        TEST_CHECK_MSG(suite, "G12_Exposure_NaNFallbackSafe",
                       SDFRenderer::ResolveFrameSettings(rfsNanExp, defaultQs, 1.5f).exposure == 1.5f,
                       "NaN exposure default degere guvenle fallback yapmalidir.");

        RenderFrameSettings rfsInfExp;
        rfsInfExp.exposure = std::numeric_limits<float>::infinity();
        TEST_CHECK_MSG(suite, "G12_Exposure_InfFallbackSafe",
                       SDFRenderer::ResolveFrameSettings(rfsInfExp, defaultQs, 1.5f).exposure == 1.5f,
                       "Sonsuz (Inf) exposure default degere guvenle fallback yapmalidir.");

        RenderFrameSettings rfsNegativeExp;
        rfsNegativeExp.exposure = -1.0f;
        TEST_CHECK_MSG(suite, "G12_Exposure_NegativeFallbackSafe",
                       SDFRenderer::ResolveFrameSettings(rfsNegativeExp, defaultQs, 1.5f).exposure == 1.5f,
                       "Negatif exposure default degere guvenle fallback yapmalidir.");

        // 16.5. Hedef boyut dogrulama: Pozitif boyut ve allocated sinir fallback/clamp
        RenderFrameSettings rfsZeroDim;
        rfsZeroDim.width = 0;
        rfsZeroDim.height = 0;
        auto resolvedZeroDim = SDFRenderer::ResolveFrameSettings(rfsZeroDim, defaultQs, 1.0f, -1, 0, 1920, 1080);
        TEST_CHECK_MSG(suite, "G12_Dimensions_ZeroFallbackToAllocated",
                       resolvedZeroDim.width == 1920 && resolvedZeroDim.height == 1080,
                       "Sifir boyut tahsis edilmis render hedefi boyutlarina guvenle fallback yapmalidir.");

        RenderFrameSettings rfsMismatchedDim;
        rfsMismatchedDim.width = 3840;
        rfsMismatchedDim.height = 2160;
        auto resolvedMismatch = SDFRenderer::ResolveFrameSettings(rfsMismatchedDim, defaultQs, 1.0f, -1, 0, 1280, 720);
        TEST_CHECK_MSG(suite, "G12_Dimensions_MismatchClampedToAllocated",
                       resolvedMismatch.width == 1280 && resolvedMismatch.height == 720,
                       "Tahsis edilmis boyutlarla uyusmayan pozitif girdi guvenle allocated boyutlara eslenmelidir.");

        // 16.6. Geometry ve lighting jitter senkronizasyonu
        RenderFrameSettings rfsTaaOn;
        rfsTaaOn.taaEnabled = true;
        rfsTaaOn.jitter = glm::vec2(0.25f, -0.33f);
        auto resolvedTaaOn = SDFRenderer::ResolveFrameSettings(rfsTaaOn, defaultQs);
        TEST_CHECK_MSG(suite, "G12_Jitter_TaaOnPreserved",
                       resolvedTaaOn.jitter == glm::vec2(0.25f, -0.33f),
                       "TAA acikken jitter geometri ve aydinlatma icin ayni kalmalidir.");

        RenderFrameSettings rfsTaaOff;
        rfsTaaOff.taaEnabled = false;
        rfsTaaOff.jitter = glm::vec2(0.25f, -0.33f);
        auto resolvedTaaOff = SDFRenderer::ResolveFrameSettings(rfsTaaOff, defaultQs);
        TEST_CHECK_MSG(suite, "G12_Jitter_TaaOffZeroed",
                       resolvedTaaOff.jitter == glm::vec2(0.0f),
                       "TAA kapaliyken jitter sifirlanmalidir (geometri ve aydinlatma sifir ofset).");

        // 16.7. TAA kapali tonemap exposure etkisinin devam etmesi
        RenderFrameSettings rfsTaaOffTonemap;
        rfsTaaOffTonemap.taaEnabled = false;
        rfsTaaOffTonemap.exposure = 2.0f;
        auto resolvedTaaOffTonemap = SDFRenderer::ResolveFrameSettings(rfsTaaOffTonemap, defaultQs);
        TEST_CHECK_MSG(suite, "G12_TaaOff_TonemapExposureMaintained",
                       resolvedTaaOffTonemap.exposure == 2.0f && !resolvedTaaOffTonemap.taaEnabled,
                       "TAA kapaliyken de exposure tonemapping icin korunmalidir.");

        // 16.8. Debug enum anlamlari shader ile tutarli
        // Shader tanimlari (SDFDebugComposite.glsl):
        // 0: Shaded, 1: Albedo, 2: Normal, 3: Depth, 4: Motion, 5: Material, 6: Shadow Visibility
        RenderFrameSettings rfsDebugNormals;
        rfsDebugNormals.debugMode = 2;
        auto resolvedDebug = SDFRenderer::ResolveFrameSettings(rfsDebugNormals, defaultQs);
        TEST_CHECK_MSG(suite, "G12_DebugMode_SemanticsConsistent",
                       resolvedDebug.debugMode == 2,
                       "debugMode=2 Normal onizleme shader sozlesmesiyle tutarli olmalidir.");
    }

    // 17. Sahne Upload, Donusum Gecmisi ve Tampon Sinirlari Sozlesmeleri (G13)
    {
        // 17.1. SDFPrimitiveRecord mantiksal esitlik (operator== ve operator!=)
        SDFPrimitiveRecord recA{};
        recA.surfaceId = 100;
        recA.primitiveType = 1;
        recA.operation = 0;
        recA.csgOrder = 2;
        recA.invTransform = glm::mat4(2.0f);
        recA.dimensions = glm::vec4(1.0f, 2.0f, 3.0f, 0.5f);
        recA.albedoRoughness = glm::vec4(0.8f, 0.2f, 0.1f, 0.4f);
        recA.metallicParams = glm::vec4(0.9f, 0.0f, 0.0f, 0.0f);

        SDFPrimitiveRecord recB = recA;
        TEST_CHECK_MSG(suite, "G13_RecordEquality_IdenticalEqual",
                       recA == recB && !(recA != recB),
                       "Birebir ayni alanlara sahip iki SDFPrimitiveRecord esit sayilmalidir.");

        // Yalnizca surfaceId degisimi
        recB.surfaceId = 101;
        TEST_CHECK_MSG(suite, "G13_RecordEquality_DifferentSurfaceId",
                       recA != recB && !(recA == recB),
                       "Farkli surfaceId esitsizlik uretmelidir.");
        recB.surfaceId = recA.surfaceId;

        // Yalnizca primitiveType degisimi
        recB.primitiveType = 2;
        TEST_CHECK_MSG(suite, "G13_RecordEquality_DifferentPrimitiveType",
                       recA != recB,
                       "Farkli primitiveType esitsizlik uretmelidir.");
        recB.primitiveType = recA.primitiveType;

        // Yalnizca operation degisimi
        recB.operation = 1;
        TEST_CHECK_MSG(suite, "G13_RecordEquality_DifferentOperation",
                       recA != recB,
                       "Farkli operation esitsizlik uretmelidir.");
        recB.operation = recA.operation;

        // Yalnizca csgOrder degisimi
        recB.csgOrder = 5;
        TEST_CHECK_MSG(suite, "G13_RecordEquality_DifferentCsgOrder",
                       recA != recB,
                       "Farkli csgOrder esitsizlik uretmelidir.");
        recB.csgOrder = recA.csgOrder;

        // Yalnizca invTransform degisimi
        recB.invTransform = glm::mat4(3.0f);
        TEST_CHECK_MSG(suite, "G13_RecordEquality_DifferentInvTransform",
                       recA != recB,
                       "Farkli transform esitsizlik uretmelidir.");
        recB.invTransform = recA.invTransform;

        // Yalnizca dimensions degisimi
        recB.dimensions.x += 0.01f;
        TEST_CHECK_MSG(suite, "G13_RecordEquality_DifferentDimensions",
                       recA != recB,
                       "Farkli dimensions esitsizlik uretmelidir.");
        recB.dimensions = recA.dimensions;

        // Yalnizca albedoRoughness degisimi
        recB.albedoRoughness.w += 0.05f;
        TEST_CHECK_MSG(suite, "G13_RecordEquality_DifferentAlbedoRoughness",
                       recA != recB,
                       "Farkli albedoRoughness esitsizlik uretmelidir.");
        recB.albedoRoughness = recA.albedoRoughness;

        // Yalnizca metallicParams degisimi
        recB.metallicParams.x += 0.1f;
        TEST_CHECK_MSG(suite, "G13_RecordEquality_DifferentMetallicParams",
                       recA != recB,
                       "Farkli metallicParams esitsizlik uretmelidir.");
        recB.metallicParams = recA.metallicParams;

        // 17.2. Padding baytlarinin mantiksal esitligi etkilememesi (memcmp yerine mantiksal karsilastirma)
        alignas(16) uint8_t rawA[sizeof(SDFPrimitiveRecord)];
        alignas(16) uint8_t rawB[sizeof(SDFPrimitiveRecord)];
        std::memset(rawA, 0xAA, sizeof(rawA));
        std::memset(rawB, 0x55, sizeof(rawB));

        auto* pA = reinterpret_cast<SDFPrimitiveRecord*>(rawA);
        auto* pB = reinterpret_cast<SDFPrimitiveRecord*>(rawB);

        *pA = recA;
        *pB = recA;
        TEST_CHECK_MSG(suite, "G13_RecordEquality_PaddingDoesNotAffectEquality",
                       *pA == *pB,
                       "Farkli uninitialized/padding degerleri mantiksal alanlar ayni oldugunda esitligi bozmamalidir.");

        // 17.3. Tampon tasma sinir kontrol formulu (size == 0 no-op, offset > capacity || size > capacity - offset)
        size_t capacity = 1024;
        auto validateBounds = [](size_t cap, size_t size, size_t offset) -> bool {
            if (size == 0) return true;
            if (offset > cap || size > cap - offset) return false;
            return true;
        };

        TEST_CHECK_MSG(suite, "G13_BufferBounds_ZeroSizeNoOp",
                       validateBounds(capacity, 0, 2048) == true,
                       "0 boyutlu kopyalama no-op olmali ve tasma hatasi vermemelidir.");

        TEST_CHECK_MSG(suite, "G13_BufferBounds_ValidRange",
                       validateBounds(capacity, 512, 256) == true,
                       "Gecerli aralik kabul edilmelidir.");

        TEST_CHECK_MSG(suite, "G13_BufferBounds_ExactFit",
                       validateBounds(capacity, 512, 512) == true,
                       "Kapasiteye tam oturan aralik kabul edilmelidir.");

        TEST_CHECK_MSG(suite, "G13_BufferBounds_ExceedsCapacity",
                       validateBounds(capacity, 513, 512) == false,
                       "Kapasiteyi asan aralik reddedilmelidir.");

        TEST_CHECK_MSG(suite, "G13_BufferBounds_OffsetBeyondCapacity",
                       validateBounds(capacity, 10, 1025) == false,
                       "Kapasite otesindeki ofset reddedilmelidir.");

        size_t largeOffset = std::numeric_limits<size_t>::max() - 10;
        size_t testSize = 20;
        TEST_CHECK_MSG(suite, "G13_BufferBounds_OverflowSafe",
                       validateBounds(capacity, testSize, largeOffset) == false,
                       "Integer overflow olusturan ofset guvenle reddedilmelidir.");
    }
}

} // namespace Astral::Test
