#include "TestFramework.hpp"
#include "AstralEngine.h"
#include "Astral/Renderer/SDFRenderer.hpp"
#include "Astral/Renderer/RenderTargets.hpp"
#include "Astral/Renderer/SceneGpuData.hpp"
#include "Astral/Renderer/Buffer.hpp"
#include "Astral/Renderer/SDFEdit.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>
#include <cmath>

namespace {
using namespace Astral;

std::vector<unsigned char> Readback(VulkanContext& context, const SDFRenderer& renderer) {
    const size_t bytes = size_t(renderer.GetWidth()) * renderer.GetHeight() * 4;
    Buffer buffer(context.GetDevice(), context.GetPhysicalDevice(), bytes,
                  vk::BufferUsageFlagBits::eTransferDst,
                  vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    context.ExecuteImmediate([&](vk::CommandBuffer cmd) {
        TransitionImage(cmd, renderer.GetStorageImage(),
            ImageUse::FragmentRead, ImageUse::TransferRead);

        vk::BufferImageCopy region{};
        region.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1};
        region.imageExtent = vk::Extent3D(renderer.GetWidth(), renderer.GetHeight(), 1);
        cmd.copyImageToBuffer(renderer.GetStorageImage(), vk::ImageLayout::eTransferSrcOptimal,
                              buffer.GetBuffer(), region);

        TransitionImage(cmd, renderer.GetStorageImage(),
            ImageUse::TransferRead, ImageUse::FragmentRead);
    });
    const auto* data = static_cast<const unsigned char*>(buffer.GetMappedData());
    return {data, data + bytes};
}

void RenderFrame(VulkanContext& context, SDFRenderer& renderer, uint32_t frameIndex,
                 bool useGrid = true, bool enableTAA = false) {
    context.ExecuteImmediate([&](vk::CommandBuffer cmd) {
        renderer.Render(cmd, static_cast<float>(frameIndex) / 60.0f, 0,
                        renderer.GetWidth(), renderer.GetHeight(),
                        useGrid, true, enableTAA, frameIndex);
    });
    renderer.CommitSubmittedFrame();
}

RenderCamera CreateCamera(int width, int height, const glm::vec3& pos = {0.0f, 0.0f, 4.0f}) {
    RenderCamera cam;
    cam.position = pos;
    cam.forward = glm::vec3(0.0f, 0.0f, -1.0f);
    cam.right = glm::vec3(1.0f, 0.0f, 0.0f);
    cam.up = glm::vec3(0.0f, 1.0f, 0.0f);
    cam.projection = glm::perspective(glm::radians(60.0f),
                                      static_cast<float>(width) / static_cast<float>(height),
                                      0.01f, 50.0f);
    cam.view = glm::lookAt(cam.position, cam.position + cam.forward, cam.up);
    return cam;
}

void RunLifecycleTests() {
    const std::string suite = "RendererLifecycle";
    std::cout << "  [INFO] RendererLifecycleGpuTests baslatiliyor...\n";

    AppConfig config;
    config.width = 64;
    config.height = 64;
    config.maxFrames = 1;
    config.enableTAA = false;

    class TestHarness final : public Application {
    public:
        explicit TestHarness(const AppConfig& cfg) : Application(cfg) {}
    } app(config);

    app.Run();

    auto* pRenderer = app.GetRenderer();
    auto* pContext = app.GetVulkanContext();

    if (!pRenderer || !pContext) {
        TEST_CHECK_MSG(suite, "HarnessInit", false, "Renderer veya VulkanContext baslatilamadi!");
        return;
    }

    auto& renderer = *pRenderer;
    auto& context = *pContext;

    // 1. Ilk sahne kurulumu: Merkezde kure
    LegacySDFEdit sphereEdit;
    sphereEdit.position = glm::vec3(0.0f, 0.0f, 0.0f);
    sphereEdit.scale = glm::vec3(1.0f);
    sphereEdit.primitiveType = 0; // Sphere
    sphereEdit.operation = 0;     // Union
    sphereEdit.albedo = glm::vec3(0.8f, 0.2f, 0.2f);
    sphereEdit.roughness = 0.4f;
    renderer.UpdateEdits({sphereEdit});

    auto cam = CreateCamera(64, 64);
    renderer.SetCamera(cam);

    // =========================================================================
    // 2. Test: Secim Tuketime Dayali Yasam Dongusu (SelectionConsumedOnce)
    // =========================================================================
    {
        renderer.SetPickingRequest(32, 32);
        RenderFrame(context, renderer, 1, false, false);

        auto firstPick = renderer.ConsumeSelectionResult();
        TEST_CHECK_MSG(suite, "SelectionFirstHasHit", firstPick.hasHit,
                       "Merkez pikseldeki kure ilk okumada secilmis olmalidir.");

        auto secondPick = renderer.ConsumeSelectionResult();
        TEST_CHECK_MSG(suite, "SelectionConsumedOnce", !secondPick.hasHit,
                       "ConsumeSelectionResult sonrasi ikinci okumada hasHit=false olmalidir.");
    }

    // =========================================================================
    // 3. Test: Kamerasiz Cikti Opak Siyah Olmali (MissingCameraProducesOpaqueBlack)
    // =========================================================================
    {
        // Once kamerayla bir kare ciz
        renderer.SetCamera(cam);
        RenderFrame(context, renderer, 2, false, false);

        // Kamerayi kaldir ve kamerasiz ciz
        renderer.SetCamera(std::nullopt);
        RenderFrame(context, renderer, 3, false, false);

        auto blackPixels = Readback(context, renderer);
        bool allOpaqueBlack = !blackPixels.empty();
        for (size_t i = 0; i < blackPixels.size(); i += 4) {
            if (blackPixels[i + 0] != 0 || blackPixels[i + 1] != 0 ||
                blackPixels[i + 2] != 0 || blackPixels[i + 3] != 255) {
                allOpaqueBlack = false;
                break;
            }
        }
        TEST_CHECK_MSG(suite, "MissingCameraProducesOpaqueBlack", allOpaqueBlack,
                       "Kamerasiz render ciktisinda tum pikseller RGB=0 ve A=255 olmalidir.");

        // Kamerayi geri yukle ve normal cizimin devam ettigini dogrula
        renderer.SetCamera(cam);
        RenderFrame(context, renderer, 4, false, false);
        auto restoredPixels = Readback(context, renderer);
        bool notAllBlack = false;
        for (size_t i = 0; i < restoredPixels.size(); i += 4) {
            if (restoredPixels[i + 0] > 0 || restoredPixels[i + 1] > 0 || restoredPixels[i + 2] > 0) {
                notAllBlack = true;
                break;
            }
        }
        TEST_CHECK_MSG(suite, "CameraRestoredProducesOutput", notAllBlack,
                       "Kamera geri yuklendikten sonra cikti siyaha kilitli kalmamalidir.");
    }

    // =========================================================================
    // 4. Test: Resize Dizisi ve 8'in Kati Olmayan Dispatch (ResizeSequence)
    // =========================================================================
    {
        // 64x64 -> 127x65 -> 64x64
        renderer.Resize(127, 65);
        TEST_CHECK_MSG(suite, "Resize127x65Width", renderer.GetWidth() == 127,
                       "Resize sonrasi genislik 127 olmalidir.");
        TEST_CHECK_MSG(suite, "Resize127x65Height", renderer.GetHeight() == 65,
                       "Resize sonrasi yukseklik 65 olmalidir.");

        auto cam127 = CreateCamera(127, 65);
        renderer.SetCamera(cam127);
        RenderFrame(context, renderer, 10, true, false);

        auto pixels127 = Readback(context, renderer);
        TEST_CHECK_MSG(suite, "Resize127x65ReadbackSize",
                       pixels127.size() == 127 * 65 * 4,
                       "127x65 cozunurlukte okunan piksel tampon boyutu dogru olmalidir.");

        // Sag ve alt kenar pikselleri (x=126, y=64) bellek disina tasmiyor ve gecerli opak alfalari var
        size_t brIndex = (64 * 127 + 126) * 4;
        bool edgeValid = (brIndex + 3 < pixels127.size()) && (pixels127[brIndex + 3] == 255);
        TEST_CHECK_MSG(suite, "Resize127x65EdgePixelValid", edgeValid,
                       "8'in kati olmayan dispatch boyutunda (127x65) sag/alt kenar gecerli olmalidir.");

        // 64x64'e geri don
        renderer.Resize(64, 64);
        TEST_CHECK_MSG(suite, "ResizeBackTo64x64Width", renderer.GetWidth() == 64,
                       "Geri donuste genislik 64 olmalidir.");
        TEST_CHECK_MSG(suite, "ResizeBackTo64x64Height", renderer.GetHeight() == 64,
                       "Geri donuste yukseklik 64 olmalidir.");

        renderer.SetCamera(cam);
        RenderFrame(context, renderer, 11, true, false);
        auto pixels64 = Readback(context, renderer);
        TEST_CHECK_MSG(suite, "ResizeRestoredReadbackValid",
                       pixels64.size() == 64 * 64 * 4 && pixels64[3] == 255,
                       "64x64 cozunurluge donus sonrasi cikti saglam olmalidir.");
    }

    // =========================================================================
    // 5. Test: TAA Acik -> Kapali -> Acik Gecisleri (TaaTransitionCycle)
    // =========================================================================
    {
        // TAA Acik
        for (uint32_t f = 0; f < 3; ++f) {
            glm::vec2 jitter = glm::vec2((f % 2) * 0.5f - 0.25f, (f % 3) / 3.0f - 0.333333f);
            renderer.SetCamera(cam, jitter);
            RenderFrame(context, renderer, 20 + f, true, true);
        }
        auto pTaaOn = Readback(context, renderer);
        TEST_CHECK_MSG(suite, "TaaEnabledValid", !pTaaOn.empty() && pTaaOn[3] == 255,
                       "TAA acikken render basarili olmalidir.");

        // TAA Kapali
        renderer.SetCamera(cam, glm::vec2(0.0f));
        RenderFrame(context, renderer, 23, true, false);
        auto pTaaOff = Readback(context, renderer);
        TEST_CHECK_MSG(suite, "TaaDisabledValid", !pTaaOff.empty() && pTaaOff[3] == 255,
                       "TAA kapatildiginda render basarili olmalidir.");

        // TAA Yeniden Acik
        renderer.SetCamera(cam, glm::vec2(0.2f, -0.2f));
        RenderFrame(context, renderer, 24, true, true);
        auto pTaaReOn = Readback(context, renderer);
        TEST_CHECK_MSG(suite, "TaaReenabledValid", !pTaaReOn.empty() && pTaaReOn[3] == 255,
                       "TAA tekrar acildiginda render basarili olmalidir.");
    }

    // =========================================================================
    // 6. Test: Debug Modlari Gecisi (DebugModeCycle)
    // =========================================================================
    {
        // Modlar: 0: Shaded, 1: Albedo, 2: Normal, 3: Depth, 4: Motion, 5: Material, ardindan 0'a donus
        for (int m : {0, 1, 2, 3, 4, 5, 0}) {
            renderer.SetDebugMode(m);
            TEST_CHECK_MSG(suite, "DebugModeGetter", renderer.GetDebugMode() == m,
                           "SetDebugMode sonrasi GetDebugMode degeri eslesmelidir.");

            renderer.SetCamera(cam);
            RenderFrame(context, renderer, 30 + m, false, false);

            auto dbgPixels = Readback(context, renderer);
            bool modeValid = (dbgPixels.size() == 64 * 64 * 4) && (dbgPixels[3] == 255);
            TEST_CHECK_MSG(suite, "DebugModeRenderValid", modeValid,
                           "Debug modunda render ciktisi opak ve gecerli boyutta olmalidir.");
        }
    }

    // =========================================================================
    // 7. Test: Sahne Gecisi ve Temporal Gecmis Tasinmama (SceneTransitionNoHistoryBleed)
    // =========================================================================
    {
        // Sahne A: Parlak kirmizi kure
        LegacySDFEdit redSphere;
        redSphere.position = glm::vec3(0.0f, 0.0f, 0.0f);
        redSphere.scale = glm::vec3(1.2f);
        redSphere.albedo = glm::vec3(1.0f, 0.0f, 0.0f);
        redSphere.roughness = 0.5f;
        renderer.UpdateEdits({redSphere});
        renderer.ResetTemporalHistory();

        for (uint32_t f = 0; f < 3; ++f) {
            renderer.SetCamera(cam, glm::vec2((f % 2) * 0.2f, (f % 3) * 0.2f));
            RenderFrame(context, renderer, 40 + f, false, true);
        }

        auto redPixels = Readback(context, renderer);
        size_t centerIdx = (32 * 64 + 32) * 4;
        bool isRedDominant = (redPixels[centerIdx] > redPixels[centerIdx + 1] + 30) &&
                             (redPixels[centerIdx] > redPixels[centerIdx + 2] + 30);
        TEST_CHECK_MSG(suite, "SceneARedDominant", isRedDominant,
                       "Sahne A merkez pikselinde kirmizi renk baskin olmalidir.");

        // Bos sahneye gecis ve reset
        renderer.UpdateEdits(std::vector<LegacySDFEdit>{});
        renderer.ResetTemporalHistory();
        renderer.SetCamera(cam, glm::vec2(0.0f));
        RenderFrame(context, renderer, 43, false, true);

        // Sahne B: Parlak mavi kure
        LegacySDFEdit blueSphere;
        blueSphere.position = glm::vec3(0.0f, 0.0f, 0.0f);
        blueSphere.scale = glm::vec3(1.2f);
        blueSphere.albedo = glm::vec3(0.0f, 0.0f, 1.0f);
        blueSphere.roughness = 0.5f;
        renderer.UpdateEdits({blueSphere});
        renderer.ResetTemporalHistory();

        renderer.SetCamera(cam, glm::vec2(0.1f, -0.1f));
        RenderFrame(context, renderer, 44, false, true);

        auto bluePixels = Readback(context, renderer);
        bool isBlueDominant = (bluePixels[centerIdx + 2] > bluePixels[centerIdx] + 30) &&
                              (bluePixels[centerIdx + 2] > bluePixels[centerIdx + 1] + 30);
        TEST_CHECK_MSG(suite, "SceneBBlueDominantNoBleed", isBlueDominant,
                       "Sahne B'de mavi renk baskin olmali, Sahne A'nin kirmizisi tasinmamalidir.");
    }

    // =========================================================================
    // 8. Test: Legacy ve Yeni Explicit RenderFrameSettings Esdegerligi (G03)
    // =========================================================================
    {
        // Standart test sahnesi: Merkezde parlak yesil kure
        LegacySDFEdit greenSphere;
        greenSphere.position = glm::vec3(0.0f, 0.0f, 0.0f);
        greenSphere.scale = glm::vec3(1.0f);
        greenSphere.albedo = glm::vec3(0.1f, 0.9f, 0.2f);
        greenSphere.roughness = 0.3f;
        renderer.UpdateEdits({greenSphere});
        renderer.ResetTemporalHistory();
        renderer.SetCamera(cam, glm::vec2(0.0f));

        // 1. Legacy cagri ile ciz ve oku
        context.ExecuteImmediate([&](vk::CommandBuffer cmd) {
            renderer.Render(cmd, 1.0f, 0, renderer.GetWidth(), renderer.GetHeight(),
                            true, true, false, 50);
        });
        renderer.CommitSubmittedFrame();
        auto legacyPixels = Readback(context, renderer);

        // Sahneyi ve gecmisi sifirla, ayni durumu yeniden kur
        renderer.ResetTemporalHistory();
        renderer.SetCamera(cam, glm::vec2(0.0f));

        // 2. Yeni acik RenderFrameSettings cagrisi ile ciz ve oku
        RenderFrameSettings settings;
        settings.camera = cam;
        settings.jitter = glm::vec2(0.0f);
        settings.frameId = 50;
        settings.width = static_cast<uint32_t>(renderer.GetWidth());
        settings.height = static_cast<uint32_t>(renderer.GetHeight());
        settings.normalMode = 0;
        settings.debugMode = 0;
        settings.selectedHitIndex = -1;
        settings.useGrid = true;
        settings.optimizedShadows = true;
        settings.taaEnabled = false;
        settings.exposure = 1.0f;

        context.ExecuteImmediate([&](vk::CommandBuffer cmd) {
            renderer.Render(cmd, settings);
        });
        renderer.CommitSubmittedFrame();
        auto explicitPixels = Readback(context, renderer);

        TEST_CHECK_MSG(suite, "LegacyExplicitPixelSizeMatch", legacyPixels.size() == explicitPixels.size(),
                       "Legacy ve explicit readback boyutlari eslesmelidir.");

        size_t diffCount = 0;
        for (size_t i = 0; i < legacyPixels.size(); ++i) {
            if (legacyPixels[i] != explicitPixels[i]) {
                diffCount++;
            }
        }

        TEST_CHECK_MSG(suite, "LegacyVsExplicitPixelIdentity", diffCount == 0,
                       "Legacy Render(...) ve yeni Render(cmd, settings) %100 bayt duzeyinde ozdes sonuc uretmelidir.");
    }

    // =========================================================================
    // 9. Test: RenderTargets RAII Alloc/Dealloc ve Bellek Sayaci (G05)
    // =========================================================================
    {
        uint32_t statsBefore = context.GetMemoryStats().total.statistics.allocationCount;

        // 9.1. Gecersiz boyut kontrolu (0 boyut std::invalid_argument firlatmalidir)
        bool caughtZeroDim = false;
        try {
            RenderTargets invalidZero(context, 0, 64);
        } catch (const std::invalid_argument&) {
            caughtZeroDim = true;
        }
        TEST_CHECK_MSG(suite, "RenderTargets_ZeroWidthThrows", caughtZeroDim,
                       "Genislik 0 oldugunda std::invalid_argument firlatilmalidir.");

        uint32_t statsAfterZero = context.GetMemoryStats().total.statistics.allocationCount;
        TEST_CHECK_MSG(suite, "RenderTargets_ZeroDimNoLeak", statsAfterZero == statsBefore,
                       "Hata firlatan olusturma hicbir VMA tahsisi sizdirmamalidir.");

        // 9.2. Gecerli RenderTargets olusturma: tam 11 goruntu tahsis edilmeli
        {
            auto targets = std::make_unique<RenderTargets>(context, 64, 64);
            uint32_t statsActive = context.GetMemoryStats().total.statistics.allocationCount;
            TEST_CHECK_MSG(suite, "RenderTargets_AllocCount11", statsActive == statsBefore + 11,
                           "Yeni RenderTargets olusturuldugunda tam 11 VMA goruntusu tahsis edilmelidir.");

            auto gbuf = targets->GetGBuffer();
            TEST_CHECK_MSG(suite, "RenderTargets_OutputValid", targets->GetOutput().image && targets->GetOutput().view && targets->GetOutput().format == vk::Format::eR8G8B8A8Unorm,
                           "Output goruntusu R8G8B8A8_UNORM formatinda gecerli olmalidir.");
            TEST_CHECK_MSG(suite, "RenderTargets_RawColorValid", targets->GetRawColor().image && targets->GetRawColor().format == vk::Format::eR16G16B16A16Sfloat,
                           "RawColor goruntusu R16G16B16A16_SFLOAT formatinda gecerli olmalidir.");
            TEST_CHECK_MSG(suite, "RenderTargets_HistoryColorValid", targets->GetHistoryColor(0).image && targets->GetHistoryColor(1).image,
                           "Tarihce renk tamponlari gecerli olmalidir.");
            TEST_CHECK_MSG(suite, "RenderTargets_HistoryExtraValid", targets->GetHistoryExtra(0).image && targets->GetHistoryExtra(1).image && targets->GetHistoryExtra(0).format == vk::Format::eR32G32B32A32Uint,
                           "Tarihce ekstra tamponlari R32G32B32A32_UINT formatinda gecerli olmalidir.");
            TEST_CHECK_MSG(suite, "RenderTargets_GBufferValid", gbuf.albedo.image && gbuf.normal.image && gbuf.material.image && gbuf.depth.image && gbuf.motion.image,
                           "Tum G-Buffer hedefleri gecerli olmalidir.");
            TEST_CHECK_MSG(suite, "RenderTargets_MaterialFormatRGBA32UI", gbuf.material.format == vk::Format::eR32G32B32A32Uint,
                           "GBuffer Material formati RGBA32UI sozlesmesine uygun olmalidir.");

            // Yok edildiginde net olarak baslangic allocationCount degerine donmeli (0 sizinti)
            targets.reset();
        }

        uint32_t statsAfterDestroy = context.GetMemoryStats().total.statistics.allocationCount;
        TEST_CHECK_MSG(suite, "RenderTargets_RAII_ZeroLeak", statsAfterDestroy == statsBefore,
                       "RenderTargets RAII yikicisi calistiktan sonra VMA tahsis sayisi baslangic seviyesine donmelidir (0 sizinti).");
    }

    // =========================================================================
    // 10. Test: Sıfır / Negatif Resize Eski Hedefi Korumalı (G05)
    // =========================================================================
    {
        uint32_t prevW = static_cast<uint32_t>(renderer.GetWidth());
        uint32_t prevH = static_cast<uint32_t>(renderer.GetHeight());

        renderer.Resize(0, 64);
        TEST_CHECK_MSG(suite, "ResizeZeroWidthPreserved", static_cast<uint32_t>(renderer.GetWidth()) == prevW && static_cast<uint32_t>(renderer.GetHeight()) == prevH,
                       "Genislik 0 girildiginde eski boyut ve hedefler korunmalidir.");

        renderer.Resize(64, 0);
        TEST_CHECK_MSG(suite, "ResizeZeroHeightPreserved", static_cast<uint32_t>(renderer.GetWidth()) == prevW && static_cast<uint32_t>(renderer.GetHeight()) == prevH,
                       "Yukseklik 0 girildiginde eski boyut ve hedefler korunmalidir.");

        renderer.Resize(-10, -20);
        TEST_CHECK_MSG(suite, "ResizeNegativePreserved", static_cast<uint32_t>(renderer.GetWidth()) == prevW && static_cast<uint32_t>(renderer.GetHeight()) == prevH,
                       "Negatif boyutlarda eski boyut ve hedefler korunmalidir.");

        // Render'in sorunsuz devam ettigi dogrulanir
        RenderFrame(context, renderer, 55, true, false);
        auto checkPixels = Readback(context, renderer);
        TEST_CHECK_MSG(suite, "ResizeInvalidPreservedRenderSuccess", !checkPixels.empty() && checkPixels[3] == 255,
                       "Gecersiz resize denemeleri sonrasinda render kesintisiz calismalidir.");
    }

    // =========================================================================
    // 11. Test: Art arda 50 Resize Stres Testi (G05)
    // =========================================================================
    {
        uint32_t statsBeforeStress = context.GetMemoryStats().total.statistics.allocationCount;
        bool allResizesSuccess = true;

        for (int r = 0; r < 50; ++r) {
            int w = (r % 2 == 0) ? 64 : 80;
            int h = (r % 2 == 0) ? 64 : 72;
            try {
                renderer.Resize(w, h);
                if (r % 10 == 0) {
                    RenderFrame(context, renderer, static_cast<uint32_t>(100 + r), true, false);
                }
            } catch (...) {
                allResizesSuccess = false;
                break;
            }
        }

        // 64x64'e dondur
        renderer.Resize(64, 64);
        RenderFrame(context, renderer, 160, true, false);
        auto finalPixels = Readback(context, renderer);

        TEST_CHECK_MSG(suite, "Resize50StressSuccess", allResizesSuccess,
                       "Art arda 50 resize dongusu basariyla tamamlanmalidir.");
        TEST_CHECK_MSG(suite, "Resize50StressPixelValid", !finalPixels.empty() && finalPixels[3] == 255,
                       "50 resize sonrasinda render ciktisi gecerli olmalidir.");

        uint32_t statsAfterStress = context.GetMemoryStats().total.statistics.allocationCount;
        TEST_CHECK_MSG(suite, "Resize50StressMemoryStable", statsAfterStress == statsBeforeStress,
                       "50 resize sonrasinda VMA tahsis sayisi baslangic seviyesiyle birebir ayni olmalidir (bellek birikmesi yok).");
    }

    // =========================================================================
    // 12. Test: SceneGpuData Yasam Dongusu, Kapasite ve Hareket Eden Primitifler (G06)
    // =========================================================================
    {
        // Dogrudan SceneGpuData nesnesi uzerinde GPU testi
        SceneGpuData sceneData(context, true);

        // 12.1. 0 / 1 / Kapasite siniri (256) ve asimi (300) kayit
        SDFChangeSet emptyChangeSet{};
        
        // 0 kayit
        sceneData.Upload(std::span<const SDFPrimitiveRecord>(), emptyChangeSet);
        TEST_CHECK_MSG(suite, "SceneGpuData_0Records", sceneData.GetActiveEditCount() == 0,
                       "0 kayit yuklendiginde activeEditCount 0 olmalidir.");
        TEST_CHECK_MSG(suite, "SceneGpuData_0Records_Views", sceneData.GetViews().primitiveCount == 0,
                       "0 kayit yuklendiginde SceneBufferViews::primitiveCount 0 olmalidir.");

        // 1 kayit
        SDFPrimitiveRecord rec1{};
        rec1.primitiveType = 0; // Sphere
        rec1.dimensions = glm::vec4(1.0f); // radius = 1
        rec1.surfaceId = 101;
        rec1.invTransform = glm::mat4(1.0f);
        sceneData.Upload(std::span<const SDFPrimitiveRecord>(&rec1, 1), emptyChangeSet);
        TEST_CHECK_MSG(suite, "SceneGpuData_1Record", sceneData.GetActiveEditCount() == 1,
                       "1 kayit yuklendiginde activeEditCount 1 olmalidir.");

        // Kapasite asimi (300 kayit) -> MAX_SDF_EDITS (256) sinirina clamp edilmeli
        std::vector<SDFPrimitiveRecord> rec300(300);
        for (size_t i = 0; i < 300; ++i) {
            rec300[i].primitiveType = 0;
            rec300[i].surfaceId = static_cast<uint32_t>(i + 1);
            rec300[i].invTransform = glm::mat4(1.0f);
        }
        sceneData.Upload(std::span<const SDFPrimitiveRecord>(rec300.data(), rec300.size()), emptyChangeSet);
        TEST_CHECK_MSG(suite, "SceneGpuData_ClampToMaxEdits", sceneData.GetActiveEditCount() == MAX_SDF_EDITS,
                       "300 kayit MAX_SDF_EDITS (256) sinirina clamp edilmelidir.");
        TEST_CHECK_MSG(suite, "SceneGpuData_ViewsMatchClampedCount", sceneData.GetViews().primitiveCount == MAX_SDF_EDITS,
                       "SceneBufferViews primitiveCount MAX_SDF_EDITS ile eslesmelidir.");

        // 12.2. Hareket eden primitif (Previous Transform takibi)
        // Kare 1: surfaceId=42, P1=(10, 0, 0)
        sceneData.ResetTransformHistory();
        glm::mat4 t1 = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 0.0f, 0.0f));
        SDFPrimitiveRecord movingRec{};
        movingRec.primitiveType = 0;
        movingRec.surfaceId = 42;
        movingRec.invTransform = glm::inverse(t1);
        sceneData.Upload(std::span<const SDFPrimitiveRecord>(&movingRec, 1), emptyChangeSet);

