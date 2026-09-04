#include "TestFramework.hpp"
#include "AstralEngine.h"
#include <iostream>

namespace Astral::Test {

class GpuSmokeTestApp : public Astral::Application {
public:
    GpuSmokeTestApp(int maxFrames = 5)
        : Astral::Application(BuildConfig(maxFrames)) {}

private:
    static Astral::AppConfig BuildConfig(int maxFrames) {
        Astral::AppConfig config;
        config.maxFrames = maxFrames;
        config.width = 1280;
        config.height = 720;
        config.enableTAA = true;
        config.useGrid = true;
        config.optShadow = true;
        return config;
    }
};

void RunGpuSmokeTest(int frames = 5) {
    const std::string suite = "GpuSmokeTestSuite";
    std::cout << "  [INFO] Vulkan 1.4 GPU & Compute Raymarching Smoke Test baslatiliyor (" << frames << " kare)...\n";

    bool appCompletedSuccessfully = false;
    try {
        GpuSmokeTestApp testApp(frames);
        testApp.Run();
        appCompletedSuccessfully = true;
    } catch (const std::exception& e) {
        std::cerr << "  [ERROR] GPU Smoke Test istisna firlatti: " << e.what() << "\n";
    }

    TEST_CHECK_MSG(suite, "GpuSmokeTestExecution", appCompletedSuccessfully,
                   "Vulkan 1.4 SDF Compute ve ana pencere dongusu hatasiz tamamlanmali!");
}

} // namespace Astral::Test
