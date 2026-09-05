#include "TestFramework.hpp"
#include "AstralEngine.h"
#include <iostream>

namespace Astral::Test {

class GpuSmokeTestApp : public Astral::Application {
public:
    explicit GpuSmokeTestApp(int maxFrames = 5)
        : Astral::Application(BuildConfig(maxFrames, "")) {}

    GpuSmokeTestApp(int maxFrames, const std::string& shaderPath)
        : Astral::Application(BuildConfig(maxFrames, shaderPath)) {}

private:
    std::shared_ptr<Scene> CreateInitialScene() override {
        // Exercise the compute path explicitly; the base application is now empty.
        auto scene = std::make_shared<Scene>("GPU Smoke");
        auto sphere = scene->CreateEntity();
        sphere.AddComponent<TransformComponent>();
        sphere.AddComponent<SDFComponent>();
        auto camera = scene->CreateEntity();
        camera.AddComponent<TransformComponent>(glm::vec3(0, 0, 4));
        camera.AddComponent<CameraComponent>(glm::radians(60.0f), 0.01f, 50.0f, 1u);
        return scene;
    }

    static Astral::AppConfig BuildConfig(int maxFrames, const std::string& shaderPath) {
        Astral::AppConfig config;
        config.maxFrames = maxFrames;
        config.width = 1280;
        config.height = 720;
        config.enableTAA = true;
        config.useGrid = true;
        config.optShadow = true;
        config.shaderPath = shaderPath;
        return config;
    }
};

void RunGpuSmokeTest(int frames = 5) {
    const std::string suite = "GpuSmokeTestSuite";
    std::cout << "  [INFO] Vulkan 1.4 GPU & Compute Raymarching Smoke Test baslatiliyor (" << frames << " kare)...\n";

    // 1. Pozitif Test (Happy Path): Gecerli sahne ve shader ile istenen kare sayisinin render edilmesi
    bool appCompletedSuccessfully = false;
    uint32_t framesRendered = 0;
    try {
        GpuSmokeTestApp testApp(frames);
        testApp.Run();
        framesRendered = testApp.GetTotalFramesRendered();
        appCompletedSuccessfully = (framesRendered == static_cast<uint32_t>(frames));
    } catch (const std::exception& e) {
        std::cerr << "  [ERROR] GPU Smoke Test istisna firlatti: " << e.what() << "\n";
    }

    TEST_CHECK_MSG(suite, "GpuSmokeTestExecution", appCompletedSuccessfully,
                   "Vulkan 1.4 SDF Compute ve ana pencere dongusu hatasiz tamamlanmali!");
    TEST_CHECK_MSG(suite, "GpuSmokeTestFrameCount", framesRendered == static_cast<uint32_t>(frames),
                   "Render edilen toplam kare sayisi istenen hedef kare sayisina tam olarak esit olmali!");

    // 2. Negatif Test (Fault Injection): Gecersiz shader yolu ile hata firlatma ve guvenli temizleme kontrolu
    std::cout << "  [INFO] Negatif Test: Gecersiz shader yolu ile hata sozlesmesi ve temizlik kontrol ediliyor...\n";
    bool failureCaught = false;
    try {
        GpuSmokeTestApp invalidApp(frames, "non_existent_shader_for_testing.spv");
        invalidApp.Run();
    } catch (const std::runtime_error& e) {
        failureCaught = true;
        std::cout << "  [INFO] Beklenen calisma zamani hatasi basariyla yakalandi: " << e.what() << "\n";
    } catch (const std::exception& e) {
        failureCaught = true;
        std::cout << "  [INFO] Beklenen genel istisna basariyla yakalandi: " << e.what() << "\n";
    }

    TEST_CHECK_MSG(suite, "GpuSmokeTestExpectedFailureOnInvalidShader", failureCaught,
                   "Gecersiz shader yapilandirmasinda Run() istisnayi disariya firlatmali!");
}

} // namespace Astral::Test
