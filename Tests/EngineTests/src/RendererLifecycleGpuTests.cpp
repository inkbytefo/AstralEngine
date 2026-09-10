#include "TestFramework.hpp"
#include "AstralEngine.h"
#include "Astral/Renderer/SDFRenderer.hpp"
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
        vk::ImageMemoryBarrier barrier{};
        barrier.oldLayout = vk::ImageLayout::eGeneral;
        barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = renderer.GetStorageImage();
        barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
        barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer,
                            {}, {}, {}, barrier);
        vk::BufferImageCopy region{};
        region.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1};
        region.imageExtent = vk::Extent3D(renderer.GetWidth(), renderer.GetHeight(), 1);
        cmd.copyImageToBuffer(renderer.GetStorageImage(), vk::ImageLayout::eTransferSrcOptimal,
                              buffer.GetBuffer(), region);
        std::swap(barrier.oldLayout, barrier.newLayout);
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands,
                            {}, {}, {}, barrier);
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