        // Frame 1'de GPU prevTransform tamponuna ilk dunya matrisi (t1) yazilmis olmali
        const auto* prevMatMapped = static_cast<const glm::mat4*>(sceneData.GetPrevTransformBuffer()->GetMappedData());
        TEST_CHECK_MSG(suite, "SceneGpuData_MovingPrim_Frame1", prevMatMapped != nullptr && prevMatMapped[0] == t1,
                       "Kare 1'de onceki transform mevcut konum (t1) ile baslatilmalidir.");
        sceneData.CommitSubmitted();

        // Kare 2: surfaceId=42, P2=(20, 0, 0)
        glm::mat4 t2 = glm::translate(glm::mat4(1.0f), glm::vec3(20.0f, 0.0f, 0.0f));
        movingRec.invTransform = glm::inverse(t2);
        sceneData.Upload(std::span<const SDFPrimitiveRecord>(&movingRec, 1), emptyChangeSet);

        // Frame 2'de GPU prevTransform tamponuna t1 yazilmis olmali
        TEST_CHECK_MSG(suite, "SceneGpuData_MovingPrim_Frame2", prevMatMapped[0] == t1,
                       "Kare 2'de onceki transform Kare 1'in konumu (t1) olmalidir.");
        sceneData.CommitSubmitted();

