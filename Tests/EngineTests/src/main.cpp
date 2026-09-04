#include "TestFramework.hpp"
#include <iostream>
#include <string>
#include <vector>

namespace Astral::Test {
    void RunEcsTests();
    void RunPhysicsPipelineTests();
    void RunGenerationalIdentityTests();
    void RunSceneTests();
    void RunSerializationTests();
    void RunGpuSmokeTest(int frames);
}

int main(int argc, char** argv) {
    std::cout << "================================================================================\n";
    std::cout << "               AstralEngine Standalone Regression Test Runner                   \n";
    std::cout << "================================================================================\n\n";

    bool runEcs = false;
    bool runPhysics = false;
    bool runIdentity = false;
    bool runScene = false;
    bool runSerialization = false;
    bool runGpu = false;
    int gpuFrames = 5;

    bool hasSpecificFlag = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Kullanim: EngineTests.exe [secenekler]\n"
                      << "Secenekler:\n"
                      << "  (argumansiz)         Tüm Headless regresyon testlerini calistirir (CI dostu)\n"
                      << "  --all                Tüm Headless testleri ve GPU smoke testini calistirir\n"
                      << "  --gpu                Yalnizca Vulkan 1.4 GPU & Compute smoke testini calistirir\n"
                      << "  --gpu-frames <N>     GPU testi icin calistirilacak kare sayisi (varsayilan: 5)\n"
                      << "  --ecs                Yalnizca ECS testlerini calistirir\n"
                      << "  --physics            Yalnizca Physics Pipeline testlerini calistirir\n"
                      << "  --identity           Yalnizca Generational Entity Handle testlerini calistirir\n"
                      << "  --scene              Yalnizca Scene Management testlerini calistirir\n"
                      << "  --serialization      Yalnizca DOD Binary Serialization testlerini calistirir\n"
                      << "  --help, -h           Bu yardim mesajini gosterir\n";
            return 0;
        } else if (arg == "--all") {
            runEcs = runPhysics = runIdentity = runScene = runSerialization = runGpu = true;
            hasSpecificFlag = true;
        } else if (arg == "--gpu") {
            runGpu = true;
            hasSpecificFlag = true;
        } else if (arg == "--gpu-frames" && i + 1 < argc) {
            gpuFrames = std::stoi(argv[++i]);
        } else if (arg == "--ecs") {
            runEcs = true;
            hasSpecificFlag = true;
        } else if (arg == "--physics") {
            runPhysics = true;
            hasSpecificFlag = true;
        } else if (arg == "--identity") {
            runIdentity = true;
            hasSpecificFlag = true;
        } else if (arg == "--scene") {
            runScene = true;
            hasSpecificFlag = true;
        } else if (arg == "--serialization") {
            runSerialization = true;
            hasSpecificFlag = true;
        }
    }

    // Varsayilan davranis: Eger ozel bir bayrak verilmediyse tum headless testler calistirilir (CI guvenli)
    if (!hasSpecificFlag) {
        runEcs = runPhysics = runIdentity = runScene = runSerialization = true;
    }

    auto& runner = Astral::Test::TestRunner::Instance();

    if (runEcs) {
        runner.RunSuite("ECS Architecture Suite", Astral::Test::RunEcsTests);
    }
    if (runPhysics) {
        runner.RunSuite("Physics & Extraction Pipeline Suite", Astral::Test::RunPhysicsPipelineTests);
    }
    if (runIdentity) {
        runner.RunSuite("Generational Identity & Lifetime Suite", Astral::Test::RunGenerationalIdentityTests);
    }
    if (runScene) {
        runner.RunSuite("Scene Management & Deep-Copy Suite", Astral::Test::RunSceneTests);
    }
    if (runSerialization) {
        runner.RunSuite("DOD Binary Scene Serialization Suite (v2)", Astral::Test::RunSerializationTests);
    }
    if (runGpu) {
        runner.RunSuite("Vulkan 1.4 GPU & SDF Compute Smoke Suite", [gpuFrames]() {
            Astral::Test::RunGpuSmokeTest(gpuFrames);
        });
    }

    return runner.PrintSummary();
}
