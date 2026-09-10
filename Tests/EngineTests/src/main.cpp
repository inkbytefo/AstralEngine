namespace Astral::Test { void RunVisualQualityTests(); }
#include "TestFramework.hpp"
#include <iostream>
#include <string>
#include <vector>

namespace Astral::Test {
    void RunCameraTests();
    void RunEcsTests();
    void RunPhysicsPipelineTests();
    void RunGenerationalIdentityTests();
    void RunSceneTests();
    void RunSerializationTests();
    void RunBrickGridTests();
    void RunCommandStackTests();
    void RunEventBusTests();
    void RunActionMapTests();
    void RunVmaTests(bool runGpu);
    void RunJobSystemTests();
    void RunTaskGraphTests();
    void RunProjectTests();
    void RunGpuSmokeTest(int frames);
    void RunApplicationLoopTests();
    void RunServiceBoundariesTests();
    void RunEditorGameplayCycleTests();
    void RunSDFShapeTests();
    void RunSDFContractTests();
    void RunSDFContractGpuTests();
    void RunDeferredLightingTests();
    void RunSDFTemporalTests();
    void RunSDFChangeSetTests();
    void RunRendererArchitectureTests();
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
    bool runBrickGrid = false;
    bool runCommand = false;
    bool runEventBus = false;
    bool runActionMap = false;
    bool runVma = false;
    bool runJobSystem = false;
    bool runTaskGraph = false;
    bool runProject = false;
    bool runLoop = false;
    bool runBoundaries = false;
    bool runGameplay = false;
    bool runContract = false;
    bool runLighting = false;
    bool runTemporal = false;
    bool runChangeSet = false;
    bool runRenderer = false;
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
                      << "  --contract           Yalnizca SDF Contract testlerini calistirir (CPU & GPU)\n"
                      << "  --lighting           Yalnizca Deferred SDF Shadows & AO testlerini calistirir\n"
                      << "  --temporal           Yalnizca SDF Temporal Confidence & Rejection testlerini calistirir\n"
                      << "  --renderer           Yalnizca Renderer Architecture CPU testlerini calistirir\n"
                      << "  --ecs                Yalnizca ECS testlerini calistirir\n"
                      << "  --physics            Yalnizca Physics Pipeline testlerini calistirir\n"
                      << "  --identity           Yalnizca Generational Entity Handle testlerini calistirir\n"
                      << "  --scene              Yalnizca Scene Management testlerini calistirir\n"
                      << "  --serialization      Yalnizca DOD Binary Serialization testlerini calistirir\n"
                      << "  --brickgrid          Yalnizca Two-Level BrickGrid testlerini calistirir\n"
                      << "  --command            Yalnizca Undo/Redo Command-Stack testlerini calistirir\n"
                      << "  --eventbus           Yalnizca EventBus mimarisi testlerini calistirir\n"
                      << "  --actionmap          Yalnizca ActionMap & Enhanced Input testlerini calistirir\n"
                      << "  --vma                Yalnizca VMA bellek yonetimi testlerini calistirir\n"
                      << "  --jobs               Yalnizca C++20 JobSystem testlerini calistirir\n"
                      << "  --taskgraph          Yalnizca TaskGraph DAG planlayici testlerini calistirir\n"
                      << "  --project            Yalnizca Project & ProjectSerializer testlerini calistirir\n"
                      << "  --loop               Yalnizca Application Loop & Fixed-Step Pipeline testlerini calistirir\n"
                      << "  --boundaries         Yalnizca Service Boundaries & Headless Simulation testlerini calistirir\n"
                      << "  --gameplay           Yalnizca Editor Gameplay Cycle & SDF Puzzle testlerini calistirir\n"
                      << "  --help, -h           Bu yardim mesajini gosterir\n";
            return 0;
        } else if (arg == "--all") {
            runEcs = runPhysics = runIdentity = runScene = runSerialization = runBrickGrid = runCommand = runEventBus = runActionMap = runVma = runJobSystem = runTaskGraph = runProject = runLoop = runBoundaries = runGameplay = runContract = runRenderer = runGpu = true;
            hasSpecificFlag = true;
        } else if (arg == "--contract") {
            runContract = true;
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
        } else if (arg == "--brickgrid") {
            runBrickGrid = true;
            hasSpecificFlag = true;
        } else if (arg == "--command") {
            runCommand = true;
            hasSpecificFlag = true;
        } else if (arg == "--eventbus") {
            runEventBus = true;
            hasSpecificFlag = true;
        } else if (arg == "--actionmap") {
            runActionMap = true;
            hasSpecificFlag = true;
        } else if (arg == "--vma") {
            runVma = true;
            hasSpecificFlag = true;
        } else if (arg == "--jobs") {
            runJobSystem = true;
            hasSpecificFlag = true;
        } else if (arg == "--taskgraph") {
            runTaskGraph = true;
            hasSpecificFlag = true;
        } else if (arg == "--project") {
            runProject = true;
            hasSpecificFlag = true;
        } else if (arg == "--loop") {
            runLoop = true;
            hasSpecificFlag = true;
        } else if (arg == "--boundaries") {
            runBoundaries = true;
            hasSpecificFlag = true;
        } else if (arg == "--gameplay") {
            runGameplay = true;
            hasSpecificFlag = true;
        } else if (arg == "--lighting") {
            runLighting = true;
            hasSpecificFlag = true;
        } else if (arg == "--temporal") {
            runTemporal = true;
            hasSpecificFlag = true;
        } else if (arg == "--changeset") {
            runChangeSet = true;
            hasSpecificFlag = true;
        } else if (arg == "--renderer") {
            runRenderer = true;
            hasSpecificFlag = true;
        } else {
            std::cerr << "[HATA] Bilinmeyen secenek: " << arg << "\n";
            std::cerr << "Yardim icin: EngineTests.exe --help\n";
            return 1;
        }
    }

    // Varsayilan davranis: Eger ozel bir bayrak verilmediyse tum headless testler calistirilir (CI guvenli)
    if (!hasSpecificFlag) {
        runEcs = runPhysics = runIdentity = runScene = runSerialization = runBrickGrid = runCommand = runEventBus = runActionMap = runVma = runJobSystem = runTaskGraph = runProject = runLoop = runBoundaries = runGameplay = runContract = runLighting = runTemporal = runChangeSet = runRenderer = true;
    }

    auto& runner = Astral::Test::TestRunner::Instance();
    if (runScene) runner.RunSuite("Visual Quality CPU Suite", Astral::Test::RunVisualQualityTests);
    if (runScene) runner.RunSuite("Camera & Client Scene Suite", Astral::Test::RunCameraTests);

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
        runner.RunSuite("SDF Shape & Explicit Dimensions Suite", Astral::Test::RunSDFShapeTests);
    }
    if (runBrickGrid) {
        runner.RunSuite("Two-Level Spatial BrickGrid Acceleration Suite", Astral::Test::RunBrickGridTests);
    }
    if (runCommand) {
        runner.RunSuite("Undo/Redo Command-Stack Architecture Suite", Astral::Test::RunCommandStackTests);
    }
    if (runEventBus) {
        runner.RunSuite("Type-Safe EventBus Architecture Suite", Astral::Test::RunEventBusTests);
    }
    if (runActionMap) {
        runner.RunSuite("Action-Mapping & Enhanced Input Suite", Astral::Test::RunActionMapTests);
    }
    if (runVma) {
        runner.RunSuite("Vulkan Memory Allocator (VMA) Architecture Suite", [runGpu]() {
            Astral::Test::RunVmaTests(runGpu);
        });
    }
    if (runJobSystem) {
        runner.RunSuite("Modern C++20 JobSystem Architecture Suite", Astral::Test::RunJobSystemTests);
    }
    if (runTaskGraph) {
        runner.RunSuite("DAG TaskGraph Frame Scheduling Suite", Astral::Test::RunTaskGraphTests);
    }
    if (runProject) {
        runner.RunSuite("Project & ProjectSerializer Management Suite", Astral::Test::RunProjectTests);
    }
    if (runLoop) {
        runner.RunSuite("Application Loop & Fixed-Step Pipeline Suite", Astral::Test::RunApplicationLoopTests);
    }
    if (runBoundaries) {
        runner.RunSuite("Service Boundaries & Headless Simulation Suite", Astral::Test::RunServiceBoundariesTests);
    }
    if (runGameplay) {
        runner.RunSuite("Editor Gameplay Cycle & Reference Playable Suite", Astral::Test::RunEditorGameplayCycleTests);
    }
    if (runContract) {
        runner.RunSuite("SDF Contract Suite", Astral::Test::RunSDFContractTests);
        if (runGpu || hasSpecificFlag) {
            runner.RunSuite("SDF Contract GPU Suite", Astral::Test::RunSDFContractGpuTests);
        }
    }
    if (runLighting) {
        runner.RunSuite("Deferred SDF Shadows & AO Suite", Astral::Test::RunDeferredLightingTests);
    }
    if (runTemporal) {
        runner.RunSuite("SDF Temporal Confidence Suite", Astral::Test::RunSDFTemporalTests);
    }
    if (runChangeSet) {
        runner.RunSuite("SDF Local ChangeSet & Invalidation Suite", Astral::Test::RunSDFChangeSetTests);
    }
    if (runRenderer) {
        runner.RunSuite("Renderer Architecture Suite", Astral::Test::RunRendererArchitectureTests);
    }
    if (runGpu) {
        runner.RunSuite("Vulkan 1.4 GPU & SDF Compute Smoke Suite", [gpuFrames]() {
            Astral::Test::RunGpuSmokeTest(gpuFrames);
        });
    }

    return runner.PrintSummary();
}