        // Kare 3: surfaceId=42, P3=(30, 0, 0)
        glm::mat4 t3 = glm::translate(glm::mat4(1.0f), glm::vec3(30.0f, 0.0f, 0.0f));
        movingRec.invTransform = glm::inverse(t3);
        sceneData.Upload(std::span<const SDFPrimitiveRecord>(&movingRec, 1), emptyChangeSet);

        // Frame 3'te GPU prevTransform tamponuna t2 yazilmis olmali
        TEST_CHECK_MSG(suite, "SceneGpuData_MovingPrim_Frame3", prevMatMapped[0] == t2,
                       "Kare 3'te onceki transform Kare 2'nin konumu (t2) olmalidir.");
        sceneData.CommitSubmitted();

        // 12.3. Silinen ve yeniden eklenen kimlik (Empty sahne gecisiyle reset)
        // Kare 4: Bos sahne yukle (surfaceId=42 silindi)
        sceneData.Upload(std::span<const SDFPrimitiveRecord>(), emptyChangeSet);
        TEST_CHECK_MSG(suite, "SceneGpuData_ClearedHistoryOnEmpty", sceneData.GetActiveEditCount() == 0,
                       "Bos sahne yuklendiginde activeEditCount 0 olmalidir.");
        sceneData.CommitSubmitted();

