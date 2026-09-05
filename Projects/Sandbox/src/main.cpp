#include "AstralEngine.h"
#include "DemoScene.hpp"
#include "PuzzleGameSubsystem.hpp"

#include <iostream>
#include <string>

struct SandboxOptions { Astral::AppConfig engine; bool stress = false; bool puzzle = true; };

static SandboxOptions ParseCommandLine(const Astral::CommandLineArgs& args) {
    SandboxOptions options;
    auto& config = options.engine;
    int maxFrames = -1;

    for (int i = 1; i < args.argc; ++i) {
        std::string arg = args.argv[i];
        if (arg == "--bench") {
            config.benchMode = true;
            options.puzzle = false;
        } else if (arg == "--bench-frames" && i + 1 < args.argc) {
            config.benchFrames = std::stoi(args.argv[++i]);
            config.benchMode = true;
            options.puzzle = false;
        } else if (arg == "--bench-out" && i + 1 < args.argc) {
            config.benchOutputFile = args.argv[++i];
            config.benchMode = true;
        } else if (arg == "--frames" && i + 1 < args.argc) {
            maxFrames = std::stoi(args.argv[++i]);
        } else if (arg == "--width" && i + 1 < args.argc) {
            config.width = std::stoi(args.argv[++i]);
        } else if (arg == "--height" && i + 1 < args.argc) {
            config.height = std::stoi(args.argv[++i]);
        } else if (arg == "--normal" && i + 1 < args.argc) {
            std::string mode = args.argv[++i];
            if (mode == "tetra" || mode == "tetrahedron" || mode == "1") {
                config.normalMode = 1;
            } else {
                config.normalMode = 0;
            }
        } else if (arg == "--shader" && i + 1 < args.argc) {
            config.shaderPath = args.argv[++i];
        } else if (arg == "--legacy-map") {
            config.legacyMap = true;
        } else if (arg == "--grid") {
            config.useGrid = true;
        } else if (arg == "--no-grid") {
            config.useGrid = false;
        } else if (arg == "--stress") {
            options.stress = true;
            options.puzzle = false;
        } else if (arg == "--demo") {
            options.puzzle = false;
        } else if (arg == "--puzzle") {
            options.puzzle = true;
        } else if (arg == "--opt-shadow") {
            config.optShadow = true;
        } else if (arg == "--no-opt-shadow") {
            config.optShadow = false;
        } else if (arg == "--taa") {
            config.enableTAA = true;
        } else if (arg == "--no-taa") {
            config.enableTAA = false;
        } else if (arg == "--gbuffer") {
            config.useGBuffer = true;
        } else if (arg == "--no-gbuffer") {
            config.useGBuffer = false;
        } else if (arg == "--debug-mode" && i + 1 < args.argc) {
            config.debugMode = std::stoi(args.argv[++i]);
        }
    }

    config.maxFrames = maxFrames;
    config.simulatePhysics = config.benchMode || maxFrames > 0;
    if (!options.puzzle && config.simulatePhysics) {
        // The benchmark fixture preserves its historical frame-indexed animation.
        config.fixedTimeStep = 0.016f;
        config.fixedDeltaTime = 0.016f;
    }
    return options;
}

/// Sandbox uygulamasi — AstralEngine'in ilk somut istemcisi (client).
/// Application sinifindan turer ve saf bir runtime/oyun simulasyonu sunar.
class SandboxApp : public Astral::Application {
public:
    SandboxApp()
        : SandboxApp(ParseCommandLine(Astral::GetCommandLineArgs())) {}
private:
    explicit SandboxApp(const SandboxOptions& options)
        : Astral::Application(options.engine), m_Stress(options.stress), m_Puzzle(options.puzzle) {
        std::cout << "[SandboxApp] Sandbox Calisma Zamani Uygulamasi baslatildi (Mod: "
                  << (m_Puzzle ? "SDF Bulmaca Referans Oyunu" : "Standart Demo") << ").\n";
    }
protected:
    std::shared_ptr<Astral::Scene> CreateInitialScene() override {
        if (m_Puzzle) {
            return Sandbox::PuzzleGameSubsystem::CreatePuzzleScene();
        }
        return m_Demo.Create(m_Stress);
    }
    void OnInitialize() override {
        if (m_Puzzle) {
            PushSystem<Sandbox::PuzzleGameSubsystem>();
        }
    }
    void OnUpdate(Astral::FrameContext&, uint32_t frameIndex) override {
        if (!m_Puzzle && GetConfig().simulatePhysics) {
            m_Demo.Update(*GetActiveScene(), m_Stress, frameIndex);
            if (frameIndex == 3) RequestPick(GetConfig().width / 2, GetConfig().height / 2);
            if (GetLastPickResult().hasHit) SetHighlightEntity(GetLastPickResult().hitEntity);
        }
    }
private:
    Sandbox::DemoScene m_Demo;
    bool m_Stress = false;
    bool m_Puzzle = true;
};

namespace Astral {

/// Motorun giris noktasi (EntryPoint) tarafindan cagirilan istemci sozlesme fonksiyonu.
Application* CreateApplication() {
    return new SandboxApp();
}

} // namespace Astral

// Giris noktasi sozlesmesi: main() fonksiyonunu bu translation unit'e dahil eder.
#include "Astral/EntryPoint.hpp"