        // Kare 5: surfaceId=42 tekrar P5=(50, 0, 0) olarak eklenir
        glm::mat4 t5 = glm::translate(glm::mat4(1.0f), glm::vec3(50.0f, 0.0f, 0.0f));
        movingRec.invTransform = glm::inverse(t5);
        sceneData.Upload(std::span<const SDFPrimitiveRecord>(&movingRec, 1), emptyChangeSet);

        // Gecmis temizlendigi icin eski t2/t3 degil, yeni konumu t5 olmalidir
        TEST_CHECK_MSG(suite, "SceneGpuData_DeletedAndReaddedIdentity", prevMatMapped[0] == t5,
                       "Silinip yeniden eklenen nesne bayat gecmis yerine yeni konumunu (t5) almali.");
        sceneData.CommitSubmitted();

        // 12.4. Legacy map vs persistent map GPU esdegerligi
        SceneGpuData legacyData(context, false);
        SceneGpuData persistentData(context, true);

        std::vector<SDFPrimitiveRecord> testRecords(10);
        for (size_t i = 0; i < 10; ++i) {
            testRecords[i].primitiveType = static_cast<uint32_t>(i % 3);
            testRecords[i].surfaceId = static_cast<uint32_t>(200 + i);
            testRecords[i].dimensions = glm::vec4(1.0f, 2.0f, 3.0f, 4.0f);
            testRecords[i].invTransform = glm::translate(glm::mat4(1.0f), glm::vec3(float(i), 0.0f, 0.0f));
        }

        legacyData.Upload(testRecords, emptyChangeSet, true); // legacyMap = true
        persistentData.Upload(testRecords, emptyChangeSet, false); // persistentMap = false

        // Buffer iceriklerinin bayt duzeyinde karsilastirilmasi
        void* legacyMapped = nullptr;
        VkResult mapRes = vmaMapMemory(context.GetAllocator(), legacyData.GetEditBuffer()->GetAllocation(), &legacyMapped);
        const void* persistentBytes = persistentData.GetEditBuffer()->GetMappedData();
        int cmpResult = -1;
        if (mapRes == VK_SUCCESS && legacyMapped && persistentBytes) {
            cmpResult = std::memcmp(legacyMapped, persistentBytes, 10 * sizeof(SDFPrimitiveRecord));
        }
        if (mapRes == VK_SUCCESS && legacyMapped) {
            vmaUnmapMemory(context.GetAllocator(), legacyData.GetEditBuffer()->GetAllocation());
        }
        TEST_CHECK_MSG(suite, "SceneGpuData_LegacyVsPersistentMapEquivalence", cmpResult == 0,
                       "Legacy map ve persistent map ile yuklenen veriler GPU tamponunda bayt bayt ozdes olmalidir.");

        // 12.5. Camera UBO Upload ve Readback
        CameraUBOData camData{};
        camData.currViewProj = glm::translate(glm::mat4(1.0f), glm::vec3(1, 2, 3));
        camData.prevViewProj = glm::translate(glm::mat4(1.0f), glm::vec3(4, 5, 6));
        camData.prevCameraPosition = glm::vec4(7, 8, 9, 0);
        camData.jitter = glm::vec4(0.25f, -0.25f, 0.1f, -0.1f);
        sceneData.UploadCamera(camData);

        const auto* camMapped = static_cast<const CameraUBOData*>(sceneData.GetCameraUBO()->GetMappedData());
        bool camUboMatches = (camMapped != nullptr && std::memcmp(camMapped, &camData, sizeof(CameraUBOData)) == 0);
        TEST_CHECK_MSG(suite, "SceneGpuData_CameraUBOUpload", camUboMatches,
                       "UploadCamera ile yazilan CameraUBOData bellekte birebir eslesmelidir.");

        // 12.6. SetLights ve LightBufferHeader
        LightGPU customLight{};
        customLight.position = glm::vec4(1.0f, 2.0f, 3.0f, 1.0f);
        customLight.direction = glm::vec4(0.0f, -1.0f, 0.0f, 5.0f);
        customLight.color = glm::vec4(1.0f, 0.5f, 0.2f, 15.0f);

        std::vector<LightGPU> lightsArray = { customLight };
        sceneData.SetLights(lightsArray);

        const auto* lightHeaderMapped = static_cast<const LightBufferHeader*>(sceneData.GetLightBuffer()->GetMappedData());
        const auto* lightArrayMapped = reinterpret_cast<const LightGPU*>(
            static_cast<const char*>(sceneData.GetLightBuffer()->GetMappedData()) + sizeof(LightBufferHeader)
        );

        TEST_CHECK_MSG(suite, "SceneGpuData_SetLights_HeaderCount",
                       lightHeaderMapped != nullptr && lightHeaderMapped->lightCount == 1,
                       "SetLights sonrasi LightBufferHeader lightCount 1 olmalidir.");
        TEST_CHECK_MSG(suite, "SceneGpuData_SetLights_DataValid",
                       lightArrayMapped != nullptr && lightArrayMapped[0].color == customLight.color,
                       "SetLights sonrasi yuklenen isik verisi dogru olmalidir.");
    }

    // =========================================================================
    // 13. G11 Submit / Commit ve Abort Sozlesmesi GPU Testi
    // =========================================================================
    {
        std::cout << "[Test 13] G11 Submit / Commit ve Abort Sozlesmesi..." << std::endl;
        SDFRenderer renderer(context, "", 320, 240);
        auto cam = CreateCamera(320, 240);

        // Aday yokken CommitSubmittedFrame cagrisi std::logic_error firlatmali
        bool caughtCommitWithoutCandidate = false;
        try {
            renderer.CommitSubmittedFrame();
        } catch (const std::logic_error&) {
            caughtCommitWithoutCandidate = true;
        }
        TEST_CHECK_MSG(suite, "G11_Facade_CommitWithoutCandidateThrows", caughtCommitWithoutCandidate,
                       "Aday yokken CommitSubmittedFrame std::logic_error firlatmalidir.");

        // Aday yokken AbortPreparedFrame no-op (noexcept) olmali
        renderer.AbortPreparedFrame();
        TEST_CHECK_MSG(suite, "G11_Facade_AbortWithoutCandidateSafe", !renderer.HasPreparedCandidate(),
                       "Aday yokken AbortPreparedFrame guvenli no-op olmalidir.");

        // 1. Kare: Sahne kurulumu, Render komut kaydi
        LegacySDFEdit edit1;
        edit1.position = glm::vec3(0.0f, 0.0f, 0.0f);
        edit1.scale = glm::vec3(1.0f);
        edit1.albedo = glm::vec3(0.5f);
        renderer.UpdateEdits({edit1});
        renderer.SetCamera(cam, glm::vec2(0.0f));

        context.ExecuteImmediate([&](vk::CommandBuffer cmd) {
            renderer.Render(cmd, 0.0f, 0, 320, 240, true, true, false, 1);
        });
        TEST_CHECK_MSG(suite, "G11_Facade_CandidatePreparedAfterRender", renderer.HasPreparedCandidate(),
                       "Render sonrasi HasPreparedCandidate true olmalidir.");

        // Kare 1: Abort senaryosu (simule edilmis submit basarisizligi)
        renderer.AbortPreparedFrame();
        TEST_CHECK_MSG(suite, "G11_Facade_CandidateClearedOnAbort", !renderer.HasPreparedCandidate(),
                       "AbortPreparedFrame sonrasi HasPreparedCandidate false olmalidir.");
        TEST_CHECK_MSG(suite, "G11_Facade_HistoryNotValidAfterAbort",
                       renderer.GetTemporalState() && !renderer.GetTemporalState()->HasValidHistory(),
                       "Iptal edilen kare sonrasinda history gecerli olmamalidir.");

        // 2. Kare: Basarili kayit ve Commit
        context.ExecuteImmediate([&](vk::CommandBuffer cmd) {
            renderer.Render(cmd, 0.0f, 0, 320, 240, true, true, false, 2);
        });
        renderer.CommitSubmittedFrame();
        TEST_CHECK_MSG(suite, "G11_Facade_CandidateClearedOnCommit", !renderer.HasPreparedCandidate(),
                       "Commit sonrasi HasPreparedCandidate false olmalidir.");
        TEST_CHECK_MSG(suite, "G11_Facade_HistoryValidAfterCommit",
                       renderer.GetTemporalState() && renderer.GetTemporalState()->HasValidHistory(),
                       "Commit edilmis gecerli kare sonrasinda history aktiflesmelidir.");

        // Ikinci kez Commit cagrilmasi std::logic_error firlatmali
        bool caughtDoubleCommit = false;
        try {
            renderer.CommitSubmittedFrame();
        } catch (const std::logic_error&) {
            caughtDoubleCommit = true;
        }
        TEST_CHECK_MSG(suite, "G11_Facade_DoubleCommitThrows", caughtDoubleCommit,
                       "Ayni kare uzerinde ikinci kez CommitSubmittedFrame std::logic_error firlatmalidir.");
    }

    // =========================================================================
    // 14. G13 Sahne Upload Optimizasyonu, Dilim Yukleme ve Tampon Sinirlari GPU Testi
    // =========================================================================
    {
        std::cout << "[Test 14] G13 Sahne Upload Optimizasyonu ve Dilim Yukleme..." << std::endl;
        SceneGpuData sceneData(context, true);
        SDFChangeSet emptyChangeSet;

        // 14.1. Buffer Tasma ve 0-Boyut Sinir Dogrulama (Live GPU Buffer)
        Buffer* editBuffer = sceneData.GetEditBuffer();
        vk::DeviceSize cap = editBuffer->GetSize();
        uint8_t dummyByte = 0xFF;

        // 0-boyut no-op, tasma veya hata olmamali
        bool zeroSizeThrew = false;
        try {
            editBuffer->UpdateData(&dummyByte, 0, cap + 100);
        } catch (...) {
            zeroSizeThrew = true;
        }
        TEST_CHECK_MSG(suite, "G13_Buffer_ZeroSizeNoOp", !zeroSizeThrew,
                       "UpdateData ile size=0 cagrisi ofset kapasiteyi assa bile no-op olmali ve firlatmamalidir.");

        // Ofset kapasiteyi asarsa std::out_of_range
        bool offsetExceedsThrew = false;
        try {
            editBuffer->UpdateData(&dummyByte, 1, cap + 1);
        } catch (const std::out_of_range&) {
            offsetExceedsThrew = true;
        }
        TEST_CHECK_MSG(suite, "G13_Buffer_OffsetExceedsThrows", offsetExceedsThrew,
                       "Kapasiteyi asan ofset std::out_of_range firlatmalidir.");

        // Boyut kapasiteyi asarsa std::out_of_range
        bool sizeExceedsThrew = false;
        try {
            editBuffer->UpdateData(&dummyByte, cap + 1, 0);
        } catch (const std::out_of_range&) {
            sizeExceedsThrew = true;
        }
        TEST_CHECK_MSG(suite, "G13_Buffer_SizeExceedsThrows", sizeExceedsThrew,
                       "Kapasiteyi asan boyut std::out_of_range firlatmalidir.");

        // Integer overflow guvenligi (SIZE_MAX - 5 ofset)
        bool overflowThrew = false;
        try {
            editBuffer->UpdateData(&dummyByte, 10, std::numeric_limits<size_t>::max() - 5);
        } catch (const std::out_of_range&) {
            overflowThrew = true;
        }
        TEST_CHECK_MSG(suite, "G13_Buffer_IntegerOverflowSafe", overflowThrew,
                       "Integer overflow olusturan devasa ofset std::out_of_range firlatmalidir.");

        // 14.2. Sahne Upload Bayt Takibi ve Dilim Yuklemesi
        sceneData.ResetTransformHistory();

        // 3 adet primitif hazirla (surfaceId 10, 20, 30)
        std::vector<SDFPrimitiveRecord> recs(3);
        glm::mat4 tA = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 0.0f, 0.0f));
        glm::mat4 tB = glm::translate(glm::mat4(1.0f), glm::vec3(20.0f, 0.0f, 0.0f));
        glm::mat4 tC = glm::translate(glm::mat4(1.0f), glm::vec3(30.0f, 0.0f, 0.0f));

        recs[0].surfaceId = 10; recs[0].primitiveType = 0; recs[0].invTransform = glm::inverse(tA);
        recs[1].surfaceId = 20; recs[1].primitiveType = 0; recs[1].invTransform = glm::inverse(tB);
        recs[2].surfaceId = 30; recs[2].primitiveType = 0; recs[2].invTransform = glm::inverse(tC);

        // Kare 1: Ilk yukleme (Full upload)
        sceneData.Upload(recs, emptyChangeSet);
        size_t expectedPrimBytes = 3 * sizeof(SDFPrimitiveRecord);
        size_t expectedTransBytes = 3 * sizeof(glm::mat4);
        TEST_CHECK_MSG(suite, "G13_Upload_Frame1_FullUpload",
                       sceneData.GetLastPrimitiveUploadBytes() == expectedPrimBytes &&
                       sceneData.GetLastTransformUploadBytes() == expectedTransBytes,
                       "Ilk karede tum primitifler ve transformlar tam yuklenmelidir.");
        sceneData.CommitSubmitted();

        // Kare 2: Statik sahne (Hicbir nesne degismedi) -> 0 bayt yukleme
        sceneData.Upload(recs, emptyChangeSet);
        TEST_CHECK_MSG(suite, "G13_Upload_Frame2_StaticZeroUpload",
                       sceneData.GetLastPrimitiveUploadBytes() == 0 &&
                       sceneData.GetLastTransformUploadBytes() == 0,
                       "Statik sahnede ardil karede 0 bayt primitif ve 0 bayt transform yuklenmelidir.");
        sceneData.CommitSubmitted();

        // Kare 3: Yalnizca recs[1] (surfaceId 20) hareket etti
        glm::mat4 tB_new = glm::translate(glm::mat4(1.0f), glm::vec3(25.0f, 0.0f, 0.0f));
        recs[1].invTransform = glm::inverse(tB_new);
        sceneData.Upload(recs, emptyChangeSet);

        TEST_CHECK_MSG(suite, "G13_Upload_Frame3_SinglePrimitiveSliceUpload",
                       sceneData.GetLastPrimitiveUploadBytes() == sizeof(SDFPrimitiveRecord),
                       "Tek bir primitif degistiginde yalnizca o primitifin baytlari yuklenmelidir.");
        sceneData.CommitSubmitted();

        // Kare 4: recs[1] ayni yeni konumunda durdu (statiklesme)
        // Primitif degismedigi icin primitive upload 0 bayt olmali,
        // ancak onceki transformu tB'den tB_new'e gectigi icin transform dilimi yuklenmeli
        sceneData.Upload(recs, emptyChangeSet);
        TEST_CHECK_MSG(suite, "G13_Upload_Frame4_TransformProgressionSlice",
                       sceneData.GetLastPrimitiveUploadBytes() == 0 &&
                       sceneData.GetLastTransformUploadBytes() == sizeof(glm::mat4),
                       "Duran nesnenin onceki transform gecisi yalnizca o slotun transform dilimini yuklemelidir.");
        sceneData.CommitSubmitted();

        // Kare 5: Tekrar tamamen statik kare
        sceneData.Upload(recs, emptyChangeSet);
        TEST_CHECK_MSG(suite, "G13_Upload_Frame5_StaticAgainZeroUpload",
                       sceneData.GetLastPrimitiveUploadBytes() == 0 &&
                       sceneData.GetLastTransformUploadBytes() == 0,
                       "Nesne durduktan sonraki ardil karede tekrar 0 bayt yuklenmelidir.");
        sceneData.CommitSubmitted();

        // 14.3. Yeniden Siralama (Reorder) Fallback Tam Yukleme
        std::swap(recs[0], recs[1]);
        sceneData.Upload(recs, emptyChangeSet);
        TEST_CHECK_MSG(suite, "G13_Upload_ReorderFallbackFullUpload",
                       sceneData.GetLastPrimitiveUploadBytes() == expectedPrimBytes,
                       "surfaceId siralamasi degistiginde tam yukleme fallback'i tetiklenmelidir.");
        sceneData.CommitSubmitted();

        // 14.4. Silinen nesne kimliginin Commit sonrasi temizlenmesi
        recs.pop_back();
        sceneData.Upload(recs, emptyChangeSet);
        sceneData.CommitSubmitted();

        SDFPrimitiveRecord readded{};
        glm::mat4 tC_readded = glm::translate(glm::mat4(1.0f), glm::vec3(99.0f, 0.0f, 0.0f));
        readded.surfaceId = 30;
        readded.invTransform = glm::inverse(tC_readded);
        recs.push_back(readded);

        sceneData.Upload(recs, emptyChangeSet);
        const auto* prevMatPtr = static_cast<const glm::mat4*>(sceneData.GetPrevTransformBuffer()->GetMappedData());
        TEST_CHECK_MSG(suite, "G13_Upload_DeletedEntityCleanupOnReadd",
                       prevMatPtr != nullptr && prevMatPtr[2] == tC_readded,
                       "Silinip tekrar eklenen nesne bayat gecmis yerine yeni transformu ile baslamalidir.");
    }
}
} // namespace

int main(int, char**) {
    std::cout << "================================================================================\n";
    std::cout << "           AstralEngine Renderer Lifecycle & Transition GPU Tests               \n";
    std::cout << "================================================================================\n\n";

    auto& runner = Astral::Test::TestRunner::Instance();
    runner.RunSuite("Renderer Lifecycle GPU Suite", RunLifecycleTests);
    return runner.PrintSummary();
}
